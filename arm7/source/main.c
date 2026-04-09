#include <nds.h>
#include <stdbool.h>
#include <string.h>

#define MINIMP3_IMPLEMENTATION
#include "../../include/minimp3.h"
#include "../../include/audio_ipc.h"

#define MUSIC_CHANNEL 0
#define PCM_SAMPLE_RATE_DEFAULT 22050
#define PCM_RING_SAMPLES 8192
#define PCM_HALF_SAMPLES (PCM_RING_SAMPLES / 2)
#define INPUT_BUFFER_SIZE 8192

static volatile AudioSharedState* gShared = (volatile AudioSharedState*)AUDIO_SHARED_ADDR;

static mp3dec_t gDecoder;
static uint8_t gInputBuffer[INPUT_BUFFER_SIZE];
static int gInputBytes = 0;

static int16_t gPcmRing[PCM_RING_SAMPLES] __attribute__((aligned(4)));
static uint32_t gPlaybackRate = PCM_SAMPLE_RATE_DEFAULT;
static uint32_t gSampleFrac = 0;
static uint32_t gPlaybackPos = 0;
static int gLastHalf = 0;

static inline uint32_t ring_used(void) {
    const uint32_t r = gShared->ring_read_pos;
    const uint32_t w = gShared->ring_write_pos;
    return (w >= r) ? (w - r) : (AUDIO_RING_SIZE - (r - w));
}

static uint32_t pull_compressed(uint8_t* dst, uint32_t max_bytes) {
    uint32_t copied = 0;
    while (copied < max_bytes) {
        const uint32_t available = ring_used();
        if (available == 0) break;

        const uint32_t r = gShared->ring_read_pos;
        uint32_t chunk = available;
        const uint32_t contiguous = AUDIO_RING_SIZE - r;
        if (chunk > contiguous) chunk = contiguous;
        if (chunk > (max_bytes - copied)) chunk = max_bytes - copied;

        memcpy(dst + copied, (const void*)(gShared->compressed_ring + r), chunk);
        gShared->ring_read_pos = (r + chunk) % AUDIO_RING_SIZE;
        copied += chunk;
    }
    return copied;
}

static void reset_decoder_state(void) {
    mp3dec_init(&gDecoder);
    gInputBytes = 0;
    gShared->arm7_track_serial = gShared->track_serial;
    gShared->decoder_reset = 0;
    gShared->arm7_playing = 0;
}

static void init_stream_channel(void) {
    SCHANNEL_CR(MUSIC_CHANNEL) = 0;
    SCHANNEL_SOURCE(MUSIC_CHANNEL) = (u32)gPcmRing;
    SCHANNEL_TIMER(MUSIC_CHANNEL) = SOUND_FREQ(gPlaybackRate);
    SCHANNEL_REPEAT_POINT(MUSIC_CHANNEL) = 0;
    SCHANNEL_LENGTH(MUSIC_CHANNEL) = (PCM_RING_SAMPLES * sizeof(int16_t)) >> 2;
    SCHANNEL_CR(MUSIC_CHANNEL) =
        SCHANNEL_ENABLE |
        SOUND_REPEAT |
        SOUND_VOL(96) |
        SOUND_PAN(64) |
        SOUND_FORMAT_16BIT;
}

static void fill_silence(int half_index) {
    int16_t* dst = &gPcmRing[half_index ? PCM_HALF_SAMPLES : 0];
    for (int i = 0; i < PCM_HALF_SAMPLES; ++i) dst[i] = 0;
}

