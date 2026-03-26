#include <nds.h>

#include "player.h"
#include "render.h"
#include "world.h"
#include "save_system.h"

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
    STATE_WORLD_SETUP,
    STATE_OPTIONS,
    STATE_GRAPHICS,
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

static void cycleSeedStep(u32& seedStep, int delta) {
    static const u32 kSteps[] = {1u, 16u, 256u, 4096u, 65536u};
    int idx = 0;
    for (int i = 0; i < (int)(sizeof(kSteps) / sizeof(kSteps[0])); ++i) {
        if (kSteps[i] == seedStep) {
            idx = i;
            break;
        }
    }
    idx = (idx + delta + 5) % 5;
    seedStep = kSteps[idx];
}

int main() {
    irqEnable(IRQ_VBLANK);

    initRenderer();

    Player player;
    initPlayer(player);
    player.selectedBlock = BLOCK_GRASS;

    GameModeState state = STATE_TITLE;
    int animTick = 0;
    WorldGenConfig newWorldConfig = {nextRandomSeed(), WORLD_TYPE_DEFAULT, WORLD_SIZE_CLASSIC, true};
    u32 seedStep = 16u;

    while (1) {
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
                    newWorldConfig.seed = nextRandomSeed();
                    setPendingWorldConfig(newWorldConfig);
                    invalidateMenuCache();
                    state = STATE_WORLD_SETUP;
                } else if (action == MENU_ACTION_LOAD_GAME) {
                    if (loadGame(player)) {
                        prepareGameplayTransition();
                        flushTransitionGhosting();
                        state = STATE_PLAYING;
                    }
                } else if (action == MENU_ACTION_OPTIONS) {
                    invalidateMenuCache();
                    state = STATE_OPTIONS;
                }
            }
            renderTitleMenu(animTick);
            swiWaitForVBlank();
            continue;
        }

        if (state == STATE_WORLD_SETUP) {
            if (down & KEY_B) {
                invalidateMenuCache();
                state = STATE_TITLE;
            }
            if (down & KEY_LEFT) newWorldConfig.worldType = (newWorldConfig.worldType + WORLD_TYPE_COUNT - 1) % WORLD_TYPE_COUNT;
            if (down & KEY_RIGHT) newWorldConfig.worldType = (newWorldConfig.worldType + 1) % WORLD_TYPE_COUNT;
            if (down & KEY_UP) newWorldConfig.sizePreset = (newWorldConfig.sizePreset + WORLD_SIZE_COUNT - 1) % WORLD_SIZE_COUNT;
            if (down & KEY_DOWN) newWorldConfig.sizePreset = (newWorldConfig.sizePreset + 1) % WORLD_SIZE_COUNT;
            if (down & KEY_L) newWorldConfig.seed -= seedStep;
            if (down & KEY_R) newWorldConfig.seed += seedStep;
            if (down & KEY_X) newWorldConfig.generateTrees = !newWorldConfig.generateTrees;
            if (down & KEY_Y) newWorldConfig.seed = nextRandomSeed();
            if (down & KEY_A) {
                setPendingWorldConfig(newWorldConfig);
                beginWorldGeneration(newWorldConfig);
                invalidateMenuCache();
                flushTransitionGhosting();
                state = STATE_LOADING;
            }
            if (down & KEY_TOUCH) {
                touchPosition touch;
                touchRead(&touch);
                int action = handleWorldSetupTouch(touch.px, touch.py);
                switch (action) {
                    case MENU_ACTION_BACK:
                        invalidateMenuCache();
                        state = STATE_TITLE;
                        break;
                    case MENU_ACTION_WORLD_TYPE_PREV:
                        newWorldConfig.worldType = (newWorldConfig.worldType + WORLD_TYPE_COUNT - 1) % WORLD_TYPE_COUNT;
                        break;
                    case MENU_ACTION_WORLD_TYPE_NEXT:
                        newWorldConfig.worldType = (newWorldConfig.worldType + 1) % WORLD_TYPE_COUNT;
                        break;
                    case MENU_ACTION_WORLD_SIZE_PREV:
                        newWorldConfig.sizePreset = (newWorldConfig.sizePreset + WORLD_SIZE_COUNT - 1) % WORLD_SIZE_COUNT;
                        break;
                    case MENU_ACTION_WORLD_SIZE_NEXT:
                        newWorldConfig.sizePreset = (newWorldConfig.sizePreset + 1) % WORLD_SIZE_COUNT;
                        break;
                    case MENU_ACTION_WORLD_TREES_TOGGLE:
                        newWorldConfig.generateTrees = !newWorldConfig.generateTrees;
                        break;
                    case MENU_ACTION_WORLD_SEED_PREV:
                        newWorldConfig.seed -= seedStep;
                        break;
                    case MENU_ACTION_WORLD_SEED_NEXT:
                        newWorldConfig.seed += seedStep;
                        break;
                    case MENU_ACTION_WORLD_STEP_PREV:
                        cycleSeedStep(seedStep, -1);
                        break;
                    case MENU_ACTION_WORLD_STEP_NEXT:
                        cycleSeedStep(seedStep, 1);
                        break;
                    case MENU_ACTION_WORLD_RANDOMIZE:
                        newWorldConfig.seed = nextRandomSeed();
                        break;
                    case MENU_ACTION_WORLD_CREATE:
                        setPendingWorldConfig(newWorldConfig);
                        beginWorldGeneration(newWorldConfig);
                        invalidateMenuCache();
                        flushTransitionGhosting();
                        state = STATE_LOADING;
                        break;
                    default:
                        break;
                }
            }
            renderWorldSetupMenu(newWorldConfig, seedStep);
            swiWaitForVBlank();
            continue;
        }

        if (state == STATE_OPTIONS) {
            if (down & KEY_TOUCH) {
                touchPosition touch;
                touchRead(&touch);
                int action = handleOptionsMenuTouch(touch.px, touch.py);
                if (action == MENU_ACTION_OPEN_GRAPHICS) {
                    invalidateMenuCache();
                    state = STATE_GRAPHICS;
                } else if (action == MENU_ACTION_BACK) {
                    invalidateMenuCache();
                    state = STATE_TITLE;
                }
            }
            renderOptionsMenu();
            swiWaitForVBlank();
            continue;
        }

        if (state == STATE_GRAPHICS) {
            if ((held | down) & KEY_TOUCH) {
                touchPosition touch;
                touchRead(&touch);
                int action = handleGraphicsMenuTouch(touch.px, touch.py);
                if ((down & KEY_TOUCH) && action == MENU_ACTION_BACK) {
                    invalidateMenuCache();
                    state = STATE_OPTIONS;
                }
            }
            renderGraphicsMenu();
            swiWaitForVBlank();
            continue;
        }

        if (state == STATE_LOADING) {
            bool done = generateWorldStep(24);
            renderLoadingScreen(getWorldGenProgress(), animTick);
            if (done) {
                initPlayer(player);
                player.selectedBlock = BLOCK_GRASS;
                prepareGameplayTransition();
                flushTransitionGhosting();
                renderFrame(player);
                renderHUD(player, castCenterRay(player, 5.0f));
                swiWaitForVBlank();
                state = STATE_PLAYING;
            }
            swiWaitForVBlank();
            continue;
        }

        if (state == STATE_PAUSED) {
            if (down & KEY_START) {
                invalidateMenuCache();
                state = STATE_PLAYING;
            }
            if (down & KEY_TOUCH) {
                touchPosition touch;
                touchRead(&touch);
                int action = handlePauseMenuTouch(touch.px, touch.py);
                if (action == MENU_ACTION_RESUME) {
                    invalidateMenuCache();
                    state = STATE_PLAYING;
                } else if (action == MENU_ACTION_SAVE_GAME) {
                    saveGame(player);
                } else if (action == MENU_ACTION_QUIT_TO_TITLE) {
                    invalidateMenuCache();
                    state = STATE_TITLE;
                }
            }
            renderPauseMenu();
            swiWaitForVBlank();
            continue;
        }

        if (down & KEY_START) {
            invalidateMenuCache();
            state = STATE_PAUSED;
            renderPauseMenu();
            swiWaitForVBlank();
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
        swiWaitForVBlank();
    }

    return 0;
}
