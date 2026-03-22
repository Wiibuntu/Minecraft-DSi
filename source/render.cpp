#include "render.h"
#include "texture_data.h"
#include "world.h"

#include <nds.h>
#include <cmath>
#include <cstdio>
#include <cstring>

static const int SCREEN_W = 256;
static const int SCREEN_H = 192;
static const int RENDER_W = 52;
static const int RENDER_H = 40;
static const float EYE_HEIGHT = 1.45f;
static const float FOV = 1.0f;
static const float MAX_RAY_DIST = 12.5f;
static const int MAX_RAY_STEPS = 54;

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
        r = (r * 13) / 10; g = (g * 13) / 10; b = (b * 13) / 10;
    } else if (face >= 2) {
        r = (r * 8) / 10; g = (g * 8) / 10; b = (b * 8) / 10;
    }

    if (highlighted) {
        r += 5; g += 5; b += 5;
    }

    r = clamp5(r - fog);
    g = clamp5(g - fog);
    b = clamp5(b - fog);
    return rgb15(r, g, b);
}

static const u16* textureForBlockFace(int block, int face, bool topFace) {
    switch (block) {
        case BLOCK_GRASS:
            return TEX_GRASS_TOP;
        case BLOCK_DIRT:
            return TEX_DIRT;
        case BLOCK_STONE:
            return TEX_STONE;
        case BLOCK_WOOD:
            return topFace ? TEX_WOOD_TOP : TEX_WOOD_SIDE;
        case BLOCK_LEAVES:
            return TEX_LEAVES;
        case BLOCK_SAND:
            return TEX_SAND;
        case BLOCK_WATER:
            return TEX_WATER;
        default:
            return TEX_STONE;
    }
}

static inline u16 sampleTexture(const u16* tex, float u, float v) {
    int tx = ((int)(u * TEX_SIZE)) & (TEX_SIZE - 1);
    int ty = ((int)(v * TEX_SIZE)) & (TEX_SIZE - 1);
    return tex[ty * TEX_SIZE + tx];
}

static void drawRect(u16* fb, int x, int y, int w, int h, u16 color) {
    if (w <= 0 || h <= 0) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > 256) w = 256 - x;
    if (y + h > 192) h = 192 - y;
    if (w <= 0 || h <= 0) return;
    for (int yy = y; yy < y + h; ++yy) {
        u16* row = fb + yy * 256;
        for (int xx = x; xx < x + w; ++xx) row[xx] = color;
    }
}

static void rebuildHudStatic() {
    u16* fb = &gHudStatic[0][0];
    for (int y = 0; y < 192; ++y) {
        u16* row = fb + y * 256;
        for (int x = 0; x < 256; ++x) row[x] = rgb15(3, 3, 5);
    }

    static const int slotBlocks[7] = {BLOCK_GRASS, BLOCK_DIRT, BLOCK_STONE, BLOCK_WOOD, BLOCK_LEAVES, BLOCK_SAND, BLOCK_WATER};
    for (int i = 0; i < 7; ++i) {
        int x = 10 + i * 34;
        int y = 146;
        drawRect(fb, x, y, 28, 28, rgb15(8, 8, 10));
        drawRect(fb, x + 2, y + 2, 24, 24, rgb15(2, 2, 3));
        const u16* tex = textureForBlockFace(slotBlocks[i], 1, true);
        for (int py = 0; py < 16; ++py) {
            u16* row = fb + (y + 6 + py) * 256 + (x + 6);
            int ty = (py * TEX_SIZE) >> 4;
            for (int px = 0; px < 16; ++px) {
                int tx = (px * TEX_SIZE) >> 4;
                row[px] = tex[ty * TEX_SIZE + tx];
            }
        }
    }

    const int mapX0 = 10;
    const int mapY0 = 12;
    const int cell = 4;
    for (int z = 0; z < WORLD_Z; ++z) {
        for (int x = 0; x < WORLD_X; ++x) {
            int topBlock = getTopVisibleBlock(x, z);
            u16 c = rgb15(1, 1, 1);
            if (topBlock != BLOCK_AIR) c = shadeColor(textureForBlockFace(topBlock, 1, true)[0], 1, false, 0);
            drawRect(fb, mapX0 + x * cell, mapY0 + z * cell, cell - 1, cell - 1, c);
        }
    }

    const int infoY = 118;
    drawRect(fb, 152, infoY, 92, 8, rgb15(6, 6, 8));
    drawRect(fb, 152, infoY + 12, 92, 8, rgb15(6, 6, 8));
    drawRect(fb, 152, infoY + 24, 92, 8, rgb15(6, 6, 8));

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

    gHudStaticValid = false;
    gHudFrameValid = false;
    gHudRevision = -1;
    gFrameCounter = 0;
    gHudFrameCounter = 0;
    gLastWorldRevision3D = -1;
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
    if (!shouldRedraw3D(p)) {
        return;
    }

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

    for (int y = 0; y < SCREEN_H; ++y) {
        gTopFb[y * 256 + SCREEN_W / 2] = rgb15(31, 31, 31);
    }
    u16* midRow = gTopFb + (SCREEN_H / 2) * 256;
    for (int x = SCREEN_W / 2 - 3; x <= SCREEN_W / 2 + 3; ++x) {
        midRow[x] = rgb15(31, 31, 31);
    }

    gLastYaw = p.yaw;
    gLastPitch = p.pitch;
    gLastX = p.x;
    gLastY = p.y;
    gLastZ = p.z;
    gLastWorldRevision3D = getWorldRevision();
}

