#include "pch.h"
#include "mediaSource.h"



void MediaSource_Init(MediaSource* mediaSource, char* path) {
	memset(mediaSource, 0, sizeof MediaSource);
	mediaSource->path = path;
}
