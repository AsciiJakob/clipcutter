#ifndef MEDIASOURCE_H
#define MEDIASOURCE_H
#include "pch.h"
#include "app.h"

#define MAX_SUPPORTED_AUDIO_TRACKS 100

typedef struct App App;

 struct MediaSource {
	char* path;
	char* filename;
	// for videos
	float length; 
	int audioTracks;
};

void MediaSource_Init(MediaSource** mediaSourceP, const char* path);
void MediaSource_Load(App* app, MediaSource* source);

#endif
