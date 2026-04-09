
#include <nds.h>
#include "mp3_streamer.h"

/*
 * Scaffold only.
 *
 * This file intentionally does not hook into the current ARM7 runtime yet.
 * A working implementation must be merged into the default libnds ARM7 core
 * so FAT/save/system services keep working.
 */

static volatile int gMp3Ready = 0;

void mp3StreamerInit(void) {
    gMp3Ready = 0;
}

void mp3StreamerUpdate(void) {
}

void mp3StreamerRequestRandomTrack(void) {
}

void mp3StreamerStop(void) {
}

int mp3StreamerIsReady(void) {
    return gMp3Ready;
}
