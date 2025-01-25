#include "pch.h"
#include "app.h"

int tracklistWidth = 100;


int trackCount = 2;
int track1Height = 30;

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

		{ // Timeline
			ImGui::SetCursorScreenPos(cursorTracklistAfter);
			ImGui::BeginGroup();
			ImU32 timeline_color = ImGui::GetColorU32(ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
			bool snappingEnabled = !ImGui::IsKeyDown(ImGuiKey_LeftShift);

			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
			ImGui::SameLine();
			ImVec2 childSize = ImVec2(ImGui::GetContentRegionAvail().x, fmax(ImGui::GetContentRegionAvail().y, (trackCount + 3) * track1Height));
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

			bool timelineClicked = ImGui::IsItemClicked(ImGuiButtonFlags_MouseButtonLeft);

			ImGui::SetCursorScreenPos(cursorTimelineBefore);

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
