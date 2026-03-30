#include "player.h"
#include "world.h"
#include "crafting.h"

#include <nds.h>
#include <cmath>
#include <cstring>

static const float kPlayerRadius = 0.28f;
static const float kPlayerHeight = 1.65f;
static const float kMoveSpeedTable[] = {0.07f, 0.09f, 0.11f};
static const float kGravity = 0.030f;
static const float kJumpSpeed = 0.26f;
static const float kLookSpeedTable[] = {0.70f, 1.00f, 1.30f};
static int gMoveSpeedIndex = 1;
static int gLookSpeedIndex = 1;
static int gCurrentGameMode = GAME_MODE_CREATIVE;
static const float kYawStep = 0.055f;
static const float kPitchStep = 0.030f;
static const float kPitchLimit = 1.50f;
static const float kWorldBorderMinX = kPlayerRadius + 0.05f;
static const float kWorldBorderMaxX = (float)WORLD_X - kPlayerRadius - 0.05f;
static const float kWorldBorderMinZ = kPlayerRadius + 0.05f;
static const float kWorldBorderMaxZ = (float)WORLD_Z - kPlayerRadius - 0.05f;

static inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static bool collidesAt(float x, float y, float z) {
    int minX = (int)std::floor(x - kPlayerRadius);
    int maxX = (int)std::floor(x + kPlayerRadius);
    int minY = (int)std::floor(y);
    int maxY = (int)std::floor(y + kPlayerHeight);
    int minZ = (int)std::floor(z - kPlayerRadius);
    int maxZ = (int)std::floor(z + kPlayerRadius);

    for (int bx = minX; bx <= maxX; ++bx)
        for (int by = minY; by <= maxY; ++by)
            for (int bz = minZ; bz <= maxZ; ++bz)
                if (isSolidBlock(bx, by, bz)) return true;
    return false;
}

static void clearHotbar(Player& p) {
    for (int i = 0; i < HOTBAR_SIZE; ++i) {
        p.hotbar[i].isBlock = false;
        p.hotbar[i].id = ITEM_NONE;
        p.hotbar[i].count = 0;
    }
}

static bool isToolItem(int item) {
    switch (item) {
        case ITEM_WOOD_PICKAXE:
        case ITEM_STONE_PICKAXE:
        case ITEM_WOOD_AXE:
        case ITEM_STONE_AXE:
        case ITEM_WOOD_SHOVEL:
        case ITEM_STONE_SHOVEL:
        case ITEM_WOOD_SWORD:
        case ITEM_STONE_SWORD:
            return true;
        default:
            return false;
    }
}

static int hardnessForBlock(int block) {
    switch (block) {
        case BLOCK_DIRT:
        case BLOCK_GRASS:
        case BLOCK_SAND:
        case BLOCK_GRAVEL:
        case BLOCK_LEAVES:
        case BLOCK_BIRCH_LEAVES:
        case BLOCK_SPRUCE_LEAVES:
        case BLOCK_JUNGLE_LEAVES:
            return 12;
        case BLOCK_WOOD:
        case BLOCK_BIRCH_WOOD:
        case BLOCK_SPRUCE_WOOD:
        case BLOCK_JUNGLE_WOOD:
        case BLOCK_PLANKS:
        case BLOCK_BIRCH_PLANKS:
        case BLOCK_SPRUCE_PLANKS:
        case BLOCK_JUNGLE_PLANKS:
        case BLOCK_BOOKSHELF:
            return 22;
        case BLOCK_STONE:
        case BLOCK_COBBLE:
        case BLOCK_BRICK:
        case BLOCK_SANDSTONE:
        case BLOCK_GOLD_BLOCK:
        case BLOCK_IRON_BLOCK:
            return 34;
        case BLOCK_OBSIDIAN:
            return 70;
        case BLOCK_GLASS:
            return 10;
        case BLOCK_WATER:
            return 9999;
        case BLOCK_TORCH:
            return 4;
        case BLOCK_CRAFTING_TABLE:
        case BLOCK_FURNACE:
        case BLOCK_COAL_ORE:
            return 28;
        default:
            return 18;
    }
}

static bool isPickaxeBlock(int block) {
    switch (block) {
        case BLOCK_STONE:
        case BLOCK_COBBLE:
        case BLOCK_BRICK:
        case BLOCK_SANDSTONE:
        case BLOCK_OBSIDIAN:
        case BLOCK_GOLD_BLOCK:
        case BLOCK_IRON_BLOCK:
        case BLOCK_FURNACE:
        case BLOCK_COAL_ORE:
            return true;
        default:
            return false;
    }
}

