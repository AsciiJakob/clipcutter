#include "pch.h"
#include "window.h"
#include "app.h"
#include "ui.h"
#include "mediaSource.h"
#include "mediaClip.h"
#include "playback.h"


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



#if defined(CC_PLATFORM_WINDOWS)
int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
    const int argc = __argc;
    char** argv = __argv;
    cc_unused(hInstance);
    cc_unused(hPrevInstance);
    cc_unused(lpCmdLine);
    cc_unused(nShowCmd);
#else
int main(int argc, char* argv[]) {
#endif

    #if defined(CC_PLATFORM_WINDOWS) && defined(CC_BUILD_DEBUG)
        // if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
        AllocConsole();
        FILE* f;
        freopen_s(&f, "conout$", "w", stdout);
        freopen_s(&f, "conout$", "w", stderr);
        // }
        
    #endif
    printf("Clipcutter v0.0.1\n");

    App* app = (App*) malloc(sizeof(App));
    App_Init(app);
    App_CalculateTimelineEvents(app);


    // window init

    if (!initWindow(app)) {
        die("failed to initialize window, shutting down");
    }

    // we have to reset the lavfi option every time we load a new video.
    // Otherwise it might try to load too many audio tracks, causing the video to not load
    const char* cmd[] = { "set", "options/reset-on-next-file", "lavfi-complex", NULL };
    mpv_command_async(app->mpv, 0, cmd);

    mpv_observe_property(app->mpv, 0, "playback-time", MPV_FORMAT_DOUBLE);



    // Main loop
    bool done = false;

    if (argc > 1) {
        const char* cmd2[] = { "set", "options/lavfi-complex", "[aid1][aid2]amix[ao]", NULL };
        mpv_command_async(app->mpv, 0, cmd2);

        MediaSource* argVideo = App_CreateMediaSource(app, argv[1]);
        // MediaClip* argClip = App_CreateMediaClip(app, argVideo);
        App_CreateMediaClip(app, argVideo);

        App_CalculateTimelineEvents(app);
        App_MovePlaybackMarker(app, 0);
        //App_InitNewMediaSource(app, argv[1]);
    } else {

    }



    bool mpvRedraw = false;

    while (!done) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT) {
                done = true;
            } else if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(app->window)) {
                done = true;
            } else if (event.type == SDL_EVENT_WINDOW_EXPOSED) {
                mpvRedraw = true;
            } else if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_END) {
                    printf("pressing deubg key\n");
                    Playback_SetMultipleAudioTracks(app);
                }

                if (event.key.key == SDLK_SPACE) {
                    const char* cmd_pause[] = { "cycle", "pause", NULL };
                    mpv_command_async(app->mpv, 0, cmd_pause);
                    // TODO 
                    app->playbackActive = !app->playbackActive;
                }
                if (event.key.key == SDLK_S) {
                    const char* cmd_scr[] = {
                        "screenshot-to-file",
                        "screenshot.png",
                        "window",
                        NULL 
                    };
                    printf("attempting to save screenshot to %s\n", cmd_scr[1]);
                    mpv_command_async(app->mpv, 0, cmd_scr);
                }
                //if (event.key.keysym.sym == SDLK_RIGHT) {
                    //setPositionRelative(app->mpv, 5);
                //}
            } else if (event.type == SDL_EVENT_DROP_BEGIN) {
                printf("file hovering\n");
            } else if (event.type == SDL_EVENT_DROP_FILE) {
                printf("file dropped: %s\n", event.drop.data);

                char* url = (char*) malloc(strlen(event.drop.data) + sizeof("file:") + 1);
                sprintf(url, "file:%s", event.drop.data);
                AVFormatContext* s = NULL;
                int ret = avformat_open_input(&s, url, NULL, NULL);
                if (ret < 0) {
                    printf("Error: ffmpeg failed to retrieve information about video source\n");
                    exit(1);
                }
                else printf("yay\n");
                avformat_find_stream_info(s, nullptr);
                printf("streams:%d", s->nb_streams);
                printf("duration:%.2f", s->duration/1000000.0);
                avformat_close_input(&s);

                // TODO: check if already loaded
                //App_InitNewMediaSource(app, event.drop.file);
                //printf("%s\n", event.drop.file);

            } else if (event.type == app->events.wakeupOnMpvRenderUpdate) {
                uint64_t flags = mpv_render_context_update(app->mpv_gl);
                if (flags & MPV_RENDER_UPDATE_FRAME) {
                    mpvRedraw = true;
                }
            } else if (event.type == app->events.wakeupOnMpvEvents) {
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
                    //printf("event: %s\n", mpv_event_name(mp_event->event_id));
                    if (mp_event->event_id == MPV_EVENT_END_FILE) {
                        printf("Unloading video file\n");
                        if (!app->isLoadingVideo) {
							app->loadedMediaSource = nullptr;
                        }
                    }
                    if (mp_event->event_id == MPV_EVENT_FILE_LOADED) {
                        app->isLoadingVideo = false;

                    }
                    if (mp_event->event_id == MPV_EVENT_GET_PROPERTY_REPLY) {
                        if (mp_event->error < 0) {
                            printf("Error getting reply from MPV");
                            continue;
                        } 
                    }

                    if (mp_event->event_id == MPV_EVENT_PROPERTY_CHANGE) {
                        mpv_event_property* prop = (mpv_event_property*) mp_event->data;
                        if (strcmp(prop->name, "playback-time") == 0) {
                            if (prop->data != nullptr) {
								double playtime = *(double*) prop->data;
								//printf("Playtime: %.2f\n", playtime);
                                app->playbackTime = playtime;
                            }

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

		if (!app->playbackBlocked && app->playbackActive) {
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
        ImGui_ImplSDL3_NewFrame();
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
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DestroyContext(app->gl_context);
    SDL_DestroyWindow(app->window);
    SDL_Quit();

    App_Free(app);
}
