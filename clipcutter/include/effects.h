#ifndef EFFECTS_H
#define EFFECTS_H
#include "pch.h"
#include "app.h"

#define MAX_ENUM_VALUES 32

typedef struct {
    char name[64];
    int64_t value;
} EffectEnumValue;

struct EffectOption {
    char name[64];
    char help[256];

    union {
        double dbl;
        int64_t i64;
        char str[256];
    } value;
    union {
        double dbl;
        int64_t i64;
        char str[256];
    } defaultValue;
    double min;
    double max;
    bool hasMinMax;

    // for options with constant enum str values
    int enumValueCount;
    EffectEnumValue enumValues[MAX_ENUM_VALUES];
    int indexOfSelectedValue;
    int indexOfDefaultValue;

    AVOptionType type;
};

struct AudioEffect{
    char filter_name[64];
    bool enabled; // unimplemented
    EffectOption* options;
    int option_count;
};

char** Effects_GetAllFilterNames(size_t* outCount);
AudioEffect* AudioEffect_Create(App* app, char* name);
bool AudioEffect_Init(AudioEffect* effect, char* name);
bool AudioEffect_SetDefaults(AudioEffect* effect);
SB AudioEffect_BuildLavfiStringOfOptions(AudioEffect* effect);
void AudioEffect_BuildLavfiString(AudioEffect* effect, char* outBuffer, size_t bufferSize);
void Effects_ApplyAudioEffects(App* app);
void AudioEffect_BuildFilterDesc(AudioEffect* effect, char* outBuffer, size_t bufferSize);
bool Effects_RenderEffectOptions(App* app, AudioEffect* effect, int effectIndex);
#endif

