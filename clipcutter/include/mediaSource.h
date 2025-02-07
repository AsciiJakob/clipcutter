#ifndef MEDIASOURCE_H
#define MEDIASOURCE_H
#include "pch.h"
#include "app.h"

typedef struct App App;

typedef struct MediaSource {
	char* path;
	char* filename;
	// for videos
	float length; 
	int audioTracks;
} MediaSource;

void MediaSource_Init(MediaSource* mediaSource);
void MediaSource_Load(App* app, MediaSource* source);

#endif