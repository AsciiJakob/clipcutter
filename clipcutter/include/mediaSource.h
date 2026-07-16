#ifndef MEDIASOURCE_H
#define MEDIASOURCE_H
#include "pch.h"
#include "app.h"

#define PEAK_BLOCK_SIZE 256

typedef struct App App;

struct PeakBlock {
    float min;
    float max;
};

struct MediaSource {
    char* path;
    char* filename;
    float length;
    int audioTracks;
    int sampleRates[MAX_SUPPORTED_AUDIO_TRACKS]; // index by audio stream id (not stream id)
    DynArr* peakBlocks;
    bool peaksGenerated;
};

void MediaSource_Init(App* app, MediaSource** mediaSourceP, const char* path);
void MediaSource_Free(MediaSource* source);
void MediaSource_Load(App* app, MediaSource* source, float startTime);

#endif
