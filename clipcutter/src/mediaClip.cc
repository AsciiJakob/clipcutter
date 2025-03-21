#include "pch.h"
#include "mediaClip.h"
#include "app.h"
#include <imgui.h>


void MediaClip_Init(MediaClip* mediaClip, MediaSource* mediaSource) {
	memset(mediaClip, 0, sizeof(MediaClip));
	mediaClip->source = mediaSource;

	mediaClip->width = mediaSource->length;
}

void MediaClip_Delete(App* app, MediaClip* mediaClip) {
    bool isBeingPlayed = MediaClip_IsBeingPlayed(app, mediaClip);
    
    // find index of mediaClip in mediaClips array
    int clipIndex = -1;
    for (int i=0; i < MEDIACLIPS_SIZE; i++) {
        if (clipIndex == -1) {
            if (app->mediaClips[i] == mediaClip)
                clipIndex = i;

        } else if (i > clipIndex) {
            // shuffle all elements after the clipIndex back by one index
            // so that the mediaClip is removed from the array
            app->mediaClips[i-1] = app->mediaClips[i];
            if (app->mediaClips[i] == nullptr)
                break;
        }
    }

    if (clipIndex == -1) {
        log_error("MediaClip to delete not found in app struct");
        assert(true && "Mediaclip to delete not found in app struct");
        return;
    }

    App_CalculateTimelineEvents(app);
    if (isBeingPlayed) { // if we're playing this clip rn
        App_MovePlaybackMarker(app, app->playbackTime);
    }

}

TimelineEvent* findClipNeighbourLeft(TimelineEvent* timelineEvents, int eventIndex) {
    if (eventIndex != 0) {
        TimelineEvent* leftEvent = &timelineEvents[eventIndex-1];
        if (leftEvent->type == TIMELINE_EVENT_BLANKSPACE) {
            if (eventIndex-1 != 0) {
                TimelineEvent* leftLeftEvent = &timelineEvents[eventIndex-2];
                if (leftLeftEvent->type == TIMELINE_EVENT_VIDEO) {
                    return leftLeftEvent;
                }
            }
        } else {
            return leftEvent;
        }
    }

    return nullptr;
}


TimelineEvent* findClipNeighbourRight(TimelineEvent* timelineEvents, int eventIndex) {
    if (eventIndex != TIMELINE_EVENTS_SIZE-1) {
        TimelineEvent* rightEvent = &timelineEvents[eventIndex+1];
        if (rightEvent->type != TIMELINE_EVENT_VIDEO) {
            if (eventIndex+1 != TIMELINE_EVENTS_SIZE-1) {
                TimelineEvent* rightRightEvent = &timelineEvents[eventIndex+2];
                if (rightRightEvent->type == TIMELINE_EVENT_VIDEO) {
                    return rightRightEvent;
                }
            }
        } else {
            if (rightEvent->type != TIMELINE_EVENT_END)
                return rightEvent;
        }
    }

    return nullptr;
}

bool MediaClip_IsBeingPlayed(App* app, MediaClip* mediaClip) {
    TimelineEvent* currentEvent = &app->timelineEvents[app->timelineEventIndex];
    if (currentEvent->type == TIMELINE_EVENT_VIDEO && currentEvent->clip == mediaClip) {
        return true;
    } else {
        return false;
    }
}

bool shouldUpdatePlaybackAfterMove(App* app, MediaClip* mediaClip, float drawClipLeftPadding, float drawTrackWidth) {
    // if nothing was changed.
    if (mediaClip->width == drawTrackWidth && mediaClip->padding == drawClipLeftPadding) { 
        return false;
    }

    // clip was positioned where the marker is before it was moved
    /*if (app->playbackTime >= mediaClip->padding && app->playbackTime < mediaClip->padding + mediaClip->width) {*/
    /*    return true;*/
    /*}*/
    if (MediaClip_IsBeingPlayed(app, mediaClip)) {
        return true;
    }
    // clip will now be positioned where the marker is
    if (app->playbackTime >= drawClipLeftPadding && app->playbackTime < drawClipLeftPadding + drawTrackWidth) {
        return true;
    }
    return false;
}

//void MediaClip_Draw(App* app, MediaClip* mediaClip) {

