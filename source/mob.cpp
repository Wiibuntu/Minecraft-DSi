#include "mob.h"
#include "world.h"

#include <cmath>
#include <cstring>

static const int MAX_MOBS = 6;
static const float kPigWidth = 0.62f;
static const float kPigHeight = 1.00f;
static const float kPigLength = 0.95f;
static const float kEyeHeight = 1.45f;
static Mob gMobs[MAX_MOBS];

static inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

float getMobWidth(const Mob& mob) {
    switch (mob.type) {
        case MOB_PIG:
        default: return kPigWidth;
    }
}

float getMobHeight(const Mob& mob) {
    switch (mob.type) {
        case MOB_PIG:
        default: return kPigHeight;
    }
}

float getMobLength(const Mob& mob) {
    switch (mob.type) {
        case MOB_PIG:
        default: return kPigLength;
    }
}

static bool mobCollidesAt(float x, float y, float z, float width, float height, float length) {
    const float rx = width * 0.5f;
    const float rz = length * 0.5f;
    int minX = (int)std::floor(x - rx);
    int maxX = (int)std::floor(x + rx);
    int minY = (int)std::floor(y);
    int maxY = (int)std::floor(y + height);
    int minZ = (int)std::floor(z - rz);
    int maxZ = (int)std::floor(z + rz);
    for (int bx = minX; bx <= maxX; ++bx)
        for (int by = minY; by <= maxY; ++by)
            for (int bz = minZ; bz <= maxZ; ++bz)
                if (isSolidBlock(bx, by, bz)) return true;
    return false;
}

static void chooseNewWander(Mob& m) {
    u32 seed = (u32)((int)(m.x * 97.0f) * 1103515245u + (int)(m.z * 57.0f) * 12345u + (u32)m.animTick * 1664525u);
    float ang = (float)(seed & 1023u) * (6.2831853f / 1024.0f);
    m.yaw = ang;
    m.wanderTicks = 40 + (int)(seed % 120u);
}

static bool rayAabb(float ox, float oy, float oz, float dx, float dy, float dz,
                    float minX, float minY, float minZ,
                    float maxX, float maxY, float maxZ,
                    float maxDist, float& outT) {
    float tmin = 0.0f;
    float tmax = maxDist;
    const float* o[3] = {&ox, &oy, &oz};
    const float* d[3] = {&dx, &dy, &dz};
    const float mins[3] = {minX, minY, minZ};
    const float maxs[3] = {maxX, maxY, maxZ};
    for (int i = 0; i < 3; ++i) {
        if (std::fabs(*d[i]) < 1e-5f) {
            if (*o[i] < mins[i] || *o[i] > maxs[i]) return false;
            continue;
        }
        float inv = 1.0f / *d[i];
        float t1 = (mins[i] - *o[i]) * inv;
        float t2 = (maxs[i] - *o[i]) * inv;
        if (t1 > t2) {
            float tmp = t1; t1 = t2; t2 = tmp;
        }
        if (t1 > tmin) tmin = t1;
        if (t2 < tmax) tmax = t2;
        if (tmin > tmax) return false;
    }
    outT = tmin;
    return tmin <= maxDist;
}

void initMobs() {
    std::memset(gMobs, 0, sizeof(gMobs));
}

void spawnWorldMobs() {
    std::memset(gMobs, 0, sizeof(gMobs));
    int spawned = 0;
    for (int pass = 0; pass < 64 && spawned < 3; ++pass) {
        int x = 3 + ((pass * 11 + 7) % (WORLD_X - 6));
        int z = 3 + ((pass * 17 + 5) % (WORLD_Z - 6));
        int topY = -1;
        int topBlock = BLOCK_AIR;
        for (int y = WORLD_Y - 2; y >= 0; --y) {
            int b = getBlock(x, y, z);
            if (b != BLOCK_AIR && b != BLOCK_WATER) {
                topY = y;
                topBlock = b;
                break;
            }
        }
        if (topY < 0) continue;
        if (!isNaturalSurfaceBlock(topBlock)) continue;
        if (getBlock(x, topY + 1, z) != BLOCK_AIR) continue;
        Mob& m = gMobs[spawned++];
        m.active = true;
        m.type = MOB_PIG;
        m.x = (float)x + 0.5f;
        m.y = (float)topY + 1.0f;
        m.z = (float)z + 0.5f;
        m.vy = 0.0f;
        m.animTick = pass * 13;
        m.health = 10;
        m.maxHealth = 10;
        m.hurtTicks = 0;
        chooseNewWander(m);
    }
}

