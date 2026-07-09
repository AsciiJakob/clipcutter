#include "effects.h"
#include "app.h"
#include "pch.h"
#include "imgui_internal.h"

static const char* AVOption_TypeToString(enum AVOptionType type) {
    switch (type) {
        case AV_OPT_TYPE_FLAGS: return "FLAGS";
        case AV_OPT_TYPE_INT: return "INT";
        case AV_OPT_TYPE_INT64: return "INT64";
        case AV_OPT_TYPE_DOUBLE: return "DOUBLE";
        case AV_OPT_TYPE_FLOAT: return "FLOAT";
        case AV_OPT_TYPE_STRING: return "STRING";
        case AV_OPT_TYPE_RATIONAL: return "RATIONAL";
        case AV_OPT_TYPE_BINARY: return "BINARY";
        case AV_OPT_TYPE_DICT: return "DICT";
        case AV_OPT_TYPE_UINT64: return "UINT64";
        case AV_OPT_TYPE_CONST: return "CONST";
        case AV_OPT_TYPE_IMAGE_SIZE: return "IMAGE_SIZE";
        case AV_OPT_TYPE_PIXEL_FMT: return "PIXEL_FMT";
        case AV_OPT_TYPE_SAMPLE_FMT: return "SAMPLE_FMT";
        case AV_OPT_TYPE_VIDEO_RATE: return "VIDEO_RATE";
        case AV_OPT_TYPE_DURATION: return "DURATION";
        case AV_OPT_TYPE_COLOR: return "COLOR";
        case AV_OPT_TYPE_BOOL: return "BOOL";
        case AV_OPT_TYPE_CHLAYOUT: return "CHLAYOUT";
        case AV_OPT_TYPE_UINT: return "UINT";
        case AV_OPT_TYPE_FLAG_ARRAY: return "FLAG_ARRAY";
        default: return "UNKNOWN";
    }
}

static void AVOption_DefaultValueString(const EffectOption* opt, char* out, size_t n)
{
    if (opt->enumValueCount > 0) {
            snprintf(out, n, "%s", opt->enumValues[opt->indexOfDefaultValue].name);
    } else {
        switch (opt->type) {
            case AV_OPT_TYPE_INT:
            case AV_OPT_TYPE_INT64:
            case AV_OPT_TYPE_BOOL:
                snprintf(out, n, "%" PRId64, opt->defaultValue.i64);
                break;

            case AV_OPT_TYPE_DOUBLE:
            case AV_OPT_TYPE_FLOAT:
                snprintf(out, n, "%.6f", opt->defaultValue.dbl);
                break;

            case AV_OPT_TYPE_STRING:
                snprintf(out, n, "%s", opt->defaultValue.str);
                break;

            case AV_OPT_TYPE_CONST:
                snprintf(out, n, "const");
                break;

            default:
                snprintf(out, n, "(complex type)");
                break;
        }
    }
}

AudioEffect* AudioEffect_Create(App* app, char* name) {
    AudioEffect* effect = (AudioEffect*) malloc(sizeof(AudioEffect));
    if (!AudioEffect_Init(effect, name)) {
        free(effect);
        return nullptr;
    }


    DynArr_Append(&app->exportState.userAudioFilters, &effect);



    return effect;
}

bool AudioEffect_Init(AudioEffect* effect, char* name) {
    memset(effect, 0, sizeof(AudioEffect));
    strncpy(effect->filter_name, name, sizeof(effect->filter_name));
    effect->enabled = true;

    const AVFilter* filterInfo = avfilter_get_by_name(effect->filter_name);
    if (!filterInfo) return false;


    int count = 0;
    const AVOption *opt = NULL;
    if (filterInfo->priv_class) {
        while ((opt = av_opt_next(&filterInfo->priv_class, opt))) {
            if (opt->type == AV_OPT_TYPE_CONST) continue;
            if (opt->flags & AV_OPT_FLAG_DEPRECATED) continue;
            count++;
        }
    }

    effect->option_count = count;
    effect->options = (EffectOption*) malloc(count*sizeof(EffectOption));
    memset(effect->options, 0, count*sizeof(EffectOption));

    if (!AudioEffect_SetDefaults(effect)) {
        free(effect);
        return false;
    }

    return true;
}

