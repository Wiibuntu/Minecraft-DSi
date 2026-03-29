#include "world.h"
#include "player.h"

#include <cstdlib>
#include <cstring>
#include <cmath>

static u8 gWorld[WORLD_X][WORLD_Y][WORLD_Z];
static u8 gTopVisible[WORLD_X][WORLD_Z];
static int gWorldRevision = 0;
static u32 gWorldSeed = 0;
static WorldGenConfig gWorldConfig = {0x13579BDFu, WORLD_TYPE_DEFAULT, WORLD_SIZE_CLASSIC, true, GAME_MODE_CREATIVE};
static WorldGenConfig gPendingConfig = {0x13579BDFu, WORLD_TYPE_DEFAULT, WORLD_SIZE_CLASSIC, true, GAME_MODE_CREATIVE};
static int gGenX = 0;
static int gGenZ = 0;
static int gWorldGenProgress = 100;
static int gSpawnX = WORLD_X / 2;
static int gSpawnY = 10;
static int gSpawnZ = WORLD_Z / 2;
static u8 gPerm[512];
static u8 gTorchLight[WORLD_X][WORLD_Y][WORLD_Z];
static int gWorldTime = 6000;
static const int kDayLength = 24000;

static const int kSeaLevel = 5;
static const int kOceanFloor = 2;

static inline bool inBounds(int x, int y, int z) {
    return x >= 0 && x < WORLD_X && y >= 0 && y < WORLD_Y && z >= 0 && z < WORLD_Z;
}

bool isLightEmitterBlock(int block) {
    return block == BLOCK_TORCH;
}

