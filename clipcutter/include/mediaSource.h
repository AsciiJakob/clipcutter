#ifndef MEDIASOURCE_H
#define MEDIASOURCE_H
#include "pch.h"

typedef struct MediaSource {
	char* path;
	// for videos
	float length; 
	int audioTracks;
} MediaSource;

void MediaSource_Init(MediaSource* mediaSource, char* path);

#endif