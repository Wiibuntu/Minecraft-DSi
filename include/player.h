#pragma once

struct Player {
    float x;
    float y;
    float z;
    float yaw;
    float pitch;
    float vy;
    bool onGround;
    int selectedBlock;
};

void initPlayer(Player& p);
void updatePlayer(Player& p, int held, int down);


float getLookSpeed();
void cycleLookSpeed(int delta);
float getMoveSpeed();
void cycleMoveSpeed(int delta);
