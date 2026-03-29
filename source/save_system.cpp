#include "save_system.h"

#include "world.h"
#include "player.h"

#include <fat.h>
#include <cstdio>
#include <cstring>

namespace {
static bool gInitAttempted = false;
static bool gFatReady = false;
static char gStatus[96] = "SAVE: IDLE";

static const u32 kSaveMagic = 0x44535631u; // DSV1
static const u32 kSaveVersion = 5;

struct SaveHeaderV5 {
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
    u32 gameMode;
    s32 health;
    s32 maxHealth;
    s32 selectedSlot;
    u32 alive;
    u32 slotIsBlock[HOTBAR_SIZE];
    s32 slotId[HOTBAR_SIZE];
    s32 slotCount[HOTBAR_SIZE];
    u32 worldBytes;
    u32 worldTime;
};

struct SaveHeaderV4 {
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
    u32 gameMode;
    s32 health;
    s32 maxHealth;
    s32 selectedSlot;
    u32 alive;
    u32 slotIsBlock[HOTBAR_SIZE];
    s32 slotId[HOTBAR_SIZE];
    s32 slotCount[HOTBAR_SIZE];
    u32 worldBytes;
};

struct SaveHeaderV3 {
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

    SaveHeaderV5 header;
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
    header.gameMode = (u32)cfg.gameMode;
    header.health = player.health;
    header.maxHealth = player.maxHealth;
    header.selectedSlot = player.selectedSlot;
    header.alive = player.alive ? 1u : 0u;
    for (int i = 0; i < HOTBAR_SIZE; ++i) {
        header.slotIsBlock[i] = player.hotbar[i].isBlock ? 1u : 0u;
        header.slotId[i] = player.hotbar[i].id;
        header.slotCount[i] = player.hotbar[i].count;
    }
    header.worldBytes = (u32)used;
    header.worldTime = (u32)getWorldTime();

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

    u32 magic = 0;
    u32 version = 0;
    if (std::fread(&magic, sizeof(magic), 1, fp) != 1 || std::fread(&version, sizeof(version), 1, fp) != 1) {
        std::fclose(fp);
        setStatus("LOAD: BAD .sav");
        return false;
    }
    std::rewind(fp);
    if (magic != kSaveMagic || (version != 3u && version != 4u && version != 5u)) {
        std::fclose(fp);
        setStatus("LOAD: BAD .sav");
        return false;
    }

