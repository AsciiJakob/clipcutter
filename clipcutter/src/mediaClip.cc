#include "pch.h"
#include "mediaClip.h"
#include "app.h"
#include <imgui.h>

#define SNAPTHRESHOLD_MARKER 5
#define SNAPTHRESHOLD_CLIP 1

// use App_CreateMediaClip instead of calling this directly
void MediaClip_Init(MediaClip* mediaClip, MediaSource* mediaSource) {
	memset(mediaClip, 0, sizeof(MediaClip));
	mediaClip->source = mediaSource;

	mediaClip->width = mediaSource->length;
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
            if (rightEvent->type == TIMELINE_EVENT_END) {
                return nullptr;
            }
            if (eventIndex+1 != TIMELINE_EVENTS_SIZE-1) {
                TimelineEvent* rightRightEvent = &timelineEvents[eventIndex+2];
                if (eventIndex+2 != TIMELINE_EVENTS_SIZE-1) {
                    if (rightRightEvent->type == TIMELINE_EVENT_VIDEO) {
                        return rightRightEvent;
                    }
                }
            }
        } else {
            if (rightEvent->type != TIMELINE_EVENT_END)
                return rightEvent;
        }
    }

    return nullptr;
}

float snapPointToGrid(App* app, float point)  {
    return ceilf(point / app->timeline.snappingPrecision) * app->timeline.snappingPrecision;
}
void findNeighbourClipsOfPoints(App* app, float pointLeft, float pointRight, int eventIndex, TimelineEvent** leftClipEventP, TimelineEvent** rightClipEventP, float* leftClipDist, float* rightClipDist) {
    TimelineEvent* leftClipEvent = findClipNeighbourLeft(app->timelineEvents, eventIndex);
    TimelineEvent* rightClipEvent = findClipNeighbourRight(app->timelineEvents, eventIndex);
    unsigned int i=0;
    while (true) {
        if (i++ > MEDIACLIPS_SIZE) {
            log_warn("Hung on snapping to neighbour clip");
            *rightClipDist = 0.0;
            break;
        }

        if (leftClipEvent) {
            *leftClipDist = (pointLeft)-(leftClipEvent->start+leftClipEvent->clip->width);
        }
        if (rightClipEvent) {
            *rightClipDist = rightClipEvent->start-(pointRight);
        }

        if (leftClipEvent && *leftClipDist < -leftClipEvent->clip->width) {
            rightClipEvent = leftClipEvent;
            leftClipEvent = findClipNeighbourLeft(app->timelineEvents, leftClipEvent->clip->timelineEventsIndex);
        } else if (rightClipEvent && *rightClipDist < -rightClipEvent->clip->width) {
            leftClipEvent = rightClipEvent;
            rightClipEvent = findClipNeighbourRight(app->timelineEvents, rightClipEvent->clip->timelineEventsIndex);
        } else {
            break;
        }

    }

    *leftClipEventP = leftClipEvent;
    *rightClipEventP = rightClipEvent;
}

