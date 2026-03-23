#include "world.h"

#include <cstdlib>
#include <cstring>
#include <cmath>

static u8 gWorld[WORLD_X][WORLD_Y][WORLD_Z];
static u8 gTopVisible[WORLD_X][WORLD_Z];
static int gWorldRevision = 0;
static u32 gWorldSeed = 0;
static int gGenX = 0;
static int gGenZ = 0;
static int gWorldGenProgress = 100;
static int gSpawnX = WORLD_X / 2;
static int gSpawnY = 6;
static int gSpawnZ = WORLD_Z / 2;

static inline bool inBounds(int x, int y, int z) {
    return x >= 0 && x < WORLD_X && y >= 0 && y < WORLD_Y && z >= 0 && z < WORLD_Z;
}

static inline u32 mixSeed(u32 seed, int x, int z, u32 salt) {
    u32 h = seed;
    h ^= (u32)(x * 374761393u);
    h = (h << 13) | (h >> 19);
    h ^= (u32)(z * 668265263u);
    h ^= salt * 2246822519u;
    h *= 3266489917u;
    h ^= h >> 16;
    return h;
}

static void refreshTopVisibleColumn(int x, int z) {
    if (x < 0 || x >= WORLD_X || z < 0 || z >= WORLD_Z) return;
    int topBlock = BLOCK_AIR;
    for (int y = WORLD_Y - 1; y >= 0; --y) {
        int b = gWorld[x][y][z];
        if (b != BLOCK_AIR && b != BLOCK_WATER) {
            topBlock = b;
            break;
        }
    }
    gTopVisible[x][z] = (u8)topBlock;
}

int getBlock(int x, int y, int z) {
    if (!inBounds(x, y, z)) return BLOCK_AIR;
    return gWorld[x][y][z];
}

void setBlock(int x, int y, int z, int block) {
    if (!inBounds(x, y, z)) return;
    if (gWorld[x][y][z] == (u8)block) return;
    gWorld[x][y][z] = (u8)block;
    refreshTopVisibleColumn(x, z);
    ++gWorldRevision;
}

bool isSolidBlock(int x, int y, int z) {
    int b = getBlock(x, y, z);
    return b != BLOCK_AIR && b != BLOCK_WATER;
}

bool isOpaqueBlock(int x, int y, int z) {
    int b = getBlock(x, y, z);
    return b != BLOCK_AIR && b != BLOCK_WATER && b != BLOCK_GLASS;
}

int getTopVisibleBlock(int x, int z) {
    if (x < 0 || x >= WORLD_X || z < 0 || z >= WORLD_Z) return BLOCK_AIR;
    return gTopVisible[x][z];
}

int getWorldRevision() {
    return gWorldRevision;
}

u32 getWorldSeed() {
    return gWorldSeed;
}

int getWorldGenProgress() {
    return gWorldGenProgress;
}

void getSpawnPosition(float& x, float& y, float& z) {
    x = (float)gSpawnX + 0.5f;
    y = (float)gSpawnY;
    z = (float)gSpawnZ + 0.5f;
}

static int terrainHeight(int x, int z) {
    int h = 4;
    h += (int)(mixSeed(gWorldSeed, x, z, 1) & 3u) - 1;
    h += (int)(mixSeed(gWorldSeed, x >> 1, z >> 1, 2) & 3u) - 1;
    h += (int)(mixSeed(gWorldSeed, x / 3, z / 3, 3) & 1u);
    if ((mixSeed(gWorldSeed, x / 5, z / 5, 4) & 7u) == 0u) h -= 1;
    if ((mixSeed(gWorldSeed, x / 7, z / 7, 5) & 7u) == 1u) h += 1;
    if (h < 2) h = 2;
    if (h > WORLD_Y - 4) h = WORLD_Y - 4;
    return h;
}

