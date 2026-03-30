#include "crafting.h"

#include <cstring>

static CraftingRecipe gRecipes[] = {
    {"OAK PLANKS", false, CRAFT_TAB_BASICS, true, BLOCK_PLANKS, 4, 1, {{true, BLOCK_WOOD, 1}, {false, ITEM_NONE, 0}, {false, ITEM_NONE, 0}, {false, ITEM_NONE, 0}}},
    {"BIRCH PLANKS", false, CRAFT_TAB_BASICS, true, BLOCK_BIRCH_PLANKS, 4, 1, {{true, BLOCK_BIRCH_WOOD, 1}, {false, ITEM_NONE, 0}, {false, ITEM_NONE, 0}, {false, ITEM_NONE, 0}}},
    {"SPRUCE PLANKS", false, CRAFT_TAB_BASICS, true, BLOCK_SPRUCE_PLANKS, 4, 1, {{true, BLOCK_SPRUCE_WOOD, 1}, {false, ITEM_NONE, 0}, {false, ITEM_NONE, 0}, {false, ITEM_NONE, 0}}},
    {"JUNGLE PLANKS", false, CRAFT_TAB_BASICS, true, BLOCK_JUNGLE_PLANKS, 4, 1, {{true, BLOCK_JUNGLE_WOOD, 1}, {false, ITEM_NONE, 0}, {false, ITEM_NONE, 0}, {false, ITEM_NONE, 0}}},
    {"STICKS", false, CRAFT_TAB_BASICS, false, ITEM_STICK, 4, 1, {{true, BLOCK_PLANKS, 2}, {false, ITEM_NONE, 0}, {false, ITEM_NONE, 0}, {false, ITEM_NONE, 0}}},
    {"CRAFT TABLE", false, CRAFT_TAB_BASICS, true, BLOCK_CRAFTING_TABLE, 1, 1, {{true, BLOCK_PLANKS, 4}, {false, ITEM_NONE, 0}, {false, ITEM_NONE, 0}, {false, ITEM_NONE, 0}}},

    {"WOOD PICK", false, CRAFT_TAB_TOOLS, false, ITEM_WOOD_PICKAXE, 1, 2, {{true, BLOCK_PLANKS, 3}, {false, ITEM_STICK, 2}, {false, ITEM_NONE, 0}, {false, ITEM_NONE, 0}}},
    {"WOOD AXE", false, CRAFT_TAB_TOOLS, false, ITEM_WOOD_AXE, 1, 2, {{true, BLOCK_PLANKS, 3}, {false, ITEM_STICK, 2}, {false, ITEM_NONE, 0}, {false, ITEM_NONE, 0}}},
    {"WOOD SHOVL", false, CRAFT_TAB_TOOLS, false, ITEM_WOOD_SHOVEL, 1, 2, {{true, BLOCK_PLANKS, 1}, {false, ITEM_STICK, 2}, {false, ITEM_NONE, 0}, {false, ITEM_NONE, 0}}},
    {"WOOD SWRD", false, CRAFT_TAB_TOOLS, false, ITEM_WOOD_SWORD, 1, 2, {{true, BLOCK_PLANKS, 2}, {false, ITEM_STICK, 1}, {false, ITEM_NONE, 0}, {false, ITEM_NONE, 0}}},

    {"STONE PICK", true, CRAFT_TAB_TOOLS, false, ITEM_STONE_PICKAXE, 1, 2, {{true, BLOCK_COBBLE, 3}, {false, ITEM_STICK, 2}, {false, ITEM_NONE, 0}, {false, ITEM_NONE, 0}}},
    {"STONE AXE", true, CRAFT_TAB_TOOLS, false, ITEM_STONE_AXE, 1, 2, {{true, BLOCK_COBBLE, 3}, {false, ITEM_STICK, 2}, {false, ITEM_NONE, 0}, {false, ITEM_NONE, 0}}},
    {"STONE SHVL", true, CRAFT_TAB_TOOLS, false, ITEM_STONE_SHOVEL, 1, 2, {{true, BLOCK_COBBLE, 1}, {false, ITEM_STICK, 2}, {false, ITEM_NONE, 0}, {false, ITEM_NONE, 0}}},
    {"STONE SWRD", true, CRAFT_TAB_TOOLS, false, ITEM_STONE_SWORD, 1, 2, {{true, BLOCK_COBBLE, 2}, {false, ITEM_STICK, 1}, {false, ITEM_NONE, 0}, {false, ITEM_NONE, 0}}},

    {"SANDSTONE", true, CRAFT_TAB_BLOCKS, true, BLOCK_SANDSTONE, 1, 1, {{true, BLOCK_SAND, 4}, {false, ITEM_NONE, 0}, {false, ITEM_NONE, 0}, {false, ITEM_NONE, 0}}},
    {"FURNACE", true, CRAFT_TAB_BLOCKS, true, BLOCK_FURNACE, 1, 1, {{true, BLOCK_COBBLE, 8}, {false, ITEM_NONE, 0}, {false, ITEM_NONE, 0}, {false, ITEM_NONE, 0}}},
    {"TORCH X4", true, CRAFT_TAB_BLOCKS, true, BLOCK_TORCH, 4, 2, {{false, ITEM_COAL, 1}, {false, ITEM_STICK, 1}, {false, ITEM_NONE, 0}, {false, ITEM_NONE, 0}}}
};

