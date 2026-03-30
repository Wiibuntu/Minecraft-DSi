#include <nds.h>

#include "player.h"
#include "render.h"
#include "world.h"
#include "save_system.h"
#include "audio_engine.h"
#include "crafting.h"
#include "mob.h"

static const int kPlaceableBlocks[] = {
    BLOCK_GRASS,
    BLOCK_DIRT,
    BLOCK_STONE,
    BLOCK_WOOD,
    BLOCK_BIRCH_WOOD,
    BLOCK_SPRUCE_WOOD,
    BLOCK_JUNGLE_WOOD,
    BLOCK_LEAVES,
    BLOCK_BIRCH_LEAVES,
    BLOCK_SPRUCE_LEAVES,
    BLOCK_JUNGLE_LEAVES,
    BLOCK_SAND,
    BLOCK_WATER,
    BLOCK_COBBLE,
    BLOCK_PLANKS,
    BLOCK_BIRCH_PLANKS,
    BLOCK_SPRUCE_PLANKS,
    BLOCK_JUNGLE_PLANKS,
    BLOCK_BRICK,
    BLOCK_GLASS,
    BLOCK_SANDSTONE,
    BLOCK_OBSIDIAN,
    BLOCK_GRAVEL,
    BLOCK_BOOKSHELF,
    BLOCK_WHITE_WOOL,
    BLOCK_GOLD_BLOCK,
    BLOCK_IRON_BLOCK,
    BLOCK_TORCH,
    BLOCK_CRAFTING_TABLE,
    BLOCK_FURNACE,
    BLOCK_COAL_ORE
};

static const int kPlaceableBlockCount = sizeof(kPlaceableBlocks) / sizeof(kPlaceableBlocks[0]);

struct BreakState {
    bool active;
    int x;
    int y;
    int z;
    int ticks;
};

enum GameModeState {
    STATE_TITLE = 0,
    STATE_WORLD_SETUP,
    STATE_OPTIONS,
    STATE_GRAPHICS,
    STATE_LOADING,
    STATE_PLAYING,
    STATE_CRAFTING,
    STATE_PAUSED
};

