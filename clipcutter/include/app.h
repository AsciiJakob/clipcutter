#ifndef APP_H
#define APP_H
#include "pch.h"
#include "mediaSource.h"
#include "mediaClip.h"

#define MEDIACLIPS_SIZE 200
#define TIMELINE_EVENTS_SIZE MEDIACLIPS_SIZE*2+1

typedef struct MediaClip MediaClip;
typedef struct MediaSource MediaSource;

typedef struct Events {
	u32 wakeupOnMpvRenderUpdate, wakeupOnMpvEvents;
};

typedef enum TimelineEventType {
	TIMELINE_EVENT_BLANKSPACE, TIMELINE_EVENT_END, TIMELINE_EVENT_VIDEO
};

typedef struct TimelineEvent {
	MediaClip* clip;
	TimelineEventType type;
	float start;
} TimelineEvent;

typedef struct AppRender {
	// put rendering things in here instead and make a part of App
} AppRender;

typedef struct Timeline {
	float scaleX;
	bool snappingEnabled;
	float snappingPrecision;
	float clipHeight;

	ImVec2 cursTopLeft;
};


typedef struct App {
	SDL_Window* window;
    SDL_GLContext gl_context;
	mpv_handle* mpv;
	mpv_render_context* mpv_gl;
	GLuint mpv_texture;
	Events events;

	int mpv_width; 
	int mpv_height;

	bool playbackActive;
	float playbackTime;
	MediaClip* selectedTrack;
	MediaSource* loadedMediaSource;

	Timeline timeline;
	int timelineEventIndex;
	TimelineEvent timelineEvents[TIMELINE_EVENTS_SIZE];
	MediaSource* mediaSources[200];
	MediaClip* mediaClips[200];
} app;

typedef struct GetPropertyCallback {
	MediaSource* mediaSource;
	int remainingRetrievals;
	void(*callback)(GetPropertyCallback*, App*);
} GetPropertyCallback;

void App_Init(App* app);
void App_MovePlaybackMarker(App* app, float secs);
void App_CalculateTimelineEvents(App* app);
TimelineEvent* App_GetNextTimelineEvent(App* app);
void App_LoadEvent(App* app, TimelineEvent* event);

#endif