// if two clips are overlapping each other, make one win the space and the other get resized.
void overrideOverlappingClips(App* app, MediaClip* priorityClip) {
    for (int i=0; i < MEDIACLIPS_SIZE; i++) {
        MediaClip* clip = app->mediaClips[i];
        if (clip == nullptr) break;
        if (clip == priorityClip) continue;

        float prioStart = priorityClip->padding;
        float prioEnd = priorityClip->padding+priorityClip->width;
        float clipStart = clip->padding;
        float clipEnd = clip->padding+clip->width;
        
        // clip = []
        // priorityClip = ()

        if (prioStart <= clipStart && prioEnd >= clipEnd) {
            // case: ([])
            // case: ( [] )
            // case: ([] )
            // case: ( [])

            clip->width = 0.0; // setting width to zero will delete the clip once drawing loop is done
            log_debug("delete case");
        } else if (prioStart <= clipEnd && prioEnd >= clipStart) {
            if (prioEnd == clipEnd) {
                // case: [ ()]
                log_debug("case: [ ()]");

                ClipSplitResult split = MediaClip_Split(app, clip, prioStart);
                App_DeleteMediaClip(app, split.clipRight);
            } else if (prioStart == clipStart) {
                // case: [() ]
                log_debug("case: [() ]");

                ClipSplitResult split = MediaClip_Split(app, clip, prioEnd);
                App_DeleteMediaClip(app, split.clipLeft);
            } else if (prioEnd < clipEnd && prioStart > clipStart) {
                // case: [ () ]
                log_debug("case: [ () ]");

                ClipSplitResult splitLeft = MediaClip_Split(app, clip, prioStart);
                ClipSplitResult splitRight = MediaClip_Split(app, splitLeft.clipRight, prioEnd);

                App_DeleteMediaClip(app, splitRight.clipLeft);
            } else {
                log_debug("double thing");
                // case: [(])
                // case: ([)]
                // and cases with spaces too, of course
                if (prioStart < clipEnd && prioStart > clipStart) {
                    // case: [(])
                    log_debug("case: [(])");

                    ClipSplitResult split = MediaClip_Split(app, clip, prioStart);
                    App_DeleteMediaClip(app, split.clipRight);

                } else if (prioStart < clipStart) {
                    // case: ([)]
                    log_debug("case: ([)]");

                    ClipSplitResult split = MediaClip_Split(app, clip, prioEnd);
                    App_DeleteMediaClip(app, split.clipLeft);
                }


            }

        }
    }
}


bool MediaClip_IsBeingPlayed(App* app, MediaClip* mediaClip) {
    TimelineEvent* currentEvent = &app->timelineEvents[app->timelineEventIndex];
    if (currentEvent->type == TIMELINE_EVENT_VIDEO && currentEvent->clip == mediaClip) {
        return true;
    } else {
        return false;
    }
}

// splits the clip at the timestamp specified
// does not invoke App_CalculateTImelineEvents() by itself
ClipSplitResult MediaClip_Split(App* app, MediaClip* clip, float timestamp) {
    MediaClip* rightClip = clip;
    MediaClip* leftClip = App_CreateMediaClip(app, rightClip->source);
    
    // make the two clips overlap each other exactly
    leftClip->padding = rightClip->padding;
    leftClip->startCutoff = rightClip->startCutoff;
    leftClip->endCutoff = rightClip->endCutoff;
    leftClip->width = rightClip->width;

    float ClipLengthRightOfMarker = leftClip->padding+leftClip->width-timestamp;
    leftClip->endCutoff += ClipLengthRightOfMarker;
    leftClip->width -= ClipLengthRightOfMarker;

    float clipLengthLeftOfMarker = timestamp-rightClip->padding;
    rightClip->startCutoff += clipLengthLeftOfMarker;
    rightClip->padding += clipLengthLeftOfMarker;
    rightClip->width -= clipLengthLeftOfMarker;

    ClipSplitResult result = {leftClip, rightClip};
    return result;
}