enum OptionsReturnState {
    OPTIONS_RETURN_TITLE = 0,
    OPTIONS_RETURN_PAUSE
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

static void resetBreakState(BreakState& state) {
    state.active = false;
    state.x = state.y = state.z = 0;
    state.ticks = 0;
}

int main() {
    irqEnable(IRQ_VBLANK);

    initRenderer();
    initAudioEngine();
    initMobs();

    Player player;
    setCurrentGameMode(GAME_MODE_CREATIVE);
    initPlayer(player);
    player.selectedBlock = BLOCK_GRASS;

    GameModeState state = STATE_TITLE;
    int animTick = 0;
    int worldTickFrameAccum = 0;
    WorldGenConfig newWorldConfig = {nextRandomSeed(), WORLD_TYPE_DEFAULT, WORLD_SIZE_CLASSIC, true, GAME_MODE_CREATIVE};
    u32 seedStep = 16u;
    OptionsReturnState optionsReturnState = OPTIONS_RETURN_TITLE;
    BreakState breakState{};
    resetBreakState(breakState);
    int craftingTab = 0;
    int craftingSelection = 0;

    while (1) {
        scanKeys();
        int held = keysHeld();
        int down = keysDown();
        ++animTick;
        updateAudioEngine(16);
        if (state == STATE_PLAYING) {
            ++worldTickFrameAccum;
            while (worldTickFrameAccum >= 3) {
                updateWorldTime(1);
                worldTickFrameAccum -= 3;
            }
        } else {
            worldTickFrameAccum = 0;
        }

        if (state == STATE_TITLE) {
            if (down & KEY_TOUCH) {
                touchPosition touch;
                touchRead(&touch);
                int action = handleTitleMenuTouch(touch.px, touch.py);
                if (action == MENU_ACTION_NEW_GAME) {
                    newWorldConfig.seed = nextRandomSeed();
                    newWorldConfig.gameMode = GAME_MODE_CREATIVE;
                    setPendingWorldConfig(newWorldConfig);
                    invalidateMenuCache();
                    state = STATE_WORLD_SETUP;
                } else if (action == MENU_ACTION_LOAD_GAME) {
                    if (loadGame(player)) {
                        spawnWorldMobs();
                        resetBreakState(breakState);
                        prepareGameplayTransition();
                        flushTransitionGhosting();
                        state = STATE_PLAYING;
                    }
                } else if (action == MENU_ACTION_OPTIONS) {
                    optionsReturnState = OPTIONS_RETURN_TITLE;
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
            if (down & KEY_SELECT) newWorldConfig.gameMode = (newWorldConfig.gameMode + 1) % GAME_MODE_COUNT;
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
                    case MENU_ACTION_WORLD_MODE_PREV:
                    case MENU_ACTION_WORLD_MODE_NEXT:
                        newWorldConfig.gameMode = (newWorldConfig.gameMode + 1) % GAME_MODE_COUNT;
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
                } else if (action == MENU_ACTION_RENDER_DIST_PREV) {
                    cycleRenderDistance(-1);
                } else if (action == MENU_ACTION_RENDER_DIST_NEXT) {
                    cycleRenderDistance(1);
                } else if (action == MENU_ACTION_LOOK_SPEED_PREV) {
                    cycleLookSpeed(-1);
                } else if (action == MENU_ACTION_LOOK_SPEED_NEXT) {
                    cycleLookSpeed(1);
                } else if (action == MENU_ACTION_MOVE_SPEED_PREV) {
                    cycleMoveSpeed(-1);
                } else if (action == MENU_ACTION_MOVE_SPEED_NEXT) {
                    cycleMoveSpeed(1);
                } else if (action == MENU_ACTION_BACK) {
                    invalidateMenuCache();
                    state = (optionsReturnState == OPTIONS_RETURN_PAUSE) ? STATE_PAUSED : STATE_TITLE;
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
                setCurrentGameMode(newWorldConfig.gameMode);
                initPlayer(player);
                player.selectedBlock = BLOCK_GRASS;
                resetBreakState(breakState);
                spawnWorldMobs();
                prepareGameplayTransition();
                flushTransitionGhosting();
                renderFrame(player);
                renderHUD(player, castCenterRay(player, 5.0f), breakState.ticks, 0);
                swiWaitForVBlank();
                state = STATE_PLAYING;
            }
            swiWaitForVBlank();
            continue;
        }

        if (state == STATE_CRAFTING) {
            bool nearTable = isPlayerNearCraftingTable(player);
            if (down & KEY_B) {
                state = STATE_PLAYING;
                swiWaitForVBlank();
                continue;
            }
            if (down & KEY_L) {
                craftingTab = (craftingTab + getCraftingTabCount() - 1) % getCraftingTabCount();
                craftingSelection = 0;
            }
            if (down & KEY_R) {
                craftingTab = (craftingTab + 1) % getCraftingTabCount();
                craftingSelection = 0;
            }
            int count = getRecipeCountForTab(craftingTab, nearTable);
            if (count > 0) {
                if (down & KEY_UP) craftingSelection = (craftingSelection + count - 1) % count;
                if (down & KEY_DOWN) craftingSelection = (craftingSelection + 1) % count;
                if (down & KEY_A) {
                    const CraftingRecipe* recipe = getRecipeForTabIndex(craftingTab, craftingSelection, nearTable);
                    if (recipe) craftRecipe(player, *recipe, nearTable);
                }
            } else {
                craftingSelection = 0;
            }
            renderFrame(player);
            renderCraftingMenu(player, craftingTab, craftingSelection, nearTable);
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
                } else if (action == MENU_ACTION_OPTIONS) {
                    optionsReturnState = OPTIONS_RETURN_PAUSE;
                    invalidateMenuCache();
                    state = STATE_OPTIONS;
                } else if (action == MENU_ACTION_QUIT_TO_TITLE) {
                    optionsReturnState = OPTIONS_RETURN_TITLE;
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

        if (player.alive) {
            if (isCreativeMode()) {
                if (down & KEY_SELECT) changeSelectedBlock(player, -1);
            } else {
                if (down & KEY_SELECT) cycleSelectedSlot(player, 1);
            }

            if (down & KEY_TOUCH) {
                touchPosition touch;
                touchRead(&touch);
                HudTouchAction action = handleHudTouch(touch.px, touch.py);
                if (action.type == HUD_TOUCH_SELECT_BLOCK) {
                    if (isCreativeMode()) player.selectedBlock = action.value;
                    else setSelectedSlot(player, action.value);
                } else if (action.type == HUD_TOUCH_OPEN_CRAFTING && isSurvivalMode()) {
                    craftingTab = 0;
                    craftingSelection = 0;
                    state = STATE_CRAFTING;
                    renderCraftingMenu(player, craftingTab, craftingSelection, isPlayerNearCraftingTable(player));
                    swiWaitForVBlank();
                    continue;
                }
            }


            updatePlayer(player, held, down);
            updateMobs(player);
        } else if ((down & KEY_A) && isSurvivalMode()) {
            respawnPlayer(player);
            resetBreakState(breakState);
        }

        RayHit hit = castCenterRay(player, 5.0f);
        float mobHitDist = 5.0f;
        int hitMobIndex = raycastMob(player, 5.0f, &mobHitDist);

        if (player.alive) {
            if (isCreativeMode()) {
                if ((down & KEY_L) && hit.hit && !(hit.x == (int)player.x && hit.z == (int)player.z)) {
                    setBlock(hit.x, hit.y, hit.z, BLOCK_AIR);
                }
                if ((down & KEY_R) && hit.hit) {
                    if (!isSolidBlock(hit.placeX, hit.placeY, hit.placeZ)) {
                        setBlock(hit.placeX, hit.placeY, hit.placeZ, player.selectedBlock);
                    }
                }
            } else {
                int selectedTool = getSelectedToolItem(player);
                if ((down & KEY_R) && !consumeSelectedItemUse(player) && hit.hit) {
                    int blockToPlace = BLOCK_AIR;
                    if (consumeSelectedPlacement(player, &blockToPlace) && blockToPlace != BLOCK_AIR) {
                        if (!isSolidBlock(hit.placeX, hit.placeY, hit.placeZ)) {
                            setBlock(hit.placeX, hit.placeY, hit.placeZ, blockToPlace);
                        } else {
                            addBlockDropToInventory(player, blockToPlace);
                        }
                    }
                }
                bool mobIsCloser = (hitMobIndex >= 0) && (!hit.hit || mobHitDist <= hit.dist + 0.001f);
                if ((down & KEY_L) && mobIsCloser) {
                    int damage = 1;
                    if (selectedTool == ITEM_WOOD_SWORD) damage = 4;
                    else if (selectedTool == ITEM_STONE_SWORD) damage = 5;
                    else if (selectedTool == ITEM_WOOD_AXE || selectedTool == ITEM_WOOD_PICKAXE || selectedTool == ITEM_WOOD_SHOVEL) damage = 2;
                    else if (selectedTool == ITEM_STONE_AXE || selectedTool == ITEM_STONE_PICKAXE || selectedTool == ITEM_STONE_SHOVEL) damage = 3;
                    damageMob(hitMobIndex, damage);
                    resetBreakState(breakState);
                } else if ((held & KEY_L) && hit.hit && !(hit.x == (int)player.x && hit.z == (int)player.z) && canSurvivalBreakBlock(hit.block, selectedTool) && !mobIsCloser) {
                    if (!breakState.active || breakState.x != hit.x || breakState.y != hit.y || breakState.z != hit.z) {
                        breakState.active = true;
                        breakState.x = hit.x;
                        breakState.y = hit.y;
                        breakState.z = hit.z;
                        breakState.ticks = 0;
                    }
                    ++breakState.ticks;
                    const int needed = getBreakTicksForBlock(hit.block, selectedTool);
                    if (breakState.ticks >= needed) {
                        int brokenBlock = getBlock(hit.x, hit.y, hit.z);
                        setBlock(hit.x, hit.y, hit.z, BLOCK_AIR);
                        addBlockDropToInventory(player, getBlockDropForSurvival(brokenBlock));
                        resetBreakState(breakState);
                    }
                } else {
                    resetBreakState(breakState);
                }
            }
        }

        renderFrame(player);
        int breakNeeded = 0;
        if (breakState.active && hit.hit && hit.x == breakState.x && hit.y == breakState.y && hit.z == breakState.z) {
            breakNeeded = getBreakTicksForBlock(hit.block, getSelectedToolItem(player));
        }
        renderHUD(player, hit, breakState.ticks, breakNeeded);
        swiWaitForVBlank();
    }

    return 0;
}
