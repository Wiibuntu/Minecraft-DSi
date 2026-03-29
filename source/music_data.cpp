#include "music_data.h"

namespace {
struct MusicTrackEntry { const char* name; const char* path; int sampleRate; };
static const MusicTrackEntry kTracks[] = {
    {"Minecraft Volume Alpha - 1 - Key - C418 (128k)", "nitro:/music/Minecraft_Volume_Alpha___1___Key___C418__128k_.raw", 11025},
    {"Minecraft Volume Alpha - 10 - Equinoxe - C418 (128k)", "nitro:/music/Minecraft_Volume_Alpha___10___Equinoxe___C418__128k_.raw", 11025},
    {"Minecraft Volume Alpha - 11 - Mice on Venus - C418 (128k)", "nitro:/music/Minecraft_Volume_Alpha___11___Mice_on_Venus___C418__128k_.raw", 11025},
    {"Minecraft Volume Alpha - 12 - Dry Hands - C418 (128k)", "nitro:/music/Minecraft_Volume_Alpha___12___Dry_Hands___C418__128k_.raw", 11025},
    {"Minecraft Volume Alpha - 13 - Wet Hands - C418 (128k)", "nitro:/music/Minecraft_Volume_Alpha___13___Wet_Hands___C418__128k_.raw", 11025},
    {"Minecraft Volume Alpha - 14 - Clark - C418 (128k)", "nitro:/music/Minecraft_Volume_Alpha___14___Clark___C418__128k_.raw", 11025},
    {"Minecraft Volume Alpha - 15 - Chris - C418 (128k)", "nitro:/music/Minecraft_Volume_Alpha___15___Chris___C418__128k_.raw", 11025},
    {"Minecraft Volume Alpha - 16 - Thirteen - C418 (128k)", "nitro:/music/Minecraft_Volume_Alpha___16___Thirteen___C418__128k_.raw", 11025},
    {"Minecraft Volume Alpha - 17 - Excuse - C418 (128k)", "nitro:/music/Minecraft_Volume_Alpha___17___Excuse___C418__128k_.raw", 11025},
    {"Minecraft Volume Alpha - 18 - Sweden - C418 (128k)", "nitro:/music/Minecraft_Volume_Alpha___18___Sweden___C418__128k_.raw", 11025},
    {"Minecraft Volume Alpha - 19 - Cat - C418 (128k)", "nitro:/music/Minecraft_Volume_Alpha___19___Cat___C418__128k_.raw", 11025},
    {"Minecraft Volume Alpha - 2 - Door - C418 (128k)", "nitro:/music/Minecraft_Volume_Alpha___2___Door___C418__128k_.raw", 11025},
    {"Minecraft Volume Alpha - 20 - Dog - C418 (128k)", "nitro:/music/Minecraft_Volume_Alpha___20___Dog___C418__128k_.raw", 11025},
    {"Minecraft Volume Alpha - 21 - Danny - C418 (128k)", "nitro:/music/Minecraft_Volume_Alpha___21___Danny___C418__128k_.raw", 11025},
    {"Minecraft Volume Alpha - 22 - Beginning - C418 (128k)", "nitro:/music/Minecraft_Volume_Alpha___22___Beginning___C418__128k_.raw", 11025},
    {"Minecraft Volume Alpha - 23 - Droopy Likes Ricochet - C418 (128k)", "nitro:/music/Minecraft_Volume_Alpha___23___Droopy_Likes_Ricochet___C418__128k_.raw", 11025},
    {"Minecraft Volume Alpha - 24 - Droopy Likes your Face - C418 (128k)", "nitro:/music/Minecraft_Volume_Alpha___24___Droopy_Likes_your_Face___C418__128k_.raw", 11025},
    {"Minecraft Volume Alpha - 3 - Subwoofer Lullaby - C418 (128k)", "nitro:/music/Minecraft_Volume_Alpha___3___Subwoofer_Lullaby___C418__128k_.raw", 11025},
    {"Minecraft Volume Alpha - 4 - Death - C418 (128k)", "nitro:/music/Minecraft_Volume_Alpha___4___Death___C418__128k_.raw", 11025},
    {"Minecraft Volume Alpha - 5 - Living Mice - C418 (128k)", "nitro:/music/Minecraft_Volume_Alpha___5___Living_Mice___C418__128k_.raw", 11025},
    {"Minecraft Volume Alpha - 6 - Moog City - C418 (128k)", "nitro:/music/Minecraft_Volume_Alpha___6___Moog_City___C418__128k_.raw", 11025},
    {"Minecraft Volume Alpha - 7 - Haggstrom - C418 (128k)", "nitro:/music/Minecraft_Volume_Alpha___7___Haggstrom___C418__128k_.raw", 11025},
    {"Minecraft Volume Alpha - 8 - Minecraft - C418 (128k)", "nitro:/music/Minecraft_Volume_Alpha___8___Minecraft___C418__128k_.raw", 11025},
    {"Minecraft Volume Alpha - 9 - Oxygene - C418 (128k)", "nitro:/music/Minecraft_Volume_Alpha___9___Oxygene___C418__128k_.raw", 11025},
};
static const MusicTrackEntry kEmptyTrack = {"NO MUSIC", nullptr, 11025};
} // namespace

int getMusicTrackCount() {
    return (int)(sizeof(kTracks) / sizeof(kTracks[0]));
}

const char* getMusicTrackPath(int index) {
    const int count = getMusicTrackCount();
    if (index < 0 || index >= count) return nullptr;
    return kTracks[index].path;
}

const char* getMusicTrackName(int index) {
    const int count = getMusicTrackCount();
    if (index < 0 || index >= count) return kEmptyTrack.name;
    return kTracks[index].name;
}

int getMusicTrackSampleRate(int index) {
    const int count = getMusicTrackCount();
    if (index < 0 || index >= count) return kEmptyTrack.sampleRate;
    return kTracks[index].sampleRate;
}
