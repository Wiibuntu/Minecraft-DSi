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
    BLOCK_GLASS = 11,
    BLOCK_BIRCH_WOOD = 12,
    BLOCK_SPRUCE_WOOD = 13,
    BLOCK_JUNGLE_WOOD = 14,
    BLOCK_BIRCH_LEAVES = 15,
    BLOCK_SPRUCE_LEAVES = 16,
    BLOCK_JUNGLE_LEAVES = 17,
    BLOCK_BIRCH_PLANKS = 18,
    BLOCK_SPRUCE_PLANKS = 19,
    BLOCK_JUNGLE_PLANKS = 20,
    BLOCK_SANDSTONE = 21,
    BLOCK_OBSIDIAN = 22,
    BLOCK_GRAVEL = 23,
    BLOCK_BOOKSHELF = 24,
    BLOCK_WHITE_WOOL = 25,
    BLOCK_GOLD_BLOCK = 26,
    BLOCK_IRON_BLOCK = 27,
    BLOCK_TORCH = 28
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
    int gameMode;
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

void updateWorldTime(int ticks);
int getWorldTime();
bool isDayTime();
int getSkyLightLevel();
int getBlockLight(int x, int y, int z);
int getCombinedLightLevel(int x, int y, int z);
bool isLightEmitterBlock(int block);
