#ifndef PLAYBACK_H
#define PLAYBACK_H
#include "pch.h"
#include "app.h"

void Playback_SetMultipleAudioTracks(App* app);
void Playback_SetPlaybackPos(App* app, float secs);
void Playback_LoadVideo(App* app, char* path);
#endif

