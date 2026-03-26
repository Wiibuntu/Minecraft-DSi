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
static const int WORLD_Y = 24;
static const int WORLD_Z = 32;

enum WorldType {
    WORLD_TYPE_DEFAULT = 0,
    WORLD_TYPE_PERLIN,
    WORLD_TYPE_SUPERFLAT,
    WORLD_TYPE_LARGE_BIOMES,
    WORLD_TYPE_COUNT
};

enum WorldSizePreset {
    WORLD_SIZE_CLASSIC = 0,
    WORLD_SIZE_SMALL,
    WORLD_SIZE_MEDIUM,
    WORLD_SIZE_LARGE,
    WORLD_SIZE_COUNT
};

struct WorldGenConfig {
    u32 seed;
    int worldType;
    int sizePreset;
    bool generateTrees;
};

void initWorld();
void setPendingWorldConfig(const WorldGenConfig& config);
WorldGenConfig getPendingWorldConfig();
void beginWorldGeneration(const WorldGenConfig& config);
void beginWorldGeneration(u32 seed);
bool generateWorldStep(int columnsPerStep);
bool exportWorldState(void* dst, int dstSize, int* outUsedSize);
bool importWorldState(const void* src, int srcSize, const WorldGenConfig* config, int spawnX, int spawnY, int spawnZ);
int getWorldGenProgress();
u32 getWorldSeed();
const WorldGenConfig& getWorldConfig();
const char* getWorldTypeName(int type);
const char* getWorldSizeName(int sizePreset);
void getSpawnPosition(float& x, float& y, float& z);

int getBlock(int x, int y, int z);
void setBlock(int x, int y, int z, int block);
bool isSolidBlock(int x, int y, int z);
bool isOpaqueBlock(int x, int y, int z);
int getTopVisibleBlock(int x, int z);
int getWorldRevision();
