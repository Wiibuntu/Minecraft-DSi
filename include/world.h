#pragma once

#include <nds.h>

enum BlockType {
    BLOCK_AIR = 0,
    BLOCK_GRASS = 1,
    BLOCK_DIRT = 2,
    BLOCK_STONE = 3,
    BLOCK_WOOD = 4,
    BLOCK_LEAVES = 5,
    BLOCK_SAND = 6,
    BLOCK_WATER = 7,
    BLOCK_COBBLE = 8,
    BLOCK_PLANKS = 9,
    BLOCK_BRICK = 10,
    BLOCK_GLASS = 11
};

static const int WORLD_X = 32;
static const int WORLD_Y = 16;
static const int WORLD_Z = 32;

void initWorld();
int getBlock(int x, int y, int z);
void setBlock(int x, int y, int z, int block);
bool isSolidBlock(int x, int y, int z);
bool isOpaqueBlock(int x, int y, int z);
int getTopVisibleBlock(int x, int z);
int getWorldRevision();
