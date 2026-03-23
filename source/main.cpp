#include <nds.h>

#include "player.h"
#include "render.h"
#include "world.h"

static const int kPlaceableBlocks[] = {
    BLOCK_GRASS,
    BLOCK_DIRT,
    BLOCK_STONE,
    BLOCK_WOOD,
    BLOCK_LEAVES,
    BLOCK_SAND,
    BLOCK_WATER,
    BLOCK_COBBLE,
    BLOCK_PLANKS,
    BLOCK_BRICK,
    BLOCK_GLASS
};

static const int kPlaceableBlockCount = sizeof(kPlaceableBlocks) / sizeof(kPlaceableBlocks[0]);

enum GameModeState {
    STATE_TITLE = 0,
    STATE_LOADING,
    STATE_PLAYING,
    STATE_PAUSED
};

static int selectedIndexFromBlock(int block) {
    for (int i = 0; i < kPlaceableBlockCount; ++i) {
        if (kPlaceableBlocks[i] == block) return i;
    }
    return 0;
}

static void changeSelectedBlock(Player& p, int delta) {
    int idx = selectedIndexFromBlock(p.selectedBlock);
    idx = (idx + delta + kPlaceableBlockCount) % kPlaceableBlockCount;
    p.selectedBlock = kPlaceableBlocks[idx];
}

static u32 nextRandomSeed() {
    static u32 seed = 0x13579BDFu;
    seed = seed * 1664525u + 1013904223u + (u32)REG_VCOUNT;
    return seed ^ ((u32)keysHeld() << 16) ^ (u32)keysDown();
}

int main() {
    irqEnable(IRQ_VBLANK);

    initRenderer();

    Player player;
    initPlayer(player);
    player.selectedBlock = BLOCK_GRASS;

    GameModeState state = STATE_TITLE;
    int animTick = 0;
    u32 pendingSeed = 0;

    while (1) {
        swiWaitForVBlank();
        scanKeys();
        int held = keysHeld();
        int down = keysDown();
        ++animTick;

        if (state == STATE_TITLE) {
            if (down & KEY_TOUCH) {
                touchPosition touch;
                touchRead(&touch);
                int action = handleTitleMenuTouch(touch.px, touch.py);
                if (action == MENU_ACTION_NEW_GAME) {
                    pendingSeed = nextRandomSeed();
                    beginWorldGeneration(pendingSeed);
                    state = STATE_LOADING;
                }
            }
            renderTitleMenu(animTick);
            continue;
        }

        if (state == STATE_LOADING) {
            bool done = generateWorldStep(24);
            renderLoadingScreen(getWorldGenProgress(), animTick);
            if (done) {
                initPlayer(player);
                player.selectedBlock = BLOCK_GRASS;
                state = STATE_PLAYING;
            }
            continue;
        }

        if (state == STATE_PAUSED) {
            if (down & KEY_START) {
                state = STATE_PLAYING;
            }
            if (down & KEY_TOUCH) {
                touchPosition touch;
                touchRead(&touch);
                int action = handlePauseMenuTouch(touch.px, touch.py);
                if (action == MENU_ACTION_RESUME) {
                    state = STATE_PLAYING;
                } else if (action == MENU_ACTION_QUIT_TO_TITLE) {
                    state = STATE_TITLE;
                }
            }
            renderPauseMenu();
            continue;
        }

        if (down & KEY_START) {
            state = STATE_PAUSED;
            renderPauseMenu();
            continue;
        }

        if (down & KEY_SELECT) changeSelectedBlock(player, -1);
        if (down & KEY_TOUCH) {
            touchPosition touch;
            touchRead(&touch);
            HudTouchAction action = handleHudTouch(touch.px, touch.py);
            if (action.type == HUD_TOUCH_SELECT_BLOCK) {
                player.selectedBlock = action.value;
            }
        }

        updatePlayer(player, held, down);

        RayHit hit = castCenterRay(player, 5.0f);
        if ((down & KEY_L) && hit.hit && !(hit.x == (int)player.x && hit.z == (int)player.z)) {
            setBlock(hit.x, hit.y, hit.z, BLOCK_AIR);
        }
        if ((down & KEY_R) && hit.hit) {
            if (!isSolidBlock(hit.placeX, hit.placeY, hit.placeZ)) {
                setBlock(hit.placeX, hit.placeY, hit.placeZ, player.selectedBlock);
            }
        }

        renderFrame(player);
        renderHUD(player, hit);
    }

    return 0;
}
