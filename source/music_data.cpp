#include "music_data.h"

int getMusicTrackCount() { return 1; }
const char* getMusicTrackPath(int index) { (void)index; return nullptr; }
const char* getMusicTrackName(int index) { return index == 0 ? "Subwoofer Lullaby" : "NO MUSIC"; }
unsigned int getMusicTrackRate(int index) { return index == 0 ? 11025u : 0u; }
