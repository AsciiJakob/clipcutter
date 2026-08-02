#include "pch.h"
#include "app.h"
#include "mediaClip.h"

// Updates lavfi-complex to mix together multiple audio streams 
// and set the gain of audio streams
void Playback_ApplyLavfiComplex(App* app) {
    int audioTrackCount = app->loadedMediaSource ? app->loadedMediaSource->audioTracks : 0;
    log_trace("Playback_SetAudioTracks() with %d as count", audioTrackCount);
    // https://mpv.io/manual/stable/#options-lavfi-complex

    SB valueOptionStr;
    SB_init(&valueOptionStr, 64);

    for (int i=1; i < audioTrackCount+1; i++) { // assuming video has exactly one video track
        if (app->streamDisabled[i]) {
            SB_appendf(&valueOptionStr, "[aid%d]volume=0[a%d];", i, i);
        } else {
            SB_appendf(&valueOptionStr, "[aid%d]volume=%.3fdB[a%d];", i, app->streamAudioGain[i], i);
        }
    }

    int enabledTrackCount = 0;

    for (int i=1; i < audioTrackCount+1; i++) { // assuming video has exactly one video track
        SB_appendf(&valueOptionStr, "[a%d]", i);
        if (!app->streamDisabled[i]) {
            enabledTrackCount++;
        } 
    }
    SB_appendf(&valueOptionStr, "amix=inputs=%d[ao]", audioTrackCount);
    // Examples of what valueOptionStr can look like:
    // two audio tracks and -2dB gain on first audio stream
    // [aid1]volume=-2.000dB[a1];[aid2]volume=0.000dB[a2];[a1][a2]amix=inputs=2[ao]

    if (enabledTrackCount == 0) {
        const char* cmd[] = { "set", "options/lavfi-complex", "", NULL };
        App_Queue_AddCommand(app, cmd);
        return;
    }

    const char* cmd[] = { "set", "options/lavfi-complex", valueOptionStr.buf, NULL };
    App_Queue_AddCommand(app, cmd);
    SB_free(&valueOptionStr);
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
