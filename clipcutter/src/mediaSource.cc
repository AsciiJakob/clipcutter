#include "pch.h"
#include "mediaSource.h"
#include "app.h"
#include "playback.h"

 char* GetFileNameFromPath( char* _buffer) {
	char c;
	int  i;
	for (i = 0; ; ++i) {
		c = *((char*)_buffer + i);
		if (c == '\\' || c == '/')
			return GetFileNameFromPath((char*)_buffer + i + 1);
		if (c == '\0')
			return _buffer;
	}
	return nullptr;
}

void MediaSource_Init(MediaSource* mediaSource, char* path) {
	memset(mediaSource, 0, sizeof(MediaSource));

	mediaSource->filename = nullptr;
	//strcpy(mediaSource->path, path);
	mediaSource->path = strdup(path);
	mediaSource->filename = GetFileNameFromPath(path);
	if (mediaSource->filename == nullptr) {
		printf("Error: Failed to get filename from path\n");
		exit(1);
	}

	char* url = (char*) malloc(strlen(path) + sizeof("file:") + 1);
	sprintf(url, "file:%s", path);

	AVFormatContext* s = NULL;
	int ret = avformat_open_input(&s, url, NULL, NULL);
	if (ret < 0) {
		printf("Error: ffmpeg failed to retrieve information about video source\n");
		exit(1);
	} else printf("yay\n");

	avformat_find_stream_info(s, nullptr);
	printf("streams:%d", s->nb_streams);
	printf("duration:%.2f", s->duration/1000000.0);

	mediaSource->audioTracks = s->nb_streams - 1; // TODO: account for multiple video track
	mediaSource->length = s->duration / 1000000.0;



	avformat_close_input(&s);

}

void MediaSource_Load(App* app, MediaSource* source) {
	app->playbackBlocked = true;
	app->isLoadingVideo = true;
	app->loadedMediaSource = source;
	Playback_LoadVideo(app, source->path);
}