    SaveHeaderV5 header;
    std::memset(&header, 0, sizeof(header));
    if (version == 5u) {
        bool ok = std::fread(&header, sizeof(header), 1, fp) == 1;
        if (!ok || header.worldBytes != (u32)(WORLD_X * WORLD_Y * WORLD_Z)) {
            std::fclose(fp);
            setStatus("LOAD: BAD .sav");
            return false;
        }
    } else if (version == 4u) {
        SaveHeaderV4 oldHeader;
        std::memset(&oldHeader, 0, sizeof(oldHeader));
        bool ok = std::fread(&oldHeader, sizeof(oldHeader), 1, fp) == 1;
        if (!ok || oldHeader.worldBytes != (u32)(WORLD_X * WORLD_Y * WORLD_Z)) {
            std::fclose(fp);
            setStatus("LOAD: BAD .sav");
            return false;
        }
        header.magic = oldHeader.magic;
        header.version = 5u;
        header.seed = oldHeader.seed;
        header.spawnX = oldHeader.spawnX;
        header.spawnY = oldHeader.spawnY;
        header.spawnZ = oldHeader.spawnZ;
        header.playerX = oldHeader.playerX;
        header.playerY = oldHeader.playerY;
        header.playerZ = oldHeader.playerZ;
        header.playerYaw = oldHeader.playerYaw;
        header.playerPitch = oldHeader.playerPitch;
        header.playerVy = oldHeader.playerVy;
        header.playerOnGround = oldHeader.playerOnGround;
        header.selectedBlock = oldHeader.selectedBlock;
        header.worldType = oldHeader.worldType;
        header.sizePreset = oldHeader.sizePreset;
        header.generateTrees = oldHeader.generateTrees;
        header.gameMode = oldHeader.gameMode;
        header.health = oldHeader.health;
        header.maxHealth = oldHeader.maxHealth;
        header.selectedSlot = oldHeader.selectedSlot;
        header.alive = oldHeader.alive;
        for (int i = 0; i < HOTBAR_SIZE; ++i) {
            header.slotIsBlock[i] = oldHeader.slotIsBlock[i];
            header.slotId[i] = oldHeader.slotId[i];
            header.slotCount[i] = oldHeader.slotCount[i];
        }
        header.worldBytes = oldHeader.worldBytes;
        header.worldTime = 6000u;
        header.worldTime = 6000u;
    } else {
        SaveHeaderV3 oldHeader;
        std::memset(&oldHeader, 0, sizeof(oldHeader));
        bool ok = std::fread(&oldHeader, sizeof(oldHeader), 1, fp) == 1;
        if (!ok || oldHeader.worldBytes != (u32)(WORLD_X * WORLD_Y * WORLD_Z)) {
            std::fclose(fp);
            setStatus("LOAD: BAD .sav");
            return false;
        }
        header.magic = oldHeader.magic;
        header.version = 5u;
        header.seed = oldHeader.seed;
        header.spawnX = oldHeader.spawnX;
        header.spawnY = oldHeader.spawnY;
        header.spawnZ = oldHeader.spawnZ;
        header.playerX = oldHeader.playerX;
        header.playerY = oldHeader.playerY;
        header.playerZ = oldHeader.playerZ;
        header.playerYaw = oldHeader.playerYaw;
        header.playerPitch = oldHeader.playerPitch;
        header.playerVy = oldHeader.playerVy;
        header.playerOnGround = oldHeader.playerOnGround;
        header.selectedBlock = oldHeader.selectedBlock;
        header.worldType = oldHeader.worldType;
        header.sizePreset = oldHeader.sizePreset;
        header.generateTrees = oldHeader.generateTrees;
        header.gameMode = GAME_MODE_CREATIVE;
        header.health = 20;
        header.maxHealth = 20;
        header.selectedSlot = 0;
        header.alive = 1u;
        header.worldBytes = oldHeader.worldBytes;
        header.worldTime = 6000u;
    }

    static u8 worldData[WORLD_X * WORLD_Y * WORLD_Z];
    bool ok = std::fread(worldData, 1, sizeof(worldData), fp) == sizeof(worldData);
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
    cfg.gameMode = (int)header.gameMode;
    if (cfg.gameMode < 0 || cfg.gameMode >= GAME_MODE_COUNT) cfg.gameMode = GAME_MODE_CREATIVE;

    if (!importWorldState(worldData, sizeof(worldData), &cfg, header.spawnX, header.spawnY, header.spawnZ)) {
        setStatus("LOAD: IMPORT FAILED");
        return false;
    }

    setCurrentGameMode(cfg.gameMode);
    updateWorldTime(-getWorldTime());
    updateWorldTime((int)header.worldTime);
    player.x = header.playerX;
    player.y = header.playerY;
    player.z = header.playerZ;
    player.yaw = header.playerYaw;
    player.pitch = header.playerPitch;
    player.vy = header.playerVy;
    player.onGround = (header.playerOnGround != 0);
    player.selectedBlock = header.selectedBlock;
    player.health = header.health > 0 ? header.health : 20;
    player.maxHealth = header.maxHealth > 0 ? header.maxHealth : 20;
    player.selectedSlot = header.selectedSlot;
    if (player.selectedSlot < 0 || player.selectedSlot >= HOTBAR_SIZE) player.selectedSlot = 0;
    player.alive = (header.alive != 0u);
    player.fallDistance = 0.0f;
    resetHotbar(player);
    if (cfg.gameMode == GAME_MODE_SURVIVAL) {
        for (int i = 0; i < HOTBAR_SIZE; ++i) {
            player.hotbar[i].isBlock = (header.slotIsBlock[i] != 0u);
            player.hotbar[i].id = header.slotId[i];
            player.hotbar[i].count = header.slotCount[i];
        }
    }

    if (readPathIndex == 0) {
        setStatus("LOAD: OK (Minecraft.sav)");
    } else {
        setStatus("LOAD: OK (fat:/Minecraft.sav)");
    }
    return true;
}
