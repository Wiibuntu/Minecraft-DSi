
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void mp3StreamerInit(void);
void mp3StreamerUpdate(void);
void mp3StreamerRequestRandomTrack(void);
void mp3StreamerStop(void);
int mp3StreamerIsReady(void);

#ifdef __cplusplus
}
#endif