bool shouldUpdatePlaybackAfterMove(App* app, MediaClip* mediaClip, float drawClipLeftPadding, float drawClipWidth) {
    // if nothing was changed.
    if (mediaClip->width == drawClipWidth && mediaClip->padding == drawClipLeftPadding) { 
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
    if (app->playbackTime >= drawClipLeftPadding && app->playbackTime < drawClipLeftPadding + drawClipWidth) {
        return true;
    }
    return false;
}

//void MediaClip_Draw(App* app, MediaClip* mediaClip) {

ImVec2 MediaClip_Draw_DrawTracks(App* app, MediaClip* mediaClip, int clipIndex, float drawClipLeftPadding, float drawClipWidth, bool isGhostClip) {
    ImU32 normal_border_color;
    if (isGhostClip) {
        normal_border_color = ImGui::GetColorU32(ImVec4(0.7, 0.7, 0.7, 1));
    } else {
        normal_border_color = ImGui::GetColorU32(ImVec4(1, 1, 1, 1));
    }

    ImVec2 cursor_trackclip(0, 0);
    ImGui::SetCursorScreenPos(app->timeline.cursTopLeft);
    for (int i = 0; i <= mediaClip->source->audioTracks; i++) {
        cursor_trackclip = ImGui::GetCursorScreenPos();
        ImVec2 cursor_trackclip_padded = ImGui::GetCursorScreenPos();
        cursor_trackclip_padded.x = (cursor_trackclip_padded.x + drawClipLeftPadding * app->timeline.scaleX);

        // Imgui doesn't allow the width to be 0
        // width will only be zero when it's about to get deleted when resizing
        float drawWidth = drawClipWidth;
        if (drawClipWidth == 0.0) {
            drawWidth = 0.001;
        }

        ImVec2 track_size(drawWidth * app->timeline.scaleX, app->timeline.clipHeight);

        ImGui::SetCursorScreenPos(cursor_trackclip_padded);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        ImVec2 tracNamePos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton(("track" + std::to_string(clipIndex) + "Button#" + std::to_string(i)).c_str(), track_size);

        ImGui::PopStyleVar();


        ImVec2 r_min = ImGui::GetItemRectMin();
        ImVec2 r_max = ImGui::GetItemRectMax();

        ImU32 track_color = ImGui::GetColorU32(ImVec4(0., 0.5, 0.95, 1));
        if (i == 0) { // if video track

            track_color = ImGui::GetColorU32(ImVec4(0.96, 0.655, 0., 1));
        }
        if (isGhostClip) {
            track_color = ImGui::GetColorU32(ImVec4(0.5, 0.5, 0.5, 1));
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

        // ######### border seperating track from track below in clip

        float thickness = 1;
        if (i != 0) {
            ImU32 border_color = normal_border_color;
            ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(r_min.x, r_min.y), ImVec2(r_max.x, r_min.y + thickness), border_color);
        }
    }

    
    ImU32 border_color;
    if (mediaClip->isSelected) { // ########### clip selection
        border_color = normal_border_color;
        if (drawClipWidth == 0.0) {
            border_color = ImGui::GetColorU32(ImVec4(0.8, 0.1, 0.1, 1));
        }
        ImVec2 posStart(app->timeline.cursTopLeft.x + drawClipLeftPadding * app->timeline.scaleX, app->timeline.cursTopLeft.y);
        ImVec2 posEnd(app->timeline.cursTopLeft.x + (drawClipLeftPadding + drawClipWidth) * app->timeline.scaleX, app->timeline.cursTopLeft.y + app->timeline.clipHeight * (mediaClip->source->audioTracks+1));
        ImGui::GetWindowDrawList()->AddRect(posStart, posEnd, border_color, 0.0f, 0, 1.0f);
    } else { // ########### clip left & right borders
        ImU32 border_color = ImGui::GetColorU32(ImVec4(0, 0, 0, 1));
        ImVec2 posStart(app->timeline.cursTopLeft.x + drawClipLeftPadding * app->timeline.scaleX, app->timeline.cursTopLeft.y);
        ImVec2 posEnd(app->timeline.cursTopLeft.x + (drawClipLeftPadding + drawClipWidth) * app->timeline.scaleX, app->timeline.cursTopLeft.y + app->timeline.clipHeight * (mediaClip->source->audioTracks+1));
        ImGui::GetWindowDrawList()->AddRect(posStart, posEnd, border_color, 0.0f, 0, 1.0f);
    }

    return cursor_trackclip;
}

void MediaClip_Draw(App* app, MediaClip* mediaClip, int clipIndex) {
	bool mouseLetGo = !ImGui::IsMouseDown(ImGuiMouseButton_Left);
	ImVec2 mousePos = ImGui::GetMousePos();

	float drawClipWidth = mediaClip->width;
	float drawClipLeftPadding = mediaClip->padding;

	if (mediaClip->isBeingMoved) {
		float diff = (mousePos.x - mediaClip->moveStartPos.x) / app->timeline.scaleX;
		if (app->timeline.snappingEnabled) {
            
            bool snapToGrid = true;
            // snap to neighbouring clips
            {

                float leftClipDist;
                float rightClipDist;
                TimelineEvent* leftClipEvent = nullptr;
                TimelineEvent* rightClipEvent = nullptr;

                float pointLeft = drawClipLeftPadding+diff;
                float pointRight = drawClipLeftPadding+diff+drawClipWidth;
                findNeighbourClipsOfPoints(app, pointLeft, pointRight, mediaClip->timelineEventsIndex, &leftClipEvent, &rightClipEvent, &leftClipDist, &rightClipDist);

                if (leftClipEvent != nullptr) {
                    if (leftClipDist < SNAPTHRESHOLD_CLIP) {
                        drawClipLeftPadding = leftClipEvent->start+leftClipEvent->clip->width;
                        snapToGrid = false;
                    }
                }
                if (rightClipEvent != nullptr) {
                    if (rightClipDist < SNAPTHRESHOLD_CLIP && (!leftClipEvent || (rightClipDist < leftClipDist))) {
                        drawClipLeftPadding = rightClipEvent->start-mediaClip->width;
                        snapToGrid = false;
                    }
                }

            }


            // snap to playback marker
            float markerDistLeft, markerDistRight;
            {
                markerDistLeft = fabs(app->playbackTime-(mediaClip->padding+diff));
                markerDistRight = fabs(app->playbackTime-(mediaClip->padding+diff+mediaClip->width));
                if (markerDistLeft < SNAPTHRESHOLD_MARKER) {
                    drawClipLeftPadding = app->playbackTime;
                    snapToGrid = false;
                }
                if (markerDistRight < SNAPTHRESHOLD_MARKER) {
                    drawClipLeftPadding = app->playbackTime-mediaClip->width;
                    snapToGrid = false;
                }
            }
            

            if (snapToGrid) {
                drawClipLeftPadding = snapPointToGrid(app, drawClipLeftPadding+diff);
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
			bool updatePlayback = shouldUpdatePlaybackAfterMove(app, mediaClip, drawClipLeftPadding, drawClipWidth);
			mediaClip->padding = drawClipLeftPadding;
            overrideOverlappingClips(app, mediaClip);
			App_CalculateTimelineEvents(app);

			if (updatePlayback) {
				log_debug("updating playback thing---------------\n");
				App_MovePlaybackMarker(app, app->playbackTime);
			}
			
		}
        
	}

	if (mediaClip->isResizingLeft) {
		float cutoffOffset = (mousePos.x - mediaClip->resizeStartPos.x) / app->timeline.scaleX;
		float* startCutoff = &mediaClip->startCutoff;

		if (app->timeline.snappingEnabled) {
            bool snapToGrid = true;
            
            float leftClipDist;
            float rightClipDist;
            TimelineEvent* leftClipEvent = nullptr;
            TimelineEvent* rightClipEvent = nullptr;

            float pointLeft = drawClipLeftPadding+cutoffOffset;
            float pointRight = pointLeft;
            findNeighbourClipsOfPoints(app, pointLeft, pointRight, mediaClip->timelineEventsIndex, &leftClipEvent, &rightClipEvent, &leftClipDist, &rightClipDist);
            
            if (leftClipEvent != nullptr) {
                if (leftClipDist < SNAPTHRESHOLD_CLIP) {
                    cutoffOffset -= drawClipLeftPadding+cutoffOffset-(leftClipEvent->start+leftClipEvent->clip->width);
                    snapToGrid = false;
                }
            }
            if (rightClipEvent != nullptr) {
                if (rightClipDist < SNAPTHRESHOLD_CLIP && (!leftClipEvent || (rightClipDist < leftClipDist))) {
                    cutoffOffset -= drawClipLeftPadding+cutoffOffset-rightClipEvent->start;
                    snapToGrid = false;
                }
            }

            float markerDist = fabs(app->playbackTime-(drawClipLeftPadding+cutoffOffset));

            if (markerDist < SNAPTHRESHOLD_MARKER) {
                cutoffOffset -= drawClipLeftPadding+cutoffOffset-app->playbackTime;
                snapToGrid = false;
            }

            if (snapToGrid) {
                cutoffOffset = snapPointToGrid(app, cutoffOffset);
            }
		}

		drawClipLeftPadding = mediaClip->padding + cutoffOffset;

        float totalCutOffValue = cutoffOffset + *startCutoff;
        if (totalCutOffValue < 0) {
			cutoffOffset = -*startCutoff;
			totalCutOffValue = cutoffOffset + *startCutoff;
			drawClipLeftPadding = mediaClip->padding + cutoffOffset;
        }

        if (totalCutOffValue > mediaClip->source->length) {
			cutoffOffset = mediaClip->source->length-*startCutoff;
			totalCutOffValue = cutoffOffset + *startCutoff;
			drawClipLeftPadding = mediaClip->padding + cutoffOffset;
        }

        if (drawClipLeftPadding < 0) {
			cutoffOffset = -mediaClip->padding;
			totalCutOffValue = cutoffOffset + *startCutoff;
			drawClipLeftPadding = 0;
        }
		
        drawClipWidth -= cutoffOffset;

		if (mouseLetGo) {

			bool updatePlayback = shouldUpdatePlaybackAfterMove(app, mediaClip, drawClipLeftPadding, drawClipWidth);
			mediaClip->padding = drawClipLeftPadding;
			*startCutoff = totalCutOffValue;
			mediaClip->isResizingLeft = false;
            mediaClip->width = drawClipWidth;
            overrideOverlappingClips(app, mediaClip);
            App_CalculateTimelineEvents(app);

			if (updatePlayback) {
				log_debug("updating playback thing---------------\n");
				App_MovePlaybackMarker(app, app->playbackTime);
			}
		}
	}
	else if (mediaClip->isResizingRight) {
		float cutoffOffset = (mediaClip->resizeStartPos.x - mousePos.x) / app->timeline.scaleX;
		float* endCutoff = &mediaClip->endCutoff;

		if (app->timeline.snappingEnabled) {
            bool snapToGrid = true;
            
            float leftClipDist;
            float rightClipDist;
            TimelineEvent* leftClipEvent = nullptr;
            TimelineEvent* rightClipEvent = nullptr;

            float pointLeft = drawClipLeftPadding+drawClipWidth-cutoffOffset;
            float pointRight = pointLeft;
            findNeighbourClipsOfPoints(app, pointLeft, pointRight, mediaClip->timelineEventsIndex, &leftClipEvent, &rightClipEvent, &leftClipDist, &rightClipDist);

            if (ImGui::IsKeyPressed(ImGuiKey_Tab)) {
                log_debug("debug key");
            }
            
            if (leftClipEvent != nullptr) {
                if (leftClipDist < SNAPTHRESHOLD_CLIP) {
                    cutoffOffset -= drawClipLeftPadding+drawClipWidth-cutoffOffset-(leftClipEvent->start+leftClipEvent->clip->width);
                    snapToGrid = false;
                }
            }
            if (rightClipEvent != nullptr) {
                if (rightClipDist < SNAPTHRESHOLD_CLIP && (!leftClipEvent || (rightClipDist < leftClipDist))) {
                    cutoffOffset += drawClipLeftPadding+drawClipWidth-cutoffOffset-rightClipEvent->start;
                    snapToGrid = false;
                }
            }

            float markerDist = fabs(app->playbackTime-(drawClipLeftPadding+drawClipWidth-cutoffOffset));

            if (markerDist < SNAPTHRESHOLD_MARKER) {
                cutoffOffset += drawClipLeftPadding+drawClipWidth-cutoffOffset-app->playbackTime;
                snapToGrid = false;
            }

            if (snapToGrid) {
                cutoffOffset = snapPointToGrid(app, cutoffOffset);
            }
		}

        float totalCutOffValue = cutoffOffset + *endCutoff;
        if (totalCutOffValue < 0) { // limit resizing to the max size of the video
            cutoffOffset = -*endCutoff;
			totalCutOffValue = cutoffOffset + *endCutoff;
        }
        if (totalCutOffValue > mediaClip->source->length) {
            cutoffOffset = mediaClip->source->length-*endCutoff;
			totalCutOffValue = cutoffOffset + *endCutoff;
        }

		drawClipWidth -= cutoffOffset;


		if (mouseLetGo) {
			bool updatePlayback = shouldUpdatePlaybackAfterMove(app, mediaClip, drawClipLeftPadding, drawClipWidth);
			*endCutoff = totalCutOffValue;
			mediaClip->isResizingRight = false;
            mediaClip->width = drawClipWidth;
            overrideOverlappingClips(app, mediaClip);
            App_CalculateTimelineEvents(app);

			if (updatePlayback) {
				log_debug("updating playback thing---------------\n");
				App_MovePlaybackMarker(app, app->playbackTime);
			}
		}
	}

    mediaClip->isHovered = false; // we don't want this property inherited from the last draw
    ImVec2 cursor_trackclip = MediaClip_Draw_DrawTracks(app, mediaClip, clipIndex, drawClipLeftPadding, drawClipWidth, false);

    if (mediaClip->isHovered) {

        // handle selection
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !mediaClip->isBeingMoved) {
            if (ImGui::IsKeyDown(ImGuiKey_LeftShift)) {

                MediaClip* firstClip = (MediaClip*) app->selectedClips.items[0];
                App_ClearClipSelections(app);

                if (mediaClip->timelineEventsIndex < firstClip->timelineEventsIndex) {
                    for (int i=firstClip->timelineEventsIndex; i >= mediaClip->timelineEventsIndex; i--) {
                        TimelineEvent event = app->timelineEvents[i];
                        if (event.type == TIMELINE_EVENT_VIDEO) {
                            DynArr_Append(&app->selectedClips, event.clip);
                            event.clip->isSelected = true;
                        }
                    }

                } else { // if mediaClip->timelineEventsIndex >= lastClip->timelineEventsIndex
                    for (int i=firstClip->timelineEventsIndex; i <= mediaClip->timelineEventsIndex; i++) {
                        TimelineEvent event = app->timelineEvents[i];
                        if (event.type == TIMELINE_EVENT_VIDEO) {
                            DynArr_Append(&app->selectedClips, event.clip);
                            event.clip->isSelected = true;
                        }
                    }
                }
            } else if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
                if (mediaClip->isSelected) {
                    DynArr_RemoveElement(&app->selectedClips, mediaClip);
                    mediaClip->isSelected = false;
                } else {
                    DynArr_Append(&app->selectedClips, mediaClip);
                    mediaClip->isSelected = true;
                }
            } else {
                if (mediaClip->isSelected) {
                    // bool addBack = app->selectedClips.size > 1;
                    App_ClearClipSelections(app);
                    // if (addBack) {
                        DynArr_Append(&app->selectedClips, mediaClip);
                        mediaClip->isSelected = true;
                    // }
                } else {
                    App_ClearClipSelections(app);

                    DynArr_Append(&app->selectedClips, mediaClip);
                    mediaClip->isSelected = true;
                }
            }
        }

        // handle resizing / show resizing cursor
        float edgeLeft = (cursor_trackclip.x + drawClipLeftPadding * app->timeline.scaleX);
        float edgeRight = (cursor_trackclip.x + (drawClipWidth + drawClipLeftPadding) * app->timeline.scaleX);

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
    mediaClip->drawPadding = drawClipLeftPadding;
    mediaClip->drawWidth = drawClipWidth;
}
