#include "pch.h"
#include "window.h"
#include "app.h"
#include "ui.h"
#include "mediaSource.h"
#include "mediaClip.h"
#include "playback.h"
#include "export.h"
#include <SDL3/SDL_keycode.h>

void initConsole() {
     FILE* f;
     freopen_s(&f, "conout$", "w", stdout);
     freopen_s(&f, "conout$", "w", stderr);
}

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
    bool consoleAttached;
    if ((consoleAttached = AttachConsole(ATTACH_PARENT_PROCESS))) {
        initConsole();
        fputs("\n", stdout);
        fflush(stdout);
    }

    ArgParseError err = ARGPARSE_ERROR_SUCCESS;
    err = ArgParse_RegisterVariadicParameter("video file(s)");
    if (!err)
        err = ArgParse_RegisterFlag("debug", 'd', "Enable debug logging");
    if (!err)
        err = ArgParse_RegisterFlag("test", 't', NULL);
#ifdef CC_BUILD_DEBUG
    if (!err)
        err = ArgParse_RegisterFlag("imgui-demo-window", 'i', "Show imgui demo window");
#endif
    if (!err)
        err = ArgParse_Parse(argc, argv);
    
    if (err != ARGPARSE_ERROR_SUCCESS) {
        if (!consoleAttached) {
            // TODO: message box for linux
#if defined(CC_PLATFORM_WINDOWS)
            MessageBoxA(
                nullptr,
                ArgParse_GetErrorStr(),
                "Failed parsing arguments",
                MB_OK | MB_ICONERROR
            );
#endif
        } else {
            log_error("%s", ArgParse_GetErrorStr());
            ArgParse_ShowHelpMessage();
            ArgParse_Free();
        }
        exit(1);
        //ArgParse_Free()
    }

    if (ArgParse_IsFlagSet("debug")) {
        if (consoleAttached)
            FreeConsole();
#if defined(CC_PLATFORM_WINDOWS)
        AllocConsole();
        initConsole();
#endif
        log_debug("Debug console allocated")
    }


    log_info("Clipcutter v0.0.1 ");
    App* app = (App*) malloc(sizeof(App));
    App_Init(app);
    /*app->playbackActive = true;*/
    App_CalculateTimelineEvents(app);

    int inputVideoCount = 0;
    char** inputVideos = ArgParse_GetVariadicValues(&inputVideoCount);
    for (int i=0; i < inputVideoCount; i++) {
        char* vidPath = inputVideos[i];
        log_debug("Video arg path: %s", vidPath);

        MediaSource* argVideo = App_CreateMediaSource(app, vidPath);
        if (argVideo != nullptr)  {
            App_CreateMediaClip(app, argVideo);
            App_CalculateTimelineEvents(app);
        } else {
            log_error("Failed to import video source");
        }
    }

    // window init
    if (!initWindow(app)) {
        log_fatal("failed to initialize window, shutting down");
        App_Die();
    }

    App_MovePlaybackMarker(app, 0);

    // we have to reset the lavfi option every time we load a new video.
    // Otherwise it might try to load too many audio tracks, causing the video to not load
    /*const char* cmd[] = { "set", "options/reset-on-next-file", "lavfi-complex", NULL };*/
    /*App_Queue_AddCommand(app, cmd);*/

    mpv_observe_property(app->mpv, 0, "playback-time", MPV_FORMAT_DOUBLE);

    // Main loop
    bool done = false;
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
                // handled by ImGui. See App_ProcessKeyboardShortcuts().
            } else if (event.type == SDL_EVENT_DROP_BEGIN) {
                log_debug("file hovering");
            } else if (event.type == SDL_EVENT_DROP_FILE) {
                log_debug("file dropped: %s\n", event.drop.data);

                // TODO: check if already loaded
                MediaSource* mediaSource = App_CreateMediaSource(app, (char*) event.drop.data);
                if (mediaSource == nullptr) {
                    log_fatal("Failed to import media clip");
                } else {
                    MediaClip* newClip = App_CreateMediaClip(app, mediaSource);
                    App_CalculateTimelineEvents(app);
                    if (MediaClip_IsUnderTimeMarker(app, newClip)) {
                        App_MovePlaybackMarker(app, app->playbackTime);
                    }

                }

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
                        mpv_event_log_message* msg = (mpv_event_log_message*) (mp_event->data);
                        LOG_LEVEL level = LOG_LEVEL_INFO;
                        if (strcmp(msg->level, "error") == 0) level = LOG_LEVEL_ERROR;
                        if (strcmp(msg->level, "fatal") == 0) level = LOG_LEVEL_FATAL;
                        log_message(level, __FILE__, __LINE__, "[MPV] %s", msg->text);
                        continue;
                    }
                    //log_debug("event: %s", mpv_event_name(mp_event->event_id));
                    if (mp_event->event_id == MPV_EVENT_END_FILE) {
                        log_info("Unloading video file\n");
                        if (!app->isLoadingVideo) {
							app->loadedMediaSource = nullptr;
                        }

                        mpv_end_file_reason* reason = (mpv_end_file_reason*) mp_event->data;
                        if (*reason == MPV_END_FILE_REASON_ERROR) {
                            log_error("mpv playback of video stopped because of an error.");
                            if (app->isLoadingVideo) {
                                app->isLoadingVideo = false;
                                app->loadedMediaSource = nullptr;
                            }
                        }
                    }
                    if (mp_event->event_id == MPV_EVENT_FILE_LOADED) {
                        log_debug("MPV event: file loaded!")
                        app->isLoadingVideo = false;
                        app->playbackBlocked = false;
                        if (app->playbackActive == false) {
                            Playback_SetPaused(app, !app->playbackActive);
                        }

                        /*Playback_SetAudioTracks(app, app->loadedMediaSource->audioTracks);*/

                    }
                    if (mp_event->event_id == MPV_EVENT_GET_PROPERTY_REPLY) {
                        if (mp_event->error < 0) {
                            log_error("Error getting reply from MPV");
                            continue;
                        } 
                    }

                    if (mp_event->event_id == MPV_EVENT_PROPERTY_CHANGE) {
                        mpv_event_property* prop = (mpv_event_property*) mp_event->data;
                        if (strcmp(prop->name, "playback-time") == 0) {
                            if (prop->data != nullptr) {
								double playtime = *(double*) prop->data;
                                /*log_debug("playback: %.9f", playtime);*/
                                TimelineEvent* currentEvent = &app->timelineEvents[app->timelineEventIndex];
                                // TODO COME BACK HERE
                                if (currentEvent->type == TIMELINE_EVENT_VIDEO) {
                                    if (playtime == 0.0 && app->playbackTime != 0.0) {
                                        double seekTime = app->playbackTime-currentEvent->start+currentEvent->clip->startCutoff;
                                        /*double seekTime = app->playbackTime-currentEvent->start;*/
                                        if (seekTime > 0.1) {
                                            Playback_SetPlaybackPos(app, seekTime);
                                            log_trace("Syncing MPV playback time with cursor. Seeking to: %.6f", seekTime);
                                        }
                                    } else {
                                        app->playbackTime = currentEvent->start+playtime-currentEvent->clip->startCutoff;
                                    }
                                } else {
                                    log_error("got playback update with no clip loaded");
                                    /*assert(true && "got playback update with no clip loaded");*/
                                }
                            }

                        }
                    }
                    if (mp_event->event_id == MPV_EVENT_COMMAND_REPLY) {
                        if ((int) mp_event->reply_userdata == app->mpvCmdQueueReadIndex+1) {
                            if (app->MpvCmdQueue[app->mpvCmdQueueReadIndex].unsent == false) {
                                log_error("Id for mpv command matched, but we haven't written to it yet. odd.")
                            } else {
                                app->MpvCmdQueue[app->mpvCmdQueueReadIndex].unsent = false;

                                log_debug("Recived Confirmation of MPV command of type: %s", app->MpvCmdQueue[app->mpvCmdQueueReadIndex].command);

                                app->mpvCmdQueueReadIndex++;
                                if (app->mpvCmdQueueReadIndex > MPV_CMD_QUEUE_SIZE-1) {
                                    app->mpvCmdQueueReadIndex = 0;
                                }
                                App_Queue_SendNext(app);
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
			log_debug("not redraw");
        }

        // increment app->playbackTime if blank space is being played (if a video is loaded we set it based on the mpv value from the MPV_EVENT_PROPERTY_CHANGE event)
		if (app->loadedMediaSource == nullptr && !app->playbackBlocked && app->playbackActive) {
			app->playbackTime += ImGui::GetIO().DeltaTime;

		}

        // handle events
        TimelineEvent* nextEvent = App_GetNextTimelineEvent(app);
        if (ImGui::IsKeyDown(ImGuiKey_DownArrow)) {
            log_debug("holding down test key");
        }
        if (nextEvent != nullptr && app->playbackTime >= nextEvent->start) {
            log_debug("new event! Type: %s\n", TimelineEventType_ToString(nextEvent->type));
            App_LoadEvent(app, nextEvent, true);
            app->timelineEventIndex++;
        }


        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        App_ProcessKeyboardShortcuts(app);
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
    ArgParse_Free();
}
