#include "render.h"
#include "texture_data.h"
#include "menu_assets.h"
#include "world.h"
#include "crafting.h"
#include "mob.h"
#include "mob_assets.h"

#include <nds.h>
#include <cmath>
#include <cstdio>
#include <cstring>

static const int SCREEN_W = 256;
static const int SCREEN_H = 192;
static const int MIN_RENDER_W = 52;
static const int MIN_RENDER_H = 40;
static const int MAX_RENDER_W = 129;
static const int MAX_RENDER_H = 96;
static const float EYE_HEIGHT = 1.65f;
static const float FOV = 0.80f;
static const float kRenderDistanceTable[] = {8.0f, 12.25f, 23.5f};
static int gRenderDistanceIndex = 1;
static const int MAX_RAY_STEPS = 16;

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

static const int kBuildingBlocks[] = {
    BLOCK_STONE, BLOCK_COBBLE, BLOCK_BRICK, BLOCK_GLASS, BLOCK_SANDSTONE,
    BLOCK_OBSIDIAN, BLOCK_GRAVEL, BLOCK_PLANKS, BLOCK_BIRCH_PLANKS,
    BLOCK_SPRUCE_PLANKS, BLOCK_JUNGLE_PLANKS, BLOCK_GOLD_BLOCK, BLOCK_IRON_BLOCK, BLOCK_TORCH
};
static const int kNaturalBlocks[] = {
    BLOCK_GRASS, BLOCK_DIRT, BLOCK_SAND, BLOCK_WATER, BLOCK_WOOD,
    BLOCK_BIRCH_WOOD, BLOCK_SPRUCE_WOOD, BLOCK_JUNGLE_WOOD, BLOCK_LEAVES,
    BLOCK_BIRCH_LEAVES, BLOCK_SPRUCE_LEAVES, BLOCK_JUNGLE_LEAVES
};
static const int kDecorBlocks[] = {
    BLOCK_BOOKSHELF, BLOCK_WHITE_WOOL, BLOCK_GLASS, BLOCK_BRICK, BLOCK_SANDSTONE,
    BLOCK_PLANKS, BLOCK_BIRCH_PLANKS, BLOCK_SPRUCE_PLANKS, BLOCK_JUNGLE_PLANKS, BLOCK_TORCH
};
struct HudCategory {
    const char* label;
    const int* entries;
    int count;
    bool placeholderOnly;
};
static const HudCategory kHudCategories[] = {
    {"BUILD", kBuildingBlocks, (int)(sizeof(kBuildingBlocks) / sizeof(kBuildingBlocks[0])), false},
    {"NATURE", kNaturalBlocks, (int)(sizeof(kNaturalBlocks) / sizeof(kNaturalBlocks[0])), false},
    {"DECOR", kDecorBlocks, (int)(sizeof(kDecorBlocks) / sizeof(kDecorBlocks[0])), false},
    {"ITEMS", nullptr, 0, true}
};
static const int kHudCategoryCount = sizeof(kHudCategories) / sizeof(kHudCategories[0]);
static int gHudCategoryIndex = 0;


static bool gMapVisible = false;
static int gTopBg = -1;
static int gBottomBg = -1;
static u16* gTopFb = nullptr;
static u16* gBottomFb = nullptr;
static int gRenderW = 56;
static int gRenderH = 42;
static u16 gLowRes[MAX_RENDER_H][MAX_RENDER_W];
static float gNx[MAX_RENDER_W];
static float gNy[MAX_RENDER_H];
static float gInvLen[MAX_RENDER_H][MAX_RENDER_W];
static int gXMap[SCREEN_W];
static int gYMap[SCREEN_H];
static u16 gSkyColor[MAX_RENDER_H];
static u16 gGroundColor[MAX_RENDER_H];
static u16 gHudStatic[192][256];
static bool gHudStaticValid = false;
static int gHudRevision = -1;
static u16 gHudFrame[192][256];
static bool gHudFrameValid = false;
static int gHudFrameCounter = 0;
static int gFrameCounter = 0;
static int gLastWorldRevision3D = -1;
static float gLastYaw = 9999.0f;
static float gLastPitch = 9999.0f;
static float gLastX = 9999.0f;
static float gLastY = 9999.0f;
static float gLastZ = 9999.0f;
static int gLastHudHealth = -9999;
static bool gLastHudAlive = false;
static int gLastHudSelectedBlock = -9999;
static int gLastHudSelectedSlot = -9999;
static int gLastHudBreakTicks = -9999;
static int gLastHudBreakNeeded = -9999;
static int gLastHudMode = -9999;
static bool gLastHudMapVisible = false;
static int gLastHudPlayerMapX = -9999;
static int gLastHudPlayerMapZ = -9999;
static int gLastHudCategoryIndex = -9999;
static int gLastHudSelectedItemId = -9999;
static int gLastHudSelectedItemCount = -9999;

static u16 gMenuTopBase[192][256];
static u16 gMenuBottomBase[192][256];

enum MenuCacheScreen {
    MENU_CACHE_NONE = 0,
    MENU_CACHE_TITLE,
    MENU_CACHE_OPTIONS,
    MENU_CACHE_GRAPHICS,
    MENU_CACHE_WORLD_SETUP,
    MENU_CACHE_LOADING,
    MENU_CACHE_PAUSE
};

static MenuCacheScreen gMenuCacheScreen = MENU_CACHE_NONE;
static int gLastMenuLogoX = -1;
static int gLastMenuLogoY = -1;
static int gLastMenuLogoW = 0;
static int gLastMenuLogoH = 0;
static int gLastLoadingProgress = -1;
static int gLastGraphicsSlider = -1;
static bool gMobColorCacheValid = false;
static u16 gPigHeadFront = 0;
static u16 gPigHeadSide = 0;
static u16 gPigHeadTop = 0;
static u16 gPigSnoutFront = 0;
static u16 gPigSnoutSide = 0;
static u16 gPigSnoutTop = 0;
static u16 gPigBodyFront = 0;
static u16 gPigBodySide = 0;
static u16 gPigBodyTop = 0;
static u16 gPigLegFront = 0;
static u16 gPigLegSide = 0;
static u16 gPigLegTop = 0;

static inline u16 rgb15(int r, int g, int b);
static void drawMobBillboards(const Player& p, float eyeY, float forwardX, float forwardY, float forwardZ, float rightX, float rightY, float rightZ, float upX, float upY, float upZ);

static const char* renderDistanceName() {
    static const char* kNames[] = {"SHORT", "NORMAL", "FAR"};
    return kNames[gRenderDistanceIndex];
}

static const char* speedName(float value) {
    if (value < 0.85f) return "LOW";
    if (value > 1.15f) return "HIGH";
    return "MED";
}
static const char* blockName(int block) {
    switch (block) {
        case BLOCK_GRASS: return "GRASS";
        case BLOCK_DIRT: return "DIRT";
        case BLOCK_STONE: return "STONE";
        case BLOCK_WOOD: return "OAK LOG";
        case BLOCK_LEAVES: return "OAK LEAF";
        case BLOCK_SAND: return "SAND";
        case BLOCK_WATER: return "WATER";
        case BLOCK_COBBLE: return "COBBLE";
        case BLOCK_PLANKS: return "OAK PLNK";
        case BLOCK_BRICK: return "BRICK";
        case BLOCK_GLASS: return "GLASS";
        case BLOCK_BIRCH_WOOD: return "BIRCHLOG";
        case BLOCK_SPRUCE_WOOD: return "SPRUCE";
        case BLOCK_JUNGLE_WOOD: return "JUNGLE";
        case BLOCK_BIRCH_LEAVES: return "BIRCHLF";
        case BLOCK_SPRUCE_LEAVES: return "SPRUCLF";
        case BLOCK_JUNGLE_LEAVES: return "JUNGLF";
        case BLOCK_BIRCH_PLANKS: return "BIRCHPL";
        case BLOCK_SPRUCE_PLANKS: return "SPRUCPL";
        case BLOCK_JUNGLE_PLANKS: return "JUNGLPL";
        case BLOCK_SANDSTONE: return "SANDSTN";
        case BLOCK_OBSIDIAN: return "OBSIDIA";
        case BLOCK_GRAVEL: return "GRAVEL";
        case BLOCK_BOOKSHELF: return "SHELF";
        case BLOCK_WHITE_WOOL: return "WOOL";
        case BLOCK_GOLD_BLOCK: return "GOLD";
        case BLOCK_IRON_BLOCK: return "IRON";
        case BLOCK_TORCH: return "TORCH";
        case BLOCK_CRAFTING_TABLE: return "CRAFTTAB";
        case BLOCK_FURNACE: return "FURNACE";
        case BLOCK_COAL_ORE: return "COALORE";
        default: return "BLOCK";
    }
}

static int graphicsSliderValue() {
    const int rangeW = MAX_RENDER_W - MIN_RENDER_W;
    if (rangeW <= 0) return 0;
    int value = ((gRenderW - MIN_RENDER_W) * 100 + rangeW / 2) / rangeW;
    if (value < 0) value = 0;
    if (value > 100) value = 100;
    return value;
}

static void rebuildProjectionTables() {
    const float aspect = (float)gRenderW / (float)gRenderH;
    for (int x = 0; x < gRenderW; ++x) {
        float nx = ((x + 0.5f) / (float)gRenderW) * 2.0f - 1.0f;
        gNx[x] = nx * aspect * FOV;
    }
    for (int y = 0; y < gRenderH; ++y) {
        float ny = ((y + 0.5f) / (float)gRenderH) * 2.0f - 1.0f;
        gNy[y] = ny * FOV;
    }
    for (int y = 0; y < gRenderH; ++y) {
        const float ny = gNy[y];
        for (int x = 0; x < gRenderW; ++x) {
            const float nx = gNx[x];
            gInvLen[y][x] = 1.0f / sqrtf(nx * nx + ny * ny + 1.0f);
        }
    }
    for (int x = 0; x < SCREEN_W; ++x) gXMap[x] = (x * gRenderW) / SCREEN_W;
    for (int y = 0; y < SCREEN_H; ++y) gYMap[y] = (y * gRenderH) / SCREEN_H;

    for (int py = 0; py < gRenderH; ++py) {
        if (py < gRenderH / 2) {
            int band = 10 + py / 4;
            gSkyColor[py] = rgb15(4 + band / 4, 10 + band / 3, 18 + band / 2);
        } else {
            int band = py - gRenderH / 2;
            gGroundColor[py] = rgb15(4 + band / 4, 8 + band / 5, 4 + band / 6);
        }
    }
}

static inline u16 rgb15(int r, int g, int b) {
    return RGB15(r, g, b) | BIT(15);
}

static inline int clamp5(int v) {
    return v < 0 ? 0 : (v > 31 ? 31 : v);
}

static inline u16 shadeColor(u16 color, int face, bool highlighted, int fog, int lightLevel = 31, bool emissive = false) {
    const int baseR = color & 31;
    const int baseG = (color >> 5) & 31;
    const int baseB = (color >> 10) & 31;
    int r = baseR;
    int g = baseG;
    int b = baseB;

    if (face == 1) {
        r = (r * 13) / 10;
        g = (g * 13) / 10;
        b = (b * 13) / 10;
    } else if (face >= 2) {
        r = (r * 8) / 10;
        g = (g * 8) / 10;
        b = (b * 8) / 10;
    }

    if (emissive) {
        lightLevel = 31;
    }
    if (lightLevel < 7) lightLevel = 7;
    if (lightLevel > 31) lightLevel = 31;
    r = (r * lightLevel) / 31;
    g = (g * lightLevel) / 31;
    b = (b * lightLevel) / 31;

    if (highlighted) {
        r += 5;
        g += 5;
        b += 5;
    }

    r = clamp5(r - fog);
    g = clamp5(g - fog);
    b = clamp5(b - fog);

    if ((baseR | baseG | baseB) != 0) {
        int minAmbient = 1;
        if (lightLevel >= 10) minAmbient = 2;
        if (face == 1 && minAmbient < 2) minAmbient = 2;
        if (r < minAmbient) r = minAmbient;
        if (g < minAmbient) g = minAmbient;
        if (b < minAmbient) b = minAmbient;
    }
    return rgb15(r, g, b);
}

static inline bool isGlassLikeBlock(int block) {
    return block == BLOCK_GLASS;
}

