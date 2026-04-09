#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AUDIO_SHARED_MAGIC 0x41554430u
#define AUDIO_RING_SIZE (64 * 1024)
#define AUDIO_SHARED_ADDR ((uintptr_t)0x027E0000)

typedef struct AudioSharedState {
    volatile uint32_t magic;
    volatile uint32_t music_enabled;
    volatile uint32_t arm7_ready;
    volatile uint32_t arm7_playing;

    volatile uint32_t track_serial;
    volatile uint32_t arm7_track_serial;
    volatile uint32_t decoder_reset;
    volatile uint32_t source_eof;

    volatile uint32_t ring_size;
    volatile uint32_t ring_read_pos;
    volatile uint32_t ring_write_pos;

    volatile uint32_t decode_underruns;
    volatile uint32_t decode_errors;
    volatile uint32_t last_sample_rate;

    volatile uint32_t reserved[17];
    volatile uint8_t compressed_ring[AUDIO_RING_SIZE];
} AudioSharedState;

static inline volatile AudioSharedState* audioSharedState(void) {
    return (volatile AudioSharedState*)AUDIO_SHARED_ADDR;
}

#ifdef __cplusplus
}
#endif
