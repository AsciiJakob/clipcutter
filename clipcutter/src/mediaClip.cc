#include "pch.h"
#include "mediaClip.h"
#include "app.h"
#include <imgui.h>


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

    float ClipLengthRightOfMarker = leftClip->padding + leftClip->width - timestamp;
    leftClip->endCutoff += ClipLengthRightOfMarker;
    leftClip->width = timestamp - leftClip->padding;

    float clipLengthLeftOfMarker = timestamp - rightClip->padding;
    rightClip->startCutoff += clipLengthLeftOfMarker;
    rightClip->padding = timestamp;
    rightClip->width -= clipLengthLeftOfMarker;

    ClipSplitResult result = {leftClip, rightClip};
    return result;
}


// is a clip positioned under where the time marker is?
// useful when combined with 
// App_MovePlaybackMarker(app, app->playbackTime);
// to make sure the clip loads if function returns true.
bool MediaClip_IsUnderTimeMarker(App* app, MediaClip* clip) {
    return app->playbackTime >= clip->padding && app->playbackTime <= (clip->padding+clip->width);
}

// return true if a playback update is necessary to keep
// things synchronized after a clip has moved.
// Uses the old position from mediaClip and its new from the last two arguments.
bool shouldPlaybackUpdateAfterMove(App* app, MediaClip* mediaClip, float drawClipLeftPadding, float drawClipWidth) {
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


// legacy waveform function. Code is kept in case i decide to draw waveform in some other part of the program
// visibleStartPXOffset and visibleEndPXOffset represent the amount of pixels 
// that are outside our visible range on either side.
// void drawWaveform(DynArr* peaks, ImVec2 graphSize, ImVec4 clippedColor, ImVec4 normalColor, float visibleStartPXOffset, float visibleEndPXOffset, float startCutoff, float endCutoff, int sampleRate) {
//     ImVec2 origin = ImGui::GetCursorScreenPos();
//
//     float peaksPerSecond = (float) sampleRate / (float) PEAK_BLOCK_SIZE;
//     float blockCount = peaks->size - ((startCutoff + endCutoff) * peaksPerSecond);
//
//     ImDrawList* drawList = ImGui::GetWindowDrawList();
//     int pixelWidth = (int) graphSize.x;
//     float midY = origin.y + graphSize.y * 0.5f;
//     float halfHeight = graphSize.y * 0.5f;
//     float blocksPerPixel = (float) blockCount / (float) pixelWidth;
//
//     // log_debug("visibleStart:%.2f, visibleEnd:%.2f", visibleStartPXOffset, visibleEndPXOffset);
//
//     for (int px = visibleStartPXOffset; px < pixelWidth; px++) {
//         if (px >= pixelWidth-visibleEndPXOffset)
//             break;
//
//         int startIdx = (int) (px * blocksPerPixel) + (int) (startCutoff * peaksPerSecond);
//         int endIdx = (int) ((px + 1) * blocksPerPixel) + (int) (startCutoff * peaksPerSecond);
//         if (endIdx <= startIdx) endIdx = startIdx + 1; // fallback, always cover at least one block
//
//         float colMin = 0.0f;
//         float colMax = 0.0f;
//         bool any = false;
//         for (int idx = startIdx; idx < endIdx; idx++) {
//             if (idx < 0 || (size_t) idx >= peaks->size) continue;
//             PeakBlock* pb = (PeakBlock*) DynArr_Get(peaks, (size_t) idx);
//             if (!any) {
//                 colMin = pb->min;
//                 colMax = pb->max;
//                 any = true;
//             } else {
//                 colMin = minf(colMin, pb->min);
//                 colMax = maxf(colMax, pb->max);
//             }
//         }
//         if (!any) continue;
//
//         bool clipped = false;
//         if (colMax > 1.0f || colMin < -1.0f) {
//             clipped = true;
//         }
//
//         float drawColMin = maxf(colMin, -1.0f);
//         float drawColMax = minf(colMax, 1.0f);
//
//         float x = origin.x + (float) px;
//         drawList->AddLine(ImVec2(x, midY - drawColMax * halfHeight), ImVec2(x, midY - drawColMin * halfHeight), ImColor(normalColor));
//
//
//         if (clipped) {
//             ImU32 color = ImColor(clippedColor);
//
//             const float pxheight = 3.0f;
//
//             if (colMax > 1.0f) {
//                 drawList->AddLine(ImVec2(x, origin.y), ImVec2(x, origin.y + pxheight), color);
//             }
//             if (colMin < -1.0f) {
//                 drawList->AddLine(ImVec2(x, origin.y + graphSize.y - pxheight),
//                                    ImVec2(x, origin.y + graphSize.y), color);
//             }
//         }
//     }
// }

// TODO: move all math functions into its own file. This is really dumb and lazy. or use a library
inline double clampf(double d, double min, double max) {
  const double t = d < min ? min : d;
  return t > max ? max : t;
}

inline ImVec4 lerpImVec4(const ImVec4& a, const ImVec4& b, float t) {
    return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
}

inline float envelopeToHeightPx(float env, float halfHeight) {
    float displayGamma = 0.4f; // boosts low signals
    // determines how fast the asymptotic curve should converge. Lower = faster
    // with some test values it seems like really loud things generally don't go over 1.46 in signal
    float kOverCompression = 10.0f; 

    if (env <= 1.0f) {
        return powf(env, displayGamma) * halfHeight;
    }
    float over = env - 1.0f;
    float compressed = 1.0f - 1.0f / (1.0f + over * kOverCompression);
    return halfHeight + compressed * halfHeight;
}

// visibleStartPXOffset and visibleEndPXOffset represent the amount of pixels 
// that are outside our visible range on either side.
void drawAudioEnvelope(DynArr* peaks, ImVec2 graphSize, ImVec4 clippedWarningColor, ImVec4 clippedSeriousColor, ImVec4 normalColor, float visibleStartPXOffset, float visibleEndPXOffset, float startCutoff, float endCutoff, int sampleRate) {
    float highestEncountered = 0;

    ImVec2 origin = ImGui::GetCursorScreenPos();

    float peaksPerSecond = (float) sampleRate / (float) PEAK_BLOCK_SIZE;
    float blockCount = peaks->size - ((startCutoff + endCutoff) * peaksPerSecond);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    int pixelWidth = (int) graphSize.x;
    float halfHeight = graphSize.y * 0.5f;
    float blocksPerPixel = (float) blockCount / (float) pixelWidth;

    float seriousThreshold = 1.2f;
    float seriousHeightPx = envelopeToHeightPx(seriousThreshold, halfHeight);
    float seriousY = origin.y + graphSize.y - seriousHeightPx;

    float clippingThresholdY = origin.y + graphSize.y - envelopeToHeightPx(1.0f, halfHeight); // if signal is over 1 or under -1 it is clipping.

    for (int px = visibleStartPXOffset; px < pixelWidth; px++) {
        if (px >= pixelWidth-visibleEndPXOffset)
            break;

        int startIdx = (int) (px * blocksPerPixel) + (int) (startCutoff * peaksPerSecond);
        int endIdx = (int) ((px + 1) * blocksPerPixel) + (int) (startCutoff * peaksPerSecond);
        if (endIdx <= startIdx) endIdx = startIdx + 1; // fallback, always cover at least one block

        float colMin = 0.0f;
        float colMax = 0.0f;
        bool any = false;
        for (int idx = startIdx; idx < endIdx; idx++) {
            if (idx < 0 || (size_t) idx >= peaks->size) continue;
            PeakBlock* pb = (PeakBlock*) DynArr_Get(peaks, (size_t) idx);
            if (!any) {
                colMin = pb->min;
                colMax = pb->max;
                any = true;
            } else {
                colMin = minf(colMin, pb->min);
                colMax = maxf(colMax, pb->max);
            }
        }
        if (!any) continue;

        // column absolute highest signal
        float colAbs = maxf(fabsf(colMin), fabsf(colMax));
        // apply asymptotic curve so the height never goes above what we are able so show
        float heightPx = envelopeToHeightPx(colAbs, halfHeight);

        float topY = origin.y + graphSize.y - heightPx;
        float bottomY = origin.y + graphSize.y;

        if (colAbs > highestEncountered) {
            highestEncountered = colAbs;
        }


        float x = origin.x + (float) px;
        drawList->AddRectFilledMultiColor(ImVec2(x, max(topY, clippingThresholdY)), ImVec2(x + 1.0f, bottomY),
                                   ImColor(normalColor), ImColor(normalColor), ImColor(normalColor), ImColor(normalColor));
        if (colAbs > 1.0f) {
            float yellowTopY = maxf(topY, seriousY);
            float t = clampf((clippingThresholdY - yellowTopY) / (clippingThresholdY - seriousY), 0.0f, 1.0f);
            ImU32 yellowTopColor = ImGui::ColorConvertFloat4ToU32(lerpImVec4(ImGui::ColorConvertU32ToFloat4(ImColor(normalColor)),
                                            ImGui::ColorConvertU32ToFloat4(ImColor(clippedWarningColor)), t));

            drawList->AddRectFilledMultiColor(ImVec2(x, max(topY, seriousY)), ImVec2(x + 1.0f, clippingThresholdY),
                                       ImColor(yellowTopColor), ImColor(yellowTopColor), ImColor(normalColor), ImColor(normalColor));

            if (colAbs > seriousThreshold) {
                drawList->AddRectFilledMultiColor(ImVec2(x, topY), ImVec2(x + 1.0f, seriousY),
                                           ImColor(clippedSeriousColor), ImColor(clippedSeriousColor), ImColor(clippedSeriousColor), ImColor(clippedSeriousColor));
            }
        }
    }

    // log_debug("Highest encountered: %.4f", highestEncountered);
}


// actually draw the track, both video and audio tracks
ImVec2 MediaClip_Draw_DrawTracks(App* app, MediaClip* mediaClip, int clipIndex, float drawClipLeftPadding, float drawClipWidth, bool isGhostClip) {
    ImU32 normal_border_color;
    if (isGhostClip) {
        normal_border_color = ImColor(app->colors.trackBorderGhost);
    } else {
        normal_border_color = ImColor(app->colors.trackBorderSelected);
    }

    ImVec2 cursor_trackclip(0, 0);
    ImGui::SetCursorScreenPos(app->timeline.cursContentTopLeft);
    for (int i = 0; i <= mediaClip->source->audioTracks; i++) {
        cursor_trackclip = ImGui::GetCursorScreenPos();
        ImVec2 cursor_trackclip_padded = ImGui::GetCursorScreenPos();
        cursor_trackclip_padded.x = (cursor_trackclip_padded.x + drawClipLeftPadding * app->scaleX);

        // Imgui doesn't allow the width to be 0
        // width will only be zero when it's about to get deleted when resizing
        float drawWidth = drawClipWidth;
        if (drawClipWidth == 0.0) {
            drawWidth = 0.001;
        }

        ImVec2 track_size(drawWidth * app->scaleX, app->timeline.clipHeight*app->scale);

        ImGui::SetCursorScreenPos(cursor_trackclip_padded);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        ImVec2 tracNamePos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton(("track" + std::to_string(clipIndex) + "Button#" + std::to_string(i)).c_str(), track_size);

        ImGui::PopStyleVar();


        ImVec2 r_min = ImGui::GetItemRectMin();
        ImVec2 r_max = ImGui::GetItemRectMax();

        ImColor track_color = app->colors.trackBackgroundAudio;
        if (i == 0) { // if video track
            track_color = app->colors.trackBackgroundVideo;
        }
        if (app->streamDisabled[i]) { // if track disabled
            track_color = app->colors.trackBackgroundMuted;
        }
        if (isGhostClip) {
            track_color = app->colors.trackBackgroundGhost;
        }

        ImGui::GetWindowDrawList()->AddRectFilled(r_min, r_max, track_color, 0.0f);

        mediaClip->isHovered = mediaClip->isHovered || ImGui::IsItemHovered();

        ImVec2 savedPos = ImGui::GetCursorScreenPos();
        if (i == 0) {
            ImU32 textColor = ImGui::GetColorU32(app->colors.trackText);
            ImGui::SetCursorScreenPos(tracNamePos);
            ImGui::PushStyleColor(ImGuiCol_Text, textColor);
            ImGui::Text("%s", mediaClip->source->filename);
            ImGui::PopStyleColor();
            ImGui::SetCursorScreenPos(savedPos);
        }
        
        //──────────── waveform peak visualizer ────────────
        ImVec2 graphSize = track_size;

        DynArr* peaks = &mediaClip->source->peakBlocks[i-1];
        if (mediaClip->source->peaksGenerated && i != 0 && peaks->size > 0) {
            ImGui::SetCursorScreenPos(tracNamePos);

            // the visible timeline viewport
            float viewportLeft = ImGui::GetWindowPos().x;
            float viewportRight = viewportLeft + ImGui::GetWindowSize().x;

            float clipStartScaled = drawClipLeftPadding*app->scaleX + app->timeline.cursContentTopLeft.x;
            float clipEndScaled = clipStartScaled + drawClipWidth*app->scaleX;

            // disregard anything that is not at all visible
            if (clipEndScaled > viewportLeft && clipStartScaled < viewportRight) {
                float visibleStartPXOffset = 0.0f;
                float visibleEndPXOffset = 0.0f;

                if (clipStartScaled < viewportLeft) {
                    visibleStartPXOffset += viewportLeft-clipStartScaled;
                }

                if (clipEndScaled > viewportRight) {
                    visibleEndPXOffset += clipEndScaled-viewportRight;
                }


                // drawWaveform(peaks, graphSize, visibleStartPXOffset, visibleEndPXOffset, mediaClip->startCutoff, mediaClip->endCutoff, mediaClip->source->sampleRates[i-1]);
                ImVec4 waveformColor = app->colors.trackWaveform;
                ImVec4 clippedSeriousColor = app->colors.trackWaveformClippedSerious;
                ImVec4 clippedWarningColor = app->colors.trackWaveformClippedWarning;
                waveformColor.w = app->streamDisabled[i] ? 0.2f : 1.0f;
                // log_debug("drawing for videoclip: %s", mediaClip->source->filename);
                drawAudioEnvelope(peaks, graphSize, clippedWarningColor, clippedSeriousColor, waveformColor, visibleStartPXOffset, visibleEndPXOffset, mediaClip->startCutoff, mediaClip->endCutoff, mediaClip->source->sampleRates[i-1]);
            }









            // ImGui::PushID(i);
            //
            // ImPlot::PushStyleVar(ImPlotStyleVar_PlotPadding, ImVec2(0, 0));
            // ImPlot::PushStyleColor(ImPlotCol_PlotBg, ImVec4(0, 0, 0, 0));
            // ImPlot::PushStyleColor(ImPlotCol_PlotBorder, ImVec4(0, 0, 0, 0));
            //
            // if (ImPlot::BeginPlot("##waveform", graphSize, ImPlotFlags_CanvasOnly | ImPlotFlags_NoInputs)) {
            //     ImPlot::SetupAxes(NULL, NULL,
            //                        ImPlotAxisFlags_NoDecorations,
            //                        ImPlotAxisFlags_NoDecorations);
            //     ImPlot::SetupAxesLimits(0, (double)peaks->size, -1.0, 1.0, ImPlotCond_Always);
            //
            //     ImPlot::PlotLineG("##max", PeakGetter_Max, peaks, (int)peaks->size);
            //     ImPlot::PlotLineG("##min", PeakGetter_Min, peaks, (int)peaks->size);
            //
            //     ImPlot::EndPlot();
            // }
            //
            // ImPlot::PopStyleColor(2);
            // ImPlot::PopStyleVar();
            // ImGui::PopID();


        }
        ImGui::SetCursorScreenPos(savedPos);
        //──────────── waveform peak visualizer END ────────────


        //─ border seperating track from track below in clip ─

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
        ImVec2 posStart(app->timeline.cursContentTopLeft.x + drawClipLeftPadding * app->scaleX, app->timeline.cursContentTopLeft.y);
        ImVec2 posEnd(app->timeline.cursContentTopLeft.x + (drawClipLeftPadding + drawClipWidth) * app->scaleX, app->timeline.cursContentTopLeft.y + app->timeline.clipHeight * app->scale * (mediaClip->source->audioTracks+1));
        ImGui::GetWindowDrawList()->AddRect(posStart, posEnd, border_color, 0.0f, 0, 1.0*app->scale);
    } else { // ########### clip left & right borders
        ImU32 border_color = ImColor(app->colors.trackBorder);
        ImVec2 posStart(app->timeline.cursContentTopLeft.x + drawClipLeftPadding * app->scaleX, app->timeline.cursContentTopLeft.y);
        ImVec2 posEnd(app->timeline.cursContentTopLeft.x + (drawClipLeftPadding + drawClipWidth) * app->scaleX, app->timeline.cursContentTopLeft.y + app->timeline.clipHeight * app->scale * (mediaClip->source->audioTracks+1));
        ImGui::GetWindowDrawList()->AddRect(posStart, posEnd, border_color, 0.0f, 0, 1.0*app->scale);
    }

    return cursor_trackclip;
}

void MediaClip_Draw(App* app, MediaClip* mediaClip, int clipIndex) {
	bool mouseLetGo = !ImGui::IsMouseDown(ImGuiMouseButton_Left);
	ImVec2 mousePos = ImGui::GetMousePos();

	float drawClipWidth = mediaClip->width;
	float drawClipLeftPadding = mediaClip->padding;

	if (mediaClip->isBeingMoved) {
		float diff = (mousePos.x - mediaClip->moveStartPos.x) / app->scaleX;
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
                    // log_debug("---clip left:")
                    // log_debug("dist;%.2f", leftClipDist);
                    // log_debug("snapping precision;%.2f", app->timeline.snappingPrecision);
                    // log_debug("snapping if below;%.2f", app->timeline.snapThresholdClip);
                    // log_debug("---")
                    if (leftClipDist < app->timeline.snapThresholdClip) {
                        drawClipLeftPadding = leftClipEvent->start+leftClipEvent->clip->width;
                        snapToGrid = false;
                    }
                }
                if (rightClipEvent != nullptr) {
                    // log_debug("---clip right:")
                    // log_debug("dist;%.2f", rightClipDist);
                    // log_debug("snapping precision;%.2f", app->timeline.snappingPrecision);
                    // log_debug("snapping if below;%.2f", app->timeline.snapThresholdClip);
                    // log_debug("---")
                    if (rightClipDist < app->timeline.snapThresholdClip && (!leftClipEvent || (rightClipDist < leftClipDist))) {
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
                // log_debug("---marker:")
                // log_debug("dist;%.2f", markerDistLeft);
                // log_debug("snapping precision;%.2f", app->timeline.snappingPrecision);
                // log_debug("snapping if below;%.2f", app->timeline.snapThresholdMarker);
                // log_debug("---")

                if (markerDistLeft < app->timeline.snapThresholdMarker) {
                    drawClipLeftPadding = app->playbackTime;
                    snapToGrid = false;
                }
                if (markerDistRight < app->timeline.snapThresholdMarker) {
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
			bool updatePlayback = shouldPlaybackUpdateAfterMove(app, mediaClip, drawClipLeftPadding, drawClipWidth);
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
		float cutoffOffset = (mousePos.x - mediaClip->resizeStartPos.x) / app->scaleX;
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
                if (leftClipDist < app->timeline.snapThresholdMarker) {
                    cutoffOffset -= drawClipLeftPadding+cutoffOffset-(leftClipEvent->start+leftClipEvent->clip->width);
                    snapToGrid = false;
                }
            }
            if (rightClipEvent != nullptr) {
                if (rightClipDist < app->timeline.snapThresholdClip && (!leftClipEvent || (rightClipDist < leftClipDist))) {
                    cutoffOffset -= drawClipLeftPadding+cutoffOffset-rightClipEvent->start;
                    snapToGrid = false;
                }
            }

            float markerDist = fabs(app->playbackTime-(drawClipLeftPadding+cutoffOffset));

            if (markerDist < app->timeline.snapThresholdMarker) {
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

			bool updatePlayback = shouldPlaybackUpdateAfterMove(app, mediaClip, drawClipLeftPadding, drawClipWidth);
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
		float cutoffOffset = (mediaClip->resizeStartPos.x - mousePos.x) / app->scaleX;
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
                if (leftClipDist < app->timeline.snapThresholdClip) {
                    cutoffOffset -= drawClipLeftPadding+drawClipWidth-cutoffOffset-(leftClipEvent->start+leftClipEvent->clip->width);
                    snapToGrid = false;
                }
            }
            if (rightClipEvent != nullptr) {
                if (rightClipDist < app->timeline.snapThresholdClip && (!leftClipEvent || (rightClipDist < leftClipDist))) {
                    cutoffOffset += drawClipLeftPadding+drawClipWidth-cutoffOffset-rightClipEvent->start;
                    snapToGrid = false;
                }
            }

            float markerDist = fabs(app->playbackTime-(drawClipLeftPadding+drawClipWidth-cutoffOffset));

            if (markerDist < app->timeline.snapThresholdMarker) {
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
			bool updatePlayback = shouldPlaybackUpdateAfterMove(app, mediaClip, drawClipLeftPadding, drawClipWidth);
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
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);


        // handle selection
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !mediaClip->isBeingMoved) {
            MediaClip** pFirstClip = (MediaClip**) DynArr_Get(&app->selectedClips, 0);

            if (ImGui::IsKeyDown(ImGuiKey_LeftShift) && pFirstClip) {

                MediaClip* firstClip = *pFirstClip;
                App_ClearClipSelections(app);

                if (mediaClip->timelineEventsIndex < firstClip->timelineEventsIndex) {
                    for (int i=firstClip->timelineEventsIndex; i >= mediaClip->timelineEventsIndex; i--) {
                        TimelineEvent event = app->timelineEvents[i];
                        if (event.type == TIMELINE_EVENT_VIDEO) {
                            DynArr_Append(&app->selectedClips, &event.clip);
                            event.clip->isSelected = true;
                        }
                    }

                } else { // if mediaClip->timelineEventsIndex >= lastClip->timelineEventsIndex
                    for (int i=firstClip->timelineEventsIndex; i <= mediaClip->timelineEventsIndex; i++) {
                        TimelineEvent event = app->timelineEvents[i];
                        if (event.type == TIMELINE_EVENT_VIDEO) {
                            DynArr_Append(&app->selectedClips, &event.clip);
                            event.clip->isSelected = true;
                        }
                    }
                }
            } else if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
                if (mediaClip->isSelected) {
                    DynArr_RemoveElement(&app->selectedClips, &mediaClip);
                    mediaClip->isSelected = false;
                } else {
                    DynArr_Append(&app->selectedClips, &mediaClip);
                    mediaClip->isSelected = true;
                }
            } else {
                if (mediaClip->isSelected) {
                    // bool addBack = app->selectedClips.size > 1;
                    App_ClearClipSelections(app);
                    // if (addBack) {
                        DynArr_Append(&app->selectedClips, &mediaClip);
                        mediaClip->isSelected = true;
                    // }
                } else {
                    App_ClearClipSelections(app);

                    DynArr_Append(&app->selectedClips, &mediaClip);
                    mediaClip->isSelected = true;
                }
            }
        }

        // handle resizing / show resizing cursor
        float edgeLeft = (cursor_trackclip.x + drawClipLeftPadding * app->scaleX);
        float edgeRight = (cursor_trackclip.x + (drawClipWidth + drawClipLeftPadding) * app->scaleX);

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
