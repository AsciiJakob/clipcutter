#include "pch.h"
#include "mediaSource.h"
#include "app.h"


void MediaSource_Init(MediaSource* mediaSource, char* path) {
	memset(mediaSource, 0, sizeof MediaSource);
	mediaSource->path = path;
}

void MediaSource_Load(App* app, MediaSource* source) {
	const char* cmd[] = { "loadfile", source->path, NULL };
	if (mpv_command_async(app->mpv, 0, cmd) != MPV_ERROR_SUCCESS) {
		printf("Error: Failed loading file");
		return;
	}
	app->loadedMediaSource = source;
}
