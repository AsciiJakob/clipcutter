#ifndef THEMING_H
#define THEMING_H
#include "pch.h"

#define UI_COLOR_FIELDS(X) \
    X(knobHover) \
    X(timelineBackground) \
    X(timelineTracklist) \
    X(timelineTicksBackground) \
    X(timelineTicksMajor) \
    X(timelineTicksMinor) \
    X(timelineTicksText) \
    X(timelineTimeMarker) \
    X(timelineTrackSeparator) \
    X(trackText) \
    X(trackBackgroundAudio) \
    X(trackBackgroundVideo) \
    X(trackBackgroundMuted) \
    X(trackBackgroundGhost) \
    X(trackBorderSelected) \
    X(trackBorderGhost) \
    X(trackBorder) \
    X(trackWaveform) \
    X(trackWaveformClippedWarning) \
    X(trackWaveformClippedSerious)

struct UiColors {
#define X(name) ImVec4 name;
    UI_COLOR_FIELDS(X)
#undef X
};

typedef struct {
    const char* name;
    size_t      offset;
} ColorFieldDesc;

#define X(name) { #name, offsetof(UiColors, name) },
static const ColorFieldDesc g_clipcutterColorFields[] = {
    UI_COLOR_FIELDS(X)
};
#undef X

#define CLIPCUTTER_COLOR_FIELD_COUNT \
    (sizeof(g_clipcutterColorFields) / sizeof(g_clipcutterColorFields[0]))

typedef struct App App; // forward decleration

void UI_ApplyThemeVanillaLatte(App* app);
void UI_ApplyThemeMidnight(App* app);
void drawThemeEditor(App* app);

#endif
