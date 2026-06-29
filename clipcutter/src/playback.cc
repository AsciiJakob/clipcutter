#include "pch.h"
#include "app.h"

void Playback_SetAudioTracks(App* app, int count) {
    log_trace("Playback_SetAudioTracks() with %d as count", count);
    cc_unused(app);
    // https://mpv.io/manual/stable/#options-lavfi-complex

    assert(app->loadedMediaSource != nullptr && "loadedMediaSource was null");

    const int BUF_SIZE = 20 + 6 * MAX_SUPPORTED_AUDIO_TRACKS;
    char valueOptionStr[BUF_SIZE] = "";
    int offset = 0;
    int enabledTrackCount = 0;

    for (int i=1; i < count+1; i++) { // assuming video has one video track
        if (!app->audioStreamDisabled[i]) {
	    offset += snprintf(valueOptionStr + offset, BUF_SIZE - offset,
		   "[aid%d]", i);
            // sprintf(valueOptionStr, "%s[aid%d]", valueOptionStr, i);
            enabledTrackCount++;
        } 
    }
    snprintf(valueOptionStr + offset, BUF_SIZE - offset,
	 "amix=inputs=%d[ao]", enabledTrackCount);
    // sprintf(valueOptionStr, "%samix=inputs=%d[ao]", valueOptionStr, enabledTrackCount);
    // Examples of what valueOptionStr should look like:
    // 3 audio tracks: [aid1][aid2][aid3]amix=inputs=3[ao]
    // 4 audio tracks: [aid1][aid2][aid3][aid4]amix=inputs=4[ao]
    // log_debug("%s", valueOptionStr);

    if (enabledTrackCount == 0) {
        const char* cmd[] = { "set", "options/lavfi-complex", "", NULL };
        App_Queue_AddCommand(app, cmd);
        return;
    }

    const char* cmd[] = { "set", "options/lavfi-complex", valueOptionStr, NULL };
    App_Queue_AddCommand(app, cmd);

    // if (count == 2) {
    //     const char* cmd[] = { "set", "options/lavfi-complex", "[aid1][aid2]amix[ao]", NULL };
    //     App_Queue_AddCommand(app, cmd);
    // } else if (count == 3) {
    //     log_debug("three audiotracks\n");
    //     const char* cmd[] = { "set", "options/lavfi-complex", "[aid1][aid2][aid3]amix=inputs=3[ao]", NULL };
    //     App_Queue_AddCommand(app, cmd);
    // }
}

void Playback_SetPlaybackPos(App* app, float secs) {
	std::string timeStr = std::to_string(secs);
	const char* cmd[] = { "seek", timeStr.data(), "absolute", NULL };
    App_Queue_AddCommand(app, cmd);
	/*if (int result = mpv_command_async(app->mpv, NULL, cmd); result != MPV_ERROR_SUCCESS) {*/
	/*	log_error("Fast forward failed, reason: %s", mpv_error_string(result));*/
	/*}*/
}

void Playback_LoadVideo(App* app, char* path, float startTime) {
	log_trace("Playback_LoadVideo()");
    // https://mpv.io/manual/stable/#command-interface-[%3Coptions%3E]]]
	// const char* cmd[] = { "loadfile", path, NULL };
    char startStr[64];
    snprintf(startStr, sizeof(startStr), "start=%.6f", startTime);
	const char* cmd[] = { "loadfile", path, "replace", "-1", startStr, NULL };
    App_Queue_AddCommand(app, cmd);

	/*if (mpv_command_async(app->mpv, 0, cmd) != MPV_ERROR_SUCCESS) {*/
	/*	log_error("Failed loading file");*/
	/*	return;*/
	/*}*/
}

void Playback_StepFrames(App* app, bool forwards) {
	log_trace("Playback_StepFrames()");
    // NOTE: behaviour of this seems to have changed in newer MPV versions in case i update
    if (forwards) {
        const char* cmd[] = { "frame-step", NULL }; 
        App_Queue_AddCommand(app, cmd);
    } else {
        const char* cmd[] = { "frame-back-step", NULL }; 
        App_Queue_AddCommand(app, cmd);

    }

}

void Playback_SetPaused(App* app, bool pause) {
    const char* state = pause ? "yes" : "no";
    const char* cmd_pause[] = { "set", "pause", state, NULL };
    App_Queue_AddCommand(app, cmd_pause);
}

void Playback_Stop(App* app) {
	log_trace("Playback_Stop()");
    const char* cmd[] = { "stop", NULL };
    App_Queue_AddCommand(app, cmd);
}
