#include "pch.h"
#include "app.h"
#include "mediaSource.h"
#include "mediaClip.h"
#include "playback.h"
#include "export.h"
#include "dynArr.h"
#include "effects.h"


void App_Init(App* app) {
	memset(app, 0, sizeof(App));
	app->mpv_width = 1280;
	app->mpv_height = 720;
	app->mpv = nullptr;
	app->mpv_gl = nullptr;

	app->playbackBlocked = false;
	app->playbackActive = false;
    app->userScaleFactor = 1.0;
    app->scale = 10.0; // placeholder value, actual value gets set after imgui is initilalized
	app->timeline.clipHeight = 39;
	// app->timeline.zoomX = 1.5;
    app->timeline.zoomX = 1.5;
	app->timeline.width = 2500;
	app->timeline.snappingPrecision = 5.0;
    app->timeline.highestTrackCount = MINIMUM_DRAW_TRACK_COUNT;
    DynArr_Init(&app->selectedClips, sizeof(MediaClip*));

    getcwd(app->exportPath, sizeof(app->exportPath));
    size_t len = strlen(app->exportPath);
    size_t remaining = sizeof(app->exportPath) - len;

#ifdef CC_PLATFORM_WINDOWS
        snprintf(app->exportPath + len, remaining, "\\cc_output.mp4");
#else
        snprintf(app->exportPath + len, remaining, "/cc_output.mp4");
#endif

    app->exportState.statusString = (char*) "Not started";
    Export_SetDefaultExportOptionsVideo(app);

    app->exportState.streamDisabled = &app->streamDisabled;
    app->exportState.streamAudioGain = &app->streamAudioGain;

    app->availableFilterNames = Effects_GetAllFilterNames(&app->availableFilterNamesCount);

    DynArr_Init(&app->exportState.userAudioFilters, sizeof(AudioEffect));

    app->temp_attack = 20;
    app->temp_release = 250;
    app->temp_ratio = 2;
    app->temp_threshold = 0.125;
    app->temp_level_in = 1;
    app->temp_makeup = 1;

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

    DynArr_Free(&app->selectedClips);
	free(app);
}

const char* TimelineEventType_ToString(enum TimelineEventType type) {
    switch (type) {
        case TIMELINE_EVENT_VIDEO: return "TIMELINE_EVENT_VIDEO";
        case TIMELINE_EVENT_END: return "TIMELINE_EVENT_END";
        case TIMELINE_EVENT_BLANKSPACE: return "TIMELINE_EVENT_BLANKSPACE";
        default: return "UNKNOWN";
    }
}

// creates a new MediaSource and adds it to app megastruct, if failed returns nullpointer
MediaSource* App_CreateMediaSource(App* app, const char* path) {
	int avail_index = App_FindFirstNullptr((void**) &app->mediaSources, MEDIASOURCES_SIZE);
	if (avail_index == -1) {
		log_fatal("not enough space for a new media source");
		App_Die();
	}

	MediaSource* mediaSource = (MediaSource*) malloc(sizeof(MediaSource));
	MediaSource_Init(&mediaSource, path);
    if (mediaSource == nullptr)
        return nullptr;
    app->mediaSources[avail_index] = mediaSource;
	return mediaSource;
}

// TODO: return value for succesful or not instead of dying
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

void App_ProcessKeyboardShortcuts(App* app) {
    const int global = ImGuiInputFlags_RouteGlobal;
    const int repeat = ImGuiInputFlags_Repeat;

    if (ImGui::Shortcut(ImGuiKey_End, global)) {
        log_debug("Pressing debug key");
    }

    if (ImGui::Shortcut(ImGuiKey_Delete, global)) {
        if (app->selectedClips.size != 0)  {
            for (size_t i=0; i < app->selectedClips.size; i++) {
                App_DeleteMediaClip(app, (MediaClip*) app->selectedClips.items[i]);
            }
            DynArr_Init(&app->selectedClips, sizeof(MediaClip*));

        }
    }

    // split clip
    if (ImGui::Shortcut(ImGuiKey_S, global)) {
        TimelineEvent* currentEvent = &app->timelineEvents[app->timelineEventIndex];
        if (currentEvent->type == TIMELINE_EVENT_VIDEO) {
            log_info("Splitting clip");
            MediaClip_Split(app, currentEvent->clip, app->playbackTime);

            App_CalculateTimelineEvents(app);
        }

    }

    // move marker to start of timeline
    if (ImGui::Shortcut(ImGuiKey_0, global)) {
        app->playbackTime = 0;
        App_MovePlaybackMarker(app, 0);
    }

    if (ImGui::Shortcut(ImGuiKey_Space, global | ImGuiInputFlags_Repeat)) {
        app->playbackActive = !app->playbackActive;
        Playback_SetPaused(app, !app->playbackActive);
    }

    if (ImGui::Shortcut(ImGuiKey_RightArrow, global | repeat)) {
        Playback_StepFrames(app, true);
    }

    if (ImGui::Shortcut(ImGuiKey_LeftArrow, global | repeat)) {
        Playback_StepFrames(app, false);
    }

    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_A, global)) {
        // clear previous selections to avoid adding duplicates
        DynArr_Init(&app->selectedClips, sizeof(MediaClip*));

        for (int i=0; i < MEDIACLIPS_SIZE; i++) {
            MediaClip* clip = app->mediaClips[i];
            if (clip == nullptr) break;
            clip->isSelected = true;
            DynArr_Append(&app->selectedClips, clip);
        }
    }
}


void App_DeleteMediaClip(App* app, MediaClip* mediaClip) {
    bool isBeingPlayed = MediaClip_IsBeingPlayed(app, mediaClip);

    MediaSource* clipSource = mediaClip->source;
    bool mediaSourceUsedInOtherClips = false;
    // find index of mediaClip in mediaClips array
    int clipIndex = -1;
    for (int i=0; i < MEDIACLIPS_SIZE; i++) {
        if (app->mediaClips[i] != mediaClip && app->mediaClips[i] != nullptr && app->mediaClips[i]->source == mediaClip->source) {
            mediaSourceUsedInOtherClips = true;
        }


        if (clipIndex == -1 && app->mediaClips[i] == mediaClip) {
            clipIndex = i;
        } else if (clipIndex != -1 && i > clipIndex) {
            // shift all elements after the clipIndex back by one index
            // so that the mediaClip is removed from the array
            app->mediaClips[i-1] = app->mediaClips[i];
            if (app->mediaClips[i] == nullptr)
                break;
        }
    }

    if (clipIndex == -1) {
        log_error("MediaClip to delete not found in app struct");
        assert(true && "Mediaclip to delete not found in app struct");
        return;
    }

    if (!mediaSourceUsedInOtherClips) {
        int srcIndex = -1;
        for (int i=0; i < MEDIASOURCES_SIZE; i++) {
            if (srcIndex == -1 && app->mediaSources[i] == clipSource) {
                srcIndex = i;
            } else if (srcIndex != -1 && i > srcIndex) {
                // shuffle all elements after the srcIndex back by one index
                // so that the mediaSource is removed from the array
                app->mediaSources[i-1] = app->mediaSources[i];
                if (app->mediaSources[i] == nullptr)
                    break;
            }
        }
    }

    App_CalculateTimelineEvents(app);
    if (isBeingPlayed) {
        App_MovePlaybackMarker(app, app->playbackTime);
    }
    free(mediaClip);
}

// Updates the timeline events to be up to date.
// Call this any time you add/remove mediaClips or update their position in the timeline
void App_CalculateTimelineEvents(App* app) {
	MediaClip** mediaClips = app->mediaClips;
	//MediaClip* mediaClipsSorted = (MediaClip*) malloc(sizeof(MediaClip));

	
    app->timeline.highestTrackCount = 0;
	{ // sort array
		MediaClip* current;
		for (int i = 0; i < MEDIACLIPS_SIZE; i++) {
			current = app->mediaClips[i];
			if (current == nullptr) break;
            if (current->source->audioTracks+1 > app->timeline.highestTrackCount)
                app->timeline.highestTrackCount = current->source->audioTracks+1;

			int backI = i - 1;
			while (backI >= 0 && app->mediaClips[backI]->padding > current->padding) {
				mediaClips[backI + 1] = mediaClips[backI];
				backI--;
			}
			mediaClips[backI + 1] = current;
		}

        if (app->timeline.highestTrackCount < MINIMUM_DRAW_TRACK_COUNT) app->timeline.highestTrackCount = MINIMUM_DRAW_TRACK_COUNT;

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

            app->timeline.width = fmax(event->start+1000, app->timeline.width);

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

// properly load an event from the timeline, such as loading a new video or seeking if there has been a cut etc.
// loadedNaturallyByPlaying is set to true if the event was loaded because the video is being played
// and the cursor walked past one event over to the next. It is false if you for example skip forward by clicking in the timeline.
void App_LoadEvent(App* app, TimelineEvent* event, bool loadedNaturallyByPlaying) {
    log_trace("App_LoadEvent: called");
	if (event->type == TIMELINE_EVENT_VIDEO) {
		if (event->clip->source != app->loadedMediaSource) {
            float seekPos = app->playbackTime - event->start + event->clip->startCutoff;
            log_trace("App_LoadEvent: video source is not loaded. Loading now");
			MediaSource_Load(app, event->clip->source, seekPos);
			app->loadedMediaSource = event->clip->source;
		} else {
            log_trace("App_LoadEvent: video source is already loaded.");
            TimelineEvent eventBefore = app->timelineEvents[app->timelineEventIndex];
            if (loadedNaturallyByPlaying &&
                eventBefore.type == TIMELINE_EVENT_VIDEO &&
                eventBefore.clip->startCutoff+eventBefore.clip->width == event->clip->startCutoff) {
                log_trace("App_LoadEvent: The two clips end and start at the same moment in the source file, so we will not seek.");
            } else {
                log_trace("App_LoadEvent: seeking to startCutoff.");
                Playback_SetPlaybackPos(app, event->clip->startCutoff);
            }

        }
	} else if (event->type == TIMELINE_EVENT_BLANKSPACE) {
        log_trace("App_LoadEvent: loading blank space");
		const char* cmd[] = { "stop", NULL };
        App_Queue_AddCommand(app, cmd);
	} else if (event->type == TIMELINE_EVENT_END) {
        Playback_Stop(app);
	}
}


// moves the playback marker. Commonly called with secs=app->playtime in order to
// update the playback to reflect what timelineEvents actually looks like in memory
// after it has been modified.
// Playback_SetPlaybackPos() only sets the playback within the video in MPV,
// this function updates the visual marker and loads the appropriate events
void App_MovePlaybackMarker(App* app, float secs) {
    log_trace("App_MovePlaybackMarker(): called");

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
	
    if (currentEvent->type == TIMELINE_EVENT_VIDEO) {
        if (currentEvent->clip->source != app->loadedMediaSource) {
            log_trace("App_MovePlaybackMarker(): source not loaded, calling LoadEvent");
            App_LoadEvent(app, currentEvent, false);
        } else {
            float seekPos = secs-currentEvent->start+currentEvent->clip->startCutoff;
            log_trace("App_MovePlaybackMarker(): source already loaded, seeking to: %.2f", seekPos);
            Playback_SetPlaybackPos(app, seekPos);
        }


    } else {
        log_trace("App_MovePlaybackMarker(): target event is not video, loading its event");
        App_LoadEvent(app, currentEvent, false);
    }
}

TimelineEvent* App_GetNextTimelineEvent(App* app) {
	int indx = app->timelineEventIndex + 1;
	if (indx == TIMELINE_EVENTS_SIZE - 1 || app->timelineEvents[app->timelineEventIndex].type == TIMELINE_EVENT_END) {
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

MediaClip* App_FindClosestMediaClip(App* app, double timeToLookFrom) {
    float previousDist = 0;
    for  (int i=0; i < MEDIACLIPS_SIZE; i++) {
        MediaClip* clip = app->mediaClips[i];
        if (clip == nullptr) {
            if (i==0) {
                return nullptr;
            } else {
                return app->mediaClips[i-1];
            }
        }

        float distStart = clip->padding-timeToLookFrom;
        float distEnd = clip->padding+clip->width-timeToLookFrom;
        float dist = fmin(fabs(distStart), fabs(distEnd));

        if (i != 0 && dist > previousDist) {
            return app->mediaClips[i-1];
        }
        previousDist = dist;
    }
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
    cmd->id = *writeI;

    if (cmd->unsent) {
        log_fatal("MPV command queue overflowed");
        App_Die();
    }
    cmd->unsent = true;


    // command name and arguments seperated by null terminated character.
    // terminated by double null character.
    int offset = 0;
    int argc = 0;
    for (int i=0; i < 20; i++) {
        if (input[i] == NULL) break;
        //                         +2 for string end null char & array end null char
        if (offset+strlen(input[i])+2 > MPVCOMMAND_STR_SIZE) {
            log_fatal("Not enough storage for mpv command arguments");
            return false;
        }
        strcpy(cmd->command+offset, input[i]);
        offset += strlen(input[i])+1;
        argc++;
    }
    cmd->argc = argc;

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


    char* sendCmd[21];
    char* strP = cmdStr;

    for (int i=0; i < cmd->argc; i++) {
        sendCmd[i] = strP;
        strP += strlen(strP)+1;
    }
    sendCmd[cmd->argc] = NULL;

    //                                              using +1 to avoid having zero
    int ret;
    if ((ret = mpv_command_async(app->mpv, app->mpvCmdQueueReadIndex+1, (const char**) sendCmd)) != MPV_ERROR_SUCCESS) {
        log_error("Failed sending command to MPV of type: '%s', error message: '%s'", cmdStr, mpv_error_string(ret));
        // TODO: set unsent to right value and increment readIndex so we don't get stuck
    }
}

void App_ClearClipSelections(App* app) {
    for (size_t i=0; i < app->selectedClips.size; i++) {
        MediaClip* clip = (MediaClip*) app->selectedClips.items[i];
        if (clip != nullptr)
            clip->isSelected = false;
    }

    DynArr_Init(&app->selectedClips, sizeof(MediaClip*)); // clear selectedClips array
}


void App_Die() {
    log_fatal("Clipcutter is exiting.");
    exit(1);
}