static inline bool shouldRenderFaceAgainstNeighbor(int block, int neighbor) {
    if (block == BLOCK_AIR) return false;
    if (block == BLOCK_WATER) return neighbor == BLOCK_AIR || neighbor == BLOCK_TORCH;
    if (isGlassLikeBlock(block)) {
        return neighbor == BLOCK_AIR || neighbor == BLOCK_WATER || neighbor == BLOCK_TORCH;
    }
    if (neighbor == BLOCK_AIR || neighbor == BLOCK_WATER || neighbor == BLOCK_TORCH) return true;
    if (isGlassLikeBlock(neighbor)) return true;
    return false;
}

static const u16* textureForBlockFace(int block, int face, bool topFace) {
    (void)face;
    switch (block) {
        case BLOCK_GRASS: return TEX_GRASS_TOP;
        case BLOCK_DIRT: return TEX_DIRT;
        case BLOCK_STONE: return TEX_STONE;
        case BLOCK_WOOD: return topFace ? TEX_WOOD_TOP : TEX_WOOD_SIDE;
        case BLOCK_LEAVES: return TEX_LEAVES;
        case BLOCK_SAND: return TEX_SAND;
        case BLOCK_WATER: return TEX_WATER;
        case BLOCK_COBBLE: return TEX_COBBLE;
        case BLOCK_PLANKS: return TEX_PLANKS;
        case BLOCK_BRICK: return TEX_BRICK;
        case BLOCK_GLASS: return TEX_GLASS;
        case BLOCK_BIRCH_WOOD: return topFace ? TEX_BIRCH_WOOD_TOP : TEX_BIRCH_WOOD_SIDE;
        case BLOCK_SPRUCE_WOOD: return topFace ? TEX_SPRUCE_WOOD_TOP : TEX_SPRUCE_WOOD_SIDE;
        case BLOCK_JUNGLE_WOOD: return topFace ? TEX_JUNGLE_WOOD_TOP : TEX_JUNGLE_WOOD_SIDE;
        case BLOCK_BIRCH_LEAVES: return TEX_BIRCH_LEAVES;
        case BLOCK_SPRUCE_LEAVES: return TEX_SPRUCE_LEAVES;
        case BLOCK_JUNGLE_LEAVES: return TEX_JUNGLE_LEAVES;
        case BLOCK_BIRCH_PLANKS: return TEX_BIRCH_PLANKS;
        case BLOCK_SPRUCE_PLANKS: return TEX_SPRUCE_PLANKS;
        case BLOCK_JUNGLE_PLANKS: return TEX_JUNGLE_PLANKS;
        case BLOCK_SANDSTONE: return TEX_SANDSTONE;
        case BLOCK_OBSIDIAN: return TEX_OBSIDIAN;
        case BLOCK_GRAVEL: return TEX_GRAVEL;
        case BLOCK_BOOKSHELF: return TEX_BOOKSHELF;
        case BLOCK_WHITE_WOOL: return TEX_WHITE_WOOL;
        case BLOCK_GOLD_BLOCK: return TEX_GOLD_BLOCK;
        case BLOCK_IRON_BLOCK: return TEX_IRON_BLOCK;
        case BLOCK_TORCH: return TEX_TORCH;
        case BLOCK_CRAFTING_TABLE:
            if (topFace) return TEX_CRAFTING_TABLE_TOP;
            return face == 0 ? TEX_CRAFTING_TABLE_FRONT : TEX_CRAFTING_TABLE_SIDE;
        case BLOCK_FURNACE:
            if (topFace) return TEX_FURNACE_TOP;
            return face == 0 ? TEX_FURNACE_FRONT : TEX_FURNACE_SIDE;
        case BLOCK_COAL_ORE: return TEX_COAL_ORE;
        default: return TEX_STONE;
    }
}

static inline u16 sampleTexture(const u16* tex, float u, float v) {
    int tx = ((int)(u * TEX_SIZE)) & (TEX_SIZE - 1);
    int ty = ((int)(v * TEX_SIZE)) & (TEX_SIZE - 1);
    return tex[ty * TEX_SIZE + tx];
}

static inline u16 blendColor50(u16 a, u16 b) {
    int ar = a & 31;
    int ag = (a >> 5) & 31;
    int ab = (a >> 10) & 31;
    int br = b & 31;
    int bg = (b >> 5) & 31;
    int bb = (b >> 10) & 31;
    return rgb15((ar + br) >> 1, (ag + bg) >> 1, (ab + bb) >> 1);
}

static inline u16 sampleHitTexture(const Player& p, const RayHit& hit, float dirX, float dirY, float dirZ, float eyeY) {
    const float hx = p.x + dirX * hit.dist;
    const float hy = eyeY + dirY * hit.dist;
    const float hz = p.z + dirZ * hit.dist;
    float u = 0.0f;
    float v = 0.0f;
    bool topFace = false;
    if (hit.face == 0) {
        u = hz - floorf(hz);
        v = hy - floorf(hy);
    } else if (hit.face == 1) {
        u = hx - floorf(hx);
        v = hz - floorf(hz);
        topFace = (dirY < 0.0f);
    } else {
        u = hx - floorf(hx);
        v = hy - floorf(hy);
    }
    const u16* tex = textureForBlockFace(hit.block, hit.face, topFace);
    u16 texel = sampleTexture(tex, u, v);
    if (!texel && hit.block == BLOCK_GLASS) texel = rgb15(22, 26, 28);
    return texel;
}

static inline u16 shadeHitTexture(const Player& p, const RayHit& hit, float dirX, float dirY, float dirZ, float eyeY, bool highlighted) {
    const int light = getCombinedLightLevel(hit.x, hit.y, hit.z);
    return shadeColor(sampleHitTexture(p, hit, dirX, dirY, dirZ, eyeY), hit.face, highlighted, (int)(hit.dist * 2.0f), light, isLightEmitterBlock(hit.block));
}

static void drawRect(u16* fb, int x, int y, int w, int h, u16 color) {
    if (w <= 0 || h <= 0) return;
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > 256) w = 256 - x;
    if (y + h > 192) h = 192 - y;
    if (w <= 0 || h <= 0) return;
    for (int yy = y; yy < y + h; ++yy) {
        u16* row = fb + yy * 256;
        for (int xx = x; xx < x + w; ++xx) row[xx] = color;
    }
}

static void drawTextPixel(u16* fb, int x, int y, const char* text, u16 color) {
    static const unsigned char glyphs[][5] = {
        {0,0,0,0,0},          // space
        {31,17,17,17,17},     // A
        {31,21,21,10,0},      // B
        {14,17,17,17,0},      // C
        {31,17,17,14,0},      // D
        {31,21,21,17,0},      // E
        {31,20,20,16,0},      // F
        {14,17,19,15,0},      // G
        {31,4,4,31,0},        // H
        {17,31,17,0,0},       // I
        {1,1,17,30,0},        // J
        {31,4,10,17,0},       // K
        {31,1,1,1,0},         // L
        {31,12,3,12,31},      // M
        {31,8,4,2,31},        // N
        {14,17,17,14,0},      // O
        {31,20,20,8,0},       // P
        {14,17,19,15,1},      // Q
        {31,20,22,9,0},       // R
        {9,21,21,18,0},       // S
        {16,31,16,0,0},       // T
        {30,1,1,30,0},        // U
        {28,2,1,2,28},        // V
        {30,1,6,1,30},        // W
        {17,10,4,10,17},      // X
        {24,4,3,4,24},        // Y
        {19,21,25,17,0},      // Z
        {14,19,21,25,14},     // 0
        {0,17,31,1,0},        // 1
        {19,21,21,21,9},      // 2
        {17,21,21,21,10},     // 3
        {28,4,4,31,4},        // 4
        {29,21,21,21,18},     // 5
        {14,21,21,21,2},      // 6
        {16,23,24,20,16},     // 7
        {10,21,21,21,10},     // 8
        {8,21,21,21,14}       // 9
    };

    auto glyphIndex = [](char c) -> int {
        if (c == ' ') return 0;
        if (c >= 'A' && c <= 'Z') return 1 + (c - 'A');
        if (c >= '0' && c <= '9') return 27 + (c - '0');
        return 0;
    };

    for (int i = 0; text[i]; ++i) {
        int gi = glyphIndex(text[i]);
        for (int col = 0; col < 5; ++col) {
            unsigned char bits = glyphs[gi][col];
            for (int row = 0; row < 5; ++row) {
                if (bits & (1 << (4 - row))) {
                    drawRect(fb, x + i * 6 + col, y + row, 1, 1, color);
                }
            }
        }
    }
}

static void drawTextPixelScaled(u16* fb, int x, int y, const char* text, u16 color, int scale) {
    if (scale <= 1) {
        drawTextPixel(fb, x, y, text, color);
        return;
    }

    static const unsigned char glyphs[][5] = {
        {0,0,0,0,0}, {31,17,17,17,17}, {31,21,21,10,0}, {14,17,17,17,0}, {31,17,17,14,0},
        {31,21,21,17,0}, {31,20,20,16,0}, {14,17,19,15,0}, {31,4,4,31,0}, {17,31,17,0,0},
        {1,1,17,30,0}, {31,4,10,17,0}, {31,1,1,1,0}, {31,12,3,12,31}, {31,8,4,2,31},
        {14,17,17,14,0}, {31,20,20,8,0}, {14,17,19,15,1}, {31,20,22,9,0}, {9,21,21,18,0},
        {16,31,16,0,0}, {30,1,1,30,0}, {28,2,1,2,28}, {30,1,6,1,30}, {17,10,4,10,17},
        {24,4,3,4,24}, {19,21,25,17,0}, {14,19,21,25,14}, {0,17,31,1,0}, {19,21,21,21,9},
        {17,21,21,21,10}, {28,4,4,31,4}, {29,21,21,21,18}, {14,21,21,21,2}, {16,23,24,20,16},
        {10,21,21,21,10}, {8,21,21,21,14}
    };

    auto glyphIndex = [](char c) -> int {
        if (c == ' ') return 0;
        if (c >= 'A' && c <= 'Z') return 1 + (c - 'A');
        if (c >= '0' && c <= '9') return 27 + (c - '0');
        return 0;
    };

    for (int i = 0; text[i]; ++i) {
        int gi = glyphIndex(text[i]);
        for (int col = 0; col < 5; ++col) {
            unsigned char bits = glyphs[gi][col];
            for (int row = 0; row < 5; ++row) {
                if (bits & (1 << (4 - row))) {
                    drawRect(fb, x + i * 6 * scale + col * scale, y + row * scale, scale, scale, color);
                }
            }
        }
    }
}


struct Vec3f {
    float x;
    float y;
    float z;
};

struct ScreenPt {
    int x;
    int y;
};

static inline ScreenPt projectModelVertex(const Vec3f& v, float yaw, float pitch, float scale, int ox, int oy) {
    const float cy = std::cosf(yaw);
    const float sy = std::sinf(yaw);
    const float cp = std::cosf(pitch);
    const float sp = std::sinf(pitch);

    float rx = v.x * cy + v.z * sy;
    float rz = -v.x * sy + v.z * cy;
    float ry = v.y;

    float ry2 = ry * cp - rz * sp;
    float rz2 = ry * sp + rz * cp + 5.2f;
    if (rz2 < 0.2f) rz2 = 0.2f;

    const float inv = scale / rz2;
    ScreenPt out;
    out.x = ox + (int)(rx * inv);
    out.y = oy - (int)(ry2 * inv);
    return out;
}

static void fillTriangle(u16* fb, ScreenPt a, ScreenPt b, ScreenPt c, u16 color) {
    int minX = a.x;
    int maxX = a.x;
    int minY = a.y;
    int maxY = a.y;
    if (b.x < minX) minX = b.x;
    if (c.x < minX) minX = c.x;
    if (b.x > maxX) maxX = b.x;
    if (c.x > maxX) maxX = c.x;
    if (b.y < minY) minY = b.y;
    if (c.y < minY) minY = c.y;
    if (b.y > maxY) maxY = b.y;
    if (c.y > maxY) maxY = c.y;

    if (maxX < 0 || maxY < 0 || minX >= SCREEN_W || minY >= SCREEN_H) return;
    if (minX < 0) minX = 0;
    if (minY < 0) minY = 0;
    if (maxX >= SCREEN_W) maxX = SCREEN_W - 1;
    if (maxY >= SCREEN_H) maxY = SCREEN_H - 1;

    const int area = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (area == 0) return;

    for (int y = minY; y <= maxY; ++y) {
        u16* row = fb + y * 256;
        for (int x = minX; x <= maxX; ++x) {
            const int w0 = (b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x);
            const int w1 = (c.x - b.x) * (y - b.y) - (c.y - b.y) * (x - b.x);
            const int w2 = (a.x - c.x) * (y - c.y) - (a.y - c.y) * (x - c.x);
            if ((area > 0 && w0 >= 0 && w1 >= 0 && w2 >= 0) || (area < 0 && w0 <= 0 && w1 <= 0 && w2 <= 0)) {
                row[x] = color;
            }
        }
    }
}

