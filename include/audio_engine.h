#pragma once

void initAudioEngine();
void updateAudioEngine(int deltaMs);
void stopAudioEngine();
bool hasEmbeddedMusic();
const char* getCurrentMusicName();
int getCurrentMusicTrackCount();
const char* getAudioStatus();

// ARM9 -> ARM7 sound effect helpers.
void audioPlayUiClick();
void audioPlayPlace();
void audioPlayBreak();
void audioPlayHurt();
void audioPlayMobHit();
void audioSetMusicEnabled(bool enabled);
void audioNextTrack();
