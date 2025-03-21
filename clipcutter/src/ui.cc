#include "pch.h"
#include "export.h"
#include "app.h"
#include <imgui.h>
//#include "mediaClip.h"

int tracklistWidth = 100;

int trackCount = 5; // Todo: make function to get the max tracks of all the clips

int exportPathInputCallback(ImGuiInputTextCallbackData data) {
    /*if (data.EventFlag == ImGuiInputTextFlags_CallbackCompletion) {*/
    /**/
    /*}*/
    log_debug("Buffer: %s", data.Buf);
    return 0;
}

void UI_DrawEditor(App* app) {

	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::Button("Load File")) {

            static const SDL_DialogFileFilter filters[] = {
                { "Video files (mp4;avi)", "mp4;avi" }, // todo full list of supported formats
                { "All images", "png;jpg;jpeg" },
                { "All files", "*" }
            };

            void (*callback)(void* userdata, const char* const* filelist, int count) =
            [](void* userdata, const char* const* filelist, int count) -> void {
                App* app = (App*) userdata;
                cc_unused(count);
                cc_unused(userdata);
                cc_unused(filelist);
                if (!filelist) {
                    log_error("File dialog error: %s", SDL_GetError());
                    return;
                } else if (!*filelist) {
                    log_info("User cancelled file dialog");
                    return;
                }

                while (*filelist != NULL) {
                    const char* filePath = *filelist;
                    log_info("User is opening file '%s' through file dialog", filePath);


                    // TODO: check if media source is already loaded
                    MediaSource* src = App_CreateMediaSource(app, filePath);
                    MediaClip* clip = App_CreateMediaClip(app, src);
                    App_CalculateTimelineEvents(app);
                    cc_unused(clip);

                    filelist++;
                }

            };


            SDL_ShowOpenFileDialog(callback, app, app->window, filters, 3, NULL, true);
		}
		if (ImGui::Button("export")) {
            log_debug("Clicked button for opening export modal");
            ImGui::OpenPopup("ExportModal");
		}

        if (ImGui::BeginPopupModal("ExportModal")) {
            ImGui::Text("Hello");


            ImGui::InputTextWithHint("Export path", "Path to export to", app->exportPath, sizeof(app->exportPath), ImGuiInputTextFlags_AutoSelectAll, NULL, nullptr);

            if (ImGui::Button("Select in file explorer")) {

                static const SDL_DialogFileFilter filters[] = {
                    { "Video files (mp4;avi)", "mp4;avi" }, // todo full list of supported formats
                    { "All images", "png;jpg;jpeg" },
                    { "All files", "*" }
                };

                void (*callback)(void* userdata, const char* const* filelist, int count) =
                [](void* userdata, const char* const* filelist, int count) -> void {
                    App* app = (App*) userdata;
                    cc_unused(app);
                    cc_unused(count);
                    cc_unused(userdata);
                    cc_unused(filelist);
                    if (!filelist) {
                        log_error("File dialog error: %s", SDL_GetError());
                        return;
                    } else if (!*filelist) {
                        log_info("User cancelled file dialog");
                        return;
                    }

                    while (*filelist != NULL) {
                        const char* filePath = *filelist;
                        log_info("User is opening file '%s' through file dialog", filePath);
                        strcpy(app->exportPath, filePath);

                        filelist++;
                    }

                };

                SDL_ShowSaveFileDialog(callback, app, app->window, filters, 3, app->exportPath);
            }

            if (ImGui::Button("Remux Video, Merge audiotracks")) {
                std::thread thread_obj(exportVideo, app, true);
                thread_obj.detach();
                /*exportVideo(app, true);*/
            };
            if (ImGui::Button("Remux")) {
                exportVideo(app, false);
            };

            /*ImGui::Text("Frame: %.2f", app->exportFrame);*/
            ImGui::ProgressBar(app->exportFrame);

            if (ImGui::Button("Close")) {
                ImGui::ClosePopupToLevel(0, false);
            }

            ImGui::EndPopup();
        }

		ImGui::EndMainMenuBar();
	}


    static bool dockBuilderHasInitialized = false;
    ImGuiID dockspace_id = ImGui::DockSpaceOverViewport(0);
    if (!dockBuilderHasInitialized) {
        dockBuilderHasInitialized = true;
        ImGui::DockBuilderRemoveNode(dockspace_id); // Clear out existing layout
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace); // Add empty node
        ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

        ImGuiID dock_main_id = dockspace_id;
        ImGuiID dock_id_up;
        /*ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.80f, NULL, &dock_main_id);*/
        /*ImGuiID dock_id_bottom = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.00f, NULL, &dock_main_id);*/
        ImGuiID dock_id_down = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.35f, nullptr, &dock_id_up);
        /*ImGuiID dock_id_up = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Up, 2.20f, NULL, &dock_main_id);*/
        ImGuiID dock_id_up_right;
        ImGuiID dock_id_up_left = ImGui::DockBuilderSplitNode(dock_id_up, ImGuiDir_Left, 0.20f, nullptr, &dock_id_up_right);
        /*ImGuiID dock_id_up_right = ImGui::DockBuilderSplitNode(dock_id_up, ImGuiDir_Right, 0.3f, nullptr, &dock_id_up);*/


        ImGui::DockBuilderDockWindow("Timeline", dock_id_down); // dock_main_id docks it to the center of the main docking thing
        ImGui::DockBuilderDockWindow("DebugThingies", dock_id_up_left);
        ImGui::DockBuilderDockWindow("Video Player", dock_id_up_right);

        ImGui::DockBuilderFinish(dockspace_id);
    }

	if (ImGui::Begin("DebugThingies")) {
		ImGui::Text("playbacktime: %.2f", app->playbackTime);
		ImGui::Text("playbackActive: %d", app->playbackActive);
		ImGui::Text("scaling: %.2f", app->timeline.scaleX);
		ImGui::Text("timelineEvent: %d", app->timelineEvents[app->timelineEventIndex].type);
		if (app->loadedMediaSource != nullptr) {
			ImGui::Text("currentLoaded: %s", app->loadedMediaSource->filename);
		}

		ImGui::Text("------Track 1:");
		MediaClip* testClip = app->mediaClips[0];
		if (testClip != nullptr) {
			ImGui::Text("length: %.2f", testClip->source->length);
			ImGui::Text("width: %.2f", testClip->width);
			ImGui::Text("padding: %.2f", testClip->padding);
			ImGui::Text("cutoffstart: %.2f", testClip->drawStartCutoff);
			ImGui::Text("cutoffend: %.2f", testClip->drawEndCutoff);

			ImGui::Checkbox("track1beingMoved", &testClip->isBeingMoved);
		}

		//local iPtr = ffi.new("int[1]", 0)
		//iPtr[0] = track1Height
		//imgui.SliderInt("trackHeight", iPtr, 10, 100, "%d")
		//track1Height = iPtr[0]
		//iPtr[0] = trackCount
		//imgui.SliderInt("trackCount", iPtr, 0, 50, "%d")
		//trackCount = iPtr[0]
		//iPtr[0] = snappingPrecision
		//imgui.SliderInt("snappingPrecision", iPtr, 1, 50, "%d") --disabled when 1
		//snappingPrecision = iPtr[0]

	}
	ImGui::End();


	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(0, 0));

	if (ImGui::Begin("Timeline")) {
		ImVec2 cursorTrackListBefore;
		ImVec2 cursorTracklistAfter;
		{ // Tracklist
			ImGui::BeginGroup();
			ImU32 tracklistColor = ImGui::GetColorU32(ImVec4(0.15, 0.15, 0.15, 1));
			//ImVec2 tracklistSize = ImVec2(tracklistWidth, std::max(ImGui::GetContentRegionAvail().y, (trackCount + 3) * track1Height));
			ImVec2 tracklistSize = ImVec2(tracklistWidth, fmax(ImGui::GetContentRegionAvail().y, (float)((trackCount + 3) * app->timeline.clipHeight)));

			cursorTrackListBefore = ImGui::GetCursorScreenPos();
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
			ImGui::Dummy(tracklistSize);
			ImGui::PopStyleVar();
			ImVec2 r_min = ImGui::GetItemRectMin();
			ImVec2 r_max= ImGui::GetItemRectMax();
			cursorTracklistAfter = ImGui::GetCursorScreenPos();

			ImDrawList* timelineDrawlist = ImGui::GetWindowDrawList();
			timelineDrawlist->AddRectFilled(r_min, r_max, tracklistColor);
			ImGui::SetCursorScreenPos(cursorTrackListBefore);

			ImVec2 trackCursor = cursorTrackListBefore;
			for (int i = 0; i < trackCount+2; i++) {
				ImGui::Text("Track %d", i);
				ImGui::SameLine(tracklistWidth - 40);
				ImGui::SmallButton("Mute");
				trackCursor.y += app->timeline.clipHeight;
				ImGui::SetCursorScreenPos(trackCursor);

				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
				ImGui::Separator();
				ImGui::PopStyleVar();
			}
			ImGui::Dummy(ImVec2(0, 0)); // workaround.If there is no element(such as text or button or this) after the last track's ImGUI Separator then the SameL

			ImGui::SetCursorScreenPos(cursorTrackListBefore);

			//ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
			ImGui::EndGroup(); 
			//ImGui::PopStyleVar();
		}

		bool hoveringOverTrack = false;

		{ // Timeline
			ImGui::SetCursorScreenPos(cursorTracklistAfter);
			ImGui::BeginGroup();
			ImU32 timeline_color = ImGui::GetColorU32(ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
			app->timeline.snappingEnabled = !ImGui::IsKeyDown(ImGuiKey_LeftShift);

			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
			ImGui::SameLine();
			ImVec2 childSize = ImVec2(ImGui::GetContentRegionAvail().x, fmax(ImGui::GetContentRegionAvail().y, (trackCount + 3) * app->timeline.clipHeight));
			// create child window so that we can have a horizontal scrollbar for the timeline
			ImGui::BeginChild("TimelineWindowChild", childSize, false, ImGuiWindowFlags_HorizontalScrollbar);

			bool timelineHovered = ImGui::IsWindowHovered();

			ImGui::SetNextItemAllowOverlap();
			ImVec2 cursorTimelineBefore = ImGui::GetCursorScreenPos();
			app->timeline.cursTopLeft = cursorTimelineBefore; // todo: refac to use this
			ImVec2 timeline_size = ImVec2(5000, ImGui::GetContentRegionAvail().y);
			ImGui::InvisibleButton("timeline", timeline_size);
			ImGui::PopStyleVar();

			ImVec2 r_min = ImGui::GetItemRectMin();
			ImVec2 r_max = ImGui::GetItemRectMax();
			ImGui::GetWindowDrawList()->AddRectFilled(r_min, r_max, timeline_color);

			bool timelineClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

			ImGui::SetCursorScreenPos(cursorTimelineBefore);

			for (int i = 0; i < MEDIACLIPS_SIZE; i++) { // draw clips
				MediaClip* mediaClip = app->mediaClips[i];
				if (mediaClip == nullptr) break;
				MediaClip_Draw(app, mediaClip, i);
			}

			{ // timeMarker
				float timeMarkerValue = app->playbackTime*app->timeline.scaleX;

				ImGui::SetCursorScreenPos(cursorTimelineBefore);
				ImVec2 cursor_offset = ImGui::GetCursorScreenPos();
				cursor_offset.x = cursor_offset.x + timeMarkerValue;
				ImGui::SetCursorScreenPos(cursor_offset);

				ImU32 timeline_color = ImGui::GetColorU32(ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
				ImVec2 timeline_size(2, ImGui::GetContentRegionAvail().y);
				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
				ImGui::Dummy(timeline_size);
				ImGui::PopStyleVar();

				ImVec2 r_min = ImGui::GetItemRectMin();
				ImVec2 r_max = ImGui::GetItemRectMax();

				ImGui::GetWindowDrawList()->AddRectFilled(r_min, r_max, timeline_color);
			}

			{ // zooming in and out of the timeline
				if (timelineHovered && !ImGui::IsKeyDown(ImGuiKey_LeftShift)) {
					float mw = ImGui::GetIO().MouseWheel; // -1 for downwards, 1 for upwards
					float factor = 1.05f;

					if (mw != 0) {
						float oldZoom = app->timeline.scaleX;
						if (mw > 0) {
							app->timeline.scaleX = app->timeline.scaleX * factor;
						} else {
							app->timeline.scaleX = app->timeline.scaleX / factor;
						}

						float currentScrollPos = ImGui::GetScrollX();
						float timelineMousePos = ImGui::GetMousePos().x - cursorTimelineBefore.x;

						float diffBefore = timelineMousePos / oldZoom - currentScrollPos;
						float diffAfter = timelineMousePos / app->timeline.scaleX - currentScrollPos;

						float offset = diffBefore - diffAfter;
						ImGui::SetScrollX(currentScrollPos + offset * app->timeline.scaleX);
					}
				}
			}

			{ // changing playback cursor position
				if (!hoveringOverTrack && timelineClicked) {
					ImVec2 mousePos = ImGui::GetMousePos();
					if (mousePos.x > cursorTimelineBefore.x) {
						MediaClip* clip = app->mediaClips[0];
						if (app->timeline.snappingEnabled && clip != nullptr) {
							float snapSensitivity = 8;
							float track1LeftmostPos = cursorTimelineBefore.x + clip->padding * app->timeline.scaleX;
							float track1RightmostPos = cursorTimelineBefore.x + (clip->padding + clip->width) * app->timeline.scaleX;

							if (fabs(mousePos.x - track1LeftmostPos) < snapSensitivity) {
								mousePos.x = track1LeftmostPos;
							}
							else if (fabs(mousePos.x - track1RightmostPos) < snapSensitivity) {
								mousePos.x = track1RightmostPos;
							}
						}

						
						app->selectedTrack = nullptr;
						float secs = (mousePos.x - cursorTimelineBefore.x)/app->timeline.scaleX;
						app->playbackTime = secs;
						App_MovePlaybackMarker(app, secs);
					}
				}
			}

			if (ImGui::IsKeyPressed(ImGuiKey_RightCtrl)) {
				App_CalculateTimelineEvents(app);
			}

			ImGui::EndChild();
			ImGui::EndGroup();
		}


	}
	ImGui::PopStyleVar();
	ImGui::End();


	bool show_demo_window = true;
	// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
	if (show_demo_window)
		ImGui::ShowDemoWindow(&show_demo_window);

	// 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
	{

		ImGui::Begin("Video Player");
		{
			ImVec2 contentRegion = ImGui::GetContentRegionAvail();

			// Calculate aspect ratio preserving size
			float aspect = (float)app->mpv_width / app->mpv_height;
			ImVec2 displaySize = contentRegion;
			if (contentRegion.x / contentRegion.y > aspect) {
				displaySize.x = contentRegion.y * aspect;
			}
			else {
				displaySize.y = contentRegion.x / aspect;
			}

			// Center the image
			//ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2((contentRegion.x - displaySize.x) * 0.5f, (contentRegion.y - displaySize.y) * 0.5f));

			//ImGui::Image((void*)(intptr_t)app->mpv_texture, displaySize);
			ImGui::Image((ImTextureID)app->mpv_texture, displaySize);
		}
		ImGui::End();
	}

}
