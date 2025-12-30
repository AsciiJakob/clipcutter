#ifndef EXPORT_H
#define EXPORT_H
#include "pch.h"

typedef struct App App;

void Export_SetDefaultExportOptionsVideo(App* app);
void Export_SetDefaultExportOptionsAudio(App* app);
void exportVideo(App* app, bool combineAudioStreams);

extern const char* const ENCODER_PRESETS[];
extern const int ENCODER_PRESET_COUNT;

#endif
