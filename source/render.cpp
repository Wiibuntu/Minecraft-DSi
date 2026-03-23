#include "render.h"
#include "texture_data.h"
#include "menu_assets.h"
#include "world.h"

#include <nds.h>
#include <cmath>
#include <cstring>

static const int SCREEN_W = 256;
static const int SCREEN_H = 192;
static const int RENDER_W = 52;
static const int RENDER_H = 40;
static const float EYE_HEIGHT = 1.45f;
static const float FOV = 1.0f;
static const float MAX_RAY_DIST = 12.5f;
static const int MAX_RAY_STEPS = 54;

static const int kPlaceableBlocks[] = {
    BLOCK_GRASS,
    BLOCK_DIRT,
    BLOCK_STONE,
    BLOCK_WOOD,
    BLOCK_LEAVES,
    BLOCK_SAND,
    BLOCK_WATER,
    BLOCK_COBBLE,
    BLOCK_PLANKS,
    BLOCK_BRICK,
    BLOCK_GLASS
};
static const int kPlaceableBlockCount = sizeof(kPlaceableBlocks) / sizeof(kPlaceableBlocks[0]);

static bool gMapVisible = false;
static int gTopBg = -1;
static int gBottomBg = -1;
static u16* gTopFb = nullptr;
static u16* gBottomFb = nullptr;
static u16 gLowRes[RENDER_H][RENDER_W];
static float gNx[RENDER_W];
static float gNy[RENDER_H];
static float gInvLen[RENDER_H][RENDER_W];
static int gXMap[SCREEN_W];
static int gYMap[SCREEN_H];
static u16 gSkyColor[RENDER_H];
static u16 gGroundColor[RENDER_H];
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

static u16 gMenuTopBase[192][256];
static u16 gMenuBottomBase[192][256];

enum MenuCacheScreen {
    MENU_CACHE_NONE = 0,
    MENU_CACHE_TITLE,
    MENU_CACHE_LOADING,
    MENU_CACHE_PAUSE
};

static MenuCacheScreen gMenuCacheScreen = MENU_CACHE_NONE;
static int gLastMenuLogoX = -1;
static int gLastMenuLogoY = -1;
static int gLastMenuLogoW = 0;
static int gLastMenuLogoH = 0;
static int gLastLoadingProgress = -1;

static inline u16 rgb15(int r, int g, int b) {
    return RGB15(r, g, b) | BIT(15);
}

static inline int clamp5(int v) {
    return v < 0 ? 0 : (v > 31 ? 31 : v);
}

static inline u16 shadeColor(u16 color, int face, bool highlighted, int fog) {
    int r = color & 31;
    int g = (color >> 5) & 31;
    int b = (color >> 10) & 31;

    if (face == 1) {
        r = (r * 13) / 10;
        g = (g * 13) / 10;
        b = (b * 13) / 10;
    } else if (face >= 2) {
        r = (r * 8) / 10;
        g = (g * 8) / 10;
        b = (b * 8) / 10;
    }

    if (highlighted) {
        r += 5;
        g += 5;
        b += 5;
    }

    r = clamp5(r - fog);
    g = clamp5(g - fog);
    b = clamp5(b - fog);
    return rgb15(r, g, b);
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
        default: return TEX_STONE;
    }
}

