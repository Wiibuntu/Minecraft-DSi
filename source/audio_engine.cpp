#include "audio_engine.h"

#include <nds.h>
#include <filesystem.h>

#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#include "audio_ipc.h"
#include "music_data.h"
#include "music_track_pcm.h"

namespace {

enum SfxId {
    SFX_UI_CLICK = 0,
    SFX_UI_BACK,
    SFX_UI_ACCEPT,
    SFX_PLACE,
    SFX_BREAK,
    SFX_HURT,
    SFX_MOB_HIT,
    SFX_COUNT
};

static bool gNitroReady = false;
static bool gAudioReady = false;
static bool gMusicEnabled = true;
static int gCurrentTrack = -1;
static int gMusicChannel = -1;
static int gSfxCooldownMs[SFX_COUNT] = {0};
static char gCurrentTrackName[96] = "NO MUSIC";
static char gAudioStatus[96] = "AUDIO OFF";
static volatile AudioSharedState* gShared = audioSharedState();
static u8* gLoadedMusicData = nullptr;
static u32 gLoadedMusicSize = 0;
static u16 gLoadedMusicRate = 0;


static void copyString(char* out, size_t outSize, const char* src) {
    if (!out || outSize == 0) return;
    std::snprintf(out, outSize, "%s", src ? src : "");
}

static void setStatus(const char* text) {
    copyString(gAudioStatus, sizeof(gAudioStatus), text);
}

static void freeLoadedMusic() {
    if (gLoadedMusicData) {
        delete[] gLoadedMusicData;
        gLoadedMusicData = nullptr;
    }
    gLoadedMusicSize = 0;
    gLoadedMusicRate = 0;
}

static void closeTrackFile() {
    if (gMusicChannel >= 0) {
        soundKill(gMusicChannel);
        gMusicChannel = -1;
    }
    freeLoadedMusic();
}

static int randomTrackIndex(int exclude) {
    const int trackCount = getMusicTrackCount();
    if (trackCount <= 0) return -1;
    if (trackCount == 1) return 0;

    int idx = std::rand() % trackCount;
    if (idx == exclude) idx = (idx + 1 + (std::rand() % (trackCount - 1))) % trackCount;
    return idx;
}

static void initSharedAudioState() {
    if (!gShared) return;
    std::memset((void*)gShared, 0, sizeof(AudioSharedState));
    gShared->magic = AUDIO_SHARED_MAGIC;
    gShared->ring_size = AUDIO_RING_SIZE;
    gShared->music_enabled = 0;
    gShared->decoder_reset = 0;
    gShared->track_serial = 0;
    gShared->arm7_ready = 0;
    gShared->arm7_playing = 0;
}

static bool loadMusicFromNitro(const char* path, u32 rate) {
    if (!path || !*path) return false;

    FILE* fp = std::fopen(path, "rb");
    if (!fp) return false;

    if (std::fseek(fp, 0, SEEK_END) != 0) {
        std::fclose(fp);
        return false;
    }
    long size = std::ftell(fp);
    if (size <= 0) {
        std::fclose(fp);
        return false;
    }
    if (std::fseek(fp, 0, SEEK_SET) != 0) {
        std::fclose(fp);
        return false;
    }

    u8* buffer = new (std::nothrow) u8[(size_t)size];
    if (!buffer) {
        std::fclose(fp);
        return false;
    }

    const size_t readCount = std::fread(buffer, 1, (size_t)size, fp);
    std::fclose(fp);
    if (readCount != (size_t)size) {
        delete[] buffer;
        return false;
    }

    gLoadedMusicData = buffer;
    gLoadedMusicSize = (u32)size;
    gLoadedMusicRate = rate ? (u16)rate : 11025;
    return true;
}

static bool openTrackByIndex(int index) {
    const int trackCount = getMusicTrackCount();
    if (trackCount <= 0) {
        setStatus("NO MUSIC FILES");
        return false;
    }
    if (index < 0 || index >= trackCount) {
        setStatus("BAD TRACK IDX");
        return false;
    }

    closeTrackFile();

    const char* musicPath = getMusicTrackPath(index);
    const u32 musicRate = getMusicTrackRate(index);
    const u8* sampleData = gBackgroundMusicData;
    u32 sampleSize = gBackgroundMusicDataSize;
    u16 sampleRate = (u16)gBackgroundMusicRate;
    SoundFormat sampleFormat = gBackgroundMusicFormat;
    bool loopEnabled = gBackgroundMusicLoop;
    u16 loopPoint = (u16)gBackgroundMusicLoopPoint;

    if (loadMusicFromNitro(musicPath, musicRate)) {
        sampleData = gLoadedMusicData;
        sampleSize = gLoadedMusicSize;
        sampleRate = gLoadedMusicRate;
        sampleFormat = SoundFormat_8Bit;
        loopEnabled = true;
        loopPoint = 0;
        sampleFormat = SoundFormat_8Bit;
        loopEnabled = true;
        loopPoint = 0;
    }

    gMusicChannel = soundPlaySample(
        sampleData,
        sampleFormat,
        sampleSize,
        sampleRate,
        96,
        64,
        loopEnabled,
        loopPoint
    );

    gCurrentTrack = index;
    copyString(gCurrentTrackName, sizeof(gCurrentTrackName), getMusicTrackName(index));
    if (gMusicChannel >= 0) setStatus(gLoadedMusicData ? "RAW PLAY" : (sampleFormat == SoundFormat_ADPCM ? "ADPCM PLAY" : "PCM PLAY"));
    else setStatus(gLoadedMusicData ? "RAW FAIL" : (sampleFormat == SoundFormat_ADPCM ? "ADPCM FAIL" : "PCM FAIL"));
    return gMusicChannel >= 0;
}

static bool openRandomTrack(bool avoidCurrent) {
    const int trackCount = getMusicTrackCount();
    if (trackCount <= 0) {
        setStatus("NO MUSIC FILES");
        return false;
    }

    const int preferred = randomTrackIndex(avoidCurrent ? gCurrentTrack : -1);
    if (preferred >= 0 && openTrackByIndex(preferred)) return true;

    for (int i = 0; i < trackCount; ++i) {
        if ((!avoidCurrent || i != gCurrentTrack) && openTrackByIndex(i)) return true;
    }

    if (avoidCurrent && gCurrentTrack >= 0 && gCurrentTrack < trackCount) {
        if (openTrackByIndex(gCurrentTrack)) return true;
    }

    setStatus("ALL TRACKS FAILED");
    return false;
}

static bool beginRandomTrack(bool avoidCurrent) {
    return openRandomTrack(avoidCurrent);
}

static void feedMusicRing() {
    /* Disabled: music now uses the same direct sample path as menu SFX. */
}

static inline u8 clampU8(int v) {

    if (v < 0) return 0;
    if (v > 255) return 255;
    return (u8)v;
}

template <size_t N>
static void fillSquare(u8 (&dst)[N], int period, int center, int amp) {
    if (period < 2) period = 2;
    for (size_t i = 0; i < N; ++i) {
        const int phase = (int)(i % (size_t)period);
        const int sample = (phase < period / 2) ? (center + amp) : (center - amp);
        dst[i] = clampU8(sample);
    }
}

template <size_t N>
static void applyFadeOut(u8 (&dst)[N], int floorValue = 128) {
    for (size_t i = 0; i < N; ++i) {
        const int mix = (int)(N - i);
        const int sample = floorValue + (((int)dst[i] - floorValue) * mix) / (int)N;
        dst[i] = clampU8(sample);
    }
}

template <size_t N>
static void mixNoise(u8 (&dst)[N], int strength, unsigned seed) {
    unsigned s = seed;
    for (size_t i = 0; i < N; ++i) {
        s = s * 1664525u + 1013904223u;
        const int noise = (int)((s >> 24) & 0xFF) - 128;
        const int sample = (int)dst[i] + (noise * strength) / 128;
        dst[i] = clampU8(sample);
    }
}

static const u8* getSfxData(SfxId id, u32& outSize, u16& outRate, u8& outVolume) {
    static bool built = false;
    static u8 uiClick[880];
    static u8 uiBack[1100];
    static u8 uiAccept[1320];
    static u8 place[1320];
    static u8 blockBreak[2200];
    static u8 hurt[1650];
    static u8 mobHit[1100];

    if (!built) {
        fillSquare(uiClick, 18, 128, 34);
        applyFadeOut(uiClick);

        fillSquare(uiBack, 24, 128, 28);
        applyFadeOut(uiBack);

        fillSquare(uiAccept, 16, 128, 36);
        for (int i = 660; i < 1320; ++i) {
            const int phase = (i - 660) % 12;
            uiAccept[i] = clampU8((phase < 6) ? 174 : 82);
        }
        applyFadeOut(uiAccept);

        fillSquare(place, 26, 128, 18);
        mixNoise(place, 12, 0x1234u);
        applyFadeOut(place);

        fillSquare(blockBreak, 8, 128, 18);
        mixNoise(blockBreak, 64, 0xBEEFu);
        applyFadeOut(blockBreak);

        fillSquare(hurt, 28, 128, 44);
        for (int i = 550; i < 1100; ++i) hurt[i] = clampU8((int)hurt[i] - 22);
        applyFadeOut(hurt);

        fillSquare(mobHit, 20, 128, 26);
        mixNoise(mobHit, 24, 0xC0DEu);
        applyFadeOut(mobHit);
        built = true;
    }

    outRate = 11025;
    switch (id) {
        case SFX_UI_CLICK: outSize = sizeof(uiClick); outVolume = 88; return uiClick;
        case SFX_UI_BACK: outSize = sizeof(uiBack); outVolume = 84; return uiBack;
        case SFX_UI_ACCEPT: outSize = sizeof(uiAccept); outVolume = 92; return uiAccept;
        case SFX_PLACE: outSize = sizeof(place); outVolume = 82; return place;
        case SFX_BREAK: outSize = sizeof(blockBreak); outVolume = 96; return blockBreak;
        case SFX_HURT: outSize = sizeof(hurt); outVolume = 96; return hurt;
        case SFX_MOB_HIT: outSize = sizeof(mobHit); outVolume = 90; return mobHit;
        default: break;
    }
    outSize = 0;
    outRate = 11025;
    outVolume = 0;
    return nullptr;
}

static void playSfx(SfxId id) {
    if (id < 0 || id >= SFX_COUNT) return;
    if (gSfxCooldownMs[id] > 0) return;

    u32 size = 0;
    u16 rate = 11025;
    u8 volume = 96;
    const u8* data = getSfxData(id, size, rate, volume);
    if (!data || size == 0) return;

    soundPlaySample(data, SoundFormat_8Bit, size, rate, volume, 64, false, 0);

    switch (id) {
        case SFX_UI_CLICK: gSfxCooldownMs[id] = 45; break;
        case SFX_UI_BACK: gSfxCooldownMs[id] = 70; break;
        case SFX_UI_ACCEPT: gSfxCooldownMs[id] = 90; break;
        case SFX_PLACE: gSfxCooldownMs[id] = 55; break;
        case SFX_BREAK: gSfxCooldownMs[id] = 75; break;
        case SFX_HURT: gSfxCooldownMs[id] = 110; break;
        case SFX_MOB_HIT: gSfxCooldownMs[id] = 75; break;
        default: gSfxCooldownMs[id] = 60; break;
    }
}

} // namespace

