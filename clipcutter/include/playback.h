#ifndef PLAYBACK_H
#define PLAYBACK_H
#include "pch.h"
#include "app.h"

void Playback_ApplyLavfiComplex(App* app);
void Playback_SetPlaybackPos(App* app, float secs);
void Playback_LoadVideo(App* app, char* path, float startTime);
void Playback_StepFrames(App* app, bool forwards);
void Playback_SetPaused(App* app, bool paused);
void Playback_Stop(App* app);
#endif

