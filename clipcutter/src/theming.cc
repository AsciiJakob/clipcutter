#include "pch.h"
#include "theming.h"
#include "app.h"

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

void UI_ApplyThemeVanillaLatte(App* app) {
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
    style.Colors[ImGuiCol_Text] = ImColor(0.416f, 0.255f, 0.169f, 1.000f);
    style.Colors[ImGuiCol_TextDisabled] = ImColor(0.659f, 0.537f, 0.435f, 1.000f);
    style.Colors[ImGuiCol_WindowBg] = ImColor(0.875f, 0.780f, 0.647f, 1.000f);
    style.Colors[ImGuiCol_ChildBg] = ImColor(0.875f, 0.780f, 0.647f, 1.000f);
    style.Colors[ImGuiCol_PopupBg] = ImColor(0.875f, 0.780f, 0.647f, 1.000f);
    style.Colors[ImGuiCol_Border] = ImColor(0.416f, 0.255f, 0.169f, 0.350f);
    style.Colors[ImGuiCol_FrameBg] = ImColor(0.949f, 0.902f, 0.812f, 1.000f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImColor(0.910f, 0.827f, 0.659f, 1.000f);
    style.Colors[ImGuiCol_FrameBgActive] = ImColor(0.878f, 0.722f, 0.471f, 1.000f);
    style.Colors[ImGuiCol_TitleBg] = ImColor(0.929f, 0.855f, 0.753f, 1.000f);
    style.Colors[ImGuiCol_TitleBgActive] = ImColor(0.875f, 0.780f, 0.647f, 1.000f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImColor(0.973f, 0.941f, 0.890f, 1.000f);
    style.Colors[ImGuiCol_MenuBarBg] = ImColor(0.875f, 0.780f, 0.647f, 1.000f);
    style.Colors[ImGuiCol_ScrollbarBg] = ImColor(0.973f, 0.941f, 0.890f, 1.000f);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImColor(0.851f, 0.769f, 0.612f, 1.000f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImColor(0.804f, 0.710f, 0.529f, 1.000f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImColor(0.788f, 0.588f, 0.310f, 1.000f);
    style.Colors[ImGuiCol_CheckMark] = ImColor(0.788f, 0.588f, 0.310f, 1.000f);
    style.Colors[ImGuiCol_SliderGrab] = ImColor(0.788f, 0.588f, 0.310f, 1.000f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImColor(0.878f, 0.722f, 0.471f, 1.000f);
    style.Colors[ImGuiCol_Button] = ImColor(0.925f, 0.851f, 0.706f, 1.000f);
    style.Colors[ImGuiCol_ButtonHovered] = ImColor(0.886f, 0.780f, 0.569f, 1.000f);
    style.Colors[ImGuiCol_ButtonActive] = ImColor(0.722f, 0.510f, 0.227f, 1.000f);
    style.Colors[ImGuiCol_Header] = ImColor(0.925f, 0.851f, 0.706f, 1.000f);
    style.Colors[ImGuiCol_HeaderHovered] = ImColor(0.886f, 0.780f, 0.569f, 1.000f);
    style.Colors[ImGuiCol_HeaderActive] = ImColor(0.722f, 0.510f, 0.227f, 1.000f);
    style.Colors[ImGuiCol_Separator] = ImColor(0.416f, 0.255f, 0.169f, 0.350f);
    style.Colors[ImGuiCol_SeparatorHovered] = ImColor(0.878f, 0.722f, 0.471f, 1.000f);
    style.Colors[ImGuiCol_SeparatorActive] = ImColor(0.788f, 0.588f, 0.310f, 1.000f);
    style.Colors[ImGuiCol_ResizeGrip] = ImColor(0.788f, 0.588f, 0.310f, 0.200f);
    style.Colors[ImGuiCol_ResizeGripHovered] = ImColor(0.788f, 0.588f, 0.310f, 0.500f);
    style.Colors[ImGuiCol_ResizeGripActive] = ImColor(0.788f, 0.588f, 0.310f, 1.000f);
    style.Colors[ImGuiCol_TabHovered] = ImColor(0.886f, 0.780f, 0.569f, 1.000f);
    style.Colors[ImGuiCol_Tab] = ImColor(0.941f, 0.886f, 0.769f, 1.000f);
    style.Colors[ImGuiCol_TabSelected] = ImColor(0.925f, 0.851f, 0.706f, 1.000f);
    style.Colors[ImGuiCol_TabDimmed] = ImColor(0.973f, 0.941f, 0.890f, 1.000f);
    style.Colors[ImGuiCol_TabDimmedSelected] = ImColor(0.875f, 0.780f, 0.647f, 1.000f);
    style.Colors[ImGuiCol_DockingPreview] = ImColor(0.788f, 0.588f, 0.310f, 0.350f);
    style.Colors[ImGuiCol_DockingEmptyBg] = ImColor(0.973f, 0.941f, 0.890f, 1.000f);
    style.Colors[ImGuiCol_PlotLines] = ImColor(0.659f, 0.537f, 0.435f, 1.000f);
    style.Colors[ImGuiCol_PlotLinesHovered] = ImColor(0.878f, 0.722f, 0.471f, 1.000f);
    style.Colors[ImGuiCol_PlotHistogram] = ImColor(0.788f, 0.588f, 0.310f, 1.000f);
    style.Colors[ImGuiCol_PlotHistogramHovered] = ImColor(0.878f, 0.722f, 0.471f, 1.000f);
    style.Colors[ImGuiCol_TableHeaderBg] = ImColor(0.925f, 0.851f, 0.706f, 1.000f);
    style.Colors[ImGuiCol_TableBorderStrong] = ImColor(0.416f, 0.255f, 0.169f, 0.350f);
    style.Colors[ImGuiCol_TableBorderLight] = ImColor(0.416f, 0.255f, 0.169f, 0.180f);
    style.Colors[ImGuiCol_TableRowBgAlt] = ImColor(0.000f, 0.000f, 0.000f, 0.020f);
    style.Colors[ImGuiCol_TextSelectedBg] = ImColor(0.788f, 0.588f, 0.310f, 0.350f);
    style.Colors[ImGuiCol_DragDropTarget] = ImColor(0.878f, 0.722f, 0.471f, 1.000f);
    style.Colors[ImGuiCol_NavCursor] = ImColor(0.788f, 0.588f, 0.310f, 1.000f);
    style.Colors[ImGuiCol_NavWindowingDimBg] = ImColor(0.200f, 0.200f, 0.200f, 0.400f);
    style.Colors[ImGuiCol_ModalWindowDimBg] = ImColor(0.000f, 0.000f, 0.000f, 0.500f);

    //--------------------- Clipcutter ---------------------
    app->colors.knobHover = ImColor(0.878f, 0.722f, 0.471f, 1.000f);
    app->colors.timelineBackground = ImColor(0.875f, 0.780f, 0.647f, 1.000f);
    app->colors.timelineTracklist = ImColor(0.827f, 0.722f, 0.565f, 1.000f);
    app->colors.timelineTicksBackground = ImColor(0.973f, 0.941f, 0.890f, 1.000f);
    app->colors.timelineTicksMajor = ImColor(0.478f, 0.310f, 0.204f, 1.000f);
    app->colors.timelineTicksMinor = ImColor(0.702f, 0.612f, 0.490f, 1.000f);
    app->colors.timelineTicksText = ImColor(0.416f, 0.255f, 0.169f, 1.000f);
    app->colors.timelineTimeMarker = ImColor(0.915f, 0.547f, 0.784f, 1.000f);
    app->colors.timelineTrackSeparator = ImColor(0.416f, 0.255f, 0.169f, 0.350f);
    app->colors.trackText = ImColor(0.173f, 0.102f, 0.063f, 1.000f);
    app->colors.trackBackgroundAudio = ImColor(0.851f, 0.725f, 0.557f, 1.000f);
    app->colors.trackBackgroundVideo = ImColor(0.788f, 0.588f, 0.310f, 1.000f);
    app->colors.trackBackgroundMuted = ImColor(0.761f, 0.718f, 0.659f, 1.000f);
    app->colors.trackBackgroundGhost = ImColor(0.502f, 0.502f, 0.502f, 0.500f);
    app->colors.trackBorderSelected = ImColor(1.000f, 1.000f, 1.000f, 1.000f);
    app->colors.trackBorderGhost = ImColor(0.600f, 0.600f, 0.600f, 0.600f);
    app->colors.trackBorder = ImColor(0.165f, 0.098f, 0.059f, 1.000f);
    app->colors.trackWaveform = ImColor(0.290f, 0.180f, 0.118f, 1.000f);
    app->colors.trackWaveformClippedWarning = ImColor(1.000f, 0.620f, 0.173f, 1.000f);
    app->colors.trackWaveformClippedSerious = ImColor(0.949f, 0.263f, 0.263f, 1.000f);

}

void UI_ApplyThemeMidnight(App* app) {
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