void initAudioEngine() {
    gNitroReady = nitroFSInit(nullptr);
    gAudioReady = false;
    gMusicEnabled = true;
    gCurrentTrack = -1;
    closeTrackFile();
    for (int i = 0; i < SFX_COUNT; ++i) gSfxCooldownMs[i] = 0;
    copyString(gCurrentTrackName, sizeof(gCurrentTrackName), "NO MUSIC");
    setStatus("AUDIO INIT");

    soundEnable();
    std::srand((unsigned)(std::time(nullptr) ^ swiCRC16(0xFFFF, (void*)&gNitroReady, sizeof(gNitroReady)) ^ (unsigned)REG_VCOUNT));

    initSharedAudioState();

    if (getMusicTrackCount() <= 0) {
        setStatus("NO MUSIC FILES");
        return;
    }

    gAudioReady = true;
    openTrackByIndex(0);
}

void updateAudioEngine(int deltaMs) {
    if (deltaMs < 0) deltaMs = 0;
    for (int i = 0; i < SFX_COUNT; ++i) {
        if (gSfxCooldownMs[i] > 0) {
            gSfxCooldownMs[i] -= deltaMs;
            if (gSfxCooldownMs[i] < 0) gSfxCooldownMs[i] = 0;
        }
    }

    if (!gAudioReady) return;

    if (!gMusicEnabled) {
        setStatus("MUSIC DISABLED");
    } else if (gMusicChannel < 0) {
        openTrackByIndex(0);
    } else {
        setStatus(gLoadedMusicData ? "RAW PLAY" : (gBackgroundMusicFormat == SoundFormat_ADPCM ? "ADPCM PLAY" : "PCM PLAY"));
    }
}

