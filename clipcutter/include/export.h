#ifndef EXPORT_H
#define EXPORT_H
#include "pch.h"
#include "app.h"

typedef struct App App;


void Export_SetDefaultExportOptionsVideo(App* app);
void Export_SetDefaultExportOptionsAudio(App* app);
void exportVideo(App* app, bool combineAudioStreams);

#endif