static void buildTreeRaw(int x, int y, int z) {
    if (x < 2 || z < 2 || x >= WORLD_X - 2 || z >= WORLD_Z - 2) return;
    for (int i = 0; i < 3 && y + i < WORLD_Y; ++i) {
        gWorld[x][y + i][z] = (u8)BLOCK_WOOD;
    }
    int top = y + 2;
    for (int dx = -2; dx <= 2; ++dx) {
        for (int dz = -2; dz <= 2; ++dz) {
            for (int dy = -1; dy <= 1; ++dy) {
                if (abs(dx) + abs(dz) + abs(dy) > 4) continue;
                int ax = x + dx;
                int ay = top + dy;
                int az = z + dz;
                if (inBounds(ax, ay, az) && gWorld[ax][ay][az] == BLOCK_AIR) {
                    gWorld[ax][ay][az] = (u8)BLOCK_LEAVES;
                }
            }
        }
    }
}

static void generateColumn(int x, int z) {
    int h = terrainHeight(x, z);
    bool sandy = (x < 5 || z < 5 || x > WORLD_X - 6 || z > WORLD_Z - 6);
    for (int y = 0; y < WORLD_Y; ++y) gWorld[x][y][z] = BLOCK_AIR;

    for (int y = 0; y <= h; ++y) {
        int block = BLOCK_DIRT;
        if (y == 0) {
            block = BLOCK_STONE;
        } else if (y < h - 2) {
            block = BLOCK_STONE;
        } else if (y < h) {
            block = sandy ? BLOCK_SAND : BLOCK_DIRT;
        } else {
            block = sandy ? BLOCK_SAND : BLOCK_GRASS;
        }
        gWorld[x][y][z] = (u8)block;
    }

    const int waterLevel = 3;
    if (h < waterLevel) {
        for (int y = h + 1; y <= waterLevel; ++y) gWorld[x][y][z] = BLOCK_WATER;
    }

    if (!sandy && h >= 4 && (mixSeed(gWorldSeed, x, z, 6) % 31u) == 0u) {
        buildTreeRaw(x, h + 1, z);
    }

    refreshTopVisibleColumn(x, z);
}

void beginWorldGeneration(u32 seed) {
    gWorldSeed = seed;
    std::memset(gWorld, 0, sizeof(gWorld));
    std::memset(gTopVisible, 0, sizeof(gTopVisible));
    gWorldRevision = 1;
    gGenX = 0;
    gGenZ = 0;
    gWorldGenProgress = 0;
    gSpawnX = WORLD_X / 2;
    gSpawnZ = WORLD_Z / 2;
    gSpawnY = 6;
}

bool generateWorldStep(int columnsPerStep) {
    if (gWorldGenProgress >= 100) return true;

    while (columnsPerStep-- > 0 && gGenZ < WORLD_Z) {
        generateColumn(gGenX, gGenZ);
        ++gGenX;
        if (gGenX >= WORLD_X) {
            gGenX = 0;
            ++gGenZ;
        }
    }

    int total = WORLD_X * WORLD_Z;
    int done = gGenZ * WORLD_X + gGenX;
    if (done > total) done = total;
    gWorldGenProgress = (done * 100) / total;

    if (gGenZ >= WORLD_Z) {
        int bestX = WORLD_X / 2;
        int bestZ = WORLD_Z / 2;
        int bestDist = 1 << 30;
        for (int z = 2; z < WORLD_Z - 2; ++z) {
            for (int x = 2; x < WORLD_X - 2; ++x) {
                int top = -1;
                for (int y = WORLD_Y - 1; y >= 0; --y) {
                    if (gWorld[x][y][z] != BLOCK_AIR && gWorld[x][y][z] != BLOCK_WATER) {
                        top = y;
                        break;
                    }
                }
                if (top < 0) continue;
                int d = abs(x - WORLD_X / 2) + abs(z - WORLD_Z / 2) + abs(top - 6) * 2;
                if (d < bestDist) {
                    bestDist = d;
                    bestX = x;
                    bestZ = z;
                    gSpawnY = top + 2;
                }
            }
        }
        gSpawnX = bestX;
        gSpawnZ = bestZ;
        gWorldGenProgress = 100;
        ++gWorldRevision;
        return true;
    }

    ++gWorldRevision;
    return false;
}

void initWorld() {
    beginWorldGeneration(0xC0FFEE11u);
    while (!generateWorldStep(WORLD_X * WORLD_Z)) {}
}
