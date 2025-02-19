#include "pch.h"
#include "app.h"

void Playback_SetAudioTracks(App* app, int count) {
	cc_unused(app);
    // https://mpv.io/manual/stable/#options-lavfi-complex

    assert(app->loadedMediaSource != nullptr && "loadedMediaSource was null");
    /*if (app->loadedMediaSource->audioTracks == 2) {*/
    if (count == 2) {
        log_debug("two audiotracks\n");
        const char* cmd[] = { "set", "options/lavfi-complex", "[aid1][aid2]amix[ao]", NULL };
        mpv_command_async(app->mpv, 0, cmd);
    } else if (app->loadedMediaSource->audioTracks == 3) {
        log_debug("three audiotracks\n");
        const char* cmd[] = { "set", "options/lavfi-complex", "[aid1][aid2][aid3]amix=inputs=3[ao]", NULL };
    mpv_command_async(app->mpv, 0, cmd);
    } 
}

void Playback_SetPlaybackPos(App* app, float secs) {
	std::string timeStr = std::to_string(secs);
	const char* cmd[] = { "seek", timeStr.data(), "absolute", NULL };
	if (int result = mpv_command_async(app->mpv, NULL, cmd); result != MPV_ERROR_SUCCESS) {
		log_error("Fast forward failed, reason: %s", mpv_error_string(result));
	}
}

void Playback_LoadVideo(App* app, char* path) {
	log_trace("calling loadfile\n");
	const char* cmd[] = { "loadfile", path, NULL };
	if (mpv_command_async(app->mpv, 0, cmd) != MPV_ERROR_SUCCESS) {
		log_error("Failed loading file");
		return;
	}
}
