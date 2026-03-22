#include <nds.h>

#include "player.h"
#include "render.h"
#include "world.h"

static void changeSelectedBlock(Player& p, int delta) {
    static const int blocks[] = {BLOCK_GRASS, BLOCK_DIRT, BLOCK_STONE, BLOCK_WOOD, BLOCK_LEAVES, BLOCK_SAND, BLOCK_WATER};
    int idx = 0;
    for (int i = 0; i < 7; ++i) {
        if (blocks[i] == p.selectedBlock) idx = i;
    }
    idx = (idx + delta + 7) % 7;
    p.selectedBlock = blocks[idx];
}

int main() {
    irqEnable(IRQ_VBLANK);

    initRenderer();
    initWorld();

    Player player;
    initPlayer(player);

    while (1) {
        swiWaitForVBlank();
        scanKeys();
        int held = keysHeld();
        int down = keysDown();

        if (down & KEY_START) changeSelectedBlock(player, +1);
        if (down & KEY_TOUCH) changeSelectedBlock(player, -1);

        updatePlayer(player, held, down);

        RayHit hit = castCenterRay(player, 5.0f);
        if ((down & KEY_L) && hit.hit && !(hit.x == (int)player.x && hit.z == (int)player.z)) {
            setBlock(hit.x, hit.y, hit.z, BLOCK_AIR);
        }
        if ((down & KEY_R) && hit.hit) {
            if (!isSolidBlock(hit.placeX, hit.placeY, hit.placeZ)) {
                setBlock(hit.placeX, hit.placeY, hit.placeZ, player.selectedBlock);
            }
        }

        renderFrame(player);
        renderHUD(player, hit);
    }

    return 0;
}
