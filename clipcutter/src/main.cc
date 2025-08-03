#include "pch.h"
#include "window.h"
#include "app.h"
#include "ui.h"
#include "mediaSource.h"
#include "mediaClip.h"
#include "playback.h"
#include "export.h"
#include <SDL3/SDL_keycode.h>


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
    // __debugbreak();
    log_info("Clipcutter v0.0.1");

    App* app = (App*) malloc(sizeof(App));
    App_Init(app);
    /*app->playbackActive = true;*/
    App_CalculateTimelineEvents(app);


    // window init

    if (!initWindow(app)) {
        log_fatal("failed to initialize window, shutting down");
        App_Die();
    }

    // we have to reset the lavfi option every time we load a new video.
    // Otherwise it might try to load too many audio tracks, causing the video to not load
    /*const char* cmd[] = { "set", "options/reset-on-next-file", "lavfi-complex", NULL };*/
    /*App_Queue_AddCommand(app, cmd);*/

    mpv_observe_property(app->mpv, 0, "playback-time", MPV_FORMAT_DOUBLE);




    if (argc > 1) {
        cc_unused(argv);
        MediaSource* argVideo = App_CreateMediaSource(app, argv[1]);
        if (argVideo != nullptr)  {
            App_CreateMediaClip(app, argVideo);
            App_CalculateTimelineEvents(app);
            App_MovePlaybackMarker(app, 0);
        } else {
            log_error("Failed to import video source");
        }

        //
        //
        MediaSource* secondVid = App_CreateMediaSource(app, "D:/notCDrive/Videos/cc_debug/another-2-AT.mp4");
        if (secondVid != nullptr)  {
            App_CreateMediaClip(app, secondVid);
            App_CalculateTimelineEvents(app);
            App_MovePlaybackMarker(app, 0);
        } else {
            log_error("Failed to import video source");
        }

        /*MediaSource* thirdVid = App_CreateMediaSource(app, "D:/notCDrive/Videos/cc_debug/yetanother-2-AT.mp4");*/
        /*App_CreateMediaClip(app, thirdVid);*/
        /*App_CalculateTimelineEvents(app);*/
        /**/
        /**/
        /*MediaSource* fourthVid = App_CreateMediaSource(app, "D:/notCDrive/Videos/cc_debug/3-audiotracks.mp4");*/
        /*App_CreateMediaClip(app, fourthVid);*/
        /*App_CalculateTimelineEvents(app);*/
    } else {

    }


    /*for (int i=0; i < 200; i++) {*/
    /*    log_debug("I: %d", i);*/
    /*    if (i== 27) {*/
    /*        log_debug("27!!!!!!!!!!!!!!");*/
    /*    }*/
    /*    const char* cmd[] = { "set", "options/reset-on-next-file", "lavfi-complex", NULL };*/
    /*    App_QueueCommand(app, cmd);*/
    /*}*/


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
                if (event.key.key == SDLK_END) {
                    log_debug("Pressing debug key");
                }

                if (event.key.key == SDLK_DELETE) {
                    if (app->selectedClips.size != 0)  {
                        for (size_t i=0; i < app->selectedClips.size; i++) {
                            App_DeleteMediaClip(app, (MediaClip*) app->selectedClips.items[i]);
                        }
                        DynArr_Init(&app->selectedClips, sizeof(MediaClip*));

                    }
                }

                // split clip
                if (event.key.key == SDLK_S) {
                    TimelineEvent* currentEvent = &app->timelineEvents[app->timelineEventIndex];
                    if (currentEvent->type == TIMELINE_EVENT_VIDEO) {
                        MediaClip_Split(app, currentEvent->clip, app->playbackTime);

                        App_CalculateTimelineEvents(app);
                    }

                }

                if (event.key.key == SDLK_SPACE) {
                    app->playbackActive = !app->playbackActive;
                    Playback_SetPaused(app, !app->playbackActive);
                }


                if (SDL_GetModState() & SDL_KMOD_CTRL) {
                    if (event.key.key == SDLK_A) {
                        for (int i=0; i < MEDIACLIPS_SIZE; i++) {
                            MediaClip* clip = app->mediaClips[i];
                            if (clip == nullptr) break;
                            clip->isSelected = true;
                            DynArr_Append(&app->selectedClips, clip);
                        }
                    }
                }

                //if (event.key.keysym.sym == SDLK_RIGHT) {
                    //setPositionRelative(app->mpv, 5);
                //}
            } else if (event.type == SDL_EVENT_DROP_BEGIN) {
                log_debug("file hovering");
            } else if (event.type == SDL_EVENT_DROP_FILE) {
                log_debug("file dropped: %s\n", event.drop.data);

                // TODO: check if already loaded
                MediaSource* mediaSource = App_CreateMediaSource(app, (char*) event.drop.data);
                if (mediaSource == nullptr) {
                    log_fatal("Failed to import media clip");
                } else {
                    App_CreateMediaClip(app, mediaSource);
                    App_CalculateTimelineEvents(app);
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
                        if (strstr(msg->text, "DR image"))
                            /*log_info("MPV: %s", msg->text);*/
                        continue;
                    }
                    //log_debug("event: %s", mpv_event_name(mp_event->event_id));
                    if (mp_event->event_id == MPV_EVENT_END_FILE) {
                        log_info("Unloading video file\n");
                        if (!app->isLoadingVideo) {
							app->loadedMediaSource = nullptr;
                        }
                    }
                    if (mp_event->event_id == MPV_EVENT_FILE_LOADED) {
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
            log_debug("new event! Type: %d\n", nextEvent->type);
            app->timelineEventIndex++;
            App_LoadEvent(app, nextEvent);
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