static bool isAxeBlock(int block) {
    switch (block) {
        case BLOCK_WOOD:
        case BLOCK_BIRCH_WOOD:
        case BLOCK_SPRUCE_WOOD:
        case BLOCK_JUNGLE_WOOD:
        case BLOCK_PLANKS:
        case BLOCK_BIRCH_PLANKS:
        case BLOCK_SPRUCE_PLANKS:
        case BLOCK_JUNGLE_PLANKS:
        case BLOCK_BOOKSHELF:
        case BLOCK_CRAFTING_TABLE:
            return true;
        default:
            return false;
    }
}

static bool isShovelBlock(int block) {
    switch (block) {
        case BLOCK_DIRT:
        case BLOCK_GRASS:
        case BLOCK_SAND:
        case BLOCK_GRAVEL:
            return true;
        default:
            return false;
    }
}

static int toolSpeedBonus(int block, int toolItem) {
    if (toolItem == ITEM_STONE_PICKAXE && isPickaxeBlock(block)) return 18;
    if (toolItem == ITEM_WOOD_PICKAXE && isPickaxeBlock(block)) return 12;
    if (toolItem == ITEM_STONE_AXE && isAxeBlock(block)) return 14;
    if (toolItem == ITEM_WOOD_AXE && isAxeBlock(block)) return 10;
    if (toolItem == ITEM_STONE_SHOVEL && isShovelBlock(block)) return 14;
    if (toolItem == ITEM_WOOD_SHOVEL && isShovelBlock(block)) return 10;
    if ((toolItem == ITEM_WOOD_SWORD || toolItem == ITEM_STONE_SWORD) && (block == BLOCK_LEAVES || block == BLOCK_BIRCH_LEAVES || block == BLOCK_SPRUCE_LEAVES || block == BLOCK_JUNGLE_LEAVES)) return 10 + (toolItem == ITEM_STONE_SWORD ? 4 : 0);
    return 0;
}

void setCurrentGameMode(int mode) {
    if (mode < 0 || mode >= GAME_MODE_COUNT) mode = GAME_MODE_CREATIVE;
    gCurrentGameMode = mode;
}

int getCurrentGameMode() { return gCurrentGameMode; }

const char* getGameModeName(int mode) {
    switch (mode) {
        case GAME_MODE_SURVIVAL: return "SURVIVAL";
        case GAME_MODE_CREATIVE:
        default: return "CREATIVE";
    }
}

bool isCreativeMode() { return gCurrentGameMode == GAME_MODE_CREATIVE; }
bool isSurvivalMode() { return gCurrentGameMode == GAME_MODE_SURVIVAL; }

void resetHotbar(Player& p) {
    clearHotbar(p);
    p.selectedSlot = 0;
}

void initSurvivalLoadout(Player& p) {
    clearHotbar(p);
    p.hotbar[0] = {true, BLOCK_WOOD, 8};
    p.hotbar[1] = {true, BLOCK_PLANKS, 16};
    p.hotbar[2] = {false, ITEM_COAL, 4};
    p.hotbar[3] = {false, ITEM_APPLE, 5};
    p.hotbar[4] = {false, ITEM_WOOD_PICKAXE, 1};
    p.hotbar[5] = {false, ITEM_WOOD_AXE, 1};
    p.hotbar[6] = {false, ITEM_WOOD_SHOVEL, 1};
    p.hotbar[7] = {false, ITEM_WOOD_SWORD, 1};
    p.hotbar[8] = {true, BLOCK_CRAFTING_TABLE, 1};
    p.selectedSlot = 0;
}

void initPlayer(Player& p) {
    getSpawnPosition(p.x, p.y, p.z);
    p.yaw = 0.0f;
    p.pitch = -0.08f;
    p.vy = 0.0f;
    p.onGround = false;
    p.selectedBlock = BLOCK_GRASS;
    p.health = 20;
    p.maxHealth = 20;
    p.selectedSlot = 0;
    p.fallDistance = 0.0f;
    p.alive = true;
    resetHotbar(p);
    if (isSurvivalMode()) initSurvivalLoadout(p);

    while (collidesAt(p.x, p.y, p.z) && p.y < WORLD_Y - 2) p.y += 1.0f;
}

