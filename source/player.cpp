#include "player.h"
#include "world.h"

#include <nds.h>
#include <cmath>

static const float kPlayerRadius = 0.28f;
static const float kPlayerHeight = 1.65f;
static const float kMoveSpeed = 0.09f;
static const float kGravity = 0.030f;
static const float kJumpSpeed = 0.22f;
static const float kYawStep = 0.055f;
static const float kPitchStep = 0.030f;
static const float kWorldBorderMinX = kPlayerRadius + 0.05f;
static const float kWorldBorderMaxX = (float)WORLD_X - kPlayerRadius - 0.05f;
static const float kWorldBorderMinZ = kPlayerRadius + 0.05f;
static const float kWorldBorderMaxZ = (float)WORLD_Z - kPlayerRadius - 0.05f;

static inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static bool collidesAt(float x, float y, float z) {
    int minX = (int)std::floor(x - kPlayerRadius);
    int maxX = (int)std::floor(x + kPlayerRadius);
    int minY = (int)std::floor(y);
    int maxY = (int)std::floor(y + kPlayerHeight);
    int minZ = (int)std::floor(z - kPlayerRadius);
    int maxZ = (int)std::floor(z + kPlayerRadius);

    for (int bx = minX; bx <= maxX; ++bx)
        for (int by = minY; by <= maxY; ++by)
            for (int bz = minZ; bz <= maxZ; ++bz)
                if (isSolidBlock(bx, by, bz)) return true;
    return false;
}

void initPlayer(Player& p) {
    getSpawnPosition(p.x, p.y, p.z);
    p.yaw = 0.0f;
    p.pitch = -0.08f;
    p.vy = 0.0f;
    p.onGround = false;
    p.selectedBlock = BLOCK_GRASS;

    while (collidesAt(p.x, p.y, p.z) && p.y < WORLD_Y - 2) p.y += 1.0f;
}

void updatePlayer(Player& p, int held, int down) {
    if (held & KEY_Y) p.yaw -= kYawStep;
    if (held & KEY_A) p.yaw += kYawStep;

    if (held & KEY_B) p.pitch -= kPitchStep;
    if (held & KEY_X) p.pitch += kPitchStep;
    if (p.pitch < -0.8f) p.pitch = -0.8f;
    if (p.pitch > 0.8f) p.pitch = 0.8f;

    float forwardX = std::sinf(p.yaw);
    float forwardZ = std::cosf(p.yaw);
    float rightX = std::cosf(p.yaw);
    float rightZ = -std::sinf(p.yaw);

    float moveX = 0.0f;
    float moveZ = 0.0f;

    if (held & KEY_UP) {
        moveX += forwardX * kMoveSpeed;
        moveZ += forwardZ * kMoveSpeed;
    }
    if (held & KEY_DOWN) {
        moveX -= forwardX * kMoveSpeed;
        moveZ -= forwardZ * kMoveSpeed;
    }
    if (held & KEY_LEFT) {
        moveX -= rightX * kMoveSpeed;
        moveZ -= rightZ * kMoveSpeed;
    }
    if (held & KEY_RIGHT) {
        moveX += rightX * kMoveSpeed;
        moveZ += rightZ * kMoveSpeed;
    }

    float nx = clampf(p.x + moveX, kWorldBorderMinX, kWorldBorderMaxX);
    float nz = clampf(p.z + moveZ, kWorldBorderMinZ, kWorldBorderMaxZ);
    if (!collidesAt(nx, p.y, p.z)) p.x = nx;
    if (!collidesAt(p.x, p.y, nz)) p.z = nz;

    p.vy -= kGravity;
    if ((down & KEY_SELECT) && p.onGround) p.vy = kJumpSpeed;
    p.onGround = false;

    float ny = p.y + p.vy;
    if (!collidesAt(p.x, ny, p.z)) {
        p.y = ny;
    } else {
        if (p.vy < 0.0f) p.onGround = true;
        p.vy = 0.0f;
        if (p.onGround) {
            while (collidesAt(p.x, p.y, p.z) && p.y < WORLD_Y - 1) p.y += 0.05f;
        }
    }

    if (p.y < 1.0f) p.y = 1.0f;
    if (p.y > WORLD_Y - 2.0f) p.y = WORLD_Y - 2.0f;
}