static void fillQuad(u16* fb, ScreenPt a, ScreenPt b, ScreenPt c, ScreenPt d, u16 color) {
    fillTriangle(fb, a, b, c, color);
    fillTriangle(fb, a, c, d, color);
}

static u16 tintColor(u16 color, int mulNum, int mulDen, int add) {
    int r = ((color & 31) * mulNum) / mulDen + add;
    int g = ((((color >> 5) & 31) * mulNum) / mulDen) + add;
    int b = ((((color >> 10) & 31) * mulNum) / mulDen) + add;
    return rgb15(clamp5(r), clamp5(g), clamp5(b));
}

static u16 centerTexel(const u16* tex) {
    return tex[(TEX_SIZE / 2) * TEX_SIZE + (TEX_SIZE / 2)];
}

static void drawHeldHand(u16* fb) {
    const Vec3f verts[8] = {
        {-0.75f, -0.55f, -0.55f}, {0.75f, -0.55f, -0.55f}, {0.75f, 0.55f, -0.55f}, {-0.75f, 0.55f, -0.55f},
        {-0.75f, -0.55f, 0.55f},  {0.75f, -0.55f, 0.55f},  {0.75f, 0.55f, 0.55f},  {-0.75f, 0.55f, 0.55f}
    };
    ScreenPt p[8];
    for (int i = 0; i < 8; ++i) p[i] = projectModelVertex(verts[i], -0.65f, -0.92f, 84.0f, 202, 170);

    const u16 front = rgb15(24, 13, 11);
    const u16 side = rgb15(20, 10, 8);
    const u16 top = rgb15(28, 16, 14);
    fillQuad(fb, p[0], p[1], p[2], p[3], front);
    fillQuad(fb, p[1], p[5], p[6], p[2], side);
    fillQuad(fb, p[3], p[2], p[6], p[7], top);
}

static const u16* textureForItem(int item) {
    switch (item) {
        case ITEM_APPLE: return TEX_ITEM_APPLE;
        case ITEM_STICK: return TEX_ITEM_STICK;
        case ITEM_COAL: return TEX_ITEM_COAL;
        case ITEM_WOOD_PICKAXE: return TEX_ITEM_WOOD_PICKAXE;
        case ITEM_STONE_PICKAXE: return TEX_ITEM_STONE_PICKAXE;
        case ITEM_WOOD_AXE: return TEX_ITEM_WOOD_AXE;
        case ITEM_STONE_AXE: return TEX_ITEM_STONE_AXE;
        case ITEM_WOOD_SHOVEL: return TEX_ITEM_WOOD_SHOVEL;
        case ITEM_STONE_SHOVEL: return TEX_ITEM_STONE_SHOVEL;
        case ITEM_WOOD_SWORD: return TEX_ITEM_WOOD_SWORD;
        case ITEM_STONE_SWORD: return TEX_ITEM_STONE_SWORD;
        default: return nullptr;
    }
}

static const char* itemName(int item) {
    switch (item) {
        case ITEM_APPLE: return "APPLE";
        case ITEM_STICK: return "STICK";
        case ITEM_COAL: return "COAL";
        case ITEM_WOOD_PICKAXE: return "WOODPICK";
        case ITEM_STONE_PICKAXE: return "STONEPK";
        case ITEM_WOOD_AXE: return "WOODAXE";
        case ITEM_STONE_AXE: return "STONEAXE";
        case ITEM_WOOD_SHOVEL: return "WOODSHVL";
        case ITEM_STONE_SHOVEL: return "STONESHV";
        case ITEM_WOOD_SWORD: return "WOODSWRD";
        case ITEM_STONE_SWORD: return "STONESWD";
        default: return "ITEM";
    }
}

static void drawHeldBlockModel(u16* fb, int block) {
    const Vec3f verts[8] = {
        {-1.0f, -1.0f, -1.0f}, {1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, -1.0f}, {-1.0f, 1.0f, -1.0f},
        {-1.0f, -1.0f, 1.0f},  {1.0f, -1.0f, 1.0f},  {1.0f, 1.0f, 1.0f},  {-1.0f, 1.0f, 1.0f}
    };
    ScreenPt p[8];
    for (int i = 0; i < 8; ++i) p[i] = projectModelVertex(verts[i], -0.58f, -0.62f, 68.0f, 186, 148);

    const u16 front = centerTexel(textureForBlockFace(block, 0, false));
    const u16 top = centerTexel(textureForBlockFace(block, 1, true));
    const u16 side = centerTexel(textureForBlockFace(block, 2, false));
    fillQuad(fb, p[0], p[1], p[2], p[3], tintColor(front, 1, 1, 0));
    fillQuad(fb, p[1], p[5], p[6], p[2], tintColor(side, 4, 5, -1));
    fillQuad(fb, p[3], p[2], p[6], p[7], tintColor(top, 6, 5, 1));
}

static void drawIconTexture(u16* fb, int x, int y, const u16* tex, bool selected) {
    drawRect(fb, x, y, 28, 28, rgb15(8, 8, 10));
    drawRect(fb, x + 2, y + 2, 24, 24, rgb15(2, 2, 3));
    if (selected) {
        drawRect(fb, x - 2, y - 2, 32, 2, rgb15(31, 31, 31));
        drawRect(fb, x - 2, y + 28, 32, 2, rgb15(31, 31, 31));
        drawRect(fb, x - 2, y - 2, 2, 32, rgb15(31, 31, 31));
        drawRect(fb, x + 28, y - 2, 2, 32, rgb15(31, 31, 31));
    }
    if (!tex) return;
    for (int py = 0; py < 16; ++py) {
        u16* row = fb + (y + 6 + py) * 256 + (x + 6);
        int ty = (py * TEX_SIZE) >> 4;
        for (int px = 0; px < 16; ++px) {
            int tx = (px * TEX_SIZE) >> 4;
            u16 c = tex[ty * TEX_SIZE + tx];
            if (c) row[px] = c;
        }
    }
}

static void drawBlockIcon(u16* fb, int x, int y, int block, bool selected) {
    drawIconTexture(fb, x, y, textureForBlockFace(block, 1, true), selected);
}

static void drawItemIcon(u16* fb, int x, int y, int item, bool selected) {
    drawIconTexture(fb, x, y, textureForItem(item), selected);
}

static void drawMapPanel(u16* fb, int playerX, int playerZ) {
    const int panelX = 18;
    const int panelY = 16;
    const int panelW = 148;
    const int panelH = 148;
    const int mapX0 = panelX + 10;
    const int mapY0 = panelY + 10;
    const int cell = 4;
    const int oceanInset = 6;

    drawRect(fb, panelX, panelY, panelW, panelH, rgb15(5, 6, 8));
    drawRect(fb, panelX + 2, panelY + 2, panelW - 4, panelH - 4, rgb15(1, 2, 3));
    drawRect(fb, mapX0 - oceanInset, mapY0 - oceanInset, WORLD_X * cell + oceanInset * 2, WORLD_Z * cell + oceanInset * 2, rgb15(4, 10, 20));
    drawTextPixel(fb, panelX + 10, panelY + 136, "MAP", rgb15(31, 31, 31));

    for (int z = 0; z < WORLD_Z; ++z) {
        for (int x = 0; x < WORLD_X; ++x) {
            int topBlock = getTopVisibleBlock(x, z);
            u16 c = rgb15(1, 1, 1);
            if (topBlock != BLOCK_AIR) {
                c = shadeColor(textureForBlockFace(topBlock, 1, true)[0], 1, false, 0, getCombinedLightLevel(x, 0, z), isLightEmitterBlock(topBlock));
            }
            drawRect(fb, mapX0 + x * cell, mapY0 + z * cell, cell - 1, cell - 1, c);
        }
    }

    drawRect(fb, mapX0 - 1, mapY0 - 1, WORLD_X * cell + 2, 1, rgb15(28, 28, 30));
    drawRect(fb, mapX0 - 1, mapY0 + WORLD_Z * cell, WORLD_X * cell + 2, 1, rgb15(28, 28, 30));
    drawRect(fb, mapX0 - 1, mapY0 - 1, 1, WORLD_Z * cell + 2, rgb15(28, 28, 30));
    drawRect(fb, mapX0 + WORLD_X * cell, mapY0 - 1, 1, WORLD_Z * cell + 2, rgb15(28, 28, 30));

    drawRect(fb, mapX0 + playerX * cell - 1, mapY0 + playerZ * cell - 1, 3, 3, rgb15(31, 0, 0));
}

static void drawHudTab(u16* fb, int x, int y, int w, const char* label, bool active) {
    drawRect(fb, x, y, w, 18, active ? rgb15(23, 23, 24) : rgb15(11, 11, 13));
    drawRect(fb, x + 2, y + 2, w - 4, 14, active ? rgb15(29, 29, 31) : rgb15(6, 6, 8));
    drawTextPixel(fb, x + 6, y + 6, label, active ? rgb15(3, 3, 4) : rgb15(27, 27, 28));
}

static void rebuildCreativeHudStatic() {
    u16* fb = &gHudStatic[0][0];
    for (int y = 0; y < 192; ++y) {
        u16* row = fb + y * 256;
        for (int x = 0; x < 256; ++x) row[x] = rgb15(5, 5, 7);
    }

    drawRect(fb, 8, 8, 240, 176, rgb15(8, 8, 10));
    drawRect(fb, 12, 12, 232, 168, rgb15(12, 12, 14));

    const int tabW = 54;
    for (int i = 0; i < kHudCategoryCount; ++i) {
        drawHudTab(fb, 12 + i * 58, 12, tabW, kHudCategories[i].label, i == gHudCategoryIndex);
    }

    drawRect(fb, 16, 36, 216, 110, rgb15(9, 9, 11));
    drawRect(fb, 18, 38, 212, 106, rgb15(18, 18, 19));
    drawTextPixel(fb, 24, 42, kHudCategories[gHudCategoryIndex].placeholderOnly ? "ITEMS COMING SOON" : "SELECT A BLOCK", rgb15(30, 30, 30));

    drawRect(fb, 236, 36, 8, 110, rgb15(10, 10, 11));
    drawRect(fb, 237, 42, 6, 28, rgb15(25, 25, 26));

    drawRect(fb, 16, 152, 152, 24, rgb15(9, 9, 11));
    drawRect(fb, 18, 154, 148, 20, rgb15(16, 16, 18));
    drawTextPixel(fb, 26, 161, "SELECT BACK  L BREAK  R PLACE", rgb15(28, 28, 29));

    drawRect(fb, 176, 152, 56, 24, rgb15(9, 9, 11));
    drawRect(fb, 178, 154, 52, 20, rgb15(16, 16, 18));
    drawTextPixel(fb, 191, 161, "MAP", rgb15(28, 28, 29));
}

static void rebuildSurvivalHudStatic() {
    u16* fb = &gHudStatic[0][0];
    for (int y = 0; y < 192; ++y) {
        u16* row = fb + y * 256;
        for (int x = 0; x < 256; ++x) row[x] = rgb15(4, 4, 6);
    }
    drawRect(fb, 8, 8, 240, 176, rgb15(8, 8, 10));
    drawRect(fb, 12, 12, 232, 168, rgb15(12, 12, 14));
    drawTextPixelScaled(fb, 20, 16, "SURVIVAL", rgb15(31, 31, 31), 2);
    drawTextPixel(fb, 182, 18, "TAP HOTBAR", rgb15(24, 24, 26));
    drawTextPixel(fb, 184, 28, "SELECT=NEXT", rgb15(24, 24, 26));
    drawRect(fb, 16, 52, 224, 78, rgb15(9, 9, 11));
    drawRect(fb, 18, 54, 220, 74, rgb15(18, 18, 19));
    drawRect(fb, 16, 136, 224, 40, rgb15(9, 9, 11));
    drawRect(fb, 18, 138, 220, 36, rgb15(16, 16, 18));
    drawTextPixel(fb, 24, 144, "L HOLD BREAK  R USE/PLACE", rgb15(28, 28, 29));
    drawTextPixel(fb, 24, 156, "A RESPAWN WHEN DEAD", rgb15(28, 28, 29));
}

static void rebuildHudStatic() {
    if (isCreativeMode()) rebuildCreativeHudStatic();
    else rebuildSurvivalHudStatic();
    gHudRevision = getWorldRevision();
    gHudStaticValid = true;
    gHudFrameValid = false;
}