static inline u16 sampleTexture(const u16* tex, float u, float v) {
    int tx = ((int)(u * TEX_SIZE)) & (TEX_SIZE - 1);
    int ty = ((int)(v * TEX_SIZE)) & (TEX_SIZE - 1);
    return tex[ty * TEX_SIZE + tx];
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
        {19,21,25,17,0}       // Z
    };

    auto glyphIndex = [](char c) -> int {
        if (c == ' ') return 0;
        if (c >= 'A' && c <= 'Z') return 1 + (c - 'A');
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

static void drawBlockIcon(u16* fb, int x, int y, int block, bool selected) {
    drawRect(fb, x, y, 28, 28, rgb15(8, 8, 10));
    drawRect(fb, x + 2, y + 2, 24, 24, rgb15(2, 2, 3));
    if (selected) {
        drawRect(fb, x - 2, y - 2, 32, 2, rgb15(31, 31, 31));
        drawRect(fb, x - 2, y + 28, 32, 2, rgb15(31, 31, 31));
        drawRect(fb, x - 2, y - 2, 2, 32, rgb15(31, 31, 31));
        drawRect(fb, x + 28, y - 2, 2, 32, rgb15(31, 31, 31));
    }
    const u16* tex = textureForBlockFace(block, 1, true);
    for (int py = 0; py < 16; ++py) {
        u16* row = fb + (y + 6 + py) * 256 + (x + 6);
        int ty = (py * TEX_SIZE) >> 4;
        for (int px = 0; px < 16; ++px) {
            int tx = (px * TEX_SIZE) >> 4;
            row[px] = tex[ty * TEX_SIZE + tx];
        }
    }
}

static void drawMapPanel(u16* fb, int playerX, int playerZ) {
    const int panelX = 18;
    const int panelY = 16;
    const int panelW = 148;
    const int panelH = 148;
    const int mapX0 = panelX + 10;
    const int mapY0 = panelY + 10;
    const int cell = 4;

    drawRect(fb, panelX, panelY, panelW, panelH, rgb15(5, 6, 8));
    drawRect(fb, panelX + 2, panelY + 2, panelW - 4, panelH - 4, rgb15(1, 2, 3));
    drawTextPixel(fb, panelX + 10, panelY + 136, "MAP", rgb15(31, 31, 31));

    for (int z = 0; z < WORLD_Z; ++z) {
        for (int x = 0; x < WORLD_X; ++x) {
            int topBlock = getTopVisibleBlock(x, z);
            u16 c = rgb15(1, 1, 1);
            if (topBlock != BLOCK_AIR) {
                c = shadeColor(textureForBlockFace(topBlock, 1, true)[0], 1, false, 0);
            }
            drawRect(fb, mapX0 + x * cell, mapY0 + z * cell, cell - 1, cell - 1, c);
        }
    }

    drawRect(fb, mapX0 + playerX * cell - 1, mapY0 + playerZ * cell - 1, 3, 3, rgb15(31, 0, 0));
}

static void rebuildHudStatic() {
    u16* fb = &gHudStatic[0][0];
    for (int y = 0; y < 192; ++y) {
        u16* row = fb + y * 256;
        for (int x = 0; x < 256; ++x) row[x] = rgb15(3, 3, 5);
    }

    drawRect(fb, 196, 8, 50, 18, rgb15(8, 8, 12));
    drawRect(fb, 198, 10, 46, 14, rgb15(2, 2, 3));
    drawTextPixel(fb, 206, 15, "MAP", rgb15(31, 31, 31));

    const int infoY = 12;
    drawRect(fb, 10, infoY, 92, 8, rgb15(6, 6, 8));
    drawRect(fb, 10, infoY + 12, 92, 8, rgb15(6, 6, 8));
    drawRect(fb, 10, infoY + 24, 92, 8, rgb15(6, 6, 8));

    int startX = 16;
    int startY = 62;
    int gapX = 38;
    int gapY = 36;
    for (int i = 0; i < kPlaceableBlockCount; ++i) {
        int row = i / 6;
        int col = i % 6;
        int x = startX + col * gapX;
        int y = startY + row * gapY;
        drawBlockIcon(fb, x, y, kPlaceableBlocks[i], false);
    }

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

    const float aspect = (float)RENDER_W / (float)RENDER_H;
    for (int x = 0; x < RENDER_W; ++x) {
        float nx = ((x + 0.5f) / (float)RENDER_W) * 2.0f - 1.0f;
        gNx[x] = nx * aspect * FOV;
    }
    for (int y = 0; y < RENDER_H; ++y) {
        float ny = ((y + 0.5f) / (float)RENDER_H) * 2.0f - 1.0f;
        gNy[y] = ny * FOV;
    }
    for (int y = 0; y < RENDER_H; ++y) {
        const float ny = gNy[y];
        for (int x = 0; x < RENDER_W; ++x) {
            const float nx = gNx[x];
            gInvLen[y][x] = 1.0f / sqrtf(nx * nx + ny * ny + 1.0f);
        }
    }
    for (int x = 0; x < SCREEN_W; ++x) gXMap[x] = (x * RENDER_W) / SCREEN_W;
    for (int y = 0; y < SCREEN_H; ++y) gYMap[y] = (y * RENDER_H) / SCREEN_H;

    for (int py = 0; py < RENDER_H; ++py) {
        if (py < RENDER_H / 2) {
            int band = 10 + py / 4;
            gSkyColor[py] = rgb15(4 + band / 4, 10 + band / 3, 18 + band / 2);
        } else {
            int band = py - RENDER_H / 2;
            gGroundColor[py] = rgb15(4 + band / 4, 8 + band / 5, 4 + band / 6);
        }
    }

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
}

static RayHit castRayInternal(const Player& p, float dirX, float dirY, float dirZ, float maxDist) {
    RayHit hit{};
    hit.hit = false;
    hit.dist = maxDist;
    hit.placeX = (int)std::floor(p.x);
    hit.placeY = (int)std::floor(p.y + EYE_HEIGHT);
    hit.placeZ = (int)std::floor(p.z);

    float ox = p.x;
    float oy = p.y + EYE_HEIGHT;
    float oz = p.z;

    int mapX = (int)std::floor(ox);
    int mapY = (int)std::floor(oy);
    int mapZ = (int)std::floor(oz);

    float invX = dirX == 0.0f ? 1e9f : std::fabs(1.0f / dirX);
    float invY = dirY == 0.0f ? 1e9f : std::fabs(1.0f / dirY);
    float invZ = dirZ == 0.0f ? 1e9f : std::fabs(1.0f / dirZ);

    int stepX = dirX < 0.0f ? -1 : 1;
    int stepY = dirY < 0.0f ? -1 : 1;
    int stepZ = dirZ < 0.0f ? -1 : 1;

    float sideX = (dirX < 0.0f) ? (ox - mapX) * invX : (mapX + 1.0f - ox) * invX;
    float sideY = (dirY < 0.0f) ? (oy - mapY) * invY : (mapY + 1.0f - oy) * invY;
    float sideZ = (dirZ < 0.0f) ? (oz - mapZ) * invZ : (mapZ + 1.0f - oz) * invZ;

    int lastX = mapX;
    int lastY = mapY;
    int lastZ = mapZ;
    int face = 0;

    for (int i = 0; i < MAX_RAY_STEPS; ++i) {
        float dist;
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
        int block = getBlock(mapX, mapY, mapZ);
        if (block != BLOCK_AIR) {
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
    }

    return hit;
}

RayHit castCenterRay(const Player& p, float maxDist) {
    float cp = std::cosf(p.pitch);
    float dirX = std::sinf(p.yaw) * cp;
    float dirY = std::sinf(p.pitch);
    float dirZ = std::cosf(p.yaw) * cp;
    return castRayInternal(p, dirX, dirY, dirZ, maxDist);
}

HudTouchAction handleHudTouch(int x, int y) {
    HudTouchAction action{HUD_TOUCH_NONE, 0};

    if (x >= 196 && x < 246 && y >= 8 && y < 26) {
        gMapVisible = !gMapVisible;
        gHudFrameValid = false;
        action.type = HUD_TOUCH_TOGGLE_MAP;
        return action;
    }

    int startX = 16;
    int startY = 62;
    int gapX = 38;
    int gapY = 36;
    for (int i = 0; i < kPlaceableBlockCount; ++i) {
        int row = i / 6;
        int col = i % 6;
        int sx = startX + col * gapX;
        int sy = startY + row * gapY;
        if (x >= sx && x < sx + 28 && y >= sy && y < sy + 28) {
            action.type = HUD_TOUCH_SELECT_BLOCK;
            action.value = kPlaceableBlocks[i];
            return action;
        }
    }

    return action;
}

static bool shouldRedraw3D(const Player& p) {
    int rev = getWorldRevision();
    bool worldChanged = (rev != gLastWorldRevision3D);
    float dyaw = fabsf(p.yaw - gLastYaw);
    float dpitch = fabsf(p.pitch - gLastPitch);
    float dpos = fabsf(p.x - gLastX) + fabsf(p.y - gLastY) + fabsf(p.z - gLastZ);
    bool moved = dpos > 0.03f;
    bool turned = dyaw > 0.001f || dpitch > 0.001f;
    if (worldChanged || moved || turned) return true;
    return ((gFrameCounter & 1) == 0);
}

void renderFrame(const Player& p) {
    ++gFrameCounter;
    if (!shouldRedraw3D(p)) return;

    RayHit center = castCenterRay(p, 6.0f);

    const float cp = std::cosf(p.pitch);
    const float sp = std::sinf(p.pitch);
    const float cy = std::cosf(p.yaw);
    const float sy = std::sinf(p.yaw);
    float dirXCol[RENDER_W];
    float dirZCol[RENDER_W];
    for (int px = 0; px < RENDER_W; ++px) {
        const float nx = gNx[px];
        dirXCol[px] = sy * cp + cy * nx;
        dirZCol[px] = cy * cp - sy * nx;
    }

    const float eyeY = p.y + EYE_HEIGHT;
    for (int py = 0; py < RENDER_H; ++py) {
        const float ny = gNy[py];
        const float dirYBase = sp - ny;
        u16* dstRow = gLowRes[py];
        for (int px = 0; px < RENDER_W; ++px) {
            const float invLen = gInvLen[py][px];
            const float dirX = dirXCol[px] * invLen;
            const float dirY = dirYBase * invLen;
            const float dirZ = dirZCol[px] * invLen;

            RayHit hit = castRayInternal(p, dirX, dirY, dirZ, MAX_RAY_DIST);
            u16 color;
            if (hit.hit) {
                bool hl = center.hit && hit.x == center.x && hit.y == center.y && hit.z == center.z;
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
                color = sampleTexture(tex, u, v);
                int fog = (int)(hit.dist * 2.0f);
                color = shadeColor(color, hit.face, hl, fog);
                if (hit.block == BLOCK_GLASS) {
                    color = shadeColor(rgb15(22, 24, 26), hit.face, hl, fog / 2);
                }
            } else {
                color = (py < RENDER_H / 2) ? gSkyColor[py] : gGroundColor[py];
            }
            dstRow[px] = color;
        }
    }

    for (int y = 0; y < SCREEN_H; ++y) {
        const u16* src = gLowRes[gYMap[y]];
        u16* dst = gTopFb + y * 256;
        for (int x = 0; x < SCREEN_W; ++x) dst[x] = src[gXMap[x]];
    }

    for (int y = 0; y < SCREEN_H; ++y) gTopFb[y * 256 + SCREEN_W / 2] = rgb15(31, 31, 31);
    u16* midRow = gTopFb + (SCREEN_H / 2) * 256;
    for (int x = SCREEN_W / 2 - 3; x <= SCREEN_W / 2 + 3; ++x) midRow[x] = rgb15(31, 31, 31);

    gLastYaw = p.yaw;
    gLastPitch = p.pitch;
    gLastX = p.x;
    gLastY = p.y;
    gLastZ = p.z;
    gLastWorldRevision3D = getWorldRevision();
}

void renderHUD(const Player& p, const RayHit& hit) {
    if (!gHudStaticValid || gHudRevision != getWorldRevision()) rebuildHudStatic();

    ++gHudFrameCounter;
    if (gHudFrameValid && (gHudFrameCounter & 1)) {
        std::memcpy(gBottomFb, gHudFrame, 192 * 256 * sizeof(u16));
        return;
    }

    std::memcpy(gBottomFb, gHudStatic, 192 * 256 * sizeof(u16));

    int infoY = 12;
    int yawBar = ((int)(p.yaw * 40.0f) % 92 + 92) % 92;
    int pitchBar = (int)((p.pitch + 0.8f) / 1.6f * 90.0f);
    int distBar = hit.hit ? (int)(92.0f - hit.dist * 14.0f) : 0;
    if (pitchBar < 0) pitchBar = 0;
    if (pitchBar > 90) pitchBar = 90;
    if (distBar < 0) distBar = 0;
    if (distBar > 92) distBar = 92;

    drawRect(gBottomFb, 10 + yawBar, infoY, 2, 8, rgb15(31, 18, 0));
    drawRect(gBottomFb, 10 + pitchBar, infoY + 12, 2, 8, rgb15(0, 24, 31));
    drawRect(gBottomFb, 10, infoY + 24, distBar, 8, hit.hit ? rgb15(0, 31, 0) : rgb15(12, 0, 0));

    int startX = 16;
    int startY = 62;
    int gapX = 38;
    int gapY = 36;
    for (int i = 0; i < kPlaceableBlockCount; ++i) {
        int row = i / 6;
        int col = i % 6;
        int x = startX + col * gapX;
        int y = startY + row * gapY;
        drawBlockIcon(gBottomFb, x, y, kPlaceableBlocks[i], kPlaceableBlocks[i] == p.selectedBlock);
    }

    if (gMapVisible) {
        drawMapPanel(gBottomFb, (int)p.x, (int)p.z);
        drawRect(gBottomFb, 196, 8, 50, 18, rgb15(12, 7, 7));
        drawRect(gBottomFb, 198, 10, 46, 14, rgb15(4, 1, 1));
        drawTextPixel(gBottomFb, 208, 15, "MAP", rgb15(31, 24, 24));
    }

    std::memcpy(gHudFrame, gBottomFb, 192 * 256 * sizeof(u16));
    gHudFrameValid = true;
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
    for (int y = 0; y < 192; ++y) {
        std::memcpy(gMenuTopBase[y], MENU_BG + y * 256, 256 * sizeof(u16));
        std::memcpy(gMenuBottomBase[y], MENU_BG + y * 256, 256 * sizeof(u16));
    }
}

static void drawAnimatedMenuLogo(int animTick, int baseW, int amplitude, int baseY) {
    int pulse = ((animTick / 2) % 32);
    if (pulse > 16) pulse = 32 - pulse;
    int logoW = baseW + pulse * amplitude;
    int logoH = (MENU_LOGO_H * logoW) / MENU_LOGO_W;
    int logoX = (256 - logoW) / 2;
    int logoY = baseY + pulse / 2;

    if (gLastMenuLogoW > 0 && gLastMenuLogoH > 0) {
        restoreRectFromBase(gTopFb, &gMenuTopBase[0][0], gLastMenuLogoX - 2, gLastMenuLogoY - 2, gLastMenuLogoW + 4, gLastMenuLogoH + 4);
    }

    blitImage(gTopFb, logoX, logoY, logoW, logoH, MENU_LOGO, MENU_LOGO_W, MENU_LOGO_H, true);
    gLastMenuLogoX = logoX;
    gLastMenuLogoY = logoY;
    gLastMenuLogoW = logoW;
    gLastMenuLogoH = logoH;
}

void renderTitleMenu(int animTick) {
    beginMenuScreen(MENU_CACHE_TITLE);

    if (gLastMenuLogoW <= 0) {
        std::memcpy(gTopFb, gMenuTopBase, 192 * 256 * sizeof(u16));
        std::memcpy(gBottomFb, gMenuBottomBase, 192 * 256 * sizeof(u16));

        const int bw = 176;
        const int bh = 42;
        const int bx = (256 - bw) / 2;
        drawButton(gBottomFb, bx, 62, bw, bh, TITLE_NEW_GAME, TITLE_NEW_GAME_W, TITLE_NEW_GAME_H);
        drawButton(gBottomFb, bx, 118, bw, bh, TITLE_LOAD_GAME, TITLE_LOAD_GAME_W, TITLE_LOAD_GAME_H);
    }

    drawAnimatedMenuLogo(animTick, 180, 2, 18);
}

void renderLoadingScreen(int progress, int animTick) {
    beginMenuScreen(MENU_CACHE_LOADING);

    if (gLastMenuLogoW <= 0) {
        std::memcpy(gTopFb, gMenuTopBase, 192 * 256 * sizeof(u16));
        std::memcpy(gBottomFb, gMenuBottomBase, 192 * 256 * sizeof(u16));
        blitImage(gBottomFb, (256 - LABEL_LOADING_W) / 2, 48, LABEL_LOADING_W, LABEL_LOADING_H, LABEL_LOADING, LABEL_LOADING_W, LABEL_LOADING_H, true);

        const int barX = 28;
        const int barY = 98;
        const int barW = 200;
        const int barH = 20;
        drawRect(gBottomFb, barX, barY, barW, barH, rgb15(5, 5, 7));
        drawRect(gBottomFb, barX + 2, barY + 2, barW - 4, barH - 4, rgb15(1, 1, 2));
    }

    drawAnimatedMenuLogo(animTick, 156, 1, 16);

    if (progress != gLastLoadingProgress) {
        const int barX = 28;
        const int barY = 98;
        const int barW = 200;
        const int barH = 20;
        restoreRectFromBase(gBottomFb, &gMenuBottomBase[0][0], barX + 3, barY + 3, barW - 6, barH - 6);
        int fill = ((barW - 6) * progress) / 100;
        drawRect(gBottomFb, barX + 3, barY + 3, fill, barH - 6, rgb15(6, 24, 10));
        gLastLoadingProgress = progress;
    }
}

void renderPauseMenu() {
    beginMenuScreen(MENU_CACHE_PAUSE);

    if (gMenuCacheScreen == MENU_CACHE_PAUSE && gLastMenuLogoX == -1) {
        std::memcpy(gBottomFb, gMenuBottomBase, 192 * 256 * sizeof(u16));
        blitImage(gBottomFb, (256 - LABEL_PAUSED_W) / 2, 18, LABEL_PAUSED_W, LABEL_PAUSED_H, LABEL_PAUSED, LABEL_PAUSED_W, LABEL_PAUSED_H, true);
        const int bw = 176;
        const int bh = 34;
        const int bx = (256 - bw) / 2;
        drawButton(gBottomFb, bx, 52, bw, bh, PAUSE_RESUME, PAUSE_RESUME_W, PAUSE_RESUME_H);
        drawButton(gBottomFb, bx, 94, bw, bh, PAUSE_SAVE_GAME, PAUSE_SAVE_GAME_W, PAUSE_SAVE_GAME_H);
        drawButton(gBottomFb, bx, 136, bw, bh, PAUSE_QUIT_GAME, PAUSE_QUIT_GAME_W, PAUSE_QUIT_GAME_H);
        gLastMenuLogoX = 0;
    }
}

int handleTitleMenuTouch(int x, int y) {
    const int bw = 176;
    const int bh = 42;
    const int bx = (256 - bw) / 2;
    if (buttonHit(x, y, bx, 62, bw, bh)) return MENU_ACTION_NEW_GAME;
    if (buttonHit(x, y, bx, 118, bw, bh)) return MENU_ACTION_LOAD_GAME;
    return MENU_ACTION_NONE;
}

int handlePauseMenuTouch(int x, int y) {
    const int bw = 176;
    const int bh = 34;
    const int bx = (256 - bw) / 2;
    if (buttonHit(x, y, bx, 52, bw, bh)) return MENU_ACTION_RESUME;
    if (buttonHit(x, y, bx, 94, bw, bh)) return MENU_ACTION_SAVE_GAME;
    if (buttonHit(x, y, bx, 136, bw, bh)) return MENU_ACTION_QUIT_TO_TITLE;
    return MENU_ACTION_NONE;
}
