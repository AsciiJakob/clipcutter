#include "pch.h"
#include "app.h"
//#include "mediaClip.h"

int tracklistWidth = 100;

int trackCount = 5; // Todo: make function to get the max tracks of all the clips

void setPlaybackPos(mpv_handle* mpv, double seconds) {
	printf("fuc\n");
	std::string timeStr = std::to_string(seconds);
	const char* cmd[] = { "seek", timeStr.data(), "absolute", NULL };
	if (int result = mpv_command(mpv, cmd); result != MPV_ERROR_SUCCESS) {
		fprintf(stderr, "Fast forward failed, reason: %s\n", mpv_error_string(result));
	}
}

void UI_DrawEditor(App* app) {
	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::Button("Load File")) {
                        //char* thing = mpv_get_property_string(app->mpv, "path");
                        //printf(thing);
		}
		ImGui::EndMainMenuBar();
	}

	if (ImGui::Begin("DebugThingies")) {
		ImGui::Text("playbacktime: %.2f", app->playbackTime);
		ImGui::Text("scaling: %.2f", app->timeline.scaleX);

		ImGui::Text("------Track 1:");
		MediaClip* testClip = app->mediaClips[0];
		if (testClip != NULL) {
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
			bool snappingEnabled = !ImGui::IsKeyDown(ImGuiKey_LeftShift);

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
			ImVec2 cursorTimelineAfter = ImGui::GetCursorScreenPos();
			ImGui::GetWindowDrawList()->AddRectFilled(r_min, r_max, timeline_color);

			bool timelineClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

			ImGui::SetCursorScreenPos(cursorTimelineBefore);

			for (int i = 0; i < 200; i++) { // draw clips
				MediaClip* mediaClip = app->mediaClips[i];
				if (mediaClip == NULL) break;
				MediaClip_Draw(app, mediaClip);
			}

			{ // timeMarker
				if (app->playbackActive) {
					app->playbackTime += ImGui::GetIO().DeltaTime;
				}
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
						if (snappingEnabled && clip != NULL) {
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

						
						app->selectedTrack = NULL;
						float secs = (mousePos.x - cursorTimelineBefore.x)/app->timeline.scaleX;
						app->playbackTime = secs;
						setPlaybackPos(app->mpv, secs);
					}
				}
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
		static float f = 0.0f;
		static int counter = 0;

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

			ImGui::Image((void*)(intptr_t)app->mpv_texture, displaySize);
		}
		ImGui::End();
	}
}