static int decode_samples_into_half(int half_index) {
    int16_t* dst = &gPcmRing[half_index ? PCM_HALF_SAMPLES : 0];
    int produced = 0;

    while (produced < PCM_HALF_SAMPLES) {
        if (gShared->decoder_reset || gShared->arm7_track_serial != gShared->track_serial) {
            reset_decoder_state();
        }

        if (gInputBytes < (INPUT_BUFFER_SIZE / 2)) {
            const uint32_t got = pull_compressed(gInputBuffer + gInputBytes, INPUT_BUFFER_SIZE - gInputBytes);
            gInputBytes += (int)got;
        }

        if (gInputBytes <= 0) {
            if (gShared->source_eof) break;
            gShared->decode_underruns++;
            break;
        }

        mp3dec_frame_info_t info;
        int16_t pcm_tmp[MINIMP3_MAX_SAMPLES_PER_FRAME];
        const int samples = mp3dec_decode_frame(&gDecoder, gInputBuffer, gInputBytes, pcm_tmp, &info);

        if (info.frame_bytes > 0) {
            gInputBytes -= info.frame_bytes;
            if (gInputBytes > 0) {
                memmove(gInputBuffer, gInputBuffer + info.frame_bytes, (size_t)gInputBytes);
            }
        }

        if (samples <= 0) {
            if (info.frame_bytes <= 0) {
                if (gShared->source_eof) break;
                gShared->decode_errors++;
                break;
            }
            continue;
        }

        if (info.hz > 0) {
            gPlaybackRate = (uint32_t)info.hz;
            gShared->last_sample_rate = gPlaybackRate;
            SCHANNEL_TIMER(MUSIC_CHANNEL) = SOUND_FREQ(gPlaybackRate);
        }

        if (info.channels <= 1) {
            int copy_count = samples;
            if (copy_count > (PCM_HALF_SAMPLES - produced)) copy_count = PCM_HALF_SAMPLES - produced;
            memcpy(dst + produced, pcm_tmp, (size_t)copy_count * sizeof(int16_t));
            produced += copy_count;
        } else {
            const int frames = samples / info.channels;
            int copy_frames = frames;
            if (copy_frames > (PCM_HALF_SAMPLES - produced)) copy_frames = PCM_HALF_SAMPLES - produced;

            for (int i = 0; i < copy_frames; ++i) {
                const int l = pcm_tmp[i * 2 + 0];
                const int r = pcm_tmp[i * 2 + 1];
                dst[produced++] = (int16_t)((l + r) / 2);
            }
        }
    }

    for (int i = produced; i < PCM_HALF_SAMPLES; ++i) dst[i] = 0;
    return produced;
}

static void update_stream(void) {
    if (!gShared || gShared->magic != AUDIO_SHARED_MAGIC) return;

    if (!gShared->music_enabled) {
        fill_silence(0);
        fill_silence(1);
        gShared->arm7_playing = 0;
        return;
    }

    gSampleFrac += gPlaybackRate;
    gPlaybackPos = (gPlaybackPos + (gSampleFrac / 60)) % PCM_RING_SAMPLES;
    gSampleFrac %= 60;

    const int current_half = (gPlaybackPos >= PCM_HALF_SAMPLES) ? 1 : 0;
    if (current_half != gLastHalf) {
        const int free_half = gLastHalf;
        const int produced = decode_samples_into_half(free_half);
        gShared->arm7_playing = (produced > 0) ? 1u : 0u;
        gLastHalf = current_half;
    }

    if (!gShared->arm7_playing && gShared->source_eof && ring_used() == 0) {
        fill_silence(0);
        fill_silence(1);
    }
}

static void VcountHandler(void) {
    /* Legacy libnds ARM7 builds used inputGetAndSend() here.
       Your current toolchain does not provide that symbol. */
}

static void VblankHandler(void) {
    update_stream();
}

int main(void) {
    /* Minimal standalone ARM7 init for current libnds toolchains. */

    powerOn(POWER_SOUND);
    REG_SOUNDCNT = SOUND_ENABLE | SOUND_VOL(0x7F);

    memset((void*)gShared, 0, sizeof(AudioSharedState));
    gShared->magic = AUDIO_SHARED_MAGIC;
    gShared->ring_size = AUDIO_RING_SIZE;
    gShared->arm7_ready = 1;
    gShared->music_enabled = 0;

    reset_decoder_state();
    fill_silence(0);
    fill_silence(1);
    init_stream_channel();

    irqSet(IRQ_VBLANK, VblankHandler);
    irqEnable(IRQ_VBLANK);

    while (1) {
        swiWaitForVBlank();
    }

    return 0;
}
