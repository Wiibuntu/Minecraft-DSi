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

enum HudTouchType {
    HUD_TOUCH_NONE = 0,
    HUD_TOUCH_SELECT_BLOCK = 1,
    HUD_TOUCH_TOGGLE_MAP = 2
};

struct HudTouchAction {
    int type;
    int value;
};

void initRenderer();
void renderFrame(const Player& p);
void renderHUD(const Player& p, const RayHit& hit);
RayHit castCenterRay(const Player& p, float maxDist);
HudTouchAction handleHudTouch(int x, int y);