void initRenderer() {
    powerOn(POWER_ALL_2D);

    videoSetMode(MODE_5_2D | DISPLAY_BG3_ACTIVE);
    videoSetModeSub(MODE_5_2D | DISPLAY_BG3_ACTIVE);
    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankC(VRAM_C_SUB_BG);

    gTopBg = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    gBottomBg = bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    gTopFb = (u16*)bgGetGfxPtr(gTopBg);
    gBottomFb = (u16*)bgGetGfxPtr(gBottomBg);

    std::memset(gTopFb, 0, 256 * 256 * sizeof(u16));
    std::memset(gBottomFb, 0, 256 * 256 * sizeof(u16));

    rebuildProjectionTables();

    gMapVisible = false;
    gHudStaticValid = false;
    gHudFrameValid = false;
    gHudRevision = -1;
    gFrameCounter = 0;
    gHudFrameCounter = 0;
    gLastWorldRevision3D = -1;
    gMenuCacheScreen = MENU_CACHE_NONE;
    gLastMenuLogoX = -1;
    gLastMenuLogoY = -1;
    gLastMenuLogoW = 0;
    gLastMenuLogoH = 0;
    gLastLoadingProgress = -1;
    gLastGraphicsSlider = -1;
    gMobColorCacheValid = false;
}


void invalidateMenuCache() {
    gMenuCacheScreen = MENU_CACHE_NONE;
    gLastMenuLogoX = -1;
    gLastMenuLogoY = -1;
    gLastMenuLogoW = 0;
    gLastMenuLogoH = 0;
    gLastLoadingProgress = -1;
    gLastGraphicsSlider = -1;
    gMobColorCacheValid = false;
}

void prepareGameplayTransition() {
    invalidateMenuCache();
    std::memset(gTopFb, 0, 256 * 256 * sizeof(u16));
    std::memset(gBottomFb, 0, 256 * 256 * sizeof(u16));
    gHudStaticValid = false;
    gHudFrameValid = false;
    gHudRevision = -1;
    gMapVisible = false;
    gFrameCounter = 0;
    gHudFrameCounter = 0;
    gLastWorldRevision3D = -1;
    gLastYaw = 9999.0f;
    gLastPitch = 9999.0f;
    gLastX = 9999.0f;
    gLastY = 9999.0f;
    gLastZ = 9999.0f;
    gLastHudHealth = -9999;
    gLastHudAlive = false;
    gLastHudSelectedBlock = -9999;
    gLastHudSelectedSlot = -9999;
    gLastHudBreakTicks = -9999;
    gLastHudBreakNeeded = -9999;
    gLastHudMode = -9999;
    gLastHudMapVisible = false;
    gLastHudPlayerMapX = -9999;
    gLastHudPlayerMapZ = -9999;
    gLastHudCategoryIndex = -9999;
    gLastHudSelectedItemId = -9999;
    gLastHudSelectedItemCount = -9999;
}

void flushTransitionGhosting() {
    static const u16 black = RGB15(0, 0, 0) | BIT(15);
    static const u16 white = RGB15(31, 31, 31) | BIT(15);

    for (int pass = 0; pass < 2; ++pass) {
        for (int i = 0; i < 256 * 256; ++i) {
            gTopFb[i] = black;
            gBottomFb[i] = black;
        }
        swiWaitForVBlank();
        for (int i = 0; i < 256 * 256; ++i) {
            gTopFb[i] = white;
            gBottomFb[i] = white;
        }
        swiWaitForVBlank();
    }

    for (int i = 0; i < 256 * 256; ++i) {
        gTopFb[i] = black;
        gBottomFb[i] = black;
    }
    swiWaitForVBlank();
}

int getRenderWidth() { return gRenderW; }
int getRenderHeight() { return gRenderH; }
float getRenderDistance() { return kRenderDistanceTable[gRenderDistanceIndex]; }

void setRenderResolution(int width, int height) {
    if (width < MIN_RENDER_W) width = MIN_RENDER_W;
    if (width > MAX_RENDER_W) width = MAX_RENDER_W;
    if (height < MIN_RENDER_H) height = MIN_RENDER_H;
    if (height > MAX_RENDER_H) height = MAX_RENDER_H;

    if (width == gRenderW && height == gRenderH) return;

    gRenderW = width;
    gRenderH = height;
    rebuildProjectionTables();

    gLastWorldRevision3D = -1;
    gLastYaw = 9999.0f;
    gLastPitch = 9999.0f;
    gLastX = 9999.0f;
    gLastY = 9999.0f;
    gLastZ = 9999.0f;
    gHudFrameValid = false;
    gLastGraphicsSlider = -1;
    gMobColorCacheValid = false;
}

void cycleRenderDistance(int delta) {
    const int count = (int)(sizeof(kRenderDistanceTable) / sizeof(kRenderDistanceTable[0]));
    gRenderDistanceIndex = (gRenderDistanceIndex + delta + count) % count;
    gLastWorldRevision3D = -1;
    gLastYaw = 9999.0f;
    gLastPitch = 9999.0f;
    gLastX = 9999.0f;
    gLastY = 9999.0f;
    gLastZ = 9999.0f;
}

static RayHit castRayFromPoint(float ox, float oy, float oz, float dirX, float dirY, float dirZ, float maxDist, bool cullHiddenFaces) {
    RayHit hit{};
    hit.hit = false;
    hit.dist = maxDist;
    hit.placeX = (int)std::floor(ox);
    hit.placeY = (int)std::floor(oy);
    hit.placeZ = (int)std::floor(oz);

    int mapX = (int)std::floor(ox);
    int mapY = (int)std::floor(oy);
    int mapZ = (int)std::floor(oz);

    const float invX = dirX == 0.0f ? 1e9f : std::fabs(1.0f / dirX);
    const float invY = dirY == 0.0f ? 1e9f : std::fabs(1.0f / dirY);
    const float invZ = dirZ == 0.0f ? 1e9f : std::fabs(1.0f / dirZ);

    const int stepX = dirX < 0.0f ? -1 : 1;
    const int stepY = dirY < 0.0f ? -1 : 1;
    const int stepZ = dirZ < 0.0f ? -1 : 1;

    float sideX = (dirX < 0.0f) ? (ox - mapX) * invX : (mapX + 1.0f - ox) * invX;
    float sideY = (dirY < 0.0f) ? (oy - mapY) * invY : (mapY + 1.0f - oy) * invY;
    float sideZ = (dirZ < 0.0f) ? (oz - mapZ) * invZ : (mapZ + 1.0f - oz) * invZ;

    int lastX = mapX;
    int lastY = mapY;
    int lastZ = mapZ;
    int face = 0;

    for (int i = 0; i < 96; ++i) {
        float dist = 0.0f;
        lastX = mapX;
        lastY = mapY;
        lastZ = mapZ;

        if (sideX < sideY && sideX < sideZ) {
            dist = sideX;
            sideX += invX;
            mapX += stepX;
            face = 0;
        } else if (sideY < sideZ) {
            dist = sideY;
            sideY += invY;
            mapY += stepY;
            face = 1;
        } else {
            dist = sideZ;
            sideZ += invZ;
            mapZ += stepZ;
            face = 2;
        }

        if (dist > maxDist) break;
        const int block = getBlock(mapX, mapY, mapZ);
        if (block == BLOCK_AIR) continue;
        if (cullHiddenFaces) {
            const int neighbor = getBlock(lastX, lastY, lastZ);
            if (!shouldRenderFaceAgainstNeighbor(block, neighbor)) continue;
        }
        hit.hit = true;
        hit.block = block;
        hit.x = mapX;
        hit.y = mapY;
        hit.z = mapZ;
        hit.placeX = lastX;
        hit.placeY = lastY;
        hit.placeZ = lastZ;
        hit.dist = dist;
        hit.face = face;
        return hit;
    }

    return hit;
}

static RayHit castRayInternal(const Player& p, float dirX, float dirY, float dirZ, float maxDist) {
    return castRayFromPoint(p.x, p.y + EYE_HEIGHT, p.z, dirX, dirY, dirZ, maxDist, false);
}

static RayHit castVisibleRayInternal(const Player& p, float dirX, float dirY, float dirZ, float maxDist) {
    return castRayFromPoint(p.x, p.y + EYE_HEIGHT, p.z, dirX, dirY, dirZ, maxDist, true);
}

static RayHit castVisibleRayFromDistance(const Player& p, float dirX, float dirY, float dirZ, float startDist, float maxDist) {
    const float eps = 0.0025f;
    const float ox = p.x + dirX * (startDist + eps);
    const float oy = p.y + EYE_HEIGHT + dirY * (startDist + eps);
    const float oz = p.z + dirZ * (startDist + eps);
    const float remain = maxDist - startDist - eps;
    if (remain <= 0.0f) {
        RayHit miss{};
        miss.hit = false;
        miss.dist = maxDist;
        return miss;
    }
    RayHit hit = castRayFromPoint(ox, oy, oz, dirX, dirY, dirZ, remain, true);
    if (hit.hit) hit.dist += startDist + eps;
    return hit;
}

RayHit castCenterRay(const Player& p, float maxDist) {
    const float cp = std::cosf(p.pitch);
    const float dirX = std::sinf(p.yaw) * cp;
    const float dirY = std::sinf(p.pitch);
    const float dirZ = std::cosf(p.yaw) * cp;
    return castRayInternal(p, dirX, dirY, dirZ, maxDist);
}

static bool shouldRedraw3D(const Player& p) {
    int rev = getWorldRevision();
    bool worldChanged = (rev != gLastWorldRevision3D) || ((gFrameCounter & 7) == 0);
    float dyaw = fabsf(p.yaw - gLastYaw);
    float dpitch = fabsf(p.pitch - gLastPitch);
    float dpos = fabsf(p.x - gLastX) + fabsf(p.y - gLastY) + fabsf(p.z - gLastZ);
    bool moved = dpos > 0.05f;
    bool turned = dyaw > 0.012f || dpitch > 0.012f;
    return worldChanged || moved || turned;
}

HudTouchAction handleHudTouch(int x, int y) {
    HudTouchAction action{HUD_TOUCH_NONE, 0};

    if (isCreativeMode()) {
        const int tabW = 54;
        for (int i = 0; i < kHudCategoryCount; ++i) {
            const int tx = 12 + i * 58;
            if (x >= tx && x < tx + tabW && y >= 12 && y < 30) {
                if (gHudCategoryIndex != i) {
                    gHudCategoryIndex = i;
                    gHudStaticValid = false;
                    gHudFrameValid = false;
                }
                return action;
            }
        }

        if (x >= 176 && x < 232 && y >= 152 && y < 176) {
            gMapVisible = !gMapVisible;
            gHudFrameValid = false;
            action.type = HUD_TOUCH_TOGGLE_MAP;
            return action;
        }

        const HudCategory& cat = kHudCategories[gHudCategoryIndex];
        const int gridX = 24;
        const int gridY = 56;
        for (int i = 0; i < cat.count; ++i) {
            const int cx = i % 5;
            const int cy = i / 5;
            const int sx = gridX + cx * 40;
            const int sy = gridY + cy * 28;
            if (x >= sx && x < sx + 38 && y >= sy && y < sy + 28) {
                action.type = HUD_TOUCH_SELECT_BLOCK;
                action.value = cat.entries[i];
                return action;
            }
        }
        return action;
    }

    const int slotW = 22;
    const int slotY = 100;
    const int slotX = 19;
    if (x >= 184 && x < 246 && y >= 58 && y < 82) {
        action.type = HUD_TOUCH_OPEN_CRAFTING;
        return action;
    }
    for (int i = 0; i < HOTBAR_SIZE; ++i) {
        int x0 = slotX + i * 24;
        if (x >= x0 && x < x0 + slotW && y >= slotY && y < slotY + 22) {
            action.type = HUD_TOUCH_SELECT_BLOCK;
            action.value = i;
            return action;
        }
    }
    return action;
}

