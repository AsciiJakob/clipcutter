#include "pch.h"
#include "mediaSource.h"
#include "app.h"
#include "playback.h"

 char* GetFileNameFromPath(char* _buffer) {
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

void MediaSource_Init(MediaSource** mediaSourceP, const char* path) {
    MediaSource* mediaSource = *mediaSourceP;
	memset(mediaSource, 0, sizeof(MediaSource));

	mediaSource->filename = nullptr;
	//strcpy(mediaSource->path, path);
	mediaSource->path = strdup(path);
	// mediaSource->filename = GetFileNameFromPath(strdup(path));
	mediaSource->filename = strdup(GetFileNameFromPath((char*)path));
	if (mediaSource->filename == nullptr) {
		log_fatal("Failed to get filename from path");
		App_Die();
	}

	char* url = (char*) malloc(strlen(path) + sizeof("file:") + 1);
	sprintf(url, "file:%s", path);

	AVFormatContext* s = NULL;
	int ret = avformat_open_input(&s, url, NULL, NULL);
	if (ret < 0) {
		log_fatal("ffmpeg failed to retrieve information about video source");
		App_Die();
	}

	avformat_find_stream_info(s, nullptr);
	log_debug("streams:%d", s->nb_streams);
	log_debug("duration:%.2f", s->duration/AV_TIME_BASE);

	mediaSource->audioTracks = 0;
    for (unsigned int i=0; i < s->nb_streams; i++) {
        if (s->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            mediaSource->audioTracks++;
        }
    }

    if (mediaSource->audioTracks > MAX_SUPPORTED_AUDIO_TRACKS) {
        log_error("Cannot initialize a media source with more than %d audiotracks.", MAX_SUPPORTED_AUDIO_TRACKS);
        free(mediaSource);
        *mediaSourceP = nullptr;
        avformat_close_input(&s);
        return;
    }

    mediaSource->length = (float) s->duration / AV_TIME_BASE;

	avformat_close_input(&s);
}

void MediaSource_Load(App* app, MediaSource* source) {
	app->playbackBlocked = true;
	app->isLoadingVideo = true;
	app->loadedMediaSource = source;
	Playback_LoadVideo(app, source->path);
    Playback_SetAudioTracks(app, app->loadedMediaSource->audioTracks);
}
