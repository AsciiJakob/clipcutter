#ifndef PLAYBACK_H
#define PLAYBACK_H
#include "pch.h"
#include "app.h"

void Playback_SetAudioTracks(App* app, int count);
void Playback_SetPlaybackPos(App* app, float secs);
void Playback_LoadVideo(App* app, char* path);
void Playback_StepFrames(App* app, bool forwards);
void Playback_SetPaused(App* app, bool paused);
void Playback_Stop(App* app);
#endif

