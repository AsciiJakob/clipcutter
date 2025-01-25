#include "pch.h"
#include "app.h"

int tracklistWidth = 100;
float timelineXScale = 1.5;
int snappingPrecision = 10;
bool playbackActive = false;
float playbackTime = 0.0;
float timeMarkerValue = 0.0;


int trackCount = 2;
int track1Height = 30;
int savedTrackWidth = 180;
int savedTrackLeftPadding = 0;
bool track1isBeingMoved = false;
bool track1isSelected = false;
bool isResizingLeft = false;
bool isResizingRight = false;
float track1SavedStartCutoff = 0;
float track1SavedEndCutoff = 0;

ImVec2 moveStartPos = ImVec2(0, 0);
ImVec2 resizeStartPos = ImVec2(0, 0);

void UI_DrawEditor(App* app) {

	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::Button("Load File")) {

		}
		ImGui::EndMainMenuBar();
	}

	if (ImGui::Begin("DebugThingies")) {
		ImGui::Text("playbacktime");
		ImGui::Text("cutoffstart");
		ImGui::Text("cutoffend");
		ImGui::Text("scaling");

		ImGui::Checkbox("track1beingMoved", &track1isBeingMoved);

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
			ImVec2 tracklistSize = ImVec2(tracklistWidth, fmax(ImGui::GetContentRegionAvail().y, (float)((trackCount + 3) * track1Height)));

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
				trackCursor.y += track1Height;
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
		int trackWidth = savedTrackWidth;
		int trackLeftPadding = savedTrackLeftPadding;

		{ // Timeline
			ImGui::SetCursorScreenPos(cursorTracklistAfter);
			ImGui::BeginGroup();
			ImU32 timeline_color = ImGui::GetColorU32(ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
			bool snappingEnabled = !ImGui::IsKeyDown(ImGuiKey_LeftShift);

			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
			ImGui::SameLine();
			ImVec2 childSize = ImVec2(ImGui::GetContentRegionAvail().x, fmax(ImGui::GetContentRegionAvail().y, (trackCount + 3) * track1Height));
			// create child window so that we can have a horizontal scrollbar for the timeline
			ImGui::BeginChild("TimelineWindowChild", childSize, false, ImGuiWindowFlags_HorizontalScrollbar);

			bool timelineHovered = ImGui::IsWindowHovered();

			ImGui::SetNextItemAllowOverlap();
			ImVec2 cursorTimelineBefore = ImGui::GetCursorScreenPos();
			ImVec2 timeline_size = ImVec2(5000, ImGui::GetContentRegionAvail().y);
			ImGui::InvisibleButton("timeline", timeline_size);
			ImGui::PopStyleVar();

			ImVec2 r_min = ImGui::GetItemRectMin();
			ImVec2 r_max = ImGui::GetItemRectMax();
			ImVec2 cursorTimelineAfter = ImGui::GetCursorScreenPos();
			ImGui::GetWindowDrawList()->AddRectFilled(r_min, r_max, timeline_color);

			bool timelineClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

			ImGui::SetCursorScreenPos(cursorTimelineBefore);

			{ // Track resizing/movement
				bool mouseLetGo = !ImGui::IsMouseDown(ImGuiMouseButton_Left);
				ImVec2 mousePos = ImGui::GetMousePos();

				if (track1isBeingMoved) {
					float diff = (mousePos.x - moveStartPos.x) / timelineXScale;
					if (snappingEnabled) {
						trackLeftPadding += ceil((diff) / snappingPrecision) * snappingPrecision;
					}
					else {
						trackLeftPadding += diff;
					}
					if (trackLeftPadding < 0) {
						trackLeftPadding = 0;
					}

					if (mouseLetGo) {
						track1isBeingMoved = false;
						savedTrackLeftPadding = trackLeftPadding;
					}
				}

				// there is apprently a GetMouseDragDelta() function that I might wanna look into
				if (isResizingLeft) {
					float cutoffOffset = (mousePos.x - resizeStartPos.x) / timelineXScale;
					if (snappingEnabled) {
						cutoffOffset = floor(cutoffOffset / snappingPrecision) * snappingPrecision;
					}
					trackLeftPadding = savedTrackLeftPadding + cutoffOffset;
					float totalCutOffvalue = cutoffOffset + track1SavedStartCutoff;

					if (totalCutOffvalue < 0) {
						cutoffOffset = -track1SavedStartCutoff;
						totalCutOffvalue = cutoffOffset + track1SavedStartCutoff;
						trackLeftPadding = savedTrackLeftPadding + cutoffOffset;
					}
					if (trackLeftPadding < 0) {
						cutoffOffset = -savedTrackLeftPadding;
						totalCutOffvalue = cutoffOffset + track1SavedStartCutoff;
						trackLeftPadding = 0;
					}

					trackWidth -= cutoffOffset;

					if (mouseLetGo) {
						savedTrackLeftPadding = trackLeftPadding;
						track1SavedStartCutoff = totalCutOffvalue;
						isResizingLeft = false;
					}
				}
				else if (isResizingRight) {
					float cutoffOffset = (resizeStartPos.x - mousePos.x) / timelineXScale;
					if (snappingEnabled) {
						cutoffOffset = floor(cutoffOffset / snappingPrecision) * snappingPrecision;
					}

					float totalCutOffvalue = cutoffOffset + track1SavedEndCutoff;
					if (totalCutOffvalue < 0) { // limit resizing to the max size of the video
						cutoffOffset = -track1SavedEndCutoff;
						totalCutOffvalue = cutoffOffset + track1SavedEndCutoff;
					}
					trackWidth -= cutoffOffset;

					if (mouseLetGo) {
						track1SavedEndCutoff = totalCutOffvalue;
						isResizingRight = false;
						printf("cut away time: %f\n", totalCutOffvalue);
					}
				}
				if (mouseLetGo) {
					savedTrackWidth = trackWidth;
				}

				ImVec2 cursor_before_invisButton(0, 0);
				ImVec2 cursor_after_invisButton(0, 0);
				for (int i = 0; i <= trackCount-1; i++) {
					cursor_before_invisButton = ImGui::GetCursorScreenPos();
					ImVec2 cursor_before_invisButton_padded = ImGui::GetCursorScreenPos();
					cursor_before_invisButton_padded.x = (cursor_before_invisButton_padded.x + trackLeftPadding * timelineXScale);
					ImVec2 track_size(trackWidth * timelineXScale, track1Height);

					ImGui::SetCursorScreenPos(cursor_before_invisButton_padded);
					ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
					bool trackselected = false;
					if (i <= trackCount) {
						ImGui::InvisibleButton(("track1Button#" + std::to_string(i)).c_str(), track_size);
					}
					else {
						ImGui::Dummy(track_size);
					}

					ImGui::PopStyleVar();

					cursor_after_invisButton = ImGui::GetCursorScreenPos();
					if (i <= trackCount) {
						ImVec2 r_min = ImGui::GetItemRectMin();
						ImVec2 r_max = ImGui::GetItemRectMax();

						ImU32 track_color = ImGui::GetColorU32(ImVec4(0., 0.5, 0.95, 1));
						if (i == 0) { // if video track
							track_color = ImGui::GetColorU32(ImVec4(0.96, 0.655, 0., 1));
						}
						ImGui::GetWindowDrawList()->AddRectFilled(r_min, r_max, track_color, 0.0f);

						hoveringOverTrack = hoveringOverTrack || ImGui::IsItemHovered();

						// ######### bottom border
						float thickness = 1;
						ImU32 border_color = ImGui::GetColorU32(ImVec4(0.1, 0.1, 0.5, 1));
						if (i != 0) {
							ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(r_min.x, r_min.y), ImVec2(r_max.x, r_min.y + thickness), border_color);
						}
					}

					// ########## track separator
					ImGui::SetCursorScreenPos(cursor_after_invisButton);
					ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
					ImGui::Separator();
					ImGui::PopStyleVar();
				}

				// ########### track selection
				if (track1isSelected) {
					ImU32 border_color = ImGui::GetColorU32(ImVec4(1, 1, 1, 1));
					ImVec2 posStart(cursorTimelineBefore.x + trackLeftPadding * timelineXScale, cursorTimelineBefore.y);
					ImVec2 posEnd(cursorTimelineBefore.x + (trackLeftPadding + trackWidth) * timelineXScale, cursorTimelineBefore.y + track1Height * trackCount);
					ImGui::GetWindowDrawList()->AddRect(posStart, posEnd, border_color, 0.0f, 0, 1.0f);
				}

				if (hoveringOverTrack) {
					float edgeLeft = (cursor_before_invisButton.x + trackLeftPadding * timelineXScale);
					float edgeRight = (cursor_before_invisButton.x + (trackWidth + trackLeftPadding) * timelineXScale);

					if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
						track1isSelected = true;
					}

					if (abs(ImGui::GetMousePos().x - edgeLeft) < 10 || isResizingLeft) {
						ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
						if (!isResizingLeft && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
							resizeStartPos = ImGui::GetMousePos();
							printf("set: %f\n", resizeStartPos.x);
							isResizingLeft = true;
						}
					}
					else if (abs(ImGui::GetMousePos().x - edgeRight) < 10 || isResizingRight) {
						ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
						if (!isResizingRight && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
							resizeStartPos = ImGui::GetMousePos();
							printf("set: %f\n", resizeStartPos.x);
							isResizingRight = true;
						}
					}
					else {
						if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
							track1isBeingMoved = true;
							moveStartPos = ImGui::GetMousePos();
						}
					}
				}
			}

			{ // timeMarker
				if (playbackActive) {
					playbackTime += ImGui::GetIO().DeltaTime;
					timeMarkerValue = playbackTime*timelineXScale;
				}

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
						float oldZoom = timelineXScale;
						if (mw > 0) {
							timelineXScale = timelineXScale * factor;
						} else {
							timelineXScale = timelineXScale / factor;
						}

						float currentScrollPos = ImGui::GetScrollX();
						float timelineMousePos = ImGui::GetMousePos().x - cursorTimelineBefore.x;

						float diffBefore = timelineMousePos / oldZoom - currentScrollPos;
						float diffAfter = timelineMousePos / timelineXScale - currentScrollPos;

						float offset = diffBefore - diffAfter;
						ImGui::SetScrollX(currentScrollPos + offset * timelineXScale);
					}
				}
			}

			{ // changing playback cursor position
				if (!hoveringOverTrack && timelineClicked) {
					ImVec2 mousePos = ImGui::GetMousePos();
					if (mousePos.x > cursorTimelineBefore.x) {
						if (snappingEnabled) {
							float snapSensitivity = 8;
							float track1LeftmostPos = cursorTimelineBefore.x + trackLeftPadding * timelineXScale;
							float track1RightmostPos = cursorTimelineBefore.x + (trackLeftPadding + trackWidth) * timelineXScale;

							if (abs(mousePos.x - track1LeftmostPos) < snapSensitivity) {
								mousePos.x = track1LeftmostPos;
							}
							else if (abs(mousePos.x - track1RightmostPos) < snapSensitivity) {
								mousePos.x = track1RightmostPos;
							}
						}

						track1isSelected = false;
						float secs = (mousePos.x - cursorTimelineBefore.x)/timelineXScale;
						playbackTime = secs;
						timeMarkerValue = mousePos.x - cursorTimelineBefore.x;
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
	bool show_another_window = true;
	// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
	if (1)
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

		ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

		ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
		ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
		ImGui::Checkbox("Another Window", &show_another_window);

		ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f

		if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
			counter++;
		ImGui::SameLine();
		ImGui::Text("counter = %d", counter);

		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / app->io.Framerate, app->io.Framerate);
		ImGui::End();
	}

	// 3. Show another simple window.
	if (show_another_window) {
		ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
		ImGui::Text("Hello from another window!");
		if (ImGui::Button("Close Me"))
			show_another_window = false;
		ImGui::End();
	}
}