bool AudioEffect_SetDefaults(AudioEffect* effect) {
    const AVFilter* filterInfo = avfilter_get_by_name(effect->filter_name);
    if (!filterInfo) return false;

    AVFilterGraph* graph = avfilter_graph_alloc();
    AVFilterContext* filterContext = avfilter_graph_alloc_filter(graph, filterInfo, "tmp");

    if (!filterInfo->priv_class) {
        return true;
    }

    const AVOption *opt = NULL;
    int i = 0;
    while ((opt = av_opt_next(&filterInfo->priv_class, opt))) {
        if (opt->type == AV_OPT_TYPE_CONST) continue;
        if (opt->flags & AV_OPT_FLAG_DEPRECATED) continue;

        EffectOption* newOption = &effect->options[i++];
        strncpy(newOption->name, opt->name, sizeof(newOption->name) - 1);
        strncpy(newOption->help, opt->help ? opt->help : "", sizeof(newOption->help) - 1);
        newOption->type = opt->type;
        newOption->min = opt->min;
        newOption->max = opt->max;
        // A min of -DBL_MAX / max of DBL_MAX effectively means unconstrained (according to claude)
        newOption->hasMinMax = (opt->min > -1e300 && opt->max < 1e300 && opt->max < (double)INT_MAX && opt->min > (double)INT_MIN);

        if (opt->unit) {
            const AVOption *enumOpt = NULL;

            // find all CONST enum type options and add it to the enumValues array
            while ((enumOpt = av_opt_next(&filterInfo->priv_class, enumOpt))) {
                if (enumOpt->type != AV_OPT_TYPE_CONST)
                    continue;

                if (!enumOpt->unit)
                    continue;

                if (strcmp(enumOpt->unit, opt->unit) != 0)
                    continue;

                if (newOption->enumValueCount >= MAX_ENUM_VALUES)
                    break;

                EffectEnumValue* val = &newOption->enumValues[newOption->enumValueCount];
                newOption->enumValueCount++;

                strncpy(val->name, enumOpt->name, sizeof(val->name) - 1);
                val->value = enumOpt->default_val.i64;

                if (val->value == opt->default_val.i64) {
                    newOption->indexOfSelectedValue = newOption->enumValueCount-1;
                    newOption->indexOfDefaultValue = newOption->indexOfSelectedValue;
                }

                log_debug("found enum option %s for option %s with value %d", val->name, opt->name, val->value);
            }

        }


        switch (opt->type) {
            case AV_OPT_TYPE_DOUBLE:
            case AV_OPT_TYPE_FLOAT:
                av_opt_get_double(filterContext->priv, opt->name, 0, &newOption->value.dbl);
                newOption->defaultValue.dbl = newOption->value.dbl;
                break;
            case AV_OPT_TYPE_INT:
            case AV_OPT_TYPE_INT64:
            case AV_OPT_TYPE_BOOL:
                av_opt_get_int(filterContext->priv, opt->name, 0, &newOption->value.i64);
                newOption->defaultValue.i64 = newOption->value.i64;
                break;
            case AV_OPT_TYPE_STRING:
            case AV_OPT_TYPE_SAMPLE_FMT:
            case AV_OPT_TYPE_PIXEL_FMT: {
                uint8_t *str = NULL;
                av_opt_get(filterContext->priv, opt->name, 0, &str);
                if (str) {
                    strncpy(newOption->value.str, (char *)str, sizeof(newOption->value.str) - 1);
                    newOption->value.str[sizeof(newOption->value.str)-1] = '\0';
                    strncpy(newOption->defaultValue.str, newOption->value.str, sizeof(newOption->defaultValue.str));
                    av_free(str);
                }
                break;
            }
            default:
                break;
        }
    }

    avfilter_graph_free(&graph);
    return true;
}