void renderFrame(const Player& p) {
    ++gFrameCounter;
    if (!shouldRedraw3D(p)) return;

    RayHit center = castCenterRay(p, 6.0f);

    const float cp = std::cosf(p.pitch);
    const float sp = std::sinf(p.pitch);
    const float cy = std::cosf(p.yaw);
    const float sy = std::sinf(p.yaw);

    const float forwardX = sy * cp;
    const float forwardY = sp;
    const float forwardZ = cy * cp;
    const float rightX = cy;
    const float rightY = 0.0f;
    const float rightZ = -sy;
    const float upX = -sy * sp;
    const float upY = cp;
    const float upZ = -cy * sp;

    const float eyeY = p.y + EYE_HEIGHT;
    for (int py = 0; py < gRenderH; ++py) {
        const float ny = gNy[py];
        u16* dstRow = gLowRes[py];
        for (int px = 0; px < gRenderW; ++px) {
            const float nx = gNx[px];
            const float invLen = gInvLen[py][px];
            const float dirX = (forwardX + rightX * nx - upX * ny) * invLen;
            const float dirY = (forwardY + rightY * nx - upY * ny) * invLen;
            const float dirZ = (forwardZ + rightZ * nx - upZ * ny) * invLen;

            RayHit hit = castVisibleRayInternal(p, dirX, dirY, dirZ, getRenderDistance());
            u16 color;
            if (hit.hit) {
                bool hl = center.hit && hit.x == center.x && hit.y == center.y && hit.z == center.z;
                u16 texel = sampleHitTexture(p, hit, dirX, dirY, dirZ, eyeY);
                int fog = (int)(hit.dist * 2.0f);

                int light = getCombinedLightLevel(hit.x, hit.y, hit.z);
                bool emissive = isLightEmitterBlock(hit.block);
                color = shadeColor(texel, hit.face, hl, fog, light, emissive);
                if (hit.block == BLOCK_GLASS) {
                    RayHit behind = castVisibleRayFromDistance(p, dirX, dirY, dirZ, hit.dist, getRenderDistance());
                    u16 behindColor;
                    if (behind.hit) {
                        const bool behindHl = center.hit && behind.x == center.x && behind.y == center.y && behind.z == center.z;
                        behindColor = shadeHitTexture(p, behind, dirX, dirY, dirZ, eyeY, behindHl);
                    } else {
                        int sky = getSkyLightLevel();
                        behindColor = (py < gRenderH / 2) ? shadeColor(gSkyColor[py], 1, false, 0, sky, false) : shadeColor(gGroundColor[py], 1, false, 0, (sky + 6) / 2, false);
                    }
                    const u16 glassTint = shadeColor(rgb15(20, 24, 26), hit.face, hl, fog / 2, light, false);
                    color = blendColor50(behindColor, glassTint);
                    if (texel != rgb15(22, 26, 28)) color = blendColor50(color, shadeColor(texel, hit.face, hl, fog / 2, light, false));
                }
            } else {
                int sky = getSkyLightLevel();
                color = (py < gRenderH / 2) ? shadeColor(gSkyColor[py], 1, false, 0, sky, false) : shadeColor(gGroundColor[py], 1, false, 0, (sky + 6) / 2, false);
            }
            dstRow[px] = color;
        }
    }

    drawMobBillboards(p, eyeY, forwardX, forwardY, forwardZ, rightX, rightY, rightZ, upX, upY, upZ);

    for (int y = 0; y < SCREEN_H; ++y) {
        const u16* src = gLowRes[gYMap[y]];
        u16* dst = gTopFb + y * 256;
        for (int x = 0; x < SCREEN_W; ++x) dst[x] = src[gXMap[x]];
    }

    const int cx = SCREEN_W / 2;
    const int crossY = SCREEN_H / 2;
    for (int y = crossY - 3; y <= crossY + 3; ++y) {
        gTopFb[y * 256 + cx] = rgb15(31, 31, 31);
    }
    u16* midRow = gTopFb + crossY * 256;
    for (int x = cx - 3; x <= cx + 3; ++x) midRow[x] = rgb15(31, 31, 31);

    int heldBlock = getSelectedPlaceableBlock(p);
    if (heldBlock != BLOCK_AIR) {
        drawHeldBlockModel(gTopFb, heldBlock);
    } else {
        drawHeldHand(gTopFb);
    }

    gLastYaw = p.yaw;
    gLastPitch = p.pitch;
    gLastX = p.x;
    gLastY = p.y;
    gLastZ = p.z;
    gLastWorldRevision3D = getWorldRevision();
}



struct CamPt {
    float x;
    float y;
    float z;
};

struct CuboidDef {
    float minX, minY, minZ;
    float maxX, maxY, maxZ;
    u16 frontColor, backColor, leftColor, rightColor, topColor, bottomColor;
};

struct RenderFace {
    ScreenPt p[4];
    float depth;
    u16 color;
};

static void fillTriangleLowRes(ScreenPt a, ScreenPt b, ScreenPt c, u16 color) {
    int minX = a.x, maxX = a.x, minY = a.y, maxY = a.y;
    if (b.x < minX) minX = b.x; if (c.x < minX) minX = c.x;
    if (b.x > maxX) maxX = b.x; if (c.x > maxX) maxX = c.x;
    if (b.y < minY) minY = b.y; if (c.y < minY) minY = c.y;
    if (b.y > maxY) maxY = b.y; if (c.y > maxY) maxY = c.y;
    if (maxX < 0 || maxY < 0 || minX >= gRenderW || minY >= gRenderH) return;
    if (minX < 0) minX = 0; if (minY < 0) minY = 0;
    if (maxX >= gRenderW) maxX = gRenderW - 1; if (maxY >= gRenderH) maxY = gRenderH - 1;
    const int area = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (area == 0) return;
    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            const int w0 = (b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x);
            const int w1 = (c.x - b.x) * (y - b.y) - (c.y - b.y) * (x - b.x);
            const int w2 = (a.x - c.x) * (y - c.y) - (a.y - c.y) * (x - c.x);
            if ((area > 0 && w0 >= 0 && w1 >= 0 && w2 >= 0) || (area < 0 && w0 <= 0 && w1 <= 0 && w2 <= 0)) {
                gLowRes[y][x] = color;
            }
        }
    }
}

static void fillQuadLowRes(ScreenPt a, ScreenPt b, ScreenPt c, ScreenPt d, u16 color) {
    fillTriangleLowRes(a, b, c, color);
    fillTriangleLowRes(a, c, d, color);
}

static inline CamPt toCameraSpace(float wx, float wy, float wz, const Player& p, float eyeY,
        float forwardX, float forwardY, float forwardZ,
        float rightX, float rightY, float rightZ,
        float upX, float upY, float upZ) {
    float dx = wx - p.x;
    float dy = wy - eyeY;
    float dz = wz - p.z;
    CamPt c;
    c.x = dx * rightX + dy * rightY + dz * rightZ;
    c.y = -(dx * upX + dy * upY + dz * upZ);
    c.z = dx * forwardX + dy * forwardY + dz * forwardZ;
    return c;
}

static inline ScreenPt cameraToScreen(const CamPt& c) {
    const float aspect = (float)gRenderW / (float)gRenderH;
    const float invZ = 1.0f / c.z;
    ScreenPt s;
    s.x = (int)((c.x * invZ / (FOV * aspect) + 1.0f) * 0.5f * gRenderW);
    s.y = (int)((c.y * invZ / FOV + 1.0f) * 0.5f * gRenderH);
    return s;
}

static inline u16 sampleMobColor(int x0, int y0, int w, int h) {
    long rs = 0, gs = 0, bs = 0, n = 0;
    for (int y = y0; y < y0 + h; ++y) {
        for (int x = x0; x < x0 + w; ++x) {
            u16 c = TEX_MOB_PIG[y * MOB_TEX_W + x];
            rs += (c & 31);
            gs += ((c >> 5) & 31);
            bs += ((c >> 10) & 31);
            ++n;
        }
    }
    if (n <= 0) return rgb15(24, 18, 18);
    return rgb15((int)(rs / n), (int)(gs / n), (int)(bs / n));
}

static void ensureMobColorCache() {
    if (gMobColorCacheValid) return;
    gPigHeadFront = sampleMobColor(16, 16, 16, 16);
    gPigHeadSide = sampleMobColor(0, 16, 16, 16);
    gPigHeadTop = sampleMobColor(16, 0, 16, 16);
    gPigSnoutFront = sampleMobColor(34, 34, 8, 6);
    gPigSnoutSide = sampleMobColor(32, 34, 2, 6);
    gPigSnoutTop = sampleMobColor(34, 32, 8, 2);
    gPigBodyFront = sampleMobColor(40, 16, 16, 16);
    gPigBodySide = sampleMobColor(56, 16, 8, 16);
    gPigBodyTop = sampleMobColor(40, 0, 16, 16);
    gPigLegFront = sampleMobColor(8, 40, 8, 12);
    gPigLegSide = sampleMobColor(0, 40, 8, 12);
    gPigLegTop = sampleMobColor(8, 32, 8, 8);
    gMobColorCacheValid = true;
}

static int appendCuboidFaces(RenderFace* outFaces, int faceCount, const CuboidDef& cuboid, const Mob& mob,
        const Player& p, float eyeY,
        float forwardX, float forwardY, float forwardZ,
        float rightX, float rightY, float rightZ,
        float upX, float upY, float upZ,
        int fog, int light) {
    const float cy = std::cosf(mob.yaw);
    const float sy = std::sinf(mob.yaw);
    auto tx = [&](float lx, float lz, float& ox, float& oz) {
        ox = lx * cy + lz * sy;
        oz = -lx * sy + lz * cy;
    };
    const float vx[8] = {cuboid.minX, cuboid.maxX, cuboid.maxX, cuboid.minX, cuboid.minX, cuboid.maxX, cuboid.maxX, cuboid.minX};
    const float vy[8] = {cuboid.minY, cuboid.minY, cuboid.maxY, cuboid.maxY, cuboid.minY, cuboid.minY, cuboid.maxY, cuboid.maxY};
    const float vz[8] = {cuboid.minZ, cuboid.minZ, cuboid.minZ, cuboid.minZ, cuboid.maxZ, cuboid.maxZ, cuboid.maxZ, cuboid.maxZ};
    CamPt cam[8];
    ScreenPt scr[8];
    for (int i = 0; i < 8; ++i) {
        float rx, rz;
        tx(vx[i], vz[i], rx, rz);
        cam[i] = toCameraSpace(mob.x + rx, mob.y + vy[i], mob.z + rz, p, eyeY, forwardX, forwardY, forwardZ, rightX, rightY, rightZ, upX, upY, upZ);
        if (cam[i].z < 0.08f) cam[i].z = 0.08f;
        scr[i] = cameraToScreen(cam[i]);
    }
    struct FaceInfo { int i0,i1,i2,i3; float nx,ny,nz; float cx,cy,cz; u16 color; };
    FaceInfo faces[6] = {
        {0,1,2,3, 0,0,-1, (cuboid.minX+cuboid.maxX)*0.5f, (cuboid.minY+cuboid.maxY)*0.5f, cuboid.minZ, cuboid.frontColor},
        {5,4,7,6, 0,0, 1, (cuboid.minX+cuboid.maxX)*0.5f, (cuboid.minY+cuboid.maxY)*0.5f, cuboid.maxZ, cuboid.backColor},
        {4,0,3,7,-1,0, 0, cuboid.minX, (cuboid.minY+cuboid.maxY)*0.5f, (cuboid.minZ+cuboid.maxZ)*0.5f, cuboid.leftColor},
        {1,5,6,2, 1,0, 0, cuboid.maxX, (cuboid.minY+cuboid.maxY)*0.5f, (cuboid.minZ+cuboid.maxZ)*0.5f, cuboid.rightColor},
        {3,2,6,7, 0,1, 0, (cuboid.minX+cuboid.maxX)*0.5f, cuboid.maxY, (cuboid.minZ+cuboid.maxZ)*0.5f, cuboid.topColor},
        {4,5,1,0, 0,-1,0, (cuboid.minX+cuboid.maxX)*0.5f, cuboid.minY, (cuboid.minZ+cuboid.maxZ)*0.5f, cuboid.bottomColor},
    };
    for (int fi = 0; fi < 6; ++fi) {
        float rnx, rnz, rcx, rcz;
        tx(faces[fi].nx, faces[fi].nz, rnx, rnz);
        tx(faces[fi].cx, faces[fi].cz, rcx, rcz);
        float wcx = mob.x + rcx;
        float wcy = mob.y + faces[fi].cy;
        float wcz = mob.z + rcz;
        float toCamX = p.x - wcx;
        float toCamY = eyeY - wcy;
        float toCamZ = p.z - wcz;
        if (rnx * toCamX + faces[fi].ny * toCamY + rnz * toCamZ <= 0.0f) continue;
        float depth = 0.25f * (cam[faces[fi].i0].z + cam[faces[fi].i1].z + cam[faces[fi].i2].z + cam[faces[fi].i3].z);
        RenderFace& out = outFaces[faceCount++];
        out.p[0] = scr[faces[fi].i0]; out.p[1] = scr[faces[fi].i1]; out.p[2] = scr[faces[fi].i2]; out.p[3] = scr[faces[fi].i3];
        out.depth = depth;
        out.color = shadeColor(faces[fi].color, 1, false, fog + (int)(depth * 1.5f), light, false);
    }
    return faceCount;
}