static void rebuildTorchLight() {
    std::memset(gTorchLight, 0, sizeof(gTorchLight));
    for (int x = 0; x < WORLD_X; ++x) {
        for (int y = 0; y < WORLD_Y; ++y) {
            for (int z = 0; z < WORLD_Z; ++z) {
                if (!isLightEmitterBlock(gWorld[x][y][z])) continue;
                for (int dz = -6; dz <= 6; ++dz) {
                    for (int dy = -6; dy <= 6; ++dy) {
                        for (int dx = -6; dx <= 6; ++dx) {
                            int dist = std::abs(dx) + std::abs(dy) + std::abs(dz);
                            if (dist > 6) continue;
                            int ax = x + dx;
                            int ay = y + dy;
                            int az = z + dz;
                            if (!inBounds(ax, ay, az)) continue;
                            int light = 28 - dist * 4;
                            if (light < 0) light = 0;
                            if (gTorchLight[ax][ay][az] < light) gTorchLight[ax][ay][az] = (u8)light;
                        }
                    }
                }
            }
        }
    }
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

static float sizeRadiusScale() {
    switch (gWorldConfig.sizePreset) {
        default:
        case WORLD_SIZE_CLASSIC: return 0.68f;
        case WORLD_SIZE_SMALL:   return 0.82f;
        case WORLD_SIZE_MEDIUM:  return 0.96f;
        case WORLD_SIZE_LARGE:   return 1.14f;
    }
}

static int sizeHeightBias() {
    switch (gWorldConfig.sizePreset) {
        default:
        case WORLD_SIZE_CLASSIC: return 0;
        case WORLD_SIZE_SMALL:   return 1;
        case WORLD_SIZE_MEDIUM:  return 2;
        case WORLD_SIZE_LARGE:   return 3;
    }
}

static int sizeTreeChanceDiv() {
    switch (gWorldConfig.sizePreset) {
        default:
        case WORLD_SIZE_CLASSIC: return 34;
        case WORLD_SIZE_SMALL:   return 29;
        case WORLD_SIZE_MEDIUM:  return 24;
        case WORLD_SIZE_LARGE:   return 19;
    }
}

static void initPerlin(u32 seed) {
    u8 base[256];
    for (int i = 0; i < 256; ++i) base[i] = (u8)i;

    u32 state = seed ? seed : 0xA341316Cu;
    for (int i = 255; i >= 0; --i) {
        state = state * 1664525u + 1013904223u;
        int j = (int)(state % (u32)(i + 1));
        u8 t = base[i];
        base[i] = base[j];
        base[j] = t;
    }

    for (int i = 0; i < 256; ++i) {
        gPerm[i] = base[i];
        gPerm[256 + i] = base[i];
    }
}

static inline float fadef(float t) {
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

static inline float lerpf(float a, float b, float t) {
    return a + (b - a) * t;
}

static inline float grad2(int hash, float x, float y) {
    switch (hash & 7) {
        case 0: return  x + y;
        case 1: return -x + y;
        case 2: return  x - y;
        case 3: return -x - y;
        case 4: return  x;
        case 5: return -x;
        case 6: return  y;
        default: return -y;
    }
}

static float perlin2D(float x, float y) {
    int xi0 = (int)std::floor(x) & 255;
    int yi0 = (int)std::floor(y) & 255;
    int xi1 = (xi0 + 1) & 255;
    int yi1 = (yi0 + 1) & 255;

    float xf = x - std::floor(x);
    float yf = y - std::floor(y);
    float u = fadef(xf);
    float v = fadef(yf);

    int aa = gPerm[gPerm[xi0] + yi0];
    int ab = gPerm[gPerm[xi0] + yi1];
    int ba = gPerm[gPerm[xi1] + yi0];
    int bb = gPerm[gPerm[xi1] + yi1];

    float x1 = lerpf(grad2(aa, xf, yf), grad2(ba, xf - 1.0f, yf), u);
    float x2 = lerpf(grad2(ab, xf, yf - 1.0f), grad2(bb, xf - 1.0f, yf - 1.0f), u);
    return lerpf(x1, x2, v);
}

static float fbm2D(float x, float z, int octaves, float lacunarity, float gain) {
    float value = 0.0f;
    float amp = 1.0f;
    float freq = 1.0f;
    float norm = 0.0f;
    for (int i = 0; i < octaves; ++i) {
        value += perlin2D(x * freq, z * freq) * amp;
        norm += amp;
        freq *= lacunarity;
        amp *= gain;
    }
    if (norm <= 0.0f) return 0.0f;
    return value / norm;
}

int getBlock(int x, int y, int z) {
    if (y < 0) return BLOCK_STONE;
    if (y >= WORLD_Y) return BLOCK_AIR;

    if (x < 0 || x >= WORLD_X || z < 0 || z >= WORLD_Z) {
        if (y <= kOceanFloor) return BLOCK_SAND;
        if (y <= kSeaLevel) return BLOCK_WATER;
        return BLOCK_AIR;
    }
    return gWorld[x][y][z];
}

void setBlock(int x, int y, int z, int block) {
    if (!inBounds(x, y, z)) return;
    if (gWorld[x][y][z] == (u8)block) return;
    gWorld[x][y][z] = (u8)block;
    refreshTopVisibleColumn(x, z);
    rebuildTorchLight();
    ++gWorldRevision;
}

bool isSolidBlock(int x, int y, int z) {
    int b = getBlock(x, y, z);
    return b != BLOCK_AIR && b != BLOCK_WATER && b != BLOCK_TORCH;
}

bool isOpaqueBlock(int x, int y, int z) {
    int b = getBlock(x, y, z);
    return b != BLOCK_AIR && b != BLOCK_WATER && b != BLOCK_GLASS && b != BLOCK_TORCH;
}

int getTopVisibleBlock(int x, int z) {
    if (x < 0 || x >= WORLD_X || z < 0 || z >= WORLD_Z) return BLOCK_AIR;
    return gTopVisible[x][z];
}

int getWorldRevision() {
    return gWorldRevision;
}

void updateWorldTime(int ticks) {
    if (ticks == 0) return;
    gWorldTime = (gWorldTime + ticks) % kDayLength;
    if (gWorldTime < 0) gWorldTime += kDayLength;
}

int getWorldTime() {
    return gWorldTime;
}

bool isDayTime() {
    return gWorldTime >= 0 && gWorldTime < 12000;
}

int getSkyLightLevel() {
    const float t = (float)gWorldTime / (float)kDayLength;
    const float radians = t * 6.2831853f;
    float wave = (std::cos(radians - 1.5707963f) + 1.0f) * 0.5f;
    int light = 6 + (int)std::floor(wave * 25.0f + 0.5f);
    if (light < 4) light = 4;
    if (light > 31) light = 31;
    return light;
}

int getBlockLight(int x, int y, int z) {
    if (!inBounds(x, y, z)) return 0;
    return gTorchLight[x][y][z];
}

int getCombinedLightLevel(int x, int y, int z) {
    int sky = getSkyLightLevel();
    if (inBounds(x, y, z)) {
        int occlusion = 0;
        int leafCover = 0;
        for (int yy = y + 1; yy < WORLD_Y; ++yy) {
            const int above = getBlock(x, yy, z);
            if (above == BLOCK_AIR || above == BLOCK_WATER || above == BLOCK_GLASS || above == BLOCK_TORCH) continue;
            const bool leaves = (above == BLOCK_LEAVES || above == BLOCK_BIRCH_LEAVES || above == BLOCK_SPRUCE_LEAVES || above == BLOCK_JUNGLE_LEAVES);
            if (leaves) {
                ++leafCover;
                occlusion += 2;
            } else {
                occlusion += 6;
            }
            if (occlusion >= 26) break;
        }

        // Keep some ambient skylight even under cover so shadows stay readable.
        const int skyFloor = 8;
        sky -= occlusion;
        if (leafCover > 0) sky += 2;
        if (sky < skyFloor) sky = skyFloor;
    }
    int block = getBlockLight(x, y, z);
    return sky > block ? sky : block;
}

u32 getWorldSeed() {
    return gWorldSeed;
}

const WorldGenConfig& getWorldConfig() {
    return gWorldConfig;
}

void setPendingWorldConfig(const WorldGenConfig& config) {
    gPendingConfig = config;
}

WorldGenConfig getPendingWorldConfig() {
    return gPendingConfig;
}

const char* getWorldTypeName(int type) {
    switch (type) {
        case WORLD_TYPE_PERLIN: return "PERLIN";
        case WORLD_TYPE_SUPERFLAT: return "SUPERFLAT";
        case WORLD_TYPE_LARGE_BIOMES: return "L BIOMES";
        case WORLD_TYPE_DEFAULT:
        default: return "DEFAULT";
    }
}

const char* getWorldSizeName(int sizePreset) {
    switch (sizePreset) {
        case WORLD_SIZE_SMALL: return "SMALL";
        case WORLD_SIZE_MEDIUM: return "MEDIUM";
        case WORLD_SIZE_LARGE: return "LARGE";
        case WORLD_SIZE_CLASSIC:
        default: return "CLASSIC";
    }
}

int getWorldGenProgress() {
    return gWorldGenProgress;
}

void getSpawnPosition(float& x, float& y, float& z) {
    x = (float)gSpawnX + 0.5f;
    y = (float)gSpawnY;
    z = (float)gSpawnZ + 0.5f;
}

static float islandMask(int x, int z, float innerScale, float outerScale) {
    const float scale = sizeRadiusScale();
    const float cx = (float)x - (WORLD_X * 0.5f - 0.5f);
    const float cz = (float)z - (WORLD_Z * 0.5f - 0.5f);
    const float dist = std::sqrt(cx * cx + cz * cz);
    const float inner = innerScale * scale;
    const float outer = outerScale * scale + 1.0f;
    if (dist <= inner) return 1.0f;
    if (dist >= outer) return 0.0f;
    float t = (dist - inner) / (outer - inner);
    return 1.0f - t;
}

static int clampTerrainHeight(int h) {
    if (h < 3) h = 3;
    if (h > WORLD_Y - 6) h = WORLD_Y - 6;
    return h;
}

static int terrainHeightDefault(int x, int z) {
    int h = 7;
    h += (int)(mixSeed(gWorldSeed, x, z, 1) & 3u) - 1;
    h += (int)(mixSeed(gWorldSeed, x >> 1, z >> 1, 2) & 3u) - 1;
    h += (int)(mixSeed(gWorldSeed, x / 3, z / 3, 3) & 3u) - 1;
    h += (int)(mixSeed(gWorldSeed, x / 5, z / 5, 4) & 3u) - 1;

    const float mask = islandMask(x, z, 9.0f, 13.5f);
    if (mask < 1.0f) h -= (int)((1.0f - mask) * 7.0f);
    if (mask <= 0.05f) h = 3 + (int)(mixSeed(gWorldSeed, x, z, 9) & 1u);

    if ((mixSeed(gWorldSeed, x / 7, z / 7, 5) & 7u) == 0u) h -= 2;
    if ((mixSeed(gWorldSeed, x / 9, z / 9, 6) & 7u) == 1u) h += 2;
    h += sizeHeightBias();
    return clampTerrainHeight(h);
}

static int terrainHeightPerlin(int x, int z) {
    const float scale = 0.100f;
    const float broadScale = 0.042f;
    const float ridgeScale = 0.185f;
    float n1 = fbm2D((float)x * scale, (float)z * scale, 4, 2.0f, 0.5f);
    float n2 = fbm2D((float)x * broadScale + 17.0f, (float)z * broadScale - 9.0f, 3, 2.0f, 0.55f);
    float ridge = 1.0f - std::fabs(fbm2D((float)x * ridgeScale - 5.0f, (float)z * ridgeScale + 11.0f, 2, 2.0f, 0.5f));
    float mask = islandMask(x, z, 8.5f, 14.0f);

    float terrain = 7.0f + n1 * 4.2f + n2 * 2.3f + ridge * 1.7f;
    terrain = (terrain - 3.5f) * (0.35f + mask * 0.65f) + 3.5f;
    if (mask <= 0.08f) terrain = 3.0f + ((mixSeed(gWorldSeed, x, z, 61) & 1u) ? 1.0f : 0.0f);
    terrain += (float)sizeHeightBias();
    return clampTerrainHeight((int)std::floor(terrain + 0.5f));
}

static int terrainHeightLargeBiomes(int x, int z) {
    float n1 = fbm2D((float)x * 0.052f, (float)z * 0.052f, 4, 2.0f, 0.55f);
    float n2 = fbm2D((float)x * 0.021f + 31.0f, (float)z * 0.021f - 27.0f, 3, 2.0f, 0.60f);
    float n3 = fbm2D((float)x * 0.110f - 7.0f, (float)z * 0.110f + 13.0f, 2, 2.0f, 0.45f);
    float mask = islandMask(x, z, 10.5f, 15.0f);

    float terrain = 8.0f + n1 * 5.0f + n2 * 3.2f + n3 * 1.4f + (float)sizeHeightBias();
    terrain = (terrain - 3.0f) * (0.30f + mask * 0.70f) + 3.0f;
    if (mask <= 0.06f) terrain = 3.0f + ((mixSeed(gWorldSeed, x, z, 62) & 1u) ? 1.0f : 0.0f);
    return clampTerrainHeight((int)std::floor(terrain + 0.5f));
}

static int terrainHeightSuperflat(int x, int z) {
    (void)x;
    (void)z;
    int h = 4 + sizeHeightBias() / 2;
    if (h > WORLD_Y - 8) h = WORLD_Y - 8;
    return h;
}

static int terrainHeight(int x, int z) {
    switch (gWorldConfig.worldType) {
        case WORLD_TYPE_PERLIN: return terrainHeightPerlin(x, z);
        case WORLD_TYPE_SUPERFLAT: return terrainHeightSuperflat(x, z);
        case WORLD_TYPE_LARGE_BIOMES: return terrainHeightLargeBiomes(x, z);
        case WORLD_TYPE_DEFAULT:
        default: return terrainHeightDefault(x, z);
    }
}

static bool isSandyEdge(int x, int z, int h) {
    if (gWorldConfig.worldType == WORLD_TYPE_SUPERFLAT) return false;
    const float scale = sizeRadiusScale();
    int border = 5 - (int)((scale - 0.68f) * 3.0f);
    if (border < 2) border = 2;
    return x < border || z < border || x >= WORLD_X - border || z >= WORLD_Z - border || h <= kSeaLevel + 1;
}

static void buildTreeRaw(int x, int y, int z) {
    if (x < 2 || z < 2 || x >= WORLD_X - 2 || z >= WORLD_Z - 2) return;
    const int trunkH = (gWorldConfig.worldType == WORLD_TYPE_LARGE_BIOMES) ? 4 : 3;
    for (int i = 0; i < trunkH && y + i < WORLD_Y; ++i) {
        gWorld[x][y + i][z] = (u8)BLOCK_WOOD;
    }
    int top = y + trunkH - 1;
    for (int dx = -2; dx <= 2; ++dx) {
        for (int dz = -2; dz <= 2; ++dz) {
            for (int dy = -1; dy <= 1; ++dy) {
                if (std::abs(dx) + std::abs(dz) + std::abs(dy) > 4) continue;
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
    bool sandy = isSandyEdge(x, z, h);
    for (int y = 0; y < WORLD_Y; ++y) gWorld[x][y][z] = BLOCK_AIR;

    if (gWorldConfig.worldType == WORLD_TYPE_SUPERFLAT) {
        for (int y = 0; y <= h; ++y) {
            int block = BLOCK_DIRT;
            if (y == 0) block = BLOCK_STONE;
            else if (y == h) block = BLOCK_GRASS;
            gWorld[x][y][z] = (u8)block;
        }
        if (gWorldConfig.generateTrees && (mixSeed(gWorldSeed, x / 3, z / 3, 41) % 71u) == 0u) {
            buildTreeRaw(x, h + 1, z);
        }
        refreshTopVisibleColumn(x, z);
        return;
    }

    int stoneDepth = 3;
    if (gWorldConfig.worldType == WORLD_TYPE_PERLIN) stoneDepth = 2 + (int)(mixSeed(gWorldSeed, x, z, 70) & 1u);
    if (gWorldConfig.worldType == WORLD_TYPE_LARGE_BIOMES) stoneDepth = 4;

    for (int y = 0; y <= h; ++y) {
        int block = BLOCK_DIRT;
        if (y == 0 || y < h - stoneDepth) {
            block = BLOCK_STONE;
        } else if (y < h) {
            block = sandy ? BLOCK_SAND : BLOCK_DIRT;
        } else {
            block = sandy ? BLOCK_SAND : BLOCK_GRASS;
        }
        gWorld[x][y][z] = (u8)block;
    }

    if (h < kSeaLevel) {
        for (int y = h + 1; y <= kSeaLevel; ++y) gWorld[x][y][z] = BLOCK_WATER;
    }

    if (gWorldConfig.generateTrees && !sandy && h >= kSeaLevel + 2) {
        u32 treeRoll = mixSeed(gWorldSeed, x, z, 7);
        int chanceDiv = sizeTreeChanceDiv();
        if (gWorldConfig.worldType == WORLD_TYPE_PERLIN) chanceDiv -= 2;
        if (gWorldConfig.worldType == WORLD_TYPE_LARGE_BIOMES) chanceDiv -= 4;
        if (chanceDiv < 8) chanceDiv = 8;
        if ((treeRoll % (u32)chanceDiv) == 0u) {
            buildTreeRaw(x, h + 1, z);
        }
    }

    refreshTopVisibleColumn(x, z);
}

void beginWorldGeneration(const WorldGenConfig& config) {
    gWorldConfig = config;
    gPendingConfig = config;
    gWorldSeed = config.seed;
    initPerlin(gWorldSeed ^ 0x9E3779B9u);
    std::memset(gWorld, 0, sizeof(gWorld));
    std::memset(gTopVisible, 0, sizeof(gTopVisible));
    std::memset(gTorchLight, 0, sizeof(gTorchLight));
    gWorldTime = 6000;
    gWorldRevision = 1;
    gGenX = 0;
    gGenZ = 0;
    gWorldGenProgress = 0;
    gSpawnX = WORLD_X / 2;
    gSpawnZ = WORLD_Z / 2;
    gSpawnY = 10;
}

void beginWorldGeneration(u32 seed) {
    WorldGenConfig config = gPendingConfig;
    config.seed = seed;
    beginWorldGeneration(config);
}

void initWorld() {
    beginWorldGeneration(gPendingConfig);
    while (!generateWorldStep(WORLD_X * WORLD_Z)) {
    }
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
                if (gWorldConfig.worldType != WORLD_TYPE_SUPERFLAT && top <= kSeaLevel) continue;
                int d = std::abs(x - WORLD_X / 2) + std::abs(z - WORLD_Z / 2) + std::abs(top - 9) * 2;
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
        rebuildTorchLight();
        gWorldGenProgress = 100;
        ++gWorldRevision;
        return true;
    }

    ++gWorldRevision;
    return false;
}

bool exportWorldState(void* dst, int dstSize, int* outUsedSize) {
    const int need = WORLD_X * WORLD_Y * WORLD_Z;
    if (!dst || dstSize < need) return false;
    u8* out = (u8*)dst;
    int idx = 0;
    for (int x = 0; x < WORLD_X; ++x)
        for (int y = 0; y < WORLD_Y; ++y)
            for (int z = 0; z < WORLD_Z; ++z)
                out[idx++] = gWorld[x][y][z];
    if (outUsedSize) *outUsedSize = idx;
    return true;
}

bool importWorldState(const void* src, int srcSize, const WorldGenConfig* config, int spawnX, int spawnY, int spawnZ) {
    const int need = WORLD_X * WORLD_Y * WORLD_Z;
    if (!src || srcSize < need) return false;

    const u8* in = (const u8*)src;
    int idx = 0;
    for (int x = 0; x < WORLD_X; ++x) {
        for (int y = 0; y < WORLD_Y; ++y) {
            for (int z = 0; z < WORLD_Z; ++z) {
                gWorld[x][y][z] = in[idx++];
            }
        }
    }
    for (int z = 0; z < WORLD_Z; ++z) {
        for (int x = 0; x < WORLD_X; ++x) refreshTopVisibleColumn(x, z);
    }
    if (config) {
        gWorldConfig = *config;
        gPendingConfig = *config;
        gWorldSeed = config->seed;
        initPerlin(gWorldSeed ^ 0x9E3779B9u);
    } else {
        gWorldSeed = 0;
        initPerlin(0x9E3779B9u);
    }
    gSpawnX = spawnX;
    gSpawnY = spawnY;
    gSpawnZ = spawnZ;
    rebuildTorchLight();
    ++gWorldRevision;
    gWorldGenProgress = 100;
    return true;
}
