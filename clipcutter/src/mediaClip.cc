#include "pch.h"
#include "mediaClip.h"
#include "app.h"


void MediaClip_Init(MediaClip* mediaClip, MediaSource* mediaSource) {
	memset(mediaClip, 0, sizeof MediaClip);
	mediaClip->source = mediaSource;

	mediaClip->width = mediaSource->length;
}

//void MediaClip_Draw(App* app, MediaClip* mediaClip) {

void MediaClip_Draw(App* app, MediaClip* mediaClip) {
	bool mouseLetGo = !ImGui::IsMouseDown(ImGuiMouseButton_Left);
	ImVec2 mousePos = ImGui::GetMousePos();

	float trackWidth = mediaClip->width;
	float clipLeftPadding = mediaClip->padding;

	if (mediaClip->isBeingMoved) {
		float diff = (mousePos.x - mediaClip->moveStartPos.x) / app->timeline.scaleX;
		if (app->timeline.snappingEnabled) {
			clipLeftPadding += ceilf((diff) / app->timeline.snappingPrecision) * app->timeline.snappingPrecision;
		} else {
			clipLeftPadding += diff;
		}
		if (clipLeftPadding < 0) {
			clipLeftPadding = 0;
		}

		if (mouseLetGo) {
			mediaClip->isBeingMoved = false;
			// TODO: update playback in case we're currently playing that clip
			MediaClip* currentClip = app->timelineEvents[app->timelineEventIndex].clip;
			// if currently playing this clip
			bool updateThing = false;
			if (app->playbackTime >= mediaClip->padding && app->playbackTime < mediaClip->padding + mediaClip->width) {
				printf("Old location was where marker is\n");
				updateThing = true;
			}
			if (app->playbackTime >= clipLeftPadding && app->playbackTime < clipLeftPadding + mediaClip->width) {
				printf("inside the new moved location\n");
				updateThing = true;
			}

			mediaClip->padding = clipLeftPadding;
			App_CalculateTimelineEvents(app);
			if (updateThing) {
				printf("updating playback thing---------------\n");
				App_MovePlaybackMarker(app, app->playbackTime);
				//App_MovePlaybackMarker(app, 120);
			}
			
		}
	}

	// there is apprently a GetMouseDragDelta() function that I might wanna look into
	if (mediaClip->isResizingLeft) {
		float cutoffOffset = (mousePos.x - mediaClip->resizeStartPos.x) / app->timeline.scaleX;
		if (app->timeline.snappingEnabled) {
			cutoffOffset = floorf(cutoffOffset / app->timeline.snappingPrecision) * app->timeline.snappingPrecision;
		}
		clipLeftPadding = mediaClip->padding + cutoffOffset;
		float* startCutoff = &mediaClip->drawStartCutoff;
		float totalCutOffvalue = cutoffOffset + *startCutoff;

		if (totalCutOffvalue < 0) {
			cutoffOffset = -*startCutoff;
			totalCutOffvalue = cutoffOffset + *startCutoff;
			clipLeftPadding = mediaClip->padding + cutoffOffset;
		}
		if (clipLeftPadding < 0) {
			cutoffOffset = -mediaClip->padding;
			totalCutOffvalue = cutoffOffset + *startCutoff;
			clipLeftPadding = 0;
		}

		trackWidth -= cutoffOffset;

		if (mouseLetGo) {
			mediaClip->padding = clipLeftPadding;
			*startCutoff = totalCutOffvalue;
			mediaClip->isResizingLeft = false;
			App_CalculateTimelineEvents(app);
		}
	}
	else if (mediaClip->isResizingRight) {
		float cutoffOffset = (mediaClip->resizeStartPos.x - mousePos.x) / app->timeline.scaleX;
		if (app->timeline.snappingEnabled) {
			cutoffOffset = floor(cutoffOffset / app->timeline.snappingPrecision) * app->timeline.snappingPrecision;
		}


		float* endCutoff = &mediaClip->drawEndCutoff;
		float totalCutOffvalue = cutoffOffset + *endCutoff;
		if (totalCutOffvalue < 0) { // limit resizing to the max size of the video
			cutoffOffset = -*endCutoff;
			totalCutOffvalue = cutoffOffset + *endCutoff;
		}
		trackWidth -= cutoffOffset;

		if (mouseLetGo) {
			*endCutoff = totalCutOffvalue;
			mediaClip->isResizingRight = false;
			printf("cut away time: %f\n", totalCutOffvalue);
			App_CalculateTimelineEvents(app);
		}
	}
	if (mouseLetGo) {
		mediaClip->width = trackWidth;
	}

	ImVec2 cursor_trackclip(0, 0);
	if (mediaClip != nullptr) {

		mediaClip->isHovered = false; // we don't want this property inherited from the last draw
		ImGui::SetCursorScreenPos(app->timeline.cursTopLeft);
		for (int i = 0; i <= mediaClip->source->audioTracks; i++) {
			cursor_trackclip = ImGui::GetCursorScreenPos();
			ImVec2 cursor_trackclip_padded = ImGui::GetCursorScreenPos();
			cursor_trackclip_padded.x = (cursor_trackclip_padded.x + clipLeftPadding * app->timeline.scaleX);
			ImVec2 track_size(trackWidth * app->timeline.scaleX, app->timeline.clipHeight);

			ImGui::SetCursorScreenPos(cursor_trackclip_padded);
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
			ImVec2 tracNamePos = ImGui::GetCursorScreenPos();
			if (i <= mediaClip->source->audioTracks) {
				ImGui::InvisibleButton(("track1Button#" + std::to_string(i)).c_str(), track_size);
			} else {
				ImGui::Dummy(track_size);
			}
			ImGui::PopStyleVar();


			if (i <= mediaClip->source->audioTracks) {
				ImVec2 r_min = ImGui::GetItemRectMin();
				ImVec2 r_max = ImGui::GetItemRectMax();

				ImU32 track_color = ImGui::GetColorU32(ImVec4(0., 0.5, 0.95, 1));
				if (i == 0) { // if video track

					track_color = ImGui::GetColorU32(ImVec4(0.96, 0.655, 0., 1));
				}
				ImGui::GetWindowDrawList()->AddRectFilled(r_min, r_max, track_color, 0.0f);

				mediaClip->isHovered = mediaClip->isHovered || ImGui::IsItemHovered();

				if (i == 0) {
					ImU32 textColor = ImGui::GetColorU32(ImVec4(0., 0., 0., 1));
					ImVec2 savedPos = ImGui::GetCursorScreenPos();
					ImGui::SetCursorScreenPos(tracNamePos);
					ImGui::PushStyleColor(ImGuiCol_Text, textColor);
					ImGui::Text("%s", mediaClip->source->filename);
					ImGui::PopStyleColor();
					ImGui::SetCursorScreenPos(savedPos);
				}

				// ######### bottom border
				float thickness = 1;
				ImU32 border_color = ImGui::GetColorU32(ImVec4(0.1, 0.1, 0.5, 1));
				if (i != 0) {
					ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(r_min.x, r_min.y), ImVec2(r_max.x, r_min.y + thickness), border_color);
				}
			}

			// ########## track separator
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
			ImGui::Separator();
			ImGui::PopStyleVar();
		}

		// ########### clip selection
		if (app->selectedTrack == mediaClip) {
			ImU32 border_color = ImGui::GetColorU32(ImVec4(1, 1, 1, 1));
			ImVec2 posStart(app->timeline.cursTopLeft.x + clipLeftPadding * app->timeline.scaleX, app->timeline.cursTopLeft.y);
			ImVec2 posEnd(app->timeline.cursTopLeft.x + (clipLeftPadding + trackWidth) * app->timeline.scaleX, app->timeline.cursTopLeft.y + app->timeline.clipHeight * (mediaClip->source->audioTracks+1));
			ImGui::GetWindowDrawList()->AddRect(posStart, posEnd, border_color, 0.0f, 0, 1.0f);
		}

		if (mediaClip->isHovered) {
			float edgeLeft = (cursor_trackclip.x + clipLeftPadding * app->timeline.scaleX);
			float edgeRight = (cursor_trackclip.x + (trackWidth + clipLeftPadding) * app->timeline.scaleX);

			if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
				app->selectedTrack = mediaClip;
			}

			if (fabs(ImGui::GetMousePos().x - edgeLeft) < 10 || mediaClip->isResizingLeft) {
				ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
				if (!mediaClip->isResizingLeft && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
					mediaClip->resizeStartPos = ImGui::GetMousePos();
					printf("set: %f\n", mediaClip->resizeStartPos.x);
					mediaClip->isResizingLeft = true;
				}
			}
			else if (fabs(ImGui::GetMousePos().x - edgeRight) < 10 || mediaClip->isResizingRight) {
				ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
				if (!mediaClip->isResizingRight && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
					mediaClip->resizeStartPos = ImGui::GetMousePos();
					printf("set: %f\n", mediaClip->resizeStartPos.x);
					mediaClip->isResizingRight = true;
				}
			}
			else {
				if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
					mediaClip->isBeingMoved = true;
					mediaClip->moveStartPos = ImGui::GetMousePos();
				}
			}
		}

	}
}
