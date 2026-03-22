#include "world.h"

#include <cstdlib>
#include <cstring>

static u8 gWorld[WORLD_X][WORLD_Y][WORLD_Z];
static u8 gTopVisible[WORLD_X][WORLD_Z];
static int gWorldRevision = 0;

static inline bool inBounds(int x, int y, int z) {
    return x >= 0 && x < WORLD_X && y >= 0 && y < WORLD_Y && z >= 0 && z < WORLD_Z;
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
    return b != BLOCK_AIR && b != BLOCK_WATER;
}

int getTopVisibleBlock(int x, int z) {
    if (x < 0 || x >= WORLD_X || z < 0 || z >= WORLD_Z) return BLOCK_AIR;
    return gTopVisible[x][z];
}

int getWorldRevision() {
    return gWorldRevision;
}

static int terrainHeight(int x, int z) {
    int h = 4;
    h += (((x * 13 + z * 7) & 7) - 3) / 2;
    h += (((x * x + z * 3) ^ (z * 11)) & 3) - 1;
    if (h < 2) h = 2;
    if (h > WORLD_Y - 4) h = WORLD_Y - 4;
    return h;
}

static void buildTree(int x, int y, int z) {
    if (x < 2 || z < 2 || x >= WORLD_X - 2 || z >= WORLD_Z - 2) return;
    for (int i = 0; i < 3 && y + i < WORLD_Y; ++i) {
        setBlock(x, y + i, z, BLOCK_WOOD);
    }
    int top = y + 2;
    for (int dx = -2; dx <= 2; ++dx) {
        for (int dz = -2; dz <= 2; ++dz) {
            for (int dy = -1; dy <= 1; ++dy) {
                if (abs(dx) + abs(dz) + abs(dy) > 4) continue;
                int ax = x + dx;
                int ay = top + dy;
                int az = z + dz;
                if (inBounds(ax, ay, az) && getBlock(ax, ay, az) == BLOCK_AIR) {
                    setBlock(ax, ay, az, BLOCK_LEAVES);
                }
            }
        }
    }
}

void initWorld() {
    std::memset(gWorld, 0, sizeof(gWorld));
    std::memset(gTopVisible, 0, sizeof(gTopVisible));
    gWorldRevision = 1;

    for (int x = 0; x < WORLD_X; ++x) {
        for (int z = 0; z < WORLD_Z; ++z) {
            int h = terrainHeight(x, z);
            bool sandy = (x < 5 || z < 5 || x > WORLD_X - 6 || z > WORLD_Z - 6);
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

            int waterLevel = 3;
            if (h < waterLevel) {
                for (int y = h + 1; y <= waterLevel; ++y) gWorld[x][y][z] = BLOCK_WATER;
            }

            if (!sandy && h >= 4 && ((x * 17 + z * 31) % 29) == 0) {
                buildTree(x, h + 1, z);
            }
            refreshTopVisibleColumn(x, z);
        }
    }
}
