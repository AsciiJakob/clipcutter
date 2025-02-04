#include "pch.h"
#include "window.h"
#include "app.h"
#include "ui.h"
#include "mediaSource.h"
#include "mediaClip.h"


static void die(const char* msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

//void setPositionRelative(mpv_handle* mpv, double seconds) {
//    printf("fuc\n");
//    std::string timeStr = std::to_string(seconds);
//    const char* cmd[] = { "seek", timeStr.prop(), "relative", NULL };
//    if (int result = mpv_command(mpv, cmd); result != MPV_ERROR_SUCCESS) {
//        fprintf(stderr, "Fast forward failed, reason: %s\n", mpv_error_string(result));
//    }
//}

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
    //app->playbackActive = true;
	App_CalculateTimelineEvents(app);


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
    printf("did the load thing for file\n");


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
                    app->playbackActive = !app->playbackActive;
				}
				if (event.key.keysym.sym == SDLK_s) {
					const char* cmd_scr[] = { "screenshot-to-file",
											 "screenshot.png",
											 "window",
											 NULL };
					printf("attempting to save screenshot to %s\n", cmd_scr[1]);
					mpv_command_async(app->mpv, 0, cmd_scr);
				}
				//if (event.key.keysym.sym == SDLK_RIGHT) {
					//setPositionRelative(app->mpv, 5);
				//}
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
                            //printf("log: %s", msg->text);
                        continue;
                    }
                    //printf("event: %s\n", mpv_event_name(mp_event->event_id));
                    if (mp_event->event_id == MPV_EVENT_END_FILE) {
                        printf("Unloading video file\n");
                        app->loadedMediaSource = nullptr;
                    }
                    if (mp_event->event_id == MPV_EVENT_FILE_LOADED) {
                        printf("file loaded event\n");
                        static bool temp = false; // TODO: temp
                        if (temp) continue;
                        temp = true;
                        MediaSource* mediaSource = (MediaSource*) malloc(sizeof MediaSource);
                        MediaSource_Init(mediaSource, argv[1]);
                        app->mediaSources[0] = mediaSource;

                        GetPropertyCallback* callbackData = (GetPropertyCallback*) malloc(sizeof GetPropertyCallback);
                        callbackData->mediaSource = mediaSource;
                        callbackData->callback = [](GetPropertyCallback* callbackData, App* app) {
							MediaClip* mediaClip = (MediaClip*) malloc(sizeof MediaClip);
							MediaClip_Init(mediaClip, callbackData->mediaSource);
                            app->mediaClips[0] = mediaClip;

							MediaClip* mediaClip2 = (MediaClip*) malloc(sizeof MediaClip);
							MediaClip_Init(mediaClip2, callbackData->mediaSource);
                            app->mediaClips[1] = mediaClip2;
                            mediaClip2->padding = 190;
                            App_CalculateTimelineEvents(app);

                            app->loadedMediaSource = callbackData->mediaSource;
                            app->playbackActive = true;

                        };

                        callbackData->remainingRetrievals = 2; // TODO: dangerous to do manually, add to func
                        mpv_get_property_async(app->mpv, (uint64_t)callbackData, "duration", MPV_FORMAT_INT64);
                        mpv_get_property_async(app->mpv, (uint64_t)callbackData, "track-list/count", MPV_FORMAT_INT64);

                    }
                    if (mp_event->event_id == MPV_EVENT_GET_PROPERTY_REPLY) {
                        if (mp_event->error < 0) {
                            printf("Error getting reply from MPV");
                            continue;
                        } 

                        mpv_event_property* prop = (mpv_event_property*) mp_event->data;
                        printf("Property name: %s\n", prop->name);
                        int* data = (int*) prop->data;
                       GetPropertyCallback* callbackData = (GetPropertyCallback*) mp_event->reply_userdata;
                        MediaSource* src = callbackData->mediaSource;

                        if (strcmp(prop->name, "duration")==0) {
                            src->length = *data;
                        } else if(strcmp(prop->name, "track-list/count")==0) {
                            src->audioTracks = *data-1;
                        }

                        if (--callbackData->remainingRetrievals == 0) {
                            callbackData->callback(callbackData, app);
                        }
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

		if (app->playbackActive) {
			app->playbackTime += ImGui::GetIO().DeltaTime;

            // handle events
            TimelineEvent* nextEvent = App_GetNextTimelineEvent(app);
            if (nextEvent != nullptr && app->playbackTime >= nextEvent->start) {
                printf("new event!\n");
				app->timelineEventIndex++;
                App_LoadEvent(app, nextEvent);

                // TODO: handle end event (don't increment)
            }
		}

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        UI_DrawEditor(app);

        // Rendering
        ImGui::Render();
        glViewport(0, 0, (int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y);
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