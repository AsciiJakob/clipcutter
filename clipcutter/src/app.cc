#include "pch.h"
#include "app.h"
#include "mediaSource.h"
#include "playback.h"
#include <iostream>


void App_Init(App* app) {
	memset(app, 0, sizeof(App));
	app->mpv_width = 1280;
	app->mpv_height = 720;
	app->mpv = nullptr;
	app->mpv_gl = nullptr;

	app->playbackBlocked = false;
	app->playbackActive = false;
	app->timeline.clipHeight = 30;
	app->timeline.scaleX = 1.5;
	app->timeline.snappingPrecision = 5.0;

    log_error("test!");
}

void App_Free(App* app) {
	for (int i = 0; i < MEDIASOURCES_SIZE; i++) {
		MediaSource* mediaSource = app->mediaSources[i];
		if (mediaSource == nullptr) break;

		if (mediaSource->filename != nullptr) {
			free(mediaSource->filename);
		}
		if (mediaSource->path != nullptr) {
			free(mediaSource->path);
		}

		free(mediaSource);
	}

	for (int i = 0; i < MEDIACLIPS_SIZE; i++) {
		MediaClip* mediaClip = app->mediaClips[i];
		if (mediaClip == nullptr) break;

		free(mediaClip);
	}

	free(app);
}

MediaSource* App_CreateMediaSource(App* app, const char* path) {
	int avail_index = App_FindFirstNullptr((void**) &app->mediaSources, MEDIASOURCES_SIZE);
	if (avail_index == -1) {
		log_fatal("not enough space for a new media source");
		App_Die();
	}

	MediaSource* mediaSource = (MediaSource*) malloc(sizeof(MediaSource));
	MediaSource_Init(mediaSource, path);
	app->mediaSources[avail_index] = mediaSource;
	return mediaSource;
}

MediaClip* App_CreateMediaClip(App* app, MediaSource* mediaSource) {
	int avail_index = App_FindFirstNullptr((void**) &app->mediaClips, MEDIACLIPS_SIZE);
	if (avail_index == -1) {
		log_fatal("not enough space for a new media clip");
		App_Die();
	}

	MediaClip* mediaClip = (MediaClip*)malloc(sizeof(MediaClip));
	MediaClip_Init(mediaClip, mediaSource);
	app->mediaClips[avail_index] = mediaClip;

	mediaClip->padding = App_GetTimelineEventsEnd(app)->start+20;
	if (avail_index == 0) mediaClip->padding = 0;

	return mediaClip;
}


int App_FindFirstNullptr(void** array, int maxLength) {
	for (int i = 0; i < maxLength; i++) {
		if (array[i] == nullptr) {
			return i;
		}
	}
	return -1;
}

void App_CalculateTimelineEvents(App* app) {
	MediaClip** mediaClips = app->mediaClips;
	//MediaClip* mediaClipsSorted = (MediaClip*) malloc(sizeof(MediaClip));

	
	{ // sort array
		MediaClip* current;
		for (int i = 0; i < MEDIACLIPS_SIZE; i++) {
			current = app->mediaClips[i];
			if (current == nullptr) break;
			int backI = i - 1;
			while (backI >= 0 && app->mediaClips[backI]->padding > current->padding) {
				mediaClips[backI + 1] = mediaClips[backI];
				backI--;
			}
			mediaClips[backI + 1] = current;
		}

	}

    // todo: could use a refactor
	int eventI = 0;
	for (int i = 0; i < MEDIACLIPS_SIZE; i++, eventI++) {
		MediaClip* clip = mediaClips[i];

		TimelineEvent* event = &app->timelineEvents[eventI];
		if (clip == nullptr) {
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
                event->clip->timelineEventsIndex = eventI;
				event->start = 0;

			} else {
				event->type = TIMELINE_EVENT_BLANKSPACE;
				event->start = 0;

				eventI++;
				TimelineEvent* eventAfter = &app->timelineEvents[eventI];
				eventAfter->type = TIMELINE_EVENT_VIDEO;
				eventAfter->clip = clip;
                eventAfter->clip->timelineEventsIndex = eventI;
				eventAfter->start = clip->padding;
			}
			continue;
		} else {
			MediaClip* clipBefore = mediaClips[i - 1];
			if (clip->padding == clipBefore->width + clipBefore->padding) {
				event->type = TIMELINE_EVENT_VIDEO;
				event->clip = clip;
                event->clip->timelineEventsIndex = eventI;
				event->start = clip->padding;
			} else {
				event->type = TIMELINE_EVENT_BLANKSPACE;
				event->start = clipBefore->padding + clipBefore->width;

				eventI++;
				TimelineEvent* eventAfter = &app->timelineEvents[eventI];
				eventAfter->type = TIMELINE_EVENT_VIDEO;
				eventAfter->clip = clip;
                eventAfter->clip->timelineEventsIndex = eventI;
				eventAfter->start = clip->padding;

			}

		}
	}
	// TODO: handle case if mediaclips is full and the loop is done
	//app->timelineEvents[TIMELINE_EVENTS_SIZE-1].type = TIMELINE_EVENT_END;
	//mediaClip* mediaClipBefore = app->timelineEvents[TIMELINE_EVENTS_SIZE-2].type = TIMELINE_EVENT_END;
}

void App_LoadEvent(App* app, TimelineEvent* event) {
    log_trace("App_LoadEvent: called");
	if (event->type == TIMELINE_EVENT_VIDEO) {
		if (event->clip->source != app->loadedMediaSource) {
            log_trace("App_LoadEvent: video source is not loaded. Loading now");
			MediaSource_Load(app, event->clip->source);
			app->loadedMediaSource = event->clip->source;
		}
	} else if (event->type == TIMELINE_EVENT_BLANKSPACE) {
        log_trace("App_LoadEvent: loading blank space");
		const char* cmd[] = { "stop", NULL };
		// TODO: async
		if (int result = mpv_command(app->mpv, cmd); result != MPV_ERROR_SUCCESS) {
			log_error("stopping playback failed, reason: %s", mpv_error_string(result));
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
		log_info("seeking to: %d\n", seekPos);
		Playback_SetPlaybackPos(app, seekPos);
	}
}

TimelineEvent* App_GetNextTimelineEvent(App* app) {
	int indx = app->timelineEventIndex + 1;
	if (indx == TIMELINE_EVENTS_SIZE - 1) {
		return nullptr;
	}
	return &app->timelineEvents[indx];
}

TimelineEvent* App_GetTimelineEventsEnd(App* app) {
	for (int i = 0; i < TIMELINE_EVENTS_SIZE; i++) {
		TimelineEvent* event = &app->timelineEvents[i];
		if (event == nullptr) {
			log_fatal("timelineEvent is nullptr");
		}
		assert(event != nullptr && "failed to get timeline events end. encountered nullptr");

		if (event->type == TIMELINE_EVENT_END) {
			return event;
		}
	}
	assert(true && "failed to get timeline events end. went through whole array");
	return nullptr;
}

// input is equivalent to the cmd arg in mpv_command_async()
bool App_Queue_AddCommand(App* app, const char** input) {

    #if CC_BUILD_DEBUG

        char str[MPVCOMMAND_STR_SIZE];
        str[0] = '\0';
        for (int i = 0; input[i] != NULL; i++) {
            if (i > 0) strcat(str, " ");
            strcat(str, input[i]);
        }
        log_debug("Request to add MPV command to queue: %s", str);

    #endif

    bool queueEmpty = false;
    if (app->MpvCmdQueue[app->mpvCmdQueueReadIndex].unsent == false) {
        /*log_debug("Queue empty. Event will be sent immediately");*/
        queueEmpty = true;
    }

    int* writeI = &app->mpvCmdQueueWriteIndex;

    if (*writeI > MPV_CMD_QUEUE_SIZE-1) {
        *writeI = 0;
    }
    MpvCommand* cmd = &app->MpvCmdQueue[*writeI];
    // we always want the id 1 higher so that it is never zero
    // since that's the default userdata value.
    (*writeI)++;
    cmd->id == *writeI;

    if (cmd->unsent) {
        log_fatal("MPV command queue overflowed");
        App_Die();
    }
    cmd->unsent = true;


    // command name and arguments seperated by null terminated character.
    // terminated by double null character.
    int offset = 0;
    for (int i=0; i < 20; i++) {
        if (input[i] == NULL) break;
        //                         +2 for string end null char & array end null char
        if (offset+strlen(input[i])+2 > MPVCOMMAND_STR_SIZE) {
            log_fatal("Not enough storage for mpv command arguments");
            return false;
        }
        strcpy(cmd->command+offset, input[i]);
        offset += strlen(input[i])+1;
    }
    // end is marked with two NULL characters. one for last str and one for end of data
    /*cmd.command[offset+2] = NULL;*/
    cmd->command[offset] = NULL;

    if (queueEmpty) {
        App_Queue_SendNext(app);
    }

    return true;
}


void App_Queue_SendNext(App* app) {
    MpvCommand* cmd = &app->MpvCmdQueue[app->mpvCmdQueueReadIndex];
    char* cmdStr = cmd->command;

    if (cmd->unsent == false) {
        /*log_debug("No remaining events");*/
        return;
    }


    char* sendCmd[20];
    int sendCmdIndx = 0;

    // first string starts from zero until first NULL
    sendCmd[sendCmdIndx++] = cmdStr;

    for (int i=0; i < MPVCOMMAND_STR_SIZE; i++) {
        char c = cmdStr[i];
        if (c == NULL) {
            // the array is terminated by two NULL signs after each other
            if (cmdStr[i+1] == NULL) {
                break;
            }
            sendCmd[sendCmdIndx++] = cmdStr+i+1;
        }
    }
    sendCmd[sendCmdIndx] = NULL;

    //                                              using +1 to avoid having zero
    if (mpv_command_async(app->mpv, app->mpvCmdQueueReadIndex+1, (const char**) sendCmd) != MPV_ERROR_SUCCESS) {
        log_error("Failed sending command to MPV of type: %s", cmdStr);
        // TODO: set unsent to right value and increment readIndex so we don't get stuck
    }
}


void App_Die() {
    log_fatal("Clipcutter is exiting.");
    exit(1);
}