void MediaClip_Draw(App* app, MediaClip* mediaClip, int clipIndex) {
	bool mouseLetGo = !ImGui::IsMouseDown(ImGuiMouseButton_Left);
	ImVec2 mousePos = ImGui::GetMousePos();

	float drawTrackWidth = mediaClip->width;
	float drawClipLeftPadding = mediaClip->padding;

	if (mediaClip->isBeingMoved) {
		float diff = (mousePos.x - mediaClip->moveStartPos.x) / app->timeline.scaleX;
		if (app->timeline.snappingEnabled) {

            if (ImGui::IsKeyPressed(ImGuiKey_Space))
                log_debug("debug key pressed");

            float leftDist;
            float rightDist;
            TimelineEvent* leftClipEvent = nullptr;
            TimelineEvent* rightClipEvent = nullptr;
            int searchFromIndex = mediaClip->timelineEventsIndex;
            /*bool movedThroughOtherClip = false;*/

            leftClipEvent = findClipNeighbourLeft(app->timelineEvents, searchFromIndex);
            rightClipEvent = findClipNeighbourRight(app->timelineEvents, searchFromIndex);
            unsigned int i=0;
            while (true) {
                if (i++ > MEDIACLIPS_SIZE) {
                    // TODO: come back and fix this bug. it's really annoying
                    log_warn("Hung on snapping to neighbour clip, most likely positioned right in the middle of a clip");
                    rightDist = 0.0;
                    break;
                }

                if (leftClipEvent) {
                    leftDist = (drawClipLeftPadding+diff)-(leftClipEvent->start+leftClipEvent->clip->width);
                }
                if (rightClipEvent) {
                    rightDist = rightClipEvent->start-(drawClipLeftPadding+diff+mediaClip->width);
                }


                if (leftClipEvent && leftDist < -leftClipEvent->clip->width) {
                    rightClipEvent = leftClipEvent;
                    leftClipEvent = findClipNeighbourLeft(app->timelineEvents, leftClipEvent->clip->timelineEventsIndex);

                } else if (rightClipEvent && rightDist < -rightClipEvent->clip->width) {
                    leftClipEvent = rightClipEvent;
                    rightClipEvent = findClipNeighbourRight(app->timelineEvents, rightClipEvent->clip->timelineEventsIndex);
                } else {
                    break;
                }

            }


            bool snappingToClip = false;
            if (leftClipEvent != nullptr) {
                if (leftDist < 1) {
                    drawClipLeftPadding = leftClipEvent->start+leftClipEvent->clip->width;
                    snappingToClip = true;
                }
            }
            if (rightClipEvent != nullptr) {
                if (rightDist < 1 && rightDist < leftDist) {
                    drawClipLeftPadding = rightClipEvent->start-mediaClip->width;
                    snappingToClip = true;
                }
            }

            if (!snappingToClip) {
                drawClipLeftPadding += ceilf((diff) / app->timeline.snappingPrecision) * app->timeline.snappingPrecision;
            }

		} else {
			drawClipLeftPadding += diff;
		}
		if (drawClipLeftPadding < 0) {
			drawClipLeftPadding = 0;
		}

		if (mouseLetGo) {
            // todo: figure out a way to calculate difference so that we don't refresh if we don't have to
			mediaClip->isBeingMoved = false;
			bool updatePlayback = shouldUpdatePlaybackAfterMove(app, mediaClip, drawClipLeftPadding, drawTrackWidth);
			mediaClip->padding = drawClipLeftPadding;
			App_CalculateTimelineEvents(app);

			if (updatePlayback) {
				log_debug("updating playback thing---------------\n");
				App_MovePlaybackMarker(app, app->playbackTime);
			}
			
		}
	}

	if (mediaClip->isResizingLeft) {
		float cutoffOffset = (mousePos.x - mediaClip->resizeStartPos.x) / app->timeline.scaleX;
		if (app->timeline.snappingEnabled) {
			cutoffOffset = floorf(cutoffOffset / app->timeline.snappingPrecision) * app->timeline.snappingPrecision;
		}
		drawClipLeftPadding = mediaClip->padding + cutoffOffset;
		float* startCutoff = &mediaClip->drawStartCutoff;
		float totalCutOffvalue = cutoffOffset + *startCutoff;

		if (totalCutOffvalue < 0) {
			cutoffOffset = -*startCutoff;
			totalCutOffvalue = cutoffOffset + *startCutoff;
			drawClipLeftPadding = mediaClip->padding + cutoffOffset;
		}
		if (drawClipLeftPadding < 0) {
			cutoffOffset = -mediaClip->padding;
			totalCutOffvalue = cutoffOffset + *startCutoff;
			drawClipLeftPadding = 0;
		}

		drawTrackWidth -= cutoffOffset;

		if (mouseLetGo) {
			bool updatePlayback = shouldUpdatePlaybackAfterMove(app, mediaClip, drawClipLeftPadding, drawTrackWidth);
			mediaClip->padding = drawClipLeftPadding;
			*startCutoff = totalCutOffvalue;
			mediaClip->isResizingLeft = false;
            mediaClip->width = drawTrackWidth;
            App_CalculateTimelineEvents(app);

			if (updatePlayback) {
				log_debug("updating playback thing---------------\n");
				App_MovePlaybackMarker(app, app->playbackTime);
			}
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
		drawTrackWidth -= cutoffOffset;

		if (mouseLetGo) {
			bool updatePlayback = shouldUpdatePlaybackAfterMove(app, mediaClip, drawClipLeftPadding, drawTrackWidth);
			*endCutoff = totalCutOffvalue;
			mediaClip->isResizingRight = false;
            mediaClip->width = drawTrackWidth;
            App_CalculateTimelineEvents(app);

			if (updatePlayback) {
				log_debug("updating playback thing---------------\n");
				App_MovePlaybackMarker(app, app->playbackTime);
			}
		}
	}

	ImVec2 cursor_trackclip(0, 0);

    mediaClip->isHovered = false; // we don't want this property inherited from the last draw
    ImGui::SetCursorScreenPos(app->timeline.cursTopLeft);
    for (int i = 0; i <= mediaClip->source->audioTracks; i++) {
        cursor_trackclip = ImGui::GetCursorScreenPos();
        ImVec2 cursor_trackclip_padded = ImGui::GetCursorScreenPos();
        cursor_trackclip_padded.x = (cursor_trackclip_padded.x + drawClipLeftPadding * app->timeline.scaleX);
        ImVec2 track_size(drawTrackWidth * app->timeline.scaleX, app->timeline.clipHeight);

        ImGui::SetCursorScreenPos(cursor_trackclip_padded);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        ImVec2 tracNamePos = ImGui::GetCursorScreenPos();
        if (i <= mediaClip->source->audioTracks) {
            ImGui::InvisibleButton(("track" + std::to_string(clipIndex) + "Button#" + std::to_string(i)).c_str(), track_size);
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

    
    if (app->selectedTrack == mediaClip) { // ########### clip selection
        ImU32 border_color = ImGui::GetColorU32(ImVec4(1, 1, 1, 1));
        ImVec2 posStart(app->timeline.cursTopLeft.x + drawClipLeftPadding * app->timeline.scaleX, app->timeline.cursTopLeft.y);
        ImVec2 posEnd(app->timeline.cursTopLeft.x + (drawClipLeftPadding + drawTrackWidth) * app->timeline.scaleX, app->timeline.cursTopLeft.y + app->timeline.clipHeight * (mediaClip->source->audioTracks+1));
        ImGui::GetWindowDrawList()->AddRect(posStart, posEnd, border_color, 0.0f, 0, 1.0f);
    } else { // ########### clip left & right borders
        ImU32 border_color = ImGui::GetColorU32(ImVec4(0, 0, 0, 1));
        ImVec2 posStart(app->timeline.cursTopLeft.x + drawClipLeftPadding * app->timeline.scaleX, app->timeline.cursTopLeft.y);
        ImVec2 posEnd(app->timeline.cursTopLeft.x + (drawClipLeftPadding + drawTrackWidth) * app->timeline.scaleX, app->timeline.cursTopLeft.y + app->timeline.clipHeight * (mediaClip->source->audioTracks+1));
        ImGui::GetWindowDrawList()->AddRect(posStart, posEnd, border_color, 0.0f, 0, 1.0f);
    }

    if (mediaClip->isHovered) {
        float edgeLeft = (cursor_trackclip.x + drawClipLeftPadding * app->timeline.scaleX);
        float edgeRight = (cursor_trackclip.x + (drawTrackWidth + drawClipLeftPadding) * app->timeline.scaleX);

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            app->selectedTrack = mediaClip;
        }

        if (fabs(ImGui::GetMousePos().x - edgeLeft) < 10 || mediaClip->isResizingLeft) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
            if (!mediaClip->isResizingLeft && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                mediaClip->resizeStartPos = ImGui::GetMousePos();
                log_debug("set: %f", mediaClip->resizeStartPos.x);
                mediaClip->isResizingLeft = true;
            }
        }
        else if (fabs(ImGui::GetMousePos().x - edgeRight) < 10 || mediaClip->isResizingRight) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
            if (!mediaClip->isResizingRight && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                mediaClip->resizeStartPos = ImGui::GetMousePos();
                log_debug("set: %f", mediaClip->resizeStartPos.x);
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