void respawnPlayer(Player& p) {
    float oldYaw = p.yaw;
    float oldPitch = p.pitch;
    int selectedBlock = p.selectedBlock;
    int selectedSlot = p.selectedSlot;
    InventorySlot copy[HOTBAR_SIZE];
    std::memcpy(copy, p.hotbar, sizeof(copy));
    bool hadLoadout = isSurvivalMode();
    initPlayer(p);
    p.yaw = oldYaw;
    p.pitch = oldPitch;
    p.selectedBlock = selectedBlock;
    p.selectedSlot = selectedSlot;
    if (hadLoadout) std::memcpy(p.hotbar, copy, sizeof(copy));
    p.health = p.maxHealth;
    p.alive = true;
}

void damagePlayer(Player& p, int amount) {
    if (!isSurvivalMode() || amount <= 0 || !p.alive) return;
    p.health -= amount;
    if (p.health <= 0) {
        p.health = 0;
        p.alive = false;
    }
}

void healPlayer(Player& p, int amount) {
    if (amount <= 0 || !p.alive) return;
    p.health += amount;
    if (p.health > p.maxHealth) p.health = p.maxHealth;
}

void updatePlayer(Player& p, int held, int down) {
    const float yawStep = kYawStep * kLookSpeedTable[gLookSpeedIndex];
    const float pitchStep = kPitchStep * kLookSpeedTable[gLookSpeedIndex];
    const float moveSpeed = kMoveSpeedTable[gMoveSpeedIndex];
    if (held & KEY_Y) p.yaw -= yawStep;
    if (held & KEY_A) p.yaw += yawStep;

    if (held & KEY_B) p.pitch -= pitchStep;
    if (held & KEY_X) p.pitch += pitchStep;
    if (p.pitch < -kPitchLimit) p.pitch = -kPitchLimit;
    if (p.pitch > kPitchLimit) p.pitch = kPitchLimit;

    float forwardX = std::sinf(p.yaw);
    float forwardZ = std::cosf(p.yaw);
    float rightX = std::cosf(p.yaw);
    float rightZ = -std::sinf(p.yaw);

    float moveX = 0.0f;
    float moveZ = 0.0f;

    if (held & KEY_UP) {
        moveX += forwardX * moveSpeed;
        moveZ += forwardZ * moveSpeed;
    }
    if (held & KEY_DOWN) {
        moveX -= forwardX * moveSpeed;
        moveZ -= forwardZ * moveSpeed;
    }
    if (held & KEY_LEFT) {
        moveX -= rightX * moveSpeed;
        moveZ -= rightZ * moveSpeed;
    }
    if (held & KEY_RIGHT) {
        moveX += rightX * moveSpeed;
        moveZ += rightZ * moveSpeed;
    }

    float nx = clampf(p.x + moveX, kWorldBorderMinX, kWorldBorderMaxX);
    float nz = clampf(p.z + moveZ, kWorldBorderMinZ, kWorldBorderMaxZ);
    if (!collidesAt(nx, p.y, p.z)) p.x = nx;
    if (!collidesAt(p.x, p.y, nz)) p.z = nz;

    p.vy -= kGravity;
    if ((down & KEY_SELECT) && p.onGround) p.vy = kJumpSpeed;
    p.onGround = false;

    float ny = p.y + p.vy;
    if (!collidesAt(p.x, ny, p.z)) {
        p.y = ny;
        if (p.vy < 0.0f) p.fallDistance += -p.vy;
    } else {
        if (p.vy < 0.0f) {
            p.onGround = true;
            if (isSurvivalMode() && p.fallDistance > 0.0f) {
                int fallDamage = (int)(p.fallDistance - 3.0f + 0.5f);
                if (fallDamage > 0) damagePlayer(p, fallDamage);
            }
        }
        p.fallDistance = 0.0f;
        p.vy = 0.0f;
        if (p.onGround) {
            while (collidesAt(p.x, p.y, p.z) && p.y < WORLD_Y - 1) p.y += 0.05f;
        }
    }

    if (p.y < 1.0f) {
        p.y = 1.0f;
        p.fallDistance = 0.0f;
    }
    if (p.y > WORLD_Y - 2.0f) p.y = WORLD_Y - 2.0f;
}

float getLookSpeed() { return kLookSpeedTable[gLookSpeedIndex]; }

void cycleLookSpeed(int delta) {
    const int count = (int)(sizeof(kLookSpeedTable) / sizeof(kLookSpeedTable[0]));
    gLookSpeedIndex = (gLookSpeedIndex + delta + count) % count;
}

float getMoveSpeed() { return kMoveSpeedTable[gMoveSpeedIndex]; }

void cycleMoveSpeed(int delta) {
    const int count = (int)(sizeof(kMoveSpeedTable) / sizeof(kMoveSpeedTable[0]));
    gMoveSpeedIndex = (gMoveSpeedIndex + delta + count) % count;
}

