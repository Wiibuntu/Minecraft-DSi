#include "audio_engine.h"

#include <nds.h>
#include <filesystem.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "music_data.h"

namespace {

static bool gNitroReady = false;
static bool gAudioReady = false;
static int gCurrentTrack = -1;
static int gCurrentChannel = -1;
static unsigned char* gTrackData = nullptr;
static u32 gTrackSize = 0;
static int gTrackMsRemaining = 0;
static char gCurrentTrackName[96] = "NO MUSIC";
static char gAudioStatus[96] = "AUDIO OFF";

static void copyString(char* out, size_t outSize, const char* src) {
    if (!out || outSize == 0) return;
    std::snprintf(out, outSize, "%s", src ? src : "");
}

static void setStatus(const char* text) {
    copyString(gAudioStatus, sizeof(gAudioStatus), text);
}

static void freeTrackData() {
    if (gTrackData) {
        std::free(gTrackData);
        gTrackData = nullptr;
    }
    gTrackSize = 0;
    gTrackMsRemaining = 0;
}

static void stopCurrentPlayback() {
    if (gCurrentChannel >= 0) {
        soundKill(gCurrentChannel);
        gCurrentChannel = -1;
    }
}

static bool loadFileFully(const char* path, unsigned char** outData, u32* outSize) {
    if (!path || !outData || !outSize) return false;
    *outData = nullptr;
    *outSize = 0;

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

    unsigned char* data = static_cast<unsigned char*>(std::malloc((size_t)size));
    if (!data) {
        std::fclose(fp);
        return false;
    }

    const size_t got = std::fread(data, 1, (size_t)size, fp);
    std::fclose(fp);
    if (got != (size_t)size) {
        std::free(data);
        return false;
    }

    *outData = data;
    *outSize = (u32)size;
    return true;
}

static bool startTrack(int index) {
    const int trackCount = getMusicTrackCount();
    if (trackCount <= 0) {
        setStatus("NO TRACKS");
        return false;
    }

    if (index < 0) index = 0;
    if (index >= trackCount) index %= trackCount;

    stopCurrentPlayback();
    freeTrackData();

    const char* path = getMusicTrackPath(index);
    const char* name = getMusicTrackName(index);
    const int sampleRate = getMusicTrackSampleRate(index);
    if (!path || sampleRate <= 0) {
        setStatus("BAD TRACK META");
        return false;
    }

    if (!loadFileFully(path, &gTrackData, &gTrackSize)) {
        setStatus("OPEN FAIL");
        copyString(gCurrentTrackName, sizeof(gCurrentTrackName), name ? name : "OPEN FAIL");
        return false;
    }

    // Files are generated as ffmpeg -f u8 -acodec pcm_u8 -ac 1 -ar 11025
    gCurrentChannel = soundPlaySample(
        gTrackData,
        SoundFormat_8Bit,
        gTrackSize,
        (u16)sampleRate,
        110,
        64,
        false,
        0
    );

    if (gCurrentChannel < 0) {
        freeTrackData();
        setStatus("PLAY FAIL");
        copyString(gCurrentTrackName, sizeof(gCurrentTrackName), name ? name : "PLAY FAIL");
        return false;
    }

    gCurrentTrack = index;
    copyString(gCurrentTrackName, sizeof(gCurrentTrackName), name ? name : "UNKNOWN");
    gTrackMsRemaining = (int)(((u64)gTrackSize * 1000ULL) / (u64)sampleRate);
    if (gTrackMsRemaining < 250) gTrackMsRemaining = 250;
    setStatus("PLAYING RAW PCM");
    return true;
}

static bool startNextTrack() {
    const int trackCount = getMusicTrackCount();
    if (trackCount <= 0) return false;

    for (int attempt = 0; attempt < trackCount; ++attempt) {
        const int next = (gCurrentTrack + 1 + attempt + trackCount) % trackCount;
        if (startTrack(next)) return true;
    }

    setStatus("ALL TRACKS FAILED");
    return false;
}

} // namespace

void initAudioEngine() {
    gNitroReady = nitroFSInit(nullptr);
    gAudioReady = false;
    gCurrentTrack = -1;
    gCurrentChannel = -1;
    freeTrackData();
    copyString(gCurrentTrackName, sizeof(gCurrentTrackName), "NO MUSIC");
    setStatus(gNitroReady ? "NITRO READY" : "NITRO FAIL");

    soundEnable();

    if (!gNitroReady) {
        return;
    }

    const int trackCount = getMusicTrackCount();
    if (trackCount <= 0) {
        setStatus("NO EMBEDDED MUSIC");
        return;
    }

    gAudioReady = true;
    if (!startTrack(0)) {
        setStatus("TRACK 0 FAIL");
    }
}

void updateAudioEngine(int deltaMs) {
    if (!gAudioReady) return;
    if (deltaMs < 0) deltaMs = 0;

    if (gTrackMsRemaining > 0) {
        gTrackMsRemaining -= deltaMs;
    }

    if (gTrackMsRemaining <= 0) {
        startNextTrack();
    }
}

void stopAudioEngine() {
    stopCurrentPlayback();
    freeTrackData();
    gAudioReady = false;
    gCurrentTrack = -1;
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
