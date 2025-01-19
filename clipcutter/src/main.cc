#include "pch.h"
#include "window.h"
#include "app.h"
#include "ui.h"
#include "mediaSource.h"
#include "mediaClip.h"

// globals, put in some kind of struct
bool playbackActive = false;
float playbackTime = 0;
float timemarkerPos = 0; // secToWidth(playbackTime)
bool snappingEnabled = 1;
int snappingPrecision = 10;
int trackHeight = 30;

static void die(const char* msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

void setPositionRelative(mpv_handle* mpv, double seconds) {
    printf("fuc\n");
    std::string timeStr = std::to_string(seconds);
    const char* cmd[] = { "seek", timeStr.data(), "relative", NULL };
    if (int result = mpv_command(mpv, cmd); result != MPV_ERROR_SUCCESS) {
        fprintf(stderr, "Fast forward failed, reason: %s\n", mpv_error_string(result));
    }
}

void setMultipleAudioTracks(mpv_handle* mpv) {
    const char* cmd[] = { "set", "options/lavfi-complex", "[aid1] [aid2] amix [ao]", NULL };
    if (int result = mpv_command(mpv, cmd); result != MPV_ERROR_SUCCESS) {
        fprintf(stderr, "Multiple audio tracks failed, reason: %s\n", mpv_error_string(result));
    }
}


#if defined(CC_PLATFORM_WINDOWS)
int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
    const int argc = __argc;
    char** argv = __argv;
#else
int main(int argc, char* argv[]) {
#endif // _WIN32
#if defined(CC_PLATFORM_WINDOWS) && defined(CC_BUILD_DEBUG)
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
        AllocConsole();
        FILE* f;
        freopen_s(&f, "conout$", "w", stdout);
        freopen_s(&f, "conout$", "w", stderr);
    }
#endif

    if (argc != 2) {
        die("pass a single media file as argument");
    }


	App* app = (App*) malloc(sizeof App);
    App_Init(app);



    // window init

    if (!initWindow(app)) {
        die("failed to initialize window, shutting down");
    }

    //setMultipleAudioTracks(mpv);

    // Main loop
    bool done = false;

    const char* cmd[] = { "loadfile", argv[1], NULL };
    if (mpv_command_async(app->mpv, 0, cmd) != MPV_ERROR_SUCCESS) {
        printf("Error: Failed loading file");
        return false;
    }


    bool mpvRedraw = false;

    while (!done) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(app->window))
                done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_EXPOSED)
                mpvRedraw = true;
			if (event.type == SDL_KEYDOWN) {
				if (event.key.keysym.sym == SDLK_SPACE) {
					const char* cmd_pause[] = { "cycle", "pause", NULL };
					mpv_command_async(app->mpv, 0, cmd_pause);
				}
				if (event.key.keysym.sym == SDLK_s) {
					const char* cmd_scr[] = { "screenshot-to-file",
											 "screenshot.png",
											 "window",
											 NULL };
					printf("attempting to save screenshot to %s\n", cmd_scr[1]);
					mpv_command_async(app->mpv, 0, cmd_scr);
				}
				if (event.key.keysym.sym == SDLK_RIGHT) {
					setPositionRelative(app->mpv, 5);
				}
            }
			if (event.type == app->events.wakeupOnMpvRenderUpdate) {
				uint64_t flags = mpv_render_context_update(app->mpv_gl);
				if (flags & MPV_RENDER_UPDATE_FRAME) {
					mpvRedraw = true;
				}
			}
            if (event.type == app->events.wakeupOnMpvEvents) {
                while (1) {
                    mpv_event* mp_event = mpv_wait_event(app->mpv, 0);
                    if (mp_event->event_id == MPV_EVENT_NONE) {
                        break;
                    }

                    if (mp_event->event_id == MPV_EVENT_LOG_MESSAGE) {
                        mpv_event_log_message* msg = static_cast<mpv_event_log_message*>(mp_event->data);
                        if (strstr(msg->text, "DR image"))
                            printf("log: %s", msg->text);
                        continue;
                    }
                    printf("event: %s\n", mpv_event_name(mp_event->event_id));
                    if (mp_event->event_id == MPV_EVENT_FILE_LOADED) {
                        MediaSource* mediaSource = (MediaSource*) malloc(sizeof MediaSource);
                        MediaSource_Init(mediaSource, mpv_get_property_string(app->mpv, "path"));
                        app->mediaSources[0] = mediaSource;
                        mpv_get_property(app->mpv, "track-list/count", MPV_FORMAT_INT64, &mediaSource->audioTracks);
                        mediaSource->audioTracks -= 1; // track-list/count counts the video track too. TODO: see if there is a property for actual count of audio tracks
                        printf("audio tracks: %d", mediaSource->audioTracks);


                        MediaClip* mediaClip = (MediaClip*) malloc(sizeof MediaClip);
                        MediaClip_Init(mediaClip, mediaSource);
                    }
                }
            }
        }

        if (SDL_GetWindowFlags(app->window) & SDL_WINDOW_MINIMIZED) {
            SDL_Delay(10);
            continue;
        }

        if (mpvRedraw) {
            renderMpvTexture(app);
        } else {
			printf("not redraw");
        }


        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        UI_DrawEditor(app);

        // Rendering
        ImGui::Render();
        glViewport(0, 0, (int)app->io.DisplaySize.x, (int)app->io.DisplaySize.y);
		ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(app->window);
    }

    // Cleanup
    mpv_render_context_free(app->mpv_gl);
    mpv_destroy(app->mpv);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(app->gl_context);
    SDL_DestroyWindow(app->window);
    SDL_Quit();
}