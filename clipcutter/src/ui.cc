#include "pch.h"
#include "export.h"
#include "app.h"
#include "imgui_internal.h"
#include "mediaClip.h"
#include "settings.h"
#include "playback.h"
#include "effects.h"
#include "ui.h"


int exportPathInputCallback(ImGuiInputTextCallbackData data) {
    /*if (data.EventFlag == ImGuiInputTextFlags_CallbackCompletion) {*/
    /**/
    /*}*/
    log_debug("Buffer: %s", data.Buf);
    return 0;
}

//────────────── grid/ticks functions ──────────────

double UI_GetNiceNumber(double rawStep) {
    double exp = floor(log10(rawStep));
    double f = rawStep / pow(10, exp);

    double niceF;
    if (f < 1.5) niceF = 1;
    else if (f < 3.0) niceF = 2;
    else if (f < 7.0) niceF = 5;
    else niceF = 10;

    return niceF * pow(10, exp);
}



void formatTimecode(App* app, double t, bool showFrame, char* buf, size_t bufSize) {
    if (showFrame) {
        int fps = (int)round(app->projectFps);
        int totalFrames = (int)round(t * app->projectFps);
        int frames  = totalFrames % fps;
        int totalSecs = totalFrames / fps;
        int seconds = totalSecs % 60;
        int minutes = (totalSecs / 60) % 60;
        int hours   = totalSecs / 3600;
        if (hours > 0) {
            snprintf(buf, bufSize, "%02d:%02d:%02d:%02d", hours, minutes, seconds, frames);
        } else {
            snprintf(buf, bufSize, "%02d:%02d:%02d", minutes, seconds, frames);
        }
    } else {
        int totalSecs = (int)t;
        int seconds = totalSecs % 60;
        int minutes = (totalSecs / 60) % 60;
        int hours   = totalSecs / 3600;
        if (hours > 0) {
            snprintf(buf, bufSize, "%02d:%02d:%02d", hours, minutes, seconds);
        }
        else {
            snprintf(buf, bufSize, "%02d:%02d", minutes, seconds);
        }
    }
}



double screenXToTime(App* app, float screenX) {
    return (double)(screenX - app->timeline.cursContentTopLeft.x) / app->scaleX;
}
float timelineTimeToScreenX(App* app, double t) {
    return app->timeline.cursContentTopLeft.x + (float)(t * app->scaleX);
}


void DrawTimelineGrid(App* app, float viewportTop) {
    ImDrawList* draw = ImGui::GetWindowDrawList();

    float viewportLeft = ImGui::GetWindowPos().x;
    float viewportRight = viewportLeft + ImGui::GetWindowSize().x;
    double visibleStartS = screenXToTime(app, viewportLeft);
    double visibleEndS = screenXToTime(app, viewportRight);

    double step = app->timeline.snappingPrecision;

    int startIndex = floor(visibleStartS / step);
    int endIndex = ceil(visibleEndS / step);

    // TODO: do this backwards so that no lines are drawn over text
    for (int index = startIndex; index <= endIndex; index++) {
        double t = (double)index * step;
        float x = timelineTimeToScreenX(app, t);

        int majorEvery = 20;
        bool isMajor = (index % majorEvery) == 0;

        float majorLineLength = (TIMELINE_GRID_TICKS_HEIGHT*app->scale);
        float lineBottom = isMajor ? viewportTop+majorLineLength : viewportTop+majorLineLength/2.0;
        ImColor tickColor = isMajor ? app->colors.timelineTicksMajor : app->colors.timelineTicksMinor;
        draw->AddLine(ImVec2(x, viewportTop), ImVec2(x, lineBottom), tickColor);

        if (isMajor) {
            double frameTime = 1.0 / app->projectFps;
            bool frameLocked = false;
            if (step < frameTime) {
                step = frameTime;
                frameLocked = true;
            }

            char buf[32];
            formatTimecode(app, t, frameLocked, buf, sizeof(buf));
            draw->AddText(ImVec2(x + 2, viewportTop), ImColor(app->colors.timelineTicksText), buf);
        }
    }
}

//───────────── Theme export functions ─────────────

typedef struct {
    const char* name;
    size_t      offset;   // offsetof(AppColors, field)
} ColorFieldDesc;
 
#define COLOR_FIELD(fieldname) { #fieldname, offsetof(UiColors, fieldname) }
 
static const ColorFieldDesc g_clipcutterColorFields[] = {
    COLOR_FIELD(knobHover),

    COLOR_FIELD(timelineBackground),
    COLOR_FIELD(timelineTracklist),
    COLOR_FIELD(timelineTicksBackground),
    COLOR_FIELD(timelineTicksMajor),
    COLOR_FIELD(timelineTicksMinor),
    COLOR_FIELD(timelineTicksText),
    COLOR_FIELD(timelineTimeMarker),
    COLOR_FIELD(timelineTrackSeparator),

    COLOR_FIELD(trackText),
    COLOR_FIELD(trackBackgroundAudio),
    COLOR_FIELD(trackBackgroundVideo),
    COLOR_FIELD(trackBackgroundMuted),
    COLOR_FIELD(trackBackgroundGhost),
    COLOR_FIELD(trackBorderSelected),
    COLOR_FIELD(trackBorderGhost),
    COLOR_FIELD(trackBorder),
    COLOR_FIELD(trackWaveform),
    COLOR_FIELD(trackWaveformClippedWarning),
    COLOR_FIELD(trackWaveformClippedSerious),
};
#define CLIPCUTTER_COLOR_FIELD_COUNT \
    (sizeof(g_clipcutterColorFields) / sizeof(g_clipcutterColorFields[0]))
 
static ImVec4* getColorField(App* app, const ColorFieldDesc* desc) {
    return (ImVec4*)((char*)&app->colors + desc->offset);
}


static void appendColor(SB* out, ImVec4 c) {
    SB_appendf(out, "ImColor(%.3ff, %.3ff, %.3ff, %.3ff);\n", c.x, c.y, c.z, c.w);
}

static void exportTheme(App* app, SB* out) {
    SB_appendf(out, "    //--------------------- Dear ImGui ---------------------\n");
 
    ImGuiStyle* liveStyle = &ImGui::GetStyle();
    ImGuiStyle reference;
    ImGui::StyleColorsDark(&reference);
 
    for (int i = 0; i < ImGuiCol_COUNT; i++) {
        ImVec4 c = liveStyle->Colors[i];
        ImVec4 r = reference.Colors[i];
        if (c.x != r.x || c.y != r.y || c.z != r.z || c.w != r.w) {
            SB_appendf(out, "    style.Colors[ImGuiCol_%s] = ", ImGui::GetStyleColorName(i));
            appendColor(out,  c);
        }
    }
 
    SB_appendf(out, "\n    //--------------------- Clipcutter ---------------------\n");
    for (size_t i = 0; i < CLIPCUTTER_COLOR_FIELD_COUNT; i++) {
        const ColorFieldDesc* desc = &g_clipcutterColorFields[i];
        SB_appendf(out, "    app->colors.%s = ", desc->name);
        appendColor(out, *getColorField(app, desc));
    }
}


void UI_ApplyThemeDefault(App* app) {
    ImGui::StyleColorsDark(); // Apply ImGui defaults
    ImGuiStyle& style = ImGui::GetStyle();

    //──────────────────── [General] ────────────────────
    style.TabBorderSize = 1;
    style.WindowTitleAlign.x = 0.5f;
    style.WindowMenuButtonPosition = ImGuiDir_None;
    style.DockingSeparatorSize = 1; // default is 2

    style.WindowRounding = 6.0f;
    style.ChildRounding = 6.0f;
    style.PopupRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 4.0f;
    
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    
    style.WindowPadding = ImVec2(10.0f, 10.0f);
    style.FramePadding = ImVec2(8.0f, 4.0f);
    style.ItemSpacing = ImVec2(8.0f, 5.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
    style.IndentSpacing = 20.0f;
    style.ScrollbarSize = 14.0f;
    style.GrabMinSize = 10.0f;


    
    //──────────────────── [Colors] ────────────────────
    // This part of the code may be generated from the theme editor by clicking "copy"
    //--------------------- Dear ImGui ---------------------
    //--------------------- Dear ImGui ---------------------
    style.Colors[ImGuiCol_Text] = ImColor(0.925f, 0.925f, 0.933f, 1.000f);
    style.Colors[ImGuiCol_TextDisabled] = ImColor(0.500f, 0.500f, 0.514f, 1.000f);
    style.Colors[ImGuiCol_WindowBg] = ImColor(0.071f, 0.071f, 0.078f, 1.000f);
    style.Colors[ImGuiCol_ChildBg] = ImColor(0.106f, 0.106f, 0.114f, 1.000f);
    style.Colors[ImGuiCol_PopupBg] = ImColor(0.106f, 0.106f, 0.114f, 1.000f);
    style.Colors[ImGuiCol_Border] = ImColor(0.220f, 0.220f, 0.235f, 0.500f);
    style.Colors[ImGuiCol_FrameBg] = ImColor(0.127f, 0.127f, 0.193f, 1.000f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImColor(0.165f, 0.165f, 0.247f, 1.000f);
    style.Colors[ImGuiCol_FrameBgActive] = ImColor(0.243f, 0.243f, 0.330f, 1.000f);
    style.Colors[ImGuiCol_TitleBg] = ImColor(0.071f, 0.071f, 0.078f, 1.000f);
    style.Colors[ImGuiCol_TitleBgActive] = ImColor(0.106f, 0.106f, 0.114f, 1.000f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImColor(0.071f, 0.071f, 0.078f, 1.000f);
    style.Colors[ImGuiCol_MenuBarBg] = ImColor(0.106f, 0.106f, 0.114f, 1.000f);
    style.Colors[ImGuiCol_ScrollbarBg] = ImColor(0.071f, 0.071f, 0.078f, 1.000f);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImColor(0.184f, 0.184f, 0.196f, 1.000f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImColor(0.220f, 0.220f, 0.235f, 1.000f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImColor(0.259f, 0.588f, 0.980f, 1.000f);
    style.Colors[ImGuiCol_CheckMark] = ImColor(0.259f, 0.588f, 0.980f, 1.000f);
    style.Colors[ImGuiCol_SliderGrab] = ImColor(0.259f, 0.588f, 0.980f, 1.000f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImColor(0.361f, 0.663f, 1.000f, 1.000f);
    style.Colors[ImGuiCol_Button] = ImColor(0.141f, 0.141f, 0.212f, 1.000f);
    style.Colors[ImGuiCol_ButtonHovered] = ImColor(0.164f, 0.164f, 0.249f, 1.000f);
    style.Colors[ImGuiCol_ButtonActive] = ImColor(0.176f, 0.478f, 0.882f, 1.000f);
    style.Colors[ImGuiCol_Header] = ImColor(0.145f, 0.145f, 0.157f, 1.000f);
    style.Colors[ImGuiCol_HeaderHovered] = ImColor(0.184f, 0.184f, 0.196f, 1.000f);
    style.Colors[ImGuiCol_HeaderActive] = ImColor(0.176f, 0.478f, 0.882f, 1.000f);
    style.Colors[ImGuiCol_Separator] = ImColor(0.220f, 0.220f, 0.235f, 0.500f);
    style.Colors[ImGuiCol_SeparatorHovered] = ImColor(0.361f, 0.663f, 1.000f, 1.000f);
    style.Colors[ImGuiCol_SeparatorActive] = ImColor(0.259f, 0.588f, 0.980f, 1.000f);
    style.Colors[ImGuiCol_ResizeGrip] = ImColor(0.259f, 0.588f, 0.980f, 0.200f);
    style.Colors[ImGuiCol_ResizeGripHovered] = ImColor(0.259f, 0.588f, 0.980f, 0.500f);
    style.Colors[ImGuiCol_ResizeGripActive] = ImColor(0.259f, 0.588f, 0.980f, 1.000f);
    style.Colors[ImGuiCol_TabHovered] = ImColor(0.184f, 0.184f, 0.196f, 1.000f);
    style.Colors[ImGuiCol_Tab] = ImColor(0.106f, 0.106f, 0.114f, 1.000f);
    style.Colors[ImGuiCol_TabSelected] = ImColor(0.145f, 0.145f, 0.157f, 1.000f);
    style.Colors[ImGuiCol_TabDimmed] = ImColor(0.071f, 0.071f, 0.078f, 1.000f);
    style.Colors[ImGuiCol_TabDimmedSelected] = ImColor(0.106f, 0.106f, 0.114f, 1.000f);
    style.Colors[ImGuiCol_DockingPreview] = ImColor(0.259f, 0.588f, 0.980f, 0.350f);
    style.Colors[ImGuiCol_DockingEmptyBg] = ImColor(0.071f, 0.071f, 0.078f, 1.000f);
    style.Colors[ImGuiCol_PlotLines] = ImColor(0.500f, 0.500f, 0.514f, 1.000f);
    style.Colors[ImGuiCol_PlotLinesHovered] = ImColor(0.361f, 0.663f, 1.000f, 1.000f);
    style.Colors[ImGuiCol_PlotHistogram] = ImColor(0.259f, 0.588f, 0.980f, 1.000f);
    style.Colors[ImGuiCol_PlotHistogramHovered] = ImColor(0.361f, 0.663f, 1.000f, 1.000f);
    style.Colors[ImGuiCol_TableHeaderBg] = ImColor(0.145f, 0.145f, 0.157f, 1.000f);
    style.Colors[ImGuiCol_TableBorderStrong] = ImColor(0.220f, 0.220f, 0.235f, 0.500f);
    style.Colors[ImGuiCol_TableBorderLight] = ImColor(0.220f, 0.220f, 0.235f, 0.250f);
    style.Colors[ImGuiCol_TableRowBgAlt] = ImColor(1.000f, 1.000f, 1.000f, 0.020f);
    style.Colors[ImGuiCol_TextSelectedBg] = ImColor(0.259f, 0.588f, 0.980f, 0.350f);
    style.Colors[ImGuiCol_DragDropTarget] = ImColor(0.361f, 0.663f, 1.000f, 1.000f);
    style.Colors[ImGuiCol_NavCursor] = ImColor(0.259f, 0.588f, 0.980f, 1.000f);
    style.Colors[ImGuiCol_NavWindowingDimBg] = ImColor(0.200f, 0.200f, 0.200f, 0.400f);
    style.Colors[ImGuiCol_ModalWindowDimBg] = ImColor(0.000f, 0.000f, 0.000f, 0.500f);

    //--------------------- Clipcutter ---------------------
    app->colors.knobHover = ImColor(0.504f, 0.716f, 1.000f, 1.000f);
    app->colors.timelineBackground = ImColor(0.106f, 0.106f, 0.114f, 1.000f);
    app->colors.timelineTracklist = ImColor(0.086f, 0.086f, 0.094f, 1.000f);
    app->colors.timelineTicksBackground = ImColor(0.071f, 0.071f, 0.078f, 1.000f);
    app->colors.timelineTicksMajor = ImColor(0.750f, 0.750f, 0.761f, 1.000f);
    app->colors.timelineTicksMinor = ImColor(0.353f, 0.353f, 0.365f, 1.000f);
    app->colors.timelineTicksText = ImColor(0.925f, 0.925f, 0.933f, 1.000f);
    app->colors.timelineTimeMarker = ImColor(1.000f, 0.295f, 0.295f, 1.000f);
    app->colors.timelineTrackSeparator = ImColor(0.220f, 0.220f, 0.235f, 0.500f);
    app->colors.trackText = ImColor(0.078f, 0.078f, 0.086f, 1.000f);
    app->colors.trackBackgroundAudio = ImColor(0.341f, 0.666f, 0.830f, 1.000f);
    app->colors.trackBackgroundVideo = ImColor(0.984f, 0.784f, 0.482f, 1.000f);
    app->colors.trackBackgroundMuted = ImColor(0.502f, 0.482f, 0.478f, 1.000f);
    app->colors.trackBackgroundGhost = ImColor(0.500f, 0.500f, 0.500f, 0.500f);
    app->colors.trackBorderSelected = ImColor(1.000f, 1.000f, 1.000f, 1.000f);
    app->colors.trackBorderGhost = ImColor(0.700f, 0.700f, 0.700f, 0.600f);
    app->colors.trackBorder = ImColor(0.020f, 0.020f, 0.024f, 1.000f);
    app->colors.trackWaveform = ImColor(0.086f, 0.298f, 0.478f, 1.000f);
    app->colors.trackWaveformClippedWarning = ImColor(1.000f, 0.620f, 0.000f, 1.000f);
    app->colors.trackWaveformClippedSerious = ImColor(0.973f, 0.267f, 0.267f, 1.000f);
}


void drawThemeEditor(App* app) {
    if (ImGui::Begin("Theme editor")) {
        ImGui::SeparatorText("Imgui-builtins");
        ImGui::ShowStyleEditor();

        ImGui::SeparatorText("Clipcutter specific");

        static ImGuiColorEditFlags flags = ImGuiColorEditFlags_NoInputs;

        for (size_t i = 0; i < CLIPCUTTER_COLOR_FIELD_COUNT; i++) {
            const ColorFieldDesc* desc = &g_clipcutterColorFields[i];
            float* col = (float*)getColorField(app, desc);

            ImGui::PushID(i);
            if (ImGui::Button("C")) {
                SB s;
                SB_init(&s, 24);

                const ColorFieldDesc* desc = &g_clipcutterColorFields[i];
                appendColor(&s, *getColorField(app, desc));
                s.buf[s.len-1] = '\0'; // omit one character to remove newline
                ImGui::SetClipboardText(s.buf);
                SB_free(&s);
            }
            ImGui::PopID();

            ImGui::SameLine();
            ImGui::ColorEdit4(desc->name, col, flags);
        }

        ImGui::SeparatorText("Export");

        if (ImGui::Button("Copy colors to clipboard")) {
            SB exportBuf;
            SB_init(&exportBuf, 1024);
            exportTheme(app, &exportBuf);
            ImGui::SetClipboardText(exportBuf.buf);
            SB_free(&exportBuf);
        }

    }
    ImGui::End();
}

bool drawKnob(
    App* app,
    const char *label,
    float *p_value,
    float v_min,
    float v_max,
    float speed = 0,
    const char *format = "%.3f",
    ImGuiKnobVariant variant = ImGuiKnobVariant_Tick,
    float size = 0,
    ImGuiKnobFlags flags = 0,
    int steps = 10,
    float angle_min = -1,
    float angle_max = -1) {

    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, app->colors.knobHover);
    bool ret = ImGuiKnobs::Knob(label, p_value, v_min, v_max, speed, format, variant, size, flags, steps, angle_min, angle_max);
    ImGui::PopStyleColor();

    return ret;
}

void UI_DrawEditor(App* app) {
    app->scale = ImGui::GetFontSize()/13.0*app->userScaleFactor; // divide by 13 so we can use higher, readable values rather than decimal numbers like 0.052
    app->scaleX = app->scale*app->timeline.zoomX;
    app->timeline.snappingPrecision = UI_GetNiceNumber(TIMELINE_GRID_PRECISION / app->scaleX); // rename to snappingSteps?
                                                                                               app->timeline.snapThresholdClip = app->timeline.snappingPrecision;
                                                                                               app->timeline.snapThresholdMarker = app->timeline.snappingPrecision;

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
                    if (src != nullptr)  {
                        MediaClip* newClip = App_CreateMediaClip(app, src);
                        App_CalculateTimelineEvents(app);
                        if (MediaClip_IsUnderTimeMarker(app, newClip)) {
                            App_MovePlaybackMarker(app, app->playbackTime);
                        }
                    } else {
                        log_error("Failed to import media file");
                    }

                    filelist++;
                }

            };


            SDL_ShowOpenFileDialog(callback, app, app->window, filters, 3, NULL, true);
		}
		if (ImGui::Button("Export (f9)")) {
            ImGui::OpenPopup("Export options");
		}

        if (ImGui::IsKeyPressed(ImGuiKey_F9)) {
            ImGui::OpenPopup("Export options");
        }


        bool isDrawingExportPopup = ImGui::BeginPopupModal("Export options");
        if (isDrawingExportPopup) {
            // force theme editor to be redrawn so we can use it while popup is open.
            if (app->themeEditorMode) { 
                drawThemeEditor(app);
            }


            ExportOptions* options = &app->exportState.exportOptions;

            if (ImGui::Combo("##exportAsCombo", &options->exportAsComboIndex, EXPORT_AS_OPTIONS_STRS, EXPORT_AS_OPTIONS_COUNT, -1)) {
                if (options->exportAsComboIndex == EXPORT_AS_OPTION_VIDEO) {
                    Export_SetDefaultExportOptionsVideo(app);
                } else {
                    Export_SetDefaultExportOptionsAudio(app);
                }
            }


            // ImGui::InputTextWithHint("##exportpath", "Path to export to", app->exportPath, sizeof(app->exportPath), ImGuiInputTextFlags_AutoSelectAll, NULL, nullptr);
            ImGui::Text("Output path: %s", app->exportPath);

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
                    log_trace("In SDL_ShowSaveFileDialog callback");

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

                cc_unused(filters);
                SDL_ShowSaveFileDialog(callback, app, app->window, NULL, 0, NULL);
                // SDL_ShowSaveFileDialog(callback, app, app->window, filters, 3, "Z:\\Programming\\c\\clipcutter_sdl3\\build\\bin");
                // SDL_ShowSaveFileDialog(callback, app, app->window, filters, 3, app->exportPath);
            }

            if (options->exportAsComboIndex == EXPORT_AS_OPTION_VIDEO) {
                ImGui::SeparatorText("Video Encoding Options:");

                if (ImGui::BeginCombo("Video codec", "H.264/MPEG-4 AVC")) {
                    if (ImGui::Selectable("H.264/MPEG-4 AVC", true)) {

                    }
                    ImGui::EndCombo();
                }



                if (ImGui::BeginCombo("##crforcb", "Constant Rate Factor (CRF)")) {
                    if (ImGui::Selectable("Constant Rate Factor (CRF)", true)) {

                    }
                    if (ImGui::Selectable("Constant Bitrate (CBR) (unimplemented)", false)) {

                    }

                    ImGui::EndCombo();
                }

                ImGui::SliderFloat("CRF rate factor", &options->CBRRateFactor, 0, 50);
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("The rate factor for CRF compression. The lower, the higher quality.");
                    ImGui::Text("A sane range is most likely between 17-28.");
                    ImGui::Text("0: lossless");
                    ImGui::Text("18: visually lossless");
                    ImGui::Text("51: worst possible, heavily compressed");
                    ImGui::Spacing();
                    ImGui::Text("Default: 23");

                    ImGui::EndTooltip();
                }

                ImGui::Combo("encoding speed preset", &options->encoderPresetComboIndex, ENCODER_PRESETS, ENCODER_PRESET_COUNT, -1);
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("Slower preset will provide better compression (lower filesizes) at the cost of time.");
                    ImGui::Text("medium->ultrafast: 55%% faster (with much lower quality)");
                    ImGui::Text("medium->faster: 25%% faster");
                    ImGui::Text("medium->fast: 10%% faster");
                    ImGui::Text("medium->slower: 40%% slower");
                    ImGui::Text("medium->slow: 100%% slower");
                    ImGui::Text("medium->veryslow: 280%% slower (with minimal quality improvements over slow)");
                    ImGui::Spacing();
                    ImGui::Text("Default: medium");


                    ImGui::EndTooltip();
                }

                ImGui::SeparatorText("Audio Encoding Options:");
                ImGui::Checkbox("Include audio", &options->includeAudio);
                if (options->includeAudio) {
                    ImGui::Text("Audio codec: AAC");
                    // if (ImGui::BeginCombo("Audio codec", "AAC")) {
                    //     if (ImGui::Selectable("AAC", true)) {
                    //
                    //     }
                    //     ImGui::EndCombo();
                    // }
                    ImGui::BeginDisabled();
                    ImGui::Checkbox("Merge audio-tracks (has to be enabled for now)", &options->mergeAudioTracks);
                    ImGui::EndDisabled();
                }
            } else { // we chose "Export as audio"
                ImGui::SeparatorText("Encoding Options:");
                if (ImGui::BeginCombo("Audio codec", "MP3")) {
                    if (ImGui::Selectable("MP3", true)) {

                    }
                    ImGui::EndCombo();
                }
                ImGui::BeginDisabled();
                ImGui::Checkbox("Merge audio-tracks (has to be enabled for now)", &options->mergeAudioTracks);
                ImGui::EndDisabled();
            }
            if (ImGui::Button("Render")) {
                std::thread thread_obj(exportVideo, app, true);
                thread_obj.detach();
                // SDL_SetWindowProgressState(app->window, SDL_PROGRESS_STATE_NORMAL);
            };

            ImGui::Text("Status: %s", app->exportState.statusString);
            ImGui::ProgressBar(app->exportState.exportProgress);
            // TODO: experiment with this, but have to update SDL first.
            // SDL_SetWindowProgressValue(app->window, app->exportState.exportProgress);

            if (ImGui::Button("Close")) {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        if (!isDrawingExportPopup && app->themeEditorMode) {
            drawThemeEditor(app);
        }


		if (ImGui::Button("Settings")) {
            ImGui::OpenPopup("Settings");
		}

        if (ImGui::BeginPopupModal("Settings")) {
            Settings_DrawSettings(app);

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
        ImGuiID dock_id_down = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.35*app->scale, nullptr, &dock_id_up);
        ImGuiID dock_id_up_middle;
        constexpr float sideDocksWidth = 0.2; // in percent
        ImGuiID dock_id_up_left = ImGui::DockBuilderSplitNode(dock_id_up, ImGuiDir_Left, sideDocksWidth*app->scale, nullptr, &dock_id_up_middle);
        constexpr float dock_id_up_right_size = sideDocksWidth/(1-sideDocksWidth);
        ImGuiID dock_id_up_right = ImGui::DockBuilderSplitNode(dock_id_up_middle, ImGuiDir_Right, dock_id_up_right_size*app->scale, nullptr, &dock_id_up_middle);


        ImGui::DockBuilderDockWindow("Timeline", dock_id_down); // dock_main_id docks it to the center of the main docking thing
        ImGui::DockBuilderDockWindow("DebugThingies", dock_id_up_left);
        ImGui::DockBuilderDockWindow("Effects", dock_id_up_left);
        ImGui::DockBuilderDockWindow("Video Player", dock_id_up_middle);
        ImGui::DockBuilderDockWindow("Help", dock_id_up_right);

        ImGui::DockBuilderFinish(dockspace_id);
    }

    if (ImGui::Begin("Help")) {
        ImGui::TextWrapped("Welcome to Clipcutter!");
        ImGui::Spacing();
        ImGui::TextWrapped("Playback:");
        ImGui::TextWrapped("SPACE - toggle pause of video playback");
        ImGui::TextWrapped("Left arrow - navigate one frame to the left");
        ImGui::TextWrapped("Right arrow - navigate one frame to the right");
        ImGui::Spacing();
        ImGui::TextWrapped("Timeline:");
        ImGui::TextWrapped("DEL - delete selected clip");
        ImGui::TextWrapped("ctrl + a - select all clips");
        ImGui::TextWrapped("s - split clip at marker");
        ImGui::TextWrapped("Scroll wheel - zoom in and out");
        ImGui::TextWrapped("Shift + Scroll wheel - scroll horizontally");
        ImGui::TextWrapped("middle mouse - pan timeline");
        ImGui::TextWrapped("0 - move marker to start of timeline");
        ImGui::Spacing();
        ImGui::TextWrapped("Misc:");
        ImGui::TextWrapped("F9 - open export modal");
    }
    ImGui::End();

    if (app->debugMode) {
        if (ImGui::Begin("DebugThingies")) {
            ImGui::Text("frame time: %.3f ms", ImGui::GetIO().DeltaTime * 1000.0f);
            ImGui::Text("playbacktime: %.2f", app->playbackTime);
            ImGui::Text("playbackActive: %d", app->playbackActive);
            ImGui::Text("scaling: %.2f", app->scale);
            ImGui::Text("scaling X: %.2f", app->scaleX);
            ImGui::Text("timeline width: %.2f", app->timeline.width);
            // ImGui::Text("timelineEvent: %d", app->timelineEvents[app->timelineEventIndex].type);
            ImGui::Text("timelineEvent: %s", TimelineEventType_ToString(app->timelineEvents[app->timelineEventIndex].type));
            if (app->loadedMediaSource != nullptr) {
                ImGui::Text("currentLoaded: %s", app->loadedMediaSource->filename); }

            ImGui::Text("isLoadingVideo: %d", app->isLoadingVideo);


            ImGui::InputDouble("Force seek", &app->playbackTime, -1, -1, "%.2f", 0);
            if (ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Enter, false)) {
                log_info("user is force seeking");
                Playback_SetPlaybackPos(app, app->playbackTime);
            }


            ImGui::Text("------clip 1:");
            MediaClip* testClip = app->mediaClips[0];
            if (testClip != nullptr) {
                ImGui::Text("length: %.2f", testClip->source->length);
                ImGui::Text("width: %.2f", testClip->width);
                ImGui::Text("padding: %.2f", testClip->padding);
                ImGui::Text("cutoffstart: %.2f", testClip->startCutoff);
                ImGui::Text("cutoffend: %.2f", testClip->endCutoff);

                ImGui::Checkbox("track1beingMoved", &testClip->isBeingMoved);
            }

        }
        ImGui::End();
    }

	if (ImGui::Begin("Effects")) {
        ImGui::TextWrapped("Tip: when sliders are used, ctr-click to enter precise values, double click to reset to default.\nFor detailed effect descriptions, see:");
        ImGui::TextLinkOpenURL("https://ffmpeg.org/ffmpeg-filters.html", "https://ffmpeg.org/ffmpeg-filters.html");
		ImGui::SeparatorText("Audio effects:");

        for (size_t i=0; i < app->exportState.userAudioFilters.size ; i++) {
            AudioEffect* effect = *(AudioEffect**) DynArr_Get(&app->exportState.userAudioFilters, i);
            bool showHeader = true;
            char label[64];
            snprintf(label, sizeof(label), "%s##%zu", effect->filter_name, i);
            if (ImGui::CollapsingHeader(label)) {
                bool shouldUpdate = Effects_RenderEffectOptions(app, effect, i);

                if (shouldUpdate) {
                    Effects_ApplyAudioEffects(app);
                }
            }
            if (!showHeader) {
                // delete
            }

        }

        static size_t item_selected_idx = 0; 

        const char* combo_preview_value = app->availableFilterNames[item_selected_idx];
        if (ImGui::BeginCombo("Effect", combo_preview_value, 0)) {
            static ImGuiTextFilter filter;
            if (ImGui::IsWindowAppearing()) {
                ImGui::SetKeyboardFocusHere();
                filter.Clear();
            }
            ImGui::SetNextItemShortcut(ImGuiMod_Ctrl | ImGuiKey_F);
            filter.Draw("##Filter", -FLT_MIN);

            for (size_t n = 0; n < app->availableFilterNamesCount; n++) {
                const bool is_selected = (item_selected_idx == n);
                if (filter.PassFilter(app->availableFilterNames[n]))
                    if (ImGui::Selectable(app->availableFilterNames[n], is_selected)) {
                        item_selected_idx = n;
                        log_debug("selected filter: %s", app->availableFilterNames[item_selected_idx]);
                        
                        AudioEffect_Create(app, app->availableFilterNames[item_selected_idx]);
                    }
            }
            ImGui::EndCombo();
        }

	}
	ImGui::End();


	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(0, 0));

	if (ImGui::Begin("Timeline")) {
		ImVec2 cursorTracksBefore;
		ImVec2 cursorTracklistAfter;
        //─────────────────── Tracklist ────────────────────

		{
			ImGui::BeginGroup();

			ImVec4 tracklistColor = app->colors.timelineTracklist;
            double tracklistWidth = 104*app->scale;
            // double tracklistWidth = 95.0;
			// ImVec2 tracklistSize = ImVec2(tracklistWidth, fmax(ImGui::GetContentRegionAvail().y, (float)((app->timeline.highestTrackCount) * app->timeline.clipHeight)));
			ImVec2 tracklistSize = ImVec2(tracklistWidth, fmax(ImGui::GetContentRegionAvail().y, (float)((app->timeline.highestTrackCount) * app->timeline.clipHeight)));

			ImVec2 cursorTrackListBefore = ImGui::GetCursorScreenPos();
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
			ImGui::Dummy(tracklistSize);
			ImGui::PopStyleVar();
			ImVec2 r_min = ImGui::GetItemRectMin();
			ImVec2 r_max= ImGui::GetItemRectMax();
			cursorTracklistAfter = ImGui::GetCursorScreenPos();

			ImDrawList* timelineDrawlist = ImGui::GetWindowDrawList();
			timelineDrawlist->AddRectFilled(r_min, r_max, ImColor(tracklistColor));
			ImGui::SetCursorScreenPos(cursorTrackListBefore);

            //─── aligned with grid ticks ontop of timeline ────

            ImVec2 cursorBeforeTickAligned = ImGui::GetCursorScreenPos();
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            ImGui::Dummy(ImVec2(0, TIMELINE_GRID_TICKS_HEIGHT));
			ImGui::PopStyleVar();
            ImVec2 cursorAfterTickAligned = ImGui::GetCursorScreenPos();

            ImGui::SetCursorScreenPos(cursorBeforeTickAligned);

            char timeStr[64];
            formatTimecode(app, app->playbackTime, true, timeStr, sizeof(timeStr));
            // ImGui::Text("Time: %.3f", app->playbackTime);
            ImGui::Text("Time: %s", timeStr);

            ImGui::SetCursorScreenPos(cursorAfterTickAligned);

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            ImGui::Separator();
            ImGui::PopStyleVar();

            ImGui::SetCursorScreenPos(cursorAfterTickAligned);

            //────────────── aligned with tracks ───────────────

			cursorTracksBefore = cursorAfterTickAligned;
            ImVec2 trackCursor = cursorTracksBefore;
			for (int i = 0; i < app->timeline.highestTrackCount+1; i++) {

                if (app->streamDisabled[i]) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
                    ImGui::Text("Track %d", i+1);
                    ImGui::PopStyleColor();
                } else {
                    ImGui::Text("Track %d", i+1);
                }
				ImGui::SameLine(tracklistWidth - 40*app->scale);

                if (i != 0) {
                    char muteButtonLabel[10];
                    snprintf(muteButtonLabel, sizeof(muteButtonLabel), "Mute##%d", (unsigned char) i);

                    if (ImGui::SmallButton(muteButtonLabel)) {
                        app->streamDisabled[i] = !app->streamDisabled[i];
                        Playback_ApplyLavfiComplex(app);
                    }

                    ImGui::Text("Gain: %.1f", app->streamAudioGain[i]);

                    ImGui::SameLine(tracklistWidth - 30*app->scale);

                    char gainLabel[10];
                    snprintf(gainLabel, sizeof(gainLabel), "Gain##%d", (unsigned char) i);
                    if (drawKnob(app, gainLabel, &app->streamAudioGain[i], -100.0f, 50.0f, 0.4f, "%.1f", ImGuiKnobVariant_WiperOnly, 22, ImGuiKnobFlags_AlwaysClamp | ImGuiKnobFlags_NoTitle | ImGuiKnobFlags_NoInput)) {
                    }

                    if (ImGui::IsItemDeactivated())
                        Playback_ApplyLavfiComplex(app);

                    if (ImGui::IsItemActive() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        app->streamAudioGain[i] = 0.0;
                    }

                }



				trackCursor.y += app->timeline.clipHeight*app->scale;
				ImGui::SetCursorScreenPos(trackCursor);

				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
				ImGui::Separator();
				ImGui::PopStyleVar();
			}
			ImGui::Dummy(ImVec2(0, 0)); // workaround.If there is no element(such as text or button or this) after the last track's ImGUI Separator then the SameL

			ImGui::SetCursorScreenPos(cursorTracksBefore);

			//ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
			ImGui::EndGroup(); 
			//ImGui::PopStyleVar();
		}

		bool hoveringOverTrack = false;

        //──────────────────── timeline ────────────────────

		{
			//───────────────────── setup ──────────────────────



			ImGui::SetCursorScreenPos(cursorTracklistAfter);
			ImGui::BeginGroup();
			ImColor timeline_color = app->colors.timelineBackground;
			app->timeline.snappingEnabled = !ImGui::IsKeyDown(ImGuiKey_LeftShift);

			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
			ImGui::SameLine();
			ImVec2 childSize = ImVec2(ImGui::GetContentRegionAvail().x, fmax(ImGui::GetContentRegionAvail().y, (app->timeline.highestTrackCount) * app->timeline.clipHeight * app->scale));
			// create child window so that we can have a horizontal scrollbar for the timeline
			ImGui::BeginChild("TimelineWindowChild", childSize, false, ImGuiWindowFlags_HorizontalScrollbar);

			bool timelineHovered = ImGui::IsWindowHovered();

			ImVec2 timelineSize = ImVec2(app->timeline.width*app->scaleX, ImGui::GetContentRegionAvail().y);


            //─────────────── background visuals ───────────────
			ImVec2 cursorGridTicks = ImGui::GetCursorScreenPos();
            ImVec2 rectBottomRight = cursorGridTicks;
            rectBottomRight.y += TIMELINE_GRID_TICKS_HEIGHT*app->scale;
            rectBottomRight.x += timelineSize.x;

            ImGui::GetWindowDrawList()->AddRectFilled(cursorGridTicks, rectBottomRight, ImColor(app->colors.timelineTicksBackground), 0.0f);


            ImVec2 contentStart = cursorGridTicks; // shallow copy is fine
            contentStart.y += TIMELINE_GRID_TICKS_HEIGHT * app->scale;
            app->timeline.cursContentTopLeft = contentStart; // todo: refac to use this

            // the timeline grid ticks at the top of the timeline is included in the invisibleButton.
			ImGui::SetNextItemAllowOverlap();
			ImGui::InvisibleButton("timeline", timelineSize);
			ImGui::PopStyleVar();

			ImVec2 r_max = ImGui::GetItemRectMax();
			ImGui::GetWindowDrawList()->AddRectFilled(app->timeline.cursContentTopLeft, r_max, timeline_color);

			bool timelineClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

            // grid ticks have to be drawn after other timeline background is drawn
            DrawTimelineGrid(app, cursorGridTicks.y);

                                      
            // draw track seperators
            ImVec2 separatorPos = app->timeline.cursContentTopLeft;

            for (int i=0; i < app->timeline.highestTrackCount+1; i++) {
                ImGui::SetCursorScreenPos(separatorPos);

                ImU32 separatorColor = ImColor(app->colors.timelineTrackSeparator);
                ImGui::PushStyleColor(ImGuiCol_Separator, separatorColor);
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
                ImGui::Separator();
                ImGui::PopStyleColor();
                ImGui::PopStyleVar();

                separatorPos.y += app->timeline.clipHeight*app->scale;
            }

			ImGui::SetCursorScreenPos(app->timeline.cursContentTopLeft);

            MediaClip* drawAgain = nullptr;
			for (int i = 0; i < MEDIACLIPS_SIZE; i++) { // draw clips
				MediaClip* clip = app->mediaClips[i];
				if (clip == nullptr) break;
				MediaClip_Draw(app, clip, i);
                if (clip->width == 0.0) {
                    App_DeleteMediaClip(app, clip);
                    App_CalculateTimelineEvents(app);
                    i = i-1;
                } else if (clip->isResizingLeft || clip->isResizingRight || clip->isBeingMoved) {
                    drawAgain = clip;
                }
			}

            if (drawAgain != nullptr) {
                MediaClip_Draw_DrawTracks(app, drawAgain, MEDIACLIPS_SIZE+1, drawAgain->padding, drawAgain->width, true);

                // resized/moved clip is drawn again after all other clips are drawn because that
                // can avoid clips later in the mediaClips array being drawn over the clip
                MediaClip_Draw_DrawTracks(app, drawAgain, MEDIACLIPS_SIZE+2, drawAgain->drawPadding, drawAgain->drawWidth, false);
            }

			{ // timeMarker
				float timeMarkerPos = app->playbackTime*app->scaleX;

				ImGui::SetCursorScreenPos(cursorGridTicks);
				ImVec2 cursor_offset = ImGui::GetCursorScreenPos();
				cursor_offset.x = cursor_offset.x + timeMarkerPos;
				ImGui::SetCursorScreenPos(cursor_offset);

				ImColor timeMarkerColor = app->colors.timelineTimeMarker;
				ImVec2 timeline_size(2*app->scale, ImGui::GetContentRegionAvail().y);
				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
				ImGui::Dummy(timeline_size);
				ImGui::PopStyleVar();

				ImVec2 r_min = ImGui::GetItemRectMin();
				ImVec2 r_max = ImGui::GetItemRectMax();

				ImGui::GetWindowDrawList()->AddRectFilled(r_min, r_max, timeMarkerColor );
			}

            { // panning around with middle mouse button
              if (timelineHovered && ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
                float timelineMousePos = ImGui::GetMousePos().x - app->timeline.cursContentTopLeft.x;
                cc_unused(timelineMousePos);
                float panDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Middle, 0.0).x;
                panDelta = panDelta * -1; // negate
                float currentScrollPos = ImGui::GetScrollX();
                ImGui::SetScrollX(currentScrollPos + panDelta);
                ImGui::ResetMouseDragDelta(ImGuiMouseButton_Middle);
              }
            }

			{ // zooming in and out of the timeline
				if (timelineHovered && !ImGui::IsKeyDown(ImGuiKey_LeftShift)) {
					float mw = ImGui::GetIO().MouseWheel; // -1 for downwards, 1 for upwards
					float factor = 1.05f;

					if (mw != 0) {
						float oldZoom = app->timeline.zoomX;
						if (mw > 0) {
							app->timeline.zoomX = app->timeline.zoomX * factor;
						} else {
							app->timeline.zoomX = app->timeline.zoomX / factor;
                            if (ImGui::GetWindowWidth() / app->timeline.zoomX > app->timeline.width) {
                                app->timeline.zoomX = ImGui::GetWindowWidth() / app->timeline.width; // revert change to limit how far we can zoom out.
                            }

						}

						float currentScrollPos = ImGui::GetScrollX();
						float timelineMousePos = ImGui::GetMousePos().x - app->timeline.cursContentTopLeft.x;

						float diffBefore = timelineMousePos / oldZoom - currentScrollPos;
						float diffAfter = timelineMousePos / app->timeline.zoomX - currentScrollPos;

						float offset = diffBefore - diffAfter;
						ImGui::SetScrollX(currentScrollPos + offset * app->timeline.zoomX);

					}
				}
			}

			{ // changing playback cursor position
				if (!hoveringOverTrack && timelineClicked) {
					ImVec2 mousePos = ImGui::GetMousePos();
					if (mousePos.x > app->timeline.cursContentTopLeft.x) {
						float secs = (mousePos.x - app->timeline.cursContentTopLeft.x)/app->scaleX;
						MediaClip* clip = App_FindClosestMediaClip(app, secs);
                        //log_debug("CLOSEST MEDIA CLIP IS: %s", clip->source->filename);
						if (app->timeline.snappingEnabled && clip != nullptr) {
							float snapSensitivity = 10;
							float track1LeftmostPos = app->timeline.cursContentTopLeft.x + clip->padding * app->scaleX;
							float track1RightmostPos = app->timeline.cursContentTopLeft.x + (clip->padding + clip->width) * app->scaleX;

							if (fabs(mousePos.x - track1LeftmostPos) < snapSensitivity) {
								mousePos.x = track1LeftmostPos;
							}
							else if (fabs(mousePos.x - track1RightmostPos) < snapSensitivity) {
								mousePos.x = track1RightmostPos;
							}
						}

						
						float newSecs = (mousePos.x - app->timeline.cursContentTopLeft.x)/app->scaleX;
                        App_ClearClipSelections(app);
						app->playbackTime = newSecs;
						App_MovePlaybackMarker(app, newSecs);
					}
				}
			}

			ImGui::EndChild();
			ImGui::EndGroup();
		}


	}
	ImGui::PopStyleVar();
	ImGui::End();

    bool show_demo_window = ArgParse_IsFlagSet("imgui-demo-window");
    // 1. Show the big demo window (Most of the sample code is in
    // ImGui::ShowDemoWindow()! You can browse its code to learn more about
    // Dear ImGui!).
    if (show_demo_window) {
        ImGui::ShowDemoWindow(&show_demo_window);
    }

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
            } else {
                displaySize.y = contentRegion.x / aspect;
            }

            // Center the image
            ImVec2 cursorPos = ImGui::GetCursorPos();
            ImGui::SetCursorPos(
                cursorPos +
                ImVec2((contentRegion.x - displaySize.x) * 0.5f,
                       (contentRegion.y - displaySize.y) * 0.5f));

            ImGui::Image((ImTextureID)app->mpv_texture, displaySize);

            ImGui::SetCursorPos(
                cursorPos +
                ImVec2((contentRegion.x - displaySize.x) * 0.5f,
                       (contentRegion.y - displaySize.y) * 0.5f));

            if (app->isLoadingVideo) {
                ImGui::Text("Video source is loading...");
            }
        }
        ImGui::End();
    }
}
