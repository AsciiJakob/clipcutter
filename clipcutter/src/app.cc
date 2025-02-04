#include "pch.h"
#include "app.h"
#include "mediaSource.h"

void App_Init(App* app) {
	memset(app, 0, sizeof App);
	app->mpv_width = 1280;
	app->mpv_height = 720;
	app->mpv = nullptr;
	app->mpv_gl = nullptr;

	app->playbackActive = false;
	app->timeline.clipHeight = 30;
	app->timeline.scaleX = 1.5;
	app->timeline.snappingPrecision = 5.0;
}



void App_CalculateTimelineEvents(App* app) {
	MediaClip** mediaClips = app->mediaClips;
	TimelineEvent* timelineEvents = app->timelineEvents;
	//MediaClip* mediaClipsSorted = (MediaClip*) malloc(sizeof MediaClip);

	
	{ // sort array
		MediaClip* current;
		for (int i = 0; i < 200; i++) {
			current = app->mediaClips[i];
			if (current == NULL) break;
			int backI = i - 1;
			while (backI >= 0 && app->mediaClips[backI]->padding > current->padding) {
				mediaClips[backI + 1] = mediaClips[backI];
				backI--;
			}
			mediaClips[backI + 1] = current;
		}

	}

	int eventI = 0;
	for (int i = 0; i < MEDIACLIPS_SIZE; i++, eventI++) {
		MediaClip* clip = mediaClips[i];

		TimelineEvent* event = &app->timelineEvents[eventI];
		TimelineEvent* eventAfter = &app->timelineEvents[eventI+1];
		if (clip == NULL) {
			event->type = TIMELINE_EVENT_END;
			MediaClip* clipBefore = app->timelineEvents[eventI - 1].clip;
			if (i == 0) {
				event->start = 0;
			} else {
				event->start = clipBefore->padding+clipBefore->width;
			}
			break;
		}

		if (i == 0) {
			if (clip->padding == 0) {
				event->type = TIMELINE_EVENT_VIDEO;
				event->clip = clip;
				event->start = 0;

			} else {
				event->type = TIMELINE_EVENT_BLANKSPACE;
				event->start = 0;

				eventI++;
				TimelineEvent* eventAfter = &app->timelineEvents[eventI];
				eventAfter->type = TIMELINE_EVENT_VIDEO;
				eventAfter->clip = clip;
				eventAfter->start = clip->padding;
			}
			continue;
		} else {
			MediaClip* clipBefore = mediaClips[i - 1];
			if (clip->padding == clipBefore->width + clipBefore->padding) {
				event->type = TIMELINE_EVENT_VIDEO;
				event->clip = clip;
				event->start = clip->padding;
			} else {
				event->type = TIMELINE_EVENT_BLANKSPACE;
				event->start = clipBefore->padding + clipBefore->width;

				eventI++;
				TimelineEvent* eventAfter = &app->timelineEvents[eventI];
				eventAfter->type = TIMELINE_EVENT_VIDEO;
				eventAfter->clip = clip;
				eventAfter->start = clip->padding;

			}

		}
	}
	// TODO: handle case if mediaclips is full and the loop is done
	//app->timelineEvents[TIMELINE_EVENTS_SIZE-1].type = TIMELINE_EVENT_END;
	//mediaClip* mediaClipBefore = app->timelineEvents[TIMELINE_EVENTS_SIZE-2].type = TIMELINE_EVENT_END;
}

// TODO: maybe put in an mpv.cc file?
void App_SetPlaybackPos(App* app, float secs) {
	std::string timeStr = std::to_string(secs);
	const char* cmd[] = { "seek", timeStr.data(), "absolute", NULL };
	if (int result = mpv_command(app->mpv, cmd); result != MPV_ERROR_SUCCESS) {
		fprintf(stderr, "Fast forward failed, reason: %s\n", mpv_error_string(result));
	}
}

void App_LoadEvent(App* app, TimelineEvent* event) {
	if (event->type == TIMELINE_EVENT_VIDEO) {
		if (event->clip->source != app->loadedMediaSource) {
			MediaSource_Load(app, event->clip->source);
			printf("loaded new media file\n");
			app->loadedMediaSource = event->clip->source;
		}
	} else if (event->type == TIMELINE_EVENT_BLANKSPACE) {
		const char* cmd[] = { "stop", NULL };
		if (int result = mpv_command(app->mpv, cmd); result != MPV_ERROR_SUCCESS) {
			fprintf(stderr, "stopping playback failed, reason: %s\n", mpv_error_string(result));
		}
	} else if (event->type == TIMELINE_EVENT_END) {

	}
}


void App_MovePlaybackMarker(App* app, float secs) {

	// set timelineEventIndex
	for (int i=0; i < TIMELINE_EVENTS_SIZE; i++) {
		TimelineEvent* event = &app->timelineEvents[i];
		if (event->type == TIMELINE_EVENT_END) {
			app->timelineEventIndex = i;
			break;
		}
		TimelineEvent* eventAfter = &app->timelineEvents[i+1];

		if (event->start < secs && eventAfter->start > secs) {
			app->timelineEventIndex = i;
			break;
		}

		if (secs == event->start) {
			app->timelineEventIndex = i;
			break;
		}
	}

	TimelineEvent* currentEvent = &app->timelineEvents[app->timelineEventIndex];
	
	App_LoadEvent(app, currentEvent);

	if (currentEvent->type == TIMELINE_EVENT_VIDEO) {
		int seekPos = secs-currentEvent->start;
		printf("seeking to: %d\n", seekPos);
		App_SetPlaybackPos(app, seekPos);
	}
}

TimelineEvent* App_GetNextTimelineEvent(App* app) {
	int indx = app->timelineEventIndex + 1;
	if (indx == TIMELINE_EVENTS_SIZE - 1) {
		return nullptr;
	}
	return &app->timelineEvents[indx];
}
