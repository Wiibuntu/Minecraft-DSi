#pragma once

#include "player.h"

struct CraftingRecipe {
    const char* name;
    bool tableOnly;
    int tab;
    bool outputIsBlock;
    int outputId;
    int outputCount;
    int ingredientCount;
    InventorySlot ingredients[4];
};

enum CraftingTab {
    CRAFT_TAB_BASICS = 0,
    CRAFT_TAB_TOOLS,
    CRAFT_TAB_BLOCKS,
    CRAFT_TAB_COUNT
};

void initCraftingSystem();
int getCraftingTabCount();
const char* getCraftingTabName(int tab);
int getRecipeCountForTab(int tab, bool nearTable);
const CraftingRecipe* getRecipeForTabIndex(int tab, int visibleIndex, bool nearTable);
bool canCraftRecipe(const Player& p, const CraftingRecipe& recipe, bool nearTable);
bool craftRecipe(Player& p, const CraftingRecipe& recipe, bool nearTable);
bool isPlayerNearCraftingTable(const Player& p);
int getBlockDropForSurvival(int block);