static void drawMobBillboards(const Player& p, float eyeY, float forwardX, float forwardY, float forwardZ, float rightX, float rightY, float rightZ, float upX, float upY, float upZ) {
    ensureMobColorCache();

    for (int i = 0; i < getMobCount(); ++i) {
        const Mob* mob = getMob(i);
        if (!mob || !mob->active || mob->type != MOB_PIG) continue;
        float dx = mob->x - p.x;
        float dy = (mob->y + 0.5f) - eyeY;
        float dz = mob->z - p.z;
        const float distSq = dx * dx + dy * dy + dz * dz;
        const float maxDist = getRenderDistance();
        if (distSq > maxDist * maxDist || distSq < 0.0064f) continue;
        float dist = std::sqrt(distSq);
        float invDist = 1.0f / dist;
        RayHit occlude = castVisibleRayInternal(p, dx * invDist, dy * invDist, dz * invDist, getRenderDistance());
        if (occlude.hit && occlude.dist < dist - 0.35f) continue;
        int light = getCombinedLightLevel((int)mob->x, (int)mob->y, (int)mob->z);
        int fog = (int)(dist * 2.0f);

        const float legLiftA = std::sinf((float)mob->animTick * 0.25f) * 0.03f;
        const float legLiftB = -legLiftA;
        // Canonical quadruped proportions: 4x6x4 legs, 10x8x16 body, 8x8x8 head, scaled to a 1-block-high pig.
        CuboidDef parts[6] = {
            {-0.20f, 0.00f + legLiftA, -0.40f, -0.06f, 0.38f + legLiftA, -0.24f, gPigLegFront, gPigLegFront, gPigLegSide, gPigLegSide, gPigLegTop, gPigLegTop},
            { 0.06f, 0.00f + legLiftB, -0.40f,  0.20f, 0.38f + legLiftB, -0.24f, gPigLegFront, gPigLegFront, gPigLegSide, gPigLegSide, gPigLegTop, gPigLegTop},
            {-0.20f, 0.00f + legLiftB,  0.18f, -0.06f, 0.38f + legLiftB,  0.34f, gPigLegFront, gPigLegFront, gPigLegSide, gPigLegSide, gPigLegTop, gPigLegTop},
            { 0.06f, 0.00f + legLiftA,  0.18f,  0.20f, 0.38f + legLiftA,  0.34f, gPigLegFront, gPigLegFront, gPigLegSide, gPigLegSide, gPigLegTop, gPigLegTop},
            {-0.34f, 0.30f, -0.38f,  0.34f, 0.72f,  0.38f, gPigBodyFront, gPigBodyFront, gPigBodySide, gPigBodySide, gPigBodyTop, gPigBodyTop},
            {-0.24f, 0.42f, -0.70f,  0.24f, 0.86f, -0.22f, gPigHeadFront, gPigHeadFront, gPigHeadSide, gPigHeadSide, gPigHeadTop, gPigHeadTop},
        };
        CuboidDef snout = {-0.12f, 0.50f, -0.82f, 0.12f, 0.68f, -0.70f, gPigSnoutFront, gPigSnoutFront, gPigSnoutSide, gPigSnoutSide, gPigSnoutTop, gPigSnoutTop};

        RenderFace faces[42];
        int faceCount = 0;
        for (int pi = 0; pi < 6; ++pi) {
            faceCount = appendCuboidFaces(faces, faceCount, parts[pi], *mob, p, eyeY, forwardX, forwardY, forwardZ, rightX, rightY, rightZ, upX, upY, upZ, fog, light);
        }
        faceCount = appendCuboidFaces(faces, faceCount, snout, *mob, p, eyeY, forwardX, forwardY, forwardZ, rightX, rightY, rightZ, upX, upY, upZ, fog, light);

        for (int a = 0; a < faceCount; ++a) {
            for (int b = a + 1; b < faceCount; ++b) {
                if (faces[a].depth < faces[b].depth) {
                    RenderFace tmp = faces[a];
                    faces[a] = faces[b];
                    faces[b] = tmp;
                }
            }
        }
        for (int fi = 0; fi < faceCount; ++fi) {
            fillQuadLowRes(faces[fi].p[0], faces[fi].p[1], faces[fi].p[2], faces[fi].p[3], faces[fi].color);
        }
    }
}

static void drawHeart(u16* fb, int x, int y, bool full, bool half) {
    u16 on = rgb15(31, 4, 8);
    u16 off = rgb15(12, 5, 6);
    u16 mid = rgb15(23, 3, 6);
    drawRect(fb, x + 1, y + 2, 3, 3, full || half ? on : off);
    drawRect(fb, x + 5, y + 2, 3, 3, full ? on : off);
    drawRect(fb, x + 2, y + 5, 5, 3, full ? on : off);
    drawRect(fb, x + 3, y + 8, 3, 2, full ? on : off);
    if (half) {
        drawRect(fb, x + 5, y + 2, 3, 3, off);
        drawRect(fb, x + 5, y + 5, 2, 3, off);
        drawRect(fb, x + 5, y + 8, 1, 2, off);
        drawRect(fb, x + 1, y + 2, 2, 3, mid);
    }
}

static void drawBreakMeter(u16* fb, int x, int y, int progress, int needed) {
    if (needed <= 0 || progress <= 0) return;
    if (progress > needed) progress = needed;
    drawRect(fb, x, y, 96, 10, rgb15(6, 6, 8));
    drawRect(fb, x + 2, y + 2, 92, 6, rgb15(1, 1, 2));
    int fill = (88 * progress) / needed;
    drawRect(fb, x + 4, y + 3, fill, 4, rgb15(8, 20, 8));
}

void renderHUD(const Player& p, const RayHit& hit, int breakTicks, int breakNeeded) {
    (void)hit;
    if (!gHudStaticValid || gHudRevision != getWorldRevision()) rebuildHudStatic();

    int selectedItemId = -1;
    int selectedItemCount = 0;
    if (!isCreativeMode()) {
        const InventorySlot& selected = getSelectedSlot(p);
        if (selected.count > 0) {
            selectedItemId = selected.id;
            selectedItemCount = selected.count;
        }
    }

    const bool hudDirty = !gHudFrameValid
        || gLastHudMode != getCurrentGameMode()
        || gLastHudHealth != p.health
        || gLastHudAlive != p.alive
        || gLastHudSelectedBlock != p.selectedBlock
        || gLastHudSelectedSlot != p.selectedSlot
        || gLastHudBreakTicks != breakTicks
        || gLastHudBreakNeeded != breakNeeded
        || gLastHudMapVisible != gMapVisible
        || gLastHudPlayerMapX != (int)p.x
        || gLastHudPlayerMapZ != (int)p.z
        || gLastHudCategoryIndex != gHudCategoryIndex
        || gLastHudSelectedItemId != selectedItemId
        || gLastHudSelectedItemCount != selectedItemCount;
    if (!hudDirty) return;

    ++gHudFrameCounter;
    std::memcpy(gBottomFb, gHudStatic, 192 * 256 * sizeof(u16));

    if (isCreativeMode()) {
        const HudCategory& cat = kHudCategories[gHudCategoryIndex];
        const int gridX = 24;
        const int gridY = 56;
        const int cols = 5;
        const int rows = 3;
        const int slotW = 38;
        const int slotH = 28;
        for (int i = 0; i < cols * rows; ++i) {
            const int cx = i % cols;
            const int cy = i / cols;
            const int x = gridX + cx * 40;
            const int y = gridY + cy * 28;
            drawRect(gBottomFb, x, y, slotW, slotH, rgb15(13, 13, 14));
            drawRect(gBottomFb, x + 2, y + 2, slotW - 4, slotH - 4, rgb15(22, 22, 23));
            if (!cat.placeholderOnly && i < cat.count) {
                drawBlockIcon(gBottomFb, x + 5, y + 1, cat.entries[i], cat.entries[i] == p.selectedBlock);
            }
        }

        if (cat.placeholderOnly) {
            drawTextPixelScaled(gBottomFb, 68, 94, "ITEMS", rgb15(27, 27, 28), 2);
            drawTextPixel(gBottomFb, 64, 114, "FUTURE TAB FOR TOOLS", rgb15(24, 24, 25));
        } else {
            char label[24];
            std::snprintf(label, sizeof(label), "%s", blockName(p.selectedBlock));
            drawTextPixel(gBottomFb, 24, 42, label, rgb15(30, 30, 30));
        }

        if (gMapVisible) {
            drawMapPanel(gBottomFb, (int)p.x, (int)p.z);
        }
    } else {
        for (int i = 0; i < 10; ++i) {
            int hp = p.health - i * 2;
            drawHeart(gBottomFb, 20 + i * 12, 56, hp >= 2, hp == 1);
        }
        drawTextPixel(gBottomFb, 20, 76, p.alive ? "HEALTH" : "YOU DIED", rgb15(31, 31, 31));
        drawTextPixel(gBottomFb, 150, 56, getGameModeName(getCurrentGameMode()), rgb15(26, 28, 30));
        drawRect(gBottomFb, 184, 58, 62, 24, rgb15(10, 10, 12));
        drawRect(gBottomFb, 186, 60, 58, 20, rgb15(22, 22, 24));
        drawTextPixel(gBottomFb, 198, 67, "CRAFT", rgb15(31, 31, 31));

        const int slotW = 22;
        const int slotY = 100;
        const int slotX = 19;
        for (int i = 0; i < HOTBAR_SIZE; ++i) {
            int x = slotX + i * 24;
            bool sel = (i == p.selectedSlot);
            drawRect(gBottomFb, x, slotY, slotW, 22, sel ? rgb15(24, 24, 26) : rgb15(10, 10, 12));
            drawRect(gBottomFb, x + 2, slotY + 2, slotW - 4, 18, rgb15(3, 3, 4));
            const InventorySlot& slot = p.hotbar[i];
            if (slot.count > 0) {
                if (slot.isBlock) drawBlockIcon(gBottomFb, x - 3, slotY - 5, slot.id, false);
                else drawItemIcon(gBottomFb, x - 3, slotY - 5, slot.id, false);
                char cnt[6];
                std::snprintf(cnt, sizeof(cnt), "%d", slot.count);
                drawTextPixel(gBottomFb, x + 11, slotY + 12, cnt, rgb15(31, 31, 31));
            }
        }
        const InventorySlot& selected = getSelectedSlot(p);
        if (selected.count > 0) {
            drawTextPixel(gBottomFb, 24, 88, selected.isBlock ? blockName(selected.id) : itemName(selected.id), rgb15(31, 31, 31));
        } else {
            drawTextPixel(gBottomFb, 24, 88, "EMPTY", rgb15(24, 24, 24));
        }
        drawBreakMeter(gBottomFb, 136, 82, breakTicks, breakNeeded);
    }

    if (!p.alive && isSurvivalMode()) {
        drawRect(gBottomFb, 48, 32, 160, 64, rgb15(10, 2, 2));
        drawRect(gBottomFb, 52, 36, 152, 56, rgb15(18, 4, 4));
        drawTextPixelScaled(gBottomFb, 84, 48, "YOU DIED", rgb15(31, 31, 31), 2);
        drawTextPixel(gBottomFb, 72, 72, "PRESS A TO RESPAWN", rgb15(31, 31, 31));
    }

    std::memcpy(gHudFrame, gBottomFb, 192 * 256 * sizeof(u16));
    gHudFrameValid = true;
    gLastHudMode = getCurrentGameMode();
    gLastHudHealth = p.health;
    gLastHudAlive = p.alive;
    gLastHudSelectedBlock = p.selectedBlock;
    gLastHudSelectedSlot = p.selectedSlot;
    gLastHudBreakTicks = breakTicks;
    gLastHudBreakNeeded = breakNeeded;
    gLastHudMapVisible = gMapVisible;
    gLastHudPlayerMapX = (int)p.x;
    gLastHudPlayerMapZ = (int)p.z;
    gLastHudCategoryIndex = gHudCategoryIndex;
    gLastHudSelectedItemId = selectedItemId;
    gLastHudSelectedItemCount = selectedItemCount;
}

static void blitImage(u16* fb, int dx, int dy, int dw, int dh, const unsigned short* src, int sw, int sh, bool transparent) {
    if (dw <= 0 || dh <= 0) return;
    for (int y = 0; y < dh; ++y) {
        int sy = (y * sh) / dh;
        int py = dy + y;
        if (py < 0 || py >= 192) continue;
        u16* row = fb + py * 256;
        for (int x = 0; x < dw; ++x) {
            int sx = (x * sw) / dw;
            int px = dx + x;
            if (px < 0 || px >= 256) continue;
            u16 c = src[sy * sw + sx];
            if (transparent && c == 0) continue;
            row[px] = c;
        }
    }
}