void initCraftingSystem() {}
int getCraftingTabCount() { return CRAFT_TAB_COUNT; }

const char* getCraftingTabName(int tab) {
    switch (tab) {
        case CRAFT_TAB_BASICS: return "BASICS";
        case CRAFT_TAB_TOOLS: return "TOOLS";
        case CRAFT_TAB_BLOCKS: return "BLOCKS";
        default: return "CRAFT";
    }
}

static int countMatching(const Player& p, bool isBlock, int id) {
    int total = 0;
    for (int i = 0; i < HOTBAR_SIZE; ++i) {
        const InventorySlot& slot = p.hotbar[i];
        if (slot.count > 0 && slot.isBlock == isBlock && slot.id == id) total += slot.count;
    }
    return total;
}

static bool removeFromInventory(Player& p, bool isBlock, int id, int count) {
    for (int i = 0; i < HOTBAR_SIZE && count > 0; ++i) {
        InventorySlot& slot = p.hotbar[i];
        if (slot.count > 0 && slot.isBlock == isBlock && slot.id == id) {
            int used = slot.count < count ? slot.count : count;
            slot.count -= used;
            count -= used;
            if (slot.count <= 0) slot = {false, ITEM_NONE, 0};
        }
    }
    return count == 0;
}

static bool addToInventory(Player& p, bool isBlock, int id, int count) {
    for (int i = 0; i < HOTBAR_SIZE && count > 0; ++i) {
        InventorySlot& slot = p.hotbar[i];
        if (slot.count > 0 && slot.isBlock == isBlock && slot.id == id && slot.count < 99) {
            int room = 99 - slot.count;
            int give = room < count ? room : count;
            slot.count += give;
            count -= give;
        }
    }
    for (int i = 0; i < HOTBAR_SIZE && count > 0; ++i) {
        InventorySlot& slot = p.hotbar[i];
        if (slot.count <= 0) {
            int give = count > 99 ? 99 : count;
            slot = {isBlock, id, give};
            count -= give;
        }
    }
    return count == 0;
}

int getRecipeCountForTab(int tab, bool nearTable) {
    int count = 0;
    for (const auto& recipe : gRecipes) {
        if (recipe.tab != tab) continue;
        if (recipe.tableOnly && !nearTable) continue;
        ++count;
    }
    return count;
}

const CraftingRecipe* getRecipeForTabIndex(int tab, int visibleIndex, bool nearTable) {
    for (const auto& recipe : gRecipes) {
        if (recipe.tab != tab) continue;
        if (recipe.tableOnly && !nearTable) continue;
        if (visibleIndex == 0) return &recipe;
        --visibleIndex;
    }
    return nullptr;
}

bool canCraftRecipe(const Player& p, const CraftingRecipe& recipe, bool nearTable) {
    if (recipe.tableOnly && !nearTable) return false;
    for (int i = 0; i < recipe.ingredientCount; ++i) {
        const InventorySlot& need = recipe.ingredients[i];
        if (countMatching(p, need.isBlock, need.id) < need.count) return false;
    }
    Player copy = p;
    return addToInventory(copy, recipe.outputIsBlock, recipe.outputId, recipe.outputCount);
}

bool craftRecipe(Player& p, const CraftingRecipe& recipe, bool nearTable) {
    if (!canCraftRecipe(p, recipe, nearTable)) return false;
    for (int i = 0; i < recipe.ingredientCount; ++i) {
        const InventorySlot& need = recipe.ingredients[i];
        if (!removeFromInventory(p, need.isBlock, need.id, need.count)) return false;
    }
    return addToInventory(p, recipe.outputIsBlock, recipe.outputId, recipe.outputCount);
}

bool isPlayerNearCraftingTable(const Player& p) {
    const int px = (int)p.x;
    const int py = (int)p.y;
    const int pz = (int)p.z;
    for (int dz = -1; dz <= 1; ++dz) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (getBlock(px + dx, py + dy, pz + dz) == BLOCK_CRAFTING_TABLE) return true;
            }
        }
    }
    return false;
}

int getBlockDropForSurvival(int block) {
    switch (block) {
        case BLOCK_STONE: return BLOCK_COBBLE;
        default: return block;
    }
}