void renderHUD(const Player& p, const RayHit& hit) {
    if (!gHudStaticValid || gHudRevision != getWorldRevision()) {
        rebuildHudStatic();
    }

    ++gHudFrameCounter;
    if (gHudFrameValid && (gHudFrameCounter & 1)) {
        std::memcpy(gBottomFb, gHudFrame, 192 * 256 * sizeof(u16));
        return;
    }

    std::memcpy(gBottomFb, gHudStatic, 192 * 256 * sizeof(u16));

    static const int slotBlocks[7] = {BLOCK_GRASS, BLOCK_DIRT, BLOCK_STONE, BLOCK_WOOD, BLOCK_LEAVES, BLOCK_SAND, BLOCK_WATER};
    for (int i = 0; i < 7; ++i) {
        int x = 10 + i * 34;
        int y = 146;
        if (slotBlocks[i] == p.selectedBlock) {
            drawRect(gBottomFb, x - 2, y - 2, 32, 2, rgb15(31, 31, 31));
            drawRect(gBottomFb, x - 2, y + 28, 32, 2, rgb15(31, 31, 31));
            drawRect(gBottomFb, x - 2, y - 2, 2, 32, rgb15(31, 31, 31));
            drawRect(gBottomFb, x + 28, y - 2, 2, 32, rgb15(31, 31, 31));
        }
    }

    const int mapX0 = 10;
    const int mapY0 = 12;
    const int cell = 4;
    drawRect(gBottomFb, mapX0 + (int)(p.x * cell) - 1, mapY0 + (int)(p.z * cell) - 1, 3, 3, rgb15(31, 0, 0));

    const int infoY = 118;
    int yawBar = ((int)(p.yaw * 40.0f) % 92 + 92) % 92;
    int pitchBar = (int)((p.pitch + 0.8f) / 1.6f * 90.0f);
    int distBar = hit.hit ? (int)(92.0f - hit.dist * 14.0f) : 0;
    if (pitchBar < 0) pitchBar = 0;
    if (pitchBar > 90) pitchBar = 90;
    if (distBar < 0) distBar = 0;
    if (distBar > 92) distBar = 92;

    drawRect(gBottomFb, 152 + yawBar, infoY, 2, 8, rgb15(31, 18, 0));
    drawRect(gBottomFb, 152 + pitchBar, infoY + 12, 2, 8, rgb15(0, 24, 31));
    drawRect(gBottomFb, 152, infoY + 24, distBar, 8, hit.hit ? rgb15(0, 31, 0) : rgb15(12, 0, 0));

    std::memcpy(gHudFrame, gBottomFb, 192 * 256 * sizeof(u16));
    gHudFrameValid = true;
}
