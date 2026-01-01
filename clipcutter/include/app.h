#ifndef APP_H
#define APP_H
#include "pch.h"

#define MEDIACLIPS_SIZE 248
#define MEDIASOURCES_SIZE 248
#define MAX_SUPPORTED_AUDIO_TRACKS 100
#define TIMELINE_EVENTS_SIZE MEDIACLIPS_SIZE*2+1
#define MINIMUM_DRAW_TRACK_COUNT 3

typedef struct MediaClip MediaClip;
typedef struct MediaSource MediaSource;
struct ExportOptions {
    bool exportAudio;
    bool mergeAudioTracks;
    float CBRRateFactor;
    int encoderPresetComboIndex;
    int exportAsComboIndex;
};

struct ExportState {
    ExportOptions exportOptions;

    char* statusString;
    float exportProgress;
    const char* out_filename;

    int clipIndex;
    AVStream* out_video_stream;
    AVStream* out_audio_stream;
    int64_t offsetPtsEncTBVideo;
    int64_t offsetPtsEncTBAudio;
    int64_t lastPtsEncTBVideo;
    int64_t lastPtsEncTBAudio;
    AVFormatContext* ofmt_ctx;
    bool (*audioStreamDisabled)[MAX_SUPPORTED_AUDIO_TRACKS];
    DynArr userAudioFilters;
};

struct Events {
	u32 wakeupOnMpvRenderUpdate, wakeupOnMpvEvents;
};

enum TimelineEventType {
	TIMELINE_EVENT_BLANKSPACE, TIMELINE_EVENT_END, TIMELINE_EVENT_VIDEO
};

struct TimelineEvent {
	MediaClip* clip;
	TimelineEventType type;
	float start;
};

struct AppRender {
	// put rendering things in here instead and make a part of App
};

struct Timeline {
	float zoomX;
    int highestTrackCount;
	bool snappingEnabled;
	float snappingPrecision;
	float clipHeight;
    float width;
	ImVec2 cursTopLeft;
};

// double-null-terminated list of commands
struct MpvCommand {
    bool unsent;
    int id;
    #define MPVCOMMAND_STR_SIZE 512 
    char command[MPVCOMMAND_STR_SIZE];
};

// TODO: make a longer list, because many more formats are actually supported.
constexpr const char* SUPPORTED_FILE_FORMATS[] = {
    ".mp4",
    ".avi",
    ".mpeg",
    ".mov"
};
constexpr size_t NUM_SUPPORTED_FORMATS = sizeof(SUPPORTED_FILE_FORMATS) / sizeof(SUPPORTED_FILE_FORMATS[0]);

struct App {
	SDL_Window* window;
    SDL_GLContext gl_context;
	mpv_handle* mpv;
	mpv_render_context* mpv_gl;
	GLuint mpv_texture;
	Events events;

	int mpv_width; 
	int mpv_height;

	bool playbackBlocked;
	bool playbackActive;
	double playbackTime;
	DynArr selectedClips;
	MediaSource* loadedMediaSource;
	bool isLoadingVideo;
	bool isLoadingNewSource;

    float userScaleFactor;
    float scale;
    float scaleX;
	Timeline timeline;

	int timelineEventIndex;
	TimelineEvent timelineEvents[TIMELINE_EVENTS_SIZE];
	MediaSource* mediaSources[MEDIASOURCES_SIZE];
	MediaClip* mediaClips[MEDIACLIPS_SIZE];
    bool audioStreamDisabled[MAX_SUPPORTED_AUDIO_TRACKS];

    int mpvCmdQueueWriteIndex;
    int mpvCmdQueueReadIndex;
    #define MPV_CMD_QUEUE_SIZE 30
    MpvCommand MpvCmdQueue[MPV_CMD_QUEUE_SIZE];

    ExportState exportState;
    char exportPath[1024];

    float temp_threshold;
    float temp_ratio;
    float temp_attack;
    float temp_release;
    float temp_level_in;
    float temp_makeup;
};

struct GetPropertyCallback {
	MediaSource* mediaSource;
	int remainingRetrievals;
	void(*callback)(GetPropertyCallback*, App*);
};

void App_Init(App* app);
void App_Free(App* app);
MediaSource* App_CreateMediaSource(App* app, const char* path);
MediaClip* App_CreateMediaClip(App* app, MediaSource* mediaSource);
void App_DeleteMediaClip(App* app, MediaClip* mediaClip);
int App_FindFirstNullptr(void** array, int maxLength);
void App_MovePlaybackMarker(App* app, float secs);
void App_CalculateTimelineEvents(App* app);
TimelineEvent* App_GetNextTimelineEvent(App* app);
TimelineEvent* App_GetTimelineEventsEnd(App* app);
MediaClip* App_FindClosestMediaClip(App* app, double timeToLookFrom);
void App_LoadEvent(App* app, TimelineEvent* event);
bool App_Queue_AddCommand(App* app, const char** input);
void App_Queue_SendNext(App* app);
void App_ClearClipSelections(App* app);
void App_Die();

#endif
