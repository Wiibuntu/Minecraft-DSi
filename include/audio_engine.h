#pragma once

void initAudioEngine();
void updateAudioEngine(int deltaMs);
void stopAudioEngine();
bool hasEmbeddedMusic();
const char* getCurrentMusicName();
int getCurrentMusicTrackCount();

const char* getAudioStatus();