static void drawButton(u16* fb, int x, int y, int w, int h, const unsigned short* label, int labelW, int labelH) {
    drawRect(fb, x, y, w, h, rgb15(9, 9, 12));
    drawRect(fb, x + 2, y + 2, w - 4, h - 4, rgb15(4, 4, 6));
    drawRect(fb, x + 4, y + 4, w - 8, h - 8, rgb15(12, 12, 15));
    blitImage(fb, x + (w - labelW) / 2, y + (h - labelH) / 2, labelW, labelH, label, labelW, labelH, true);
}

static int buttonHit(int x, int y, int bx, int by, int bw, int bh) {
    return x >= bx && x < bx + bw && y >= by && y < by + bh;
}

static void restoreRectFromBase(u16* fb, const u16* base, int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return;
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > 256) w = 256 - x;
    if (y + h > 192) h = 192 - y;
    if (w <= 0 || h <= 0) return;
    for (int py = 0; py < h; ++py) {
        std::memcpy(fb + (y + py) * 256 + x, base + (y + py) * 256 + x, w * sizeof(u16));
    }
}

static void beginMenuScreen(MenuCacheScreen screen) {
    if (gMenuCacheScreen == screen) return;
    gMenuCacheScreen = screen;
    gLastMenuLogoX = -1;
    gLastMenuLogoY = -1;
    gLastMenuLogoW = 0;
    gLastMenuLogoH = 0;
    gLastLoadingProgress = -1;
    gLastGraphicsSlider = -1;
    for (int y = 0; y < 192; ++y) {
        std::memcpy(gMenuTopBase[y], MENU_BG + y * 256, 256 * sizeof(u16));
        std::memcpy(gMenuBottomBase[y], MENU_BG + y * 256, 256 * sizeof(u16));
    }
}

static void drawAnimatedMenuLogo(int /*animTick*/, int baseW, int /*amplitude*/, int baseY) {
    const int logoW = baseW;
    const int logoH = (MENU_LOGO_H * logoW) / MENU_LOGO_W;
    const int logoX = (256 - logoW) / 2;
    const int logoY = baseY;

    blitImage(gTopFb, logoX, logoY, logoW, logoH, MENU_LOGO, MENU_LOGO_W, MENU_LOGO_H, true);
    gLastMenuLogoX = logoX;
    gLastMenuLogoY = logoY;
    gLastMenuLogoW = logoW;
    gLastMenuLogoH = logoH;
}


static void drawButtonText(u16* fb, int x, int y, int w, int h, const char* text) {
    drawRect(fb, x, y, w, h, rgb15(9, 9, 12));
    drawRect(fb, x + 2, y + 2, w - 4, h - 4, rgb15(4, 4, 6));
    drawRect(fb, x + 4, y + 4, w - 8, h - 8, rgb15(12, 12, 15));
    int textW = 0;
    for (const char* p = text; *p; ++p) ++textW;
    const int scale = 2;
    const int pixelW = textW * 6 * scale - scale;
    const int pixelH = 5 * scale;
    drawTextPixelScaled(fb, x + (w - pixelW) / 2, y + (h - pixelH) / 2, text, rgb15(31, 31, 31), scale);
}

static void drawSliderBar(u16* fb, int x, int y, int w, int h, int value) {
    if (value < 0) value = 0;
    if (value > 100) value = 100;
    drawRect(fb, x, y, w, h, rgb15(6, 6, 8));
    drawRect(fb, x + 2, y + 2, w - 4, h - 4, rgb15(1, 1, 2));
    int fill = ((w - 8) * value) / 100;
    drawRect(fb, x + 4, y + 4, fill, h - 8, rgb15(7, 18, 28));
    int knobX = x + 4 + fill - 4;
    if (knobX < x + 2) knobX = x + 2;
    if (knobX > x + w - 10) knobX = x + w - 10;
    drawRect(fb, knobX, y + 1, 8, h - 2, rgb15(31, 31, 31));
}

static void drawSettingRow(u16* fb, int y, const char* label, const char* value);

static void drawRenderSizeLabel(u16* fb, int x, int y) {
    char line[32];
    std::snprintf(line, sizeof(line), "%d X %d", gRenderW, gRenderH);
    drawTextPixel(fb, x, y, line, rgb15(31, 31, 31));
}

void renderTitleMenu(int animTick) {
    (void)animTick;
    beginMenuScreen(MENU_CACHE_TITLE);
    if (gLastMenuLogoX != -1) return;

    std::memcpy(gTopFb, gMenuTopBase, 192 * 256 * sizeof(u16));
    std::memcpy(gBottomFb, gMenuBottomBase, 192 * 256 * sizeof(u16));

    const int bw = 176;
    const int bh = 34;
    const int bx = (256 - bw) / 2;
    drawButton(gBottomFb, bx, 46, bw, bh, TITLE_NEW_GAME, TITLE_NEW_GAME_W, TITLE_NEW_GAME_H);
    drawButton(gBottomFb, bx, 88, bw, bh, TITLE_LOAD_GAME, TITLE_LOAD_GAME_W, TITLE_LOAD_GAME_H);
    drawButtonText(gBottomFb, bx, 130, bw, bh, "OPTIONS");

    drawAnimatedMenuLogo(0, 180, 0, 18);
}


void renderOptionsMenu() {
    beginMenuScreen(MENU_CACHE_OPTIONS);

    std::memcpy(gTopFb, gMenuTopBase, 192 * 256 * sizeof(u16));
    std::memcpy(gBottomFb, gMenuBottomBase, 192 * 256 * sizeof(u16));

    drawTextPixelScaled(gBottomFb, 83, 12, "OPTIONS", rgb15(31, 31, 31), 2);
    drawSettingRow(gBottomFb, 46, "RENDER DIST", renderDistanceName());
    drawSettingRow(gBottomFb, 78, "LOOK SPEED", speedName(getLookSpeed()));
    drawSettingRow(gBottomFb, 110, "MOVE SPEED", speedName(getMoveSpeed()));
    drawButtonText(gBottomFb, 68, 138, 120, 24, "GRAPHICS");
    drawButtonText(gBottomFb, 14, 164, 82, 20, "BACK");
    blitImage(gTopFb, (256 - MENU_LOGO_W) / 2, 26, MENU_LOGO_W, MENU_LOGO_H, MENU_LOGO, MENU_LOGO_W, MENU_LOGO_H, true);
    gLastMenuLogoX = 0;
}

void renderGraphicsMenu() {
    beginMenuScreen(MENU_CACHE_GRAPHICS);
    const int sliderValue = graphicsSliderValue();
    if (gLastMenuLogoX != -1 && gLastGraphicsSlider == sliderValue) return;

    std::memcpy(gTopFb, gMenuTopBase, 192 * 256 * sizeof(u16));
    std::memcpy(gBottomFb, gMenuBottomBase, 192 * 256 * sizeof(u16));

    drawTextPixelScaled(gBottomFb, 77, 16, "GRAPHICS", rgb15(31, 31, 31), 2);
    drawTextPixelScaled(gBottomFb, 31, 50, "RESOLUTION", rgb15(31, 31, 31), 2);
    drawSliderBar(gBottomFb, 34, 78, 188, 22, sliderValue);
    drawRenderSizeLabel(gBottomFb, 94, 112);
    drawTextPixel(gBottomFb, 24, 128, "FASTER", rgb15(26, 26, 26));
    drawTextPixel(gBottomFb, 166, 128, "SHARPER", rgb15(26, 26, 26));
    drawButtonText(gBottomFb, 14, 148, 82, 28, "BACK");
    blitImage(gTopFb, (256 - MENU_LOGO_W) / 2, 26, MENU_LOGO_W, MENU_LOGO_H, MENU_LOGO, MENU_LOGO_W, MENU_LOGO_H, true);
    gLastMenuLogoX = 0;
    gLastGraphicsSlider = sliderValue;
}


static void drawSettingRow(u16* fb, int y, const char* label, const char* value) {
    drawTextPixelScaled(fb, 18, y, label, rgb15(31, 31, 31), 2);
    drawButtonText(fb, 144, y - 4, 26, 24, "<");
    drawButtonText(fb, 226, y - 4, 26, 24, ">");
    drawRect(fb, 172, y - 2, 52, 20, rgb15(7, 7, 9));
    drawRect(fb, 174, y, 48, 16, rgb15(3, 3, 5));
    drawTextPixel(fb, 176, y + 4, value, rgb15(31, 31, 31));
}

void renderWorldSetupMenu(const WorldGenConfig& config, u32 seedStep) {
    beginMenuScreen(MENU_CACHE_WORLD_SETUP);

    std::memcpy(gTopFb, gMenuTopBase, 192 * 256 * sizeof(u16));
    std::memcpy(gBottomFb, gMenuBottomBase, 192 * 256 * sizeof(u16));

    char line[32];
    drawTextPixelScaled(gTopFb, 30, 22, "NEW WORLD", rgb15(31, 31, 31), 2);
    drawRect(gTopFb, 20, 56, 216, 98, rgb15(7, 7, 9));
    drawRect(gTopFb, 24, 60, 208, 90, rgb15(3, 3, 5));
    drawTextPixel(gTopFb, 34, 72, "DPAD TYPE/SIZE", rgb15(31, 31, 31));
    drawTextPixel(gTopFb, 34, 86, "SELECT MODE", rgb15(24, 30, 31));
    drawTextPixel(gTopFb, 34, 100, "L/R SEED  X TREES", rgb15(24, 30, 31));
    drawTextPixel(gTopFb, 34, 114, "Y RANDOM", rgb15(24, 30, 31));
    drawTextPixel(gTopFb, 34, 128, "A CREATE  B BACK", rgb15(24, 30, 31));
    drawTextPixel(gTopFb, 34, 142, "TOUCH ALSO WORKS", rgb15(24, 30, 31));

    drawRect(gBottomFb, 8, 8, 240, 176, rgb15(7, 7, 9));
    drawRect(gBottomFb, 12, 12, 232, 168, rgb15(3, 3, 5));
    drawTextPixelScaled(gBottomFb, 36, 18, "WORLD SETTINGS", rgb15(31, 31, 31), 2);

    drawTextPixel(gBottomFb, 20, 42, "TYPE", rgb15(31, 31, 31));
    drawButtonText(gBottomFb, 64, 36, 24, 20, "-");
    drawRect(gBottomFb, 92, 38, 92, 16, rgb15(7, 7, 9));
    drawRect(gBottomFb, 94, 40, 88, 12, rgb15(11, 11, 14));
    drawTextPixel(gBottomFb, 98, 44, getWorldTypeName(config.worldType), rgb15(31, 31, 31));
    drawButtonText(gBottomFb, 188, 36, 24, 20, "+");

    drawTextPixel(gBottomFb, 20, 64, "SIZE", rgb15(31, 31, 31));
    drawButtonText(gBottomFb, 64, 58, 24, 20, "-");
    drawRect(gBottomFb, 92, 60, 92, 16, rgb15(7, 7, 9));
    drawRect(gBottomFb, 94, 62, 88, 12, rgb15(11, 11, 14));
    drawTextPixel(gBottomFb, 98, 66, getWorldSizeName(config.sizePreset), rgb15(31, 31, 31));
    drawButtonText(gBottomFb, 188, 58, 24, 20, "+");

    drawTextPixel(gBottomFb, 20, 86, "MODE", rgb15(31, 31, 31));
    drawButtonText(gBottomFb, 64, 80, 24, 20, "-");
    drawRect(gBottomFb, 92, 82, 92, 16, rgb15(7, 7, 9));
    drawRect(gBottomFb, 94, 84, 88, 12, rgb15(11, 11, 14));
    drawTextPixel(gBottomFb, 102, 88, getGameModeName(config.gameMode), rgb15(31, 31, 31));
    drawButtonText(gBottomFb, 188, 80, 24, 20, "+");

    drawTextPixel(gBottomFb, 20, 108, "TREES", rgb15(31, 31, 31));
    drawButtonText(gBottomFb, 92, 102, 72, 20, config.generateTrees ? "ON" : "OFF");

    drawTextPixel(gBottomFb, 20, 130, "SEED", rgb15(31, 31, 31));
    drawButtonText(gBottomFb, 64, 124, 24, 20, "-");
    drawRect(gBottomFb, 92, 126, 120, 16, rgb15(7, 7, 9));
    drawRect(gBottomFb, 94, 128, 116, 12, rgb15(11, 11, 14));
    std::snprintf(line, sizeof(line), "%08lX", (unsigned long)config.seed);
    drawTextPixel(gBottomFb, 108, 132, line, rgb15(31, 31, 31));
    drawButtonText(gBottomFb, 216, 124, 24, 20, "+");

    drawTextPixel(gBottomFb, 20, 152, "STEP", rgb15(31, 31, 31));
    drawButtonText(gBottomFb, 64, 146, 24, 20, "-");
    drawRect(gBottomFb, 92, 148, 56, 16, rgb15(7, 7, 9));
    drawRect(gBottomFb, 94, 150, 52, 12, rgb15(11, 11, 14));
    std::snprintf(line, sizeof(line), "%lu", (unsigned long)seedStep);
    drawTextPixel(gBottomFb, 106, 154, line, rgb15(31, 31, 31));
    drawButtonText(gBottomFb, 152, 146, 24, 20, "+");
    drawButtonText(gBottomFb, 180, 146, 60, 20, "RANDOM");

    drawButtonText(gBottomFb, 20, 168, 88, 16, "BACK");
    drawButtonText(gBottomFb, 148, 168, 88, 16, "CREATE");

    gLastMenuLogoX = 0;
}