// Returns a string lavfi string for an audio effects. Remember to free.
// Example: "tempo=1.00000:pitch=1.67107:transients=0:detector=0:phase=0:window=0:smoothing=0:formant=0:pitchq=0:channels=0"
SB AudioEffect_BuildLavfiStringOfOptions(AudioEffect* effect) {
    SB optionsStr;
    SB_init(&optionsStr, 256);
        
    for (int i=0; i < effect->option_count; i++) {
        EffectOption* opt = &effect->options[i];

        if (i != 0)
            SB_appendf(&optionsStr, ":");
        SB_appendf(&optionsStr, "%s=", opt->name);

        switch (opt->type) {
            case AV_OPT_TYPE_DOUBLE:
            case AV_OPT_TYPE_FLOAT:
                SB_appendf(&optionsStr, "%.5f", opt->value.dbl);
                break;
            case AV_OPT_TYPE_INT:
            case AV_OPT_TYPE_INT64:
                SB_appendf(&optionsStr, "%" PRId64 , opt->value.i64);
                break;
            case AV_OPT_TYPE_BOOL:
                SB_appendf(&optionsStr, "%s", opt->value.i64 ? "true" : "false");
                break;
            case AV_OPT_TYPE_RATIONAL:
                continue;
            case AV_OPT_TYPE_STRING:
            case AV_OPT_TYPE_SAMPLE_FMT:
            case AV_OPT_TYPE_PIXEL_FMT:
                SB_appendf(&optionsStr, "%s", opt->value.str);
                break;
            default:
                continue; // skip types we can't handle
        }
    }

    return optionsStr;
}

// Update app->effectlavfiString to represent whatever the effects are sent to
// Example string with the effects "volume" and "aecho" (notice that the effects are seperated by a comma):
// lavfi=[volume=volume=1.0:precision=1:eval=0:replaygain=0:replaygain_preamp=0.00000:replaygain_noclip=true,aecho=in_gain=0.55034:out_gain=0.30000:delays=1000:decays=0.5]
void Effects_ApplyAudioEffects(App* app) {
    SB lavfiStr;
    SB_init(&lavfiStr, 1024);

    SB_appendf(&lavfiStr, "lavfi=[");
    for (size_t i=0; i < app->exportState.userAudioFilters.size; i++) {
        AudioEffect* effect = *(AudioEffect**) DynArr_Get(&app->exportState.userAudioFilters, i);

        if (i != 0)
            SB_appendf(&lavfiStr, ","); //"," for series, ";" for parallel.

        SB_appendf(&lavfiStr, "%s=", effect->filter_name);

        SB optionsStr = AudioEffect_BuildLavfiStringOfOptions(effect);
        SB_appendf(&lavfiStr, "%s", optionsStr.buf);
        SB_free(&optionsStr);

    }
    SB_appendf(&lavfiStr, "]");


    const char* cmd[] = { "set", "options/af", lavfiStr.buf, NULL };
    App_Queue_AddCommand(app, cmd);

    SB_free(&lavfiStr);
}


