#ifndef MEDIASOURCE_H
#define MEDIASOURCE_H
#include "pch.h"
#include "app.h"


typedef struct App App;

struct PeakBlock {
    float min;
    float max;
};

struct MediaSource {
    char* path;
    char* filename;
    // for videos
    float length;
    int audioTracks;
    DynArr* peakBlocks;
    bool peaksGenerated;
};

void MediaSource_Init(App* app, MediaSource** mediaSourceP, const char* path);
void MediaSource_Free(MediaSource* source);
void MediaSource_Load(App* app, MediaSource* source, float startTime);

#endif
