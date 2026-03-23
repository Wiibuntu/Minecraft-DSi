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

enum MenuAction {
    MENU_ACTION_NONE = 0,
    MENU_ACTION_NEW_GAME,
    MENU_ACTION_LOAD_GAME,
    MENU_ACTION_OPTIONS,
    MENU_ACTION_OPEN_GRAPHICS,
    MENU_ACTION_BACK,
    MENU_ACTION_RESUME,
    MENU_ACTION_SAVE_GAME,
    MENU_ACTION_QUIT_TO_TITLE
};

void initRenderer();
void renderFrame(const Player& p);
void renderHUD(const Player& p, const RayHit& hit);
RayHit castCenterRay(const Player& p, float maxDist);
HudTouchAction handleHudTouch(int x, int y);

void renderTitleMenu(int animTick);
void renderLoadingScreen(int progress, int animTick);
void renderPauseMenu();
int handleTitleMenuTouch(int x, int y);
int handlePauseMenuTouch(int x, int y);

void renderOptionsMenu();
void renderGraphicsMenu();
int handleOptionsMenuTouch(int x, int y);
int handleGraphicsMenuTouch(int x, int y);
int getRenderWidth();
int getRenderHeight();
void setRenderResolution(int width, int height);
