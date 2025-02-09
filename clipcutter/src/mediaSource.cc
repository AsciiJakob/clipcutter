#include "pch.h"
#include "mediaSource.h"
#include "app.h"
#include "playback.h"


void MediaSource_Init(MediaSource* mediaSource) {
	memset(mediaSource, 0, sizeof MediaSource);

	mediaSource->filename = nullptr;
	mediaSource->path = nullptr;
}

void MediaSource_Load(App* app, MediaSource* source) {
	app->playbackActive = false;
	app->isLoadingVideo = true;
	app->loadedMediaSource = source;
	Playback_LoadVideo(app, source->path);
}
