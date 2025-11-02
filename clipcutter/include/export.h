#ifndef EXPORT_H
#define EXPORT_H
#include "pch.h"
#include "app.h"

typedef struct App App;


void exportVideo(App* app, bool combineAudioStreams);

#endif