int getSelectedPlaceableBlock(const Player& p) {
    if (isCreativeMode()) return p.selectedBlock;
    const InventorySlot& slot = p.hotbar[p.selectedSlot];
    if (slot.isBlock && slot.count > 0) return slot.id;
    return BLOCK_AIR;
}

int getSelectedToolItem(const Player& p) {
    if (isCreativeMode()) return ITEM_NONE;
    const InventorySlot& slot = p.hotbar[p.selectedSlot];
    if (!slot.isBlock && slot.count > 0 && isToolItem(slot.id)) return slot.id;
    return ITEM_NONE;
}

const InventorySlot& getSelectedSlot(const Player& p) { return p.hotbar[p.selectedSlot]; }
InventorySlot& getSelectedSlot(Player& p) { return p.hotbar[p.selectedSlot]; }

void cycleSelectedSlot(Player& p, int delta) {
    int next = p.selectedSlot + delta;
    while (next < 0) next += HOTBAR_SIZE;
    p.selectedSlot = next % HOTBAR_SIZE;
}

void setSelectedSlot(Player& p, int slot) {
    if (slot < 0) slot = 0;
    if (slot >= HOTBAR_SIZE) slot = HOTBAR_SIZE - 1;
    p.selectedSlot = slot;
}

bool consumeSelectedPlacement(Player& p, int* outBlock) {
    if (outBlock) *outBlock = BLOCK_AIR;
    if (isCreativeMode()) {
        if (outBlock) *outBlock = p.selectedBlock;
        return p.selectedBlock != BLOCK_AIR;
    }
    InventorySlot& slot = getSelectedSlot(p);
    if (!slot.isBlock || slot.count <= 0) return false;
    if (outBlock) *outBlock = slot.id;
    --slot.count;
    if (slot.count <= 0) {
        slot = {false, ITEM_NONE, 0};
    }
    return true;
}

bool consumeSelectedItemUse(Player& p) {
    if (isCreativeMode()) return false;
    InventorySlot& slot = getSelectedSlot(p);
    if (slot.isBlock || slot.count <= 0) return false;
    if (slot.id == ITEM_APPLE) {
        healPlayer(p, 4);
        --slot.count;
        if (slot.count <= 0) slot = {false, ITEM_NONE, 0};
        return true;
    }
    return false;
}

bool addBlockDropToInventory(Player& p, int block) {
    if (isCreativeMode() || block == BLOCK_AIR || block == BLOCK_WATER) return true;
    if (block == BLOCK_COAL_ORE) {
        for (int i = 0; i < HOTBAR_SIZE; ++i) {
            InventorySlot& slot = p.hotbar[i];
            if (!slot.isBlock && slot.id == ITEM_COAL && slot.count < 99) {
                ++slot.count;
                return true;
            }
        }
        for (int i = 0; i < HOTBAR_SIZE; ++i) {
            InventorySlot& slot = p.hotbar[i];
            if (slot.count <= 0) {
                slot = {false, ITEM_COAL, 1};
                return true;
            }
        }
        return false;
    }
    block = getBlockDropForSurvival(block);
    for (int i = 0; i < HOTBAR_SIZE; ++i) {
        InventorySlot& slot = p.hotbar[i];
        if (slot.isBlock && slot.id == block && slot.count < 99) {
            ++slot.count;
            return true;
        }
    }
    for (int i = 0; i < HOTBAR_SIZE; ++i) {
        InventorySlot& slot = p.hotbar[i];
        if (slot.count <= 0) {
            slot = {true, block, 1};
            return true;
        }
    }
    return false;
}

bool canSurvivalBreakBlock(int block, int toolItem) {
    if (block == BLOCK_AIR || block == BLOCK_WATER) return false;
    if (block == BLOCK_OBSIDIAN) return toolItem == ITEM_STONE_PICKAXE;
    if (block == BLOCK_GOLD_BLOCK || block == BLOCK_IRON_BLOCK || block == BLOCK_FURNACE || block == BLOCK_COAL_ORE) return toolItem == ITEM_WOOD_PICKAXE || toolItem == ITEM_STONE_PICKAXE;
    return true;
}

int getBreakTicksForBlock(int block, int toolItem) {
    int ticks = hardnessForBlock(block);
    int bonus = toolSpeedBonus(block, toolItem);
    if (bonus > 0) ticks -= bonus;
    if (ticks < 4) ticks = 4;
    return ticks;
}
