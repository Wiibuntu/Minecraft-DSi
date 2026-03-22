#pragma once

#include <nds.h>
#include "player.h"

struct RayHit {
    bool hit;
    int block;
    int x;
    int y;
    int z;
    int placeX;
    int placeY;
    int placeZ;
    float dist;
    int face;
};

void initRenderer();
void renderFrame(const Player& p);
void renderHUD(const Player& p, const RayHit& hit);
RayHit castCenterRay(const Player& p, float maxDist);

