#pragma once

#include "world.h"

enum GameMode {
    GAME_MODE_CREATIVE = 0,
    GAME_MODE_SURVIVAL,
    GAME_MODE_COUNT
};

enum ItemType {
    ITEM_NONE = 0,
    ITEM_APPLE,
    ITEM_STICK,
    ITEM_COAL,
    ITEM_WOOD_PICKAXE,
    ITEM_STONE_PICKAXE,
    ITEM_WOOD_AXE,
    ITEM_STONE_AXE,
    ITEM_WOOD_SHOVEL,
    ITEM_STONE_SHOVEL,
    ITEM_WOOD_SWORD,
    ITEM_STONE_SWORD
};

static const int HOTBAR_SIZE = 9;

struct InventorySlot {
    bool isBlock;
    int id;
    int count;
};

struct Player {
    float x;
    float y;
    float z;
    float yaw;
    float pitch;
    float vy;
    bool onGround;
    int selectedBlock;

    int health;
    int maxHealth;
    int selectedSlot;
    float fallDistance;
    bool alive;
    InventorySlot hotbar[HOTBAR_SIZE];
};

void initPlayer(Player& p);
void respawnPlayer(Player& p);
void updatePlayer(Player& p, int held, int down);

float getLookSpeed();
void cycleLookSpeed(int delta);
float getMoveSpeed();
void cycleMoveSpeed(int delta);

void setCurrentGameMode(int mode);
int getCurrentGameMode();
const char* getGameModeName(int mode);
bool isCreativeMode();
bool isSurvivalMode();

void initSurvivalLoadout(Player& p);
void resetHotbar(Player& p);
int getSelectedPlaceableBlock(const Player& p);
int getSelectedToolItem(const Player& p);
const InventorySlot& getSelectedSlot(const Player& p);
InventorySlot& getSelectedSlot(Player& p);
void cycleSelectedSlot(Player& p, int delta);
void setSelectedSlot(Player& p, int slot);
bool consumeSelectedPlacement(Player& p, int* outBlock);
bool consumeSelectedItemUse(Player& p);
bool addBlockDropToInventory(Player& p, int block);
bool canSurvivalBreakBlock(int block, int toolItem);
int getBreakTicksForBlock(int block, int toolItem);
void damagePlayer(Player& p, int amount);
void healPlayer(Player& p, int amount);