void renderLoadingScreen(int progress, int animTick) {
    beginMenuScreen(MENU_CACHE_LOADING);

    std::memcpy(gTopFb, gMenuTopBase, 192 * 256 * sizeof(u16));
    std::memcpy(gBottomFb, gMenuBottomBase, 192 * 256 * sizeof(u16));
    blitImage(gBottomFb, (256 - LABEL_LOADING_W) / 2, 48, LABEL_LOADING_W, LABEL_LOADING_H, LABEL_LOADING, LABEL_LOADING_W, LABEL_LOADING_H, true);

    const int barX = 28;
    const int barY = 98;
    const int barW = 200;
    const int barH = 20;
    drawRect(gBottomFb, barX, barY, barW, barH, rgb15(5, 5, 7));
    drawRect(gBottomFb, barX + 2, barY + 2, barW - 4, barH - 4, rgb15(1, 1, 2));
    drawAnimatedMenuLogo(animTick, 156, 0, 16);

    if (progress != gLastLoadingProgress) {
        const int barX = 28;
        const int barY = 98;
        const int barW = 200;
        const int barH = 20;
        int fill = ((barW - 6) * progress) / 100;
        drawRect(gBottomFb, barX + 3, barY + 3, fill, barH - 6, rgb15(6, 24, 10));
        gLastLoadingProgress = progress;
    }
}

void renderPauseMenu() {
    beginMenuScreen(MENU_CACHE_PAUSE);

    if (gLastMenuLogoX != -1) return;

    std::memcpy(gTopFb, gMenuTopBase, 192 * 256 * sizeof(u16));
    std::memcpy(gBottomFb, gMenuBottomBase, 192 * 256 * sizeof(u16));
    blitImage(gBottomFb, (256 - LABEL_PAUSED_W) / 2, 18, LABEL_PAUSED_W, LABEL_PAUSED_H, LABEL_PAUSED, LABEL_PAUSED_W, LABEL_PAUSED_H, true);
    const int bw = 176;
    const int bh = 34;
    const int bx = (256 - bw) / 2;
    drawButton(gBottomFb, bx, 52, bw, bh, PAUSE_RESUME, PAUSE_RESUME_W, PAUSE_RESUME_H);
    drawButton(gBottomFb, bx, 88, bw, bh, PAUSE_SAVE_GAME, PAUSE_SAVE_GAME_W, PAUSE_SAVE_GAME_H);
    drawButtonText(gBottomFb, bx, 124, bw, bh, "OPTIONS");
    drawButton(gBottomFb, bx, 160, bw, 24, PAUSE_QUIT_GAME, PAUSE_QUIT_GAME_W, PAUSE_QUIT_GAME_H);
    gLastMenuLogoX = 0;
}

int handleTitleMenuTouch(int x, int y) {
    const int bw = 176;
    const int bh = 34;
    const int bx = (256 - bw) / 2;
    if (buttonHit(x, y, bx, 46, bw, bh)) return MENU_ACTION_NEW_GAME;
    if (buttonHit(x, y, bx, 88, bw, bh)) return MENU_ACTION_LOAD_GAME;
    if (buttonHit(x, y, bx, 130, bw, bh)) return MENU_ACTION_OPTIONS;
    return MENU_ACTION_NONE;
}


int handleOptionsMenuTouch(int x, int y) {
    if (buttonHit(x, y, 144, 42, 34, 24) || (x >= 172 && x < 198 && y >= 42 && y < 66)) return MENU_ACTION_RENDER_DIST_PREV;
    if (buttonHit(x, y, 218, 42, 34, 24) || (x >= 198 && x < 224 && y >= 42 && y < 66)) return MENU_ACTION_RENDER_DIST_NEXT;
    if (buttonHit(x, y, 144, 74, 34, 24) || (x >= 172 && x < 198 && y >= 74 && y < 98)) return MENU_ACTION_LOOK_SPEED_PREV;
    if (buttonHit(x, y, 218, 74, 34, 24) || (x >= 198 && x < 224 && y >= 74 && y < 98)) return MENU_ACTION_LOOK_SPEED_NEXT;
    if (buttonHit(x, y, 144, 106, 34, 24) || (x >= 172 && x < 198 && y >= 106 && y < 130)) return MENU_ACTION_MOVE_SPEED_PREV;
    if (buttonHit(x, y, 218, 106, 34, 24) || (x >= 198 && x < 224 && y >= 106 && y < 130)) return MENU_ACTION_MOVE_SPEED_NEXT;
    if (buttonHit(x, y, 68, 138, 120, 24)) return MENU_ACTION_OPEN_GRAPHICS;
    if (buttonHit(x, y, 14, 164, 82, 20)) return MENU_ACTION_BACK;
    return MENU_ACTION_NONE;
}

int handleGraphicsMenuTouch(int x, int y) {
    if (buttonHit(x, y, 14, 148, 82, 28)) return MENU_ACTION_BACK;
    if (x >= 34 && x < 222 && y >= 78 && y < 100) {
        int slider = ((x - 34) * 100) / 187;
        if (slider < 0) slider = 0;
        if (slider > 100) slider = 100;
        int width = MIN_RENDER_W + ((MAX_RENDER_W - MIN_RENDER_W) * slider + 50) / 100;
        int height = MIN_RENDER_H + ((MAX_RENDER_H - MIN_RENDER_H) * slider + 50) / 100;
        setRenderResolution(width, height);
        return MENU_ACTION_NONE;
    }
    return MENU_ACTION_NONE;
}


int handleWorldSetupTouch(int x, int y) {
    if (buttonHit(x, y, 64, 36, 24, 20)) return MENU_ACTION_WORLD_TYPE_PREV;
    if (buttonHit(x, y, 188, 36, 24, 20)) return MENU_ACTION_WORLD_TYPE_NEXT;
    if (buttonHit(x, y, 64, 58, 24, 20)) return MENU_ACTION_WORLD_SIZE_PREV;
    if (buttonHit(x, y, 188, 58, 24, 20)) return MENU_ACTION_WORLD_SIZE_NEXT;
    if (buttonHit(x, y, 64, 80, 24, 20)) return MENU_ACTION_WORLD_MODE_PREV;
    if (buttonHit(x, y, 188, 80, 24, 20)) return MENU_ACTION_WORLD_MODE_NEXT;
    if (buttonHit(x, y, 92, 102, 72, 20)) return MENU_ACTION_WORLD_TREES_TOGGLE;
    if (buttonHit(x, y, 64, 124, 24, 20)) return MENU_ACTION_WORLD_SEED_PREV;
    if (buttonHit(x, y, 216, 124, 24, 20)) return MENU_ACTION_WORLD_SEED_NEXT;
    if (buttonHit(x, y, 64, 146, 24, 20)) return MENU_ACTION_WORLD_STEP_PREV;
    if (buttonHit(x, y, 152, 146, 24, 20)) return MENU_ACTION_WORLD_STEP_NEXT;
    if (buttonHit(x, y, 180, 146, 60, 20)) return MENU_ACTION_WORLD_RANDOMIZE;
    if (buttonHit(x, y, 20, 168, 88, 16)) return MENU_ACTION_BACK;
    if (buttonHit(x, y, 148, 168, 88, 16)) return MENU_ACTION_WORLD_CREATE;
    return MENU_ACTION_NONE;
}

int handlePauseMenuTouch(int x, int y) {
    const int bw = 176;
    const int bh = 34;
    const int bx = (256 - bw) / 2;
    if (buttonHit(x, y, bx, 52, bw, bh)) return MENU_ACTION_RESUME;
    if (buttonHit(x, y, bx, 88, bw, bh)) return MENU_ACTION_SAVE_GAME;
    if (buttonHit(x, y, bx, 124, bw, bh)) return MENU_ACTION_OPTIONS;
    if (buttonHit(x, y, bx, 160, bw, 24)) return MENU_ACTION_QUIT_TO_TITLE;
    return MENU_ACTION_NONE;
}


void renderCraftingMenu(const Player& p, int tab, int selectedIndex, bool nearTable) {
    if (gHudStaticValid) {
        std::memcpy(gBottomFb, &gHudStatic[0][0], 192 * 256 * sizeof(u16));
    }
    drawRect(gBottomFb, 8, 10, 240, 172, rgb15(4, 4, 6));
    drawRect(gBottomFb, 12, 14, 232, 164, rgb15(18, 18, 20));
    drawTextPixelScaled(gBottomFb, 20, 20, nearTable ? "CRAFT TABLE" : "CRAFTING", rgb15(31,31,31), 2);
    drawTextPixel(gBottomFb, 18, 36, nearTable ? "L R TABS  DPAD SELECT  A CRAFT  B BACK" : "BASICS ANYWHERE  TABLE RECIPES LOCKED", rgb15(29,29,29));

    for (int i = 0; i < getCraftingTabCount(); ++i) {
        int x = 18 + i * 74;
        drawRect(gBottomFb, x, 48, 68, 18, i == tab ? rgb15(26,26,28) : rgb15(9,9,11));
        drawTextPixel(gBottomFb, x + 8, 54, getCraftingTabName(i), rgb15(31,31,31));
    }

    const int count = getRecipeCountForTab(tab, nearTable);
    const int scroll = selectedIndex < 6 ? 0 : (selectedIndex - 5);
    for (int i = 0; i < 6; ++i) {
        const int recipeIndex = scroll + i;
        int y = 74 + i * 16;
        drawRect(gBottomFb, 18, y, 220, 14, recipeIndex == selectedIndex ? rgb15(24,24,26) : rgb15(10,10,12));
        const CraftingRecipe* recipe = getRecipeForTabIndex(tab, recipeIndex, nearTable);
        if (!recipe) continue;
        drawTextPixel(gBottomFb, 22, y + 4, recipe->name, rgb15(31,31,31));
        drawTextPixel(gBottomFb, 120, y + 4, canCraftRecipe(p, *recipe, nearTable) ? "READY" : "MISS", rgb15(31,31,31));
        drawTextPixel(gBottomFb, 170, y + 4, recipe->outputIsBlock ? blockName(recipe->outputId) : itemName(recipe->outputId), rgb15(29,29,29));
    }

    if (count <= 0) {
        drawTextPixel(gBottomFb, 56, 98, nearTable ? "NO RECIPES" : "PLACE OR STAND NEAR A CRAFT TABLE", rgb15(31,31,31));
    } else {
        const CraftingRecipe* recipe = getRecipeForTabIndex(tab, selectedIndex, nearTable);
        if (recipe) {
            drawRect(gBottomFb, 18, 172-34, 220, 28, rgb15(8,8,10));
            drawTextPixel(gBottomFb, 22, 142, recipe->tableOnly ? "TABLE ONLY" : "HAND CRAFT", rgb15(31,31,31));
            int cx = 100;
            for (int i = 0; i < recipe->ingredientCount; ++i) {
                const InventorySlot& ing = recipe->ingredients[i];
                const u16* tex = ing.isBlock ? textureForBlockFace(ing.id, 1, true) : textureForItem(ing.id);
                drawIconTexture(gBottomFb, cx + i * 32, 136, tex, false);
                char cnt[8];
                std::snprintf(cnt, sizeof(cnt), "%d", ing.count);
                drawTextPixel(gBottomFb, cx + i * 32 + 12, 158, cnt, rgb15(31,31,31));
            }
            drawTextPixel(gBottomFb, 22, 156, recipe->outputCount > 1 ? "OUTPUT XN" : "OUTPUT X1", rgb15(31,31,31));
        }
    }
}
