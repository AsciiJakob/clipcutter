#ifndef APP_H
#define APP_H
#include "pch.h"
#include "mediaSource.h"
#include "mediaClip.h"

typedef struct Events {
	u32 wakeupOnMpvRenderUpdate, wakeupOnMpvEvents;
};

typedef struct Timeline {
	float scaleX;
	bool snappingEnabled;
	float snappingPrecision;
	float clipHeight;

	ImVec2 cursTopLeft;
};

typedef struct MediaClip MediaClip;

typedef struct App {
	SDL_Window* window;
    SDL_GLContext gl_context;
	mpv_handle* mpv;
	mpv_render_context* mpv_gl;
	GLuint mpv_texture;
	Events events;
    ImGuiIO io;

	int mpv_width; 
	int mpv_height;

	bool playbackActive;
	float playbackTime;
	MediaClip* selectedTrack;

	Timeline timeline;
	MediaSource* mediaSources[200];
	MediaClip* mediaClips[200];
} app;

typedef struct GetPropertyCallback {
	MediaSource* mediaSource;
	int remainingRetrievals;
	void(*callback)(GetPropertyCallback*, App*);
} GetPropertyCallback;

void App_Init(App* app);

#endif