void updateMobs(const Player& player) {
    (void)player;
    for (int i = 0; i < MAX_MOBS; ++i) {
        Mob& m = gMobs[i];
        if (!m.active) continue;
        ++m.animTick;
        if (m.hurtTicks > 0) --m.hurtTicks;
        if (--m.wanderTicks <= 0) chooseNewWander(m);

        const float step = (m.hurtTicks > 0) ? 0.019f : 0.012f;
        float dx = std::sinf(m.yaw) * step;
        float dz = std::cosf(m.yaw) * step;
        float nx = clampf(m.x + dx, 0.55f, (float)WORLD_X - 0.55f);
        float nz = clampf(m.z + dz, 0.55f, (float)WORLD_Z - 0.55f);
        if (!mobCollidesAt(nx, m.y, m.z, getMobWidth(m), getMobHeight(m), getMobLength(m))
                && !mobCollidesAt(nx, m.y - 0.4f, m.z, getMobWidth(m), getMobHeight(m), getMobLength(m))) {
            m.x = nx;
        } else {
            chooseNewWander(m);
        }
        if (!mobCollidesAt(m.x, m.y, nz, getMobWidth(m), getMobHeight(m), getMobLength(m))
                && !mobCollidesAt(m.x, m.y - 0.4f, nz, getMobWidth(m), getMobHeight(m), getMobLength(m))) {
            m.z = nz;
        }

        m.vy -= 0.018f;
        float ny = m.y + m.vy;
        if (!mobCollidesAt(m.x, ny, m.z, getMobWidth(m), getMobHeight(m), getMobLength(m))) {
            m.y = ny;
        } else {
            if (m.vy < 0.0f) {
                while (mobCollidesAt(m.x, m.y, m.z, getMobWidth(m), getMobHeight(m), getMobLength(m)) && m.y < WORLD_Y - 1) m.y += 0.03f;
            }
            m.vy = 0.0f;
        }
        if (m.y < 1.0f) m.y = 1.0f;
    }
}

int raycastMob(const Player& player, float maxDist, float* outDist) {
    const float cp = std::cosf(player.pitch);
    const float dx = std::sinf(player.yaw) * cp;
    const float dy = std::sinf(player.pitch);
    const float dz = std::cosf(player.yaw) * cp;
    const float ox = player.x;
    const float oy = player.y + kEyeHeight;
    const float oz = player.z;

    int best = -1;
    float bestT = maxDist;
    for (int i = 0; i < MAX_MOBS; ++i) {
        const Mob& m = gMobs[i];
        if (!m.active) continue;
        const float halfW = getMobWidth(m) * 0.5f;
        const float halfL = getMobLength(m) * 0.5f;
        float t = maxDist;
        if (rayAabb(ox, oy, oz, dx, dy, dz,
                    m.x - halfW, m.y, m.z - halfL,
                    m.x + halfW, m.y + getMobHeight(m), m.z + halfL,
                    maxDist, t)) {
            if (t < bestT) {
                bestT = t;
                best = i;
            }
        }
    }
    if (outDist) *outDist = bestT;
    return best;
}

bool damageMob(int index, int amount) {
    if (index < 0 || index >= MAX_MOBS || amount <= 0) return false;
    Mob& m = gMobs[index];
    if (!m.active || m.hurtTicks > 0) return false;
    m.health -= amount;
    m.hurtTicks = 10;
    m.yaw += 3.14159f;
    m.wanderTicks = 20;
    if (m.health <= 0) {
        m.active = false;
        return true;
    }
    return true;
}

int getMobCount() { return MAX_MOBS; }
const Mob* getMob(int index) {
    if (index < 0 || index >= MAX_MOBS) return nullptr;
    return &gMobs[index];
}
