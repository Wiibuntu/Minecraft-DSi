#include "save_system.h"

#include "world.h"

#include <fat.h>
#include <cstdio>
#include <cstring>

namespace {
static bool gInitAttempted = false;
static bool gFatReady = false;
static char gStatus[96] = "SAVE: IDLE";

static const u32 kSaveMagic = 0x44535631u; // DSV1
static const u32 kSaveVersion = 3;

struct SaveHeader {
    u32 magic;
    u32 version;
    u32 seed;
    s32 spawnX;
    s32 spawnY;
    s32 spawnZ;
    float playerX;
    float playerY;
    float playerZ;
    float playerYaw;
    float playerPitch;
    float playerVy;
    u32 playerOnGround;
    s32 selectedBlock;
    u32 worldType;
    u32 sizePreset;
    u32 generateTrees;
    u32 worldBytes;
};

static void setStatus(const char* s) {
    std::snprintf(gStatus, sizeof(gStatus), "%s", s ? s : "SAVE");
}

static bool ensureSaveReady() {
    if (gInitAttempted) return gFatReady;
    gInitAttempted = true;
    if (!fatInitDefault()) {
        setStatus("SAVE: FAT INIT FAILED");
        gFatReady = false;
        return false;
    }
    gFatReady = true;
    setStatus("SAVE: READY (.sav)");
    return true;
}

static const char* kSavePaths[] = {
    "Minecraft.sav",
    "fat:/Minecraft.sav"
};

static const int kSavePathCount = (int)(sizeof(kSavePaths) / sizeof(kSavePaths[0]));

static FILE* openForWrite(int* pathIndexOut) {
    for (int i = 0; i < kSavePathCount; ++i) {
        FILE* fp = std::fopen(kSavePaths[i], "wb");
        if (fp) {
            if (pathIndexOut) *pathIndexOut = i;
            return fp;
        }
    }
    if (pathIndexOut) *pathIndexOut = -1;
    return 0;
}

static FILE* openForRead(int* pathIndexOut) {
    for (int i = 0; i < kSavePathCount; ++i) {
        FILE* fp = std::fopen(kSavePaths[i], "rb");
        if (fp) {
            if (pathIndexOut) *pathIndexOut = i;
            return fp;
        }
    }
    if (pathIndexOut) *pathIndexOut = -1;
    return 0;
}

static void setWriteOpenFailedStatus() {
    setStatus("SAVE: OPEN FAILED (.sav)");
}
}

const char* getSaveStatusMessage() {
    return gStatus;
}

bool saveGame(const Player& player) {
    if (!ensureSaveReady()) return false;

    static u8 worldData[WORLD_X * WORLD_Y * WORLD_Z];
    int used = 0;
    if (!exportWorldState(worldData, sizeof(worldData), &used)) {
        setStatus("SAVE: EXPORT FAILED");
        return false;
    }

    SaveHeader header;
    std::memset(&header, 0, sizeof(header));
    header.magic = kSaveMagic;
    header.version = kSaveVersion;
    const WorldGenConfig& cfg = getWorldConfig();
    header.seed = cfg.seed;
    float sx, sy, sz;
    getSpawnPosition(sx, sy, sz);
    header.spawnX = (s32)sx;
    header.spawnY = (s32)sy;
    header.spawnZ = (s32)sz;
    header.playerX = player.x;
    header.playerY = player.y;
    header.playerZ = player.z;
    header.playerYaw = player.yaw;
    header.playerPitch = player.pitch;
    header.playerVy = player.vy;
    header.playerOnGround = player.onGround ? 1u : 0u;
    header.selectedBlock = player.selectedBlock;
    header.worldType = (u32)cfg.worldType;
    header.sizePreset = (u32)cfg.sizePreset;
    header.generateTrees = cfg.generateTrees ? 1u : 0u;
    header.worldBytes = (u32)used;

    int writePathIndex = -1;
    FILE* fp = openForWrite(&writePathIndex);
    if (!fp) {
        setWriteOpenFailedStatus();
        return false;
    }

    bool ok = std::fwrite(&header, sizeof(header), 1, fp) == 1;
    ok = ok && std::fwrite(worldData, 1, used, fp) == (size_t)used;
    std::fflush(fp);
    std::fclose(fp);

    if (!ok) {
        setStatus("SAVE: WRITE FAILED");
        return false;
    }

    if (writePathIndex == 0) {
        setStatus("SAVE: OK (Minecraft.sav)");
    } else {
        setStatus("SAVE: OK (fat:/Minecraft.sav)");
    }
    return true;
}

bool loadGame(Player& player) {
    if (!ensureSaveReady()) return false;

    int readPathIndex = -1;
    FILE* fp = openForRead(&readPathIndex);
    if (!fp) {
        setStatus("LOAD: NO .sav");
        return false;
    }

    SaveHeader header;
    std::memset(&header, 0, sizeof(header));
    bool ok = std::fread(&header, sizeof(header), 1, fp) == 1;
    if (!ok || header.magic != kSaveMagic || header.version != kSaveVersion || header.worldBytes != (u32)(WORLD_X * WORLD_Y * WORLD_Z)) {
        std::fclose(fp);
        setStatus("LOAD: BAD .sav");
        return false;
    }

    static u8 worldData[WORLD_X * WORLD_Y * WORLD_Z];
    ok = std::fread(worldData, 1, sizeof(worldData), fp) == sizeof(worldData);
    std::fclose(fp);
    if (!ok) {
        setStatus("LOAD: READ FAILED");
        return false;
    }

    WorldGenConfig cfg;
    cfg.seed = header.seed;
    cfg.worldType = (int)header.worldType;
    if (cfg.worldType < 0 || cfg.worldType >= WORLD_TYPE_COUNT) cfg.worldType = WORLD_TYPE_DEFAULT;
    cfg.sizePreset = (int)header.sizePreset;
    if (cfg.sizePreset < 0 || cfg.sizePreset >= WORLD_SIZE_COUNT) cfg.sizePreset = WORLD_SIZE_CLASSIC;
    cfg.generateTrees = (header.generateTrees != 0u);

    if (!importWorldState(worldData, sizeof(worldData), &cfg, header.spawnX, header.spawnY, header.spawnZ)) {
        setStatus("LOAD: IMPORT FAILED");
        return false;
    }

    player.x = header.playerX;
    player.y = header.playerY;
    player.z = header.playerZ;
    player.yaw = header.playerYaw;
    player.pitch = header.playerPitch;
    player.vy = header.playerVy;
    player.onGround = (header.playerOnGround != 0);
    player.selectedBlock = header.selectedBlock;

    if (readPathIndex == 0) {
        setStatus("LOAD: OK (Minecraft.sav)");
    } else {
        setStatus("LOAD: OK (fat:/Minecraft.sav)");
    }
    return true;
}