void stopAudioEngine() {
    closeTrackFile();
    gAudioReady = false;
    gCurrentTrack = -1;
    if (gShared && gShared->magic == AUDIO_SHARED_MAGIC) {
        gShared->music_enabled = 0;
    }
    copyString(gCurrentTrackName, sizeof(gCurrentTrackName), "NO MUSIC");
    setStatus("AUDIO OFF");
}

bool hasEmbeddedMusic() {
    return getMusicTrackCount() > 0;
}

const char* getCurrentMusicName() {
    return gCurrentTrackName;
}

int getCurrentMusicTrackCount() {
    return getMusicTrackCount();
}

const char* getAudioStatus() {
    return gAudioStatus;
}

void audioPlayUiClick() { playSfx(SFX_UI_CLICK); }
void audioPlayPlace() { playSfx(SFX_PLACE); }
void audioPlayBreak() { playSfx(SFX_BREAK); }
void audioPlayHurt() { playSfx(SFX_HURT); }
void audioPlayMobHit() { playSfx(SFX_MOB_HIT); }

void audioSetMusicEnabled(bool enabled) {
    gMusicEnabled = enabled;
    if (gShared && gShared->magic == AUDIO_SHARED_MAGIC) {
        gShared->music_enabled = enabled ? 1u : 0u;
    }

    if (!enabled) {
        closeTrackFile();
        copyString(gCurrentTrackName, sizeof(gCurrentTrackName), "MUSIC OFF");
        setStatus("MUSIC DISABLED");
        return;
    }

    gAudioReady = true;
    openTrackByIndex(0);
}

void audioNextTrack() {
    if (getMusicTrackCount() <= 0) return;
    closeTrackFile();
    beginRandomTrack(true);
}
