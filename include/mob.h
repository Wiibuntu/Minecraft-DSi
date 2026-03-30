#pragma once

#include "player.h"

struct Mob {
    bool active;
    int type;
    float x, y, z;
    float yaw;
    float vy;
    int animTick;
    int wanderTicks;
    int variant;
    int health;
    int maxHealth;
    int hurtTicks;
};

enum MobType {
    MOB_PIG = 0,
    MOB_TYPE_COUNT
};

void initMobs();
void spawnWorldMobs();
void updateMobs(const Player& player);
int getMobCount();
const Mob* getMob(int index);

int raycastMob(const Player& player, float maxDist, float* outDist);
bool damageMob(int index, int amount);
float getMobWidth(const Mob& mob);
float getMobHeight(const Mob& mob);
float getMobLength(const Mob& mob);