// returns true if should update vidoe/audio playback thing
bool Effects_RenderEffectOptions(App* app, AudioEffect* effect, int effectIndex) {
    cc_unused(effectIndex);
    bool updateEffect = false;

    for (int i=0; i < effect->option_count; i++) {
        EffectOption* opt = &effect->options[i];

        bool inputDrawed = true;

        if (opt->enumValueCount > 0) {
            if (ImGui::BeginCombo(opt->name, opt->enumValues[opt->indexOfSelectedValue].name)) {

                for (int constIdx=0; constIdx < opt->enumValueCount; constIdx++) {
                    EffectEnumValue constOpt = opt->enumValues[constIdx];
                    bool isSelected = false;
                    if (constIdx == opt->indexOfDefaultValue) {
                        char optionLabel[256];
                        snprintf(optionLabel, sizeof(optionLabel), "%s (default)", constOpt.name);
                        isSelected = ImGui::Selectable(optionLabel, constIdx==opt->indexOfSelectedValue);
                    } else {
                        isSelected = ImGui::Selectable(constOpt.name, constIdx==opt->indexOfSelectedValue);
                    }

                    if (isSelected) {
                        opt->value.i64 = constOpt.value;
                        opt->indexOfSelectedValue = constIdx;
                    }
                }
                ImGui::EndCombo();
            }

        } else {
            // The default value is in the opt->default_val union
            switch (opt->type) {
                case AV_OPT_TYPE_INT:
                case AV_OPT_TYPE_INT64:
                case AV_OPT_TYPE_UINT64:
                case AV_OPT_TYPE_FLAGS:
                    if (opt->hasMinMax) {
                        ImGui::SliderInt(opt->name, (int*) &opt->value.i64, (int) opt->min, (int) opt->max);
                        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !ImGui::TempInputIsActive(ImGui::GetItemID())) {
                            opt->value.i64 = opt->defaultValue.i64;
                            // hacky fix using imgui internals to stop value from changing back to where user double clicks
                            ImGui::ClearActiveID();
                        }
                    } else {
                        ImGui::InputInt(opt->name, (int*) &opt->value.i64);
                    }
                    break;
                case AV_OPT_TYPE_BOOL:
                    ImGui::Checkbox(opt->name, (bool*) &opt->value.i64);
                    break;
                case AV_OPT_TYPE_DOUBLE:
                case AV_OPT_TYPE_FLOAT:
                    if (opt->hasMinMax) {
                        ImGui::SliderScalar(opt->name, ImGuiDataType_Double, &opt->value.dbl, &opt->min, &opt->max);
                    } else {
                        ImGui::InputScalar(opt->name, ImGuiDataType_Double, &opt->value.dbl);
                    }
                    // only reset to default on double-click if the slider isn't in manual text input mode (ctrl+click state)

                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !ImGui::TempInputIsActive(ImGui::GetItemID())) {
                        opt->value.dbl = opt->defaultValue.dbl;
                        // hacky fix using imgui internals to stop value from changing back to where user double clicks
                        ImGui::ClearActiveID();
                    }
                    break;
                case AV_OPT_TYPE_COLOR:
                    log_debug("Color thing\n");
                case AV_OPT_TYPE_RATIONAL:
                    log_debug("Rational thing\n");
                case AV_OPT_TYPE_STRING:
                    ImGui::InputText(opt->name, opt->value.str, sizeof(opt->value.str)-1);
                    break;
                case AV_OPT_TYPE_IMAGE_SIZE:
                case AV_OPT_TYPE_VIDEO_RATE:
                default:
                    inputDrawed = false;
                    ImGui::Text("Unsupported option type %s on with option %s", AVOption_TypeToString(opt->type), opt->name);
                    log_warn("Unsupported option type %s on with option %s", AVOption_TypeToString(opt->type), opt->name);
            }

        }

        if (inputDrawed) {
            char defaultValStr[256];
            AVOption_DefaultValueString(opt, defaultValStr, sizeof(defaultValStr));
            ImGui::SetItemTooltip("%s\nDefault value:%s\nData Type:%s", opt->help, defaultValStr, AVOption_TypeToString(opt->type));
            updateEffect |= ImGui::IsItemDeactivatedAfterEdit();
        }
    }

    bool enable = true;
    if (ImGui::Checkbox("Enable", &enable)) {
        updateEffect = true;
    }

    if (ImGui::Button("Remove")) {
        DynArr_RemoveAt(&app->exportState.userAudioFilters, effectIndex);
        return true;
    }

    return updateEffect;

}

int AudioEffect_IsAudioFilter(const AVFilter *filter) {
    for (int i = 0; i < filter->nb_inputs; i++) {
        if (avfilter_pad_get_type(filter->inputs, i) == AVMEDIA_TYPE_AUDIO)
            return 1;
    }
    for (int i = 0; i < filter->nb_outputs; i++) {
        if (avfilter_pad_get_type(filter->outputs, i) == AVMEDIA_TYPE_AUDIO)
            return 1;
    }
    return 0;
}


char **Effects_GetAllFilterNames(size_t* outCount) {
    // *outCount = NUM_AVAILABLE_EFFECTS;
    // return (char**)AVAILABLE_AUDIO_EFFECTS;

    const AVFilter *filter = NULL;
    void *opaque = NULL;
    int count = 0;
    while ((filter = av_filter_iterate(&opaque)) != NULL)
        if (AudioEffect_IsAudioFilter(filter))
            count++;
    log_debug("Loaded %d audio filters from ffmpeg", count);

    char **names = (char**) malloc(count * sizeof(char *));
    if (!names) return NULL;

    opaque = NULL;
    int i = 0;
    while ((filter = av_filter_iterate(&opaque)) != NULL) {
        if (AudioEffect_IsAudioFilter(filter))
            names[i++] = strdup(filter->name);
    }

    *outCount = count;
    return names;
}

