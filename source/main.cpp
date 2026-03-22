#include <nds.h>

#include "player.h"
#include "render.h"
#include "world.h"

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

static int selectedIndexFromBlock(int block) {
    for (int i = 0; i < kPlaceableBlockCount; ++i) {
        if (kPlaceableBlocks[i] == block) return i;
    }
    return 0;
}

static void changeSelectedBlock(Player& p, int delta) {
    int idx = selectedIndexFromBlock(p.selectedBlock);
    idx = (idx + delta + kPlaceableBlockCount) % kPlaceableBlockCount;
    p.selectedBlock = kPlaceableBlocks[idx];
}

int main() {
    irqEnable(IRQ_VBLANK);

    initRenderer();
    initWorld();

    Player player;
    initPlayer(player);
    player.selectedBlock = BLOCK_GRASS;

    while (1) {
        swiWaitForVBlank();
        scanKeys();
        int held = keysHeld();
        int down = keysDown();

        if (down & KEY_START) changeSelectedBlock(player, +1);
        if (down & KEY_SELECT) changeSelectedBlock(player, -1);

        if (down & KEY_TOUCH) {
            touchPosition touch;
            touchRead(&touch);
            HudTouchAction action = handleHudTouch(touch.px, touch.py);
            if (action.type == HUD_TOUCH_SELECT_BLOCK) {
                player.selectedBlock = action.value;
            }
        }

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
