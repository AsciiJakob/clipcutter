// Argument parser library by AsciiJakob 2026-07-01

#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include "argparse.h"
#include "sb.h"

typedef union {
    char* strVal;
    int intVal;
    float floatVal;
} ArgValue;

struct FlagParameter {
    const char* name;
    ArgValue value;
    ArgType dataType;
    bool isSet;
} typedef FlagParameter;

struct Flag {
    const char* name;
    char abbreviation;
    const char* description;
    bool hasParameter;
    FlagParameter parameter; // NULL if flag takes no parameter.
    bool isSet;
} typedef Flag;


// stores all the registered flags
static struct {
    Flag* flags;
    int flagCount;
    int flagsCapacity; // for realloc growth

    const char* VariadicName; // NULL, if none has been registered
    char** VariadicValues; 
    int VariadicValuesCount; 

    bool hasParsed;
    char* lastErr;
    char* executableName;
} g_ArgsData = {};

ArgParseError error(ArgParseError err, const char* fmt, ...) {
    if (g_ArgsData.lastErr) free(g_ArgsData.lastErr);

    va_list args;
    va_start(args, fmt);
    int needed = vsnprintf(NULL, 0, fmt, args);
    g_ArgsData.lastErr = (char*) malloc(needed+1);
    vsnprintf(g_ArgsData.lastErr, needed+1, fmt, args);
    va_end(args);

    return err;
}

// Return a string explaining the last error that occured. If the error
// came from ArgParse_Parse() then it is appropriate to display this to the user.
char* ArgParse_GetErrorStr() {
    return g_ArgsData.lastErr;
}

Flag* findFlagByAbbreviation(char abbreviation) {
    if (!abbreviation) return NULL;

    for (int i=0; i < g_ArgsData.flagCount; i++) {
        Flag* flag = &g_ArgsData.flags[i];
        if (flag->abbreviation && abbreviation == flag->abbreviation) {
            return flag;
        }
    }

    return NULL;
}

Flag* findFlagByName(const char* flagName) {
    for (int i=0; i < g_ArgsData.flagCount; i++) {
        Flag* flag = &g_ArgsData.flags[i];
        if (strcmp(flagName, flag->name) == 0) {
            return flag;
        }
    }

    return NULL;
}



// Register a new flag.
// Arguments:
// name: full name of flag, prefixed by double dash "--" in arguments.
// abbreviation: letter to represent shortened form, prefixed by single  dash "-". Can be NULL
// description: description to be shown in help message. Can be NULL.
ArgParseError ArgParse_RegisterFlag(const char* name, const char abbreviation, const char* description) {
    if (findFlagByName(name)) return error(ARGPARSE_ERROR_DUPLICATE, "A flag with the name '%s'is already in use.", name);
    if (abbreviation && findFlagByAbbreviation(abbreviation)) return error(ARGPARSE_ERROR_DUPLICATE, "Abbreviation '%s' is already in use.", abbreviation);

    // if (strlen(abbreviation) > 1) return error(ARGPARSE_ERROR_ABBREVIATION_LENGTH, "Abbreviation '%s' is longer than one character.");


    if (g_ArgsData.flagCount+1 > g_ArgsData.flagsCapacity) {
        int newCapacity = g_ArgsData.flagsCapacity == 0 ? 4 : g_ArgsData.flagsCapacity * 2;
        Flag* newFlags = (Flag*) realloc(g_ArgsData.flags, sizeof(Flag) * newCapacity);
        if (!newFlags) return error(ARGPARSE_ERROR_MEMORY, "Insufficient memory.");
        g_ArgsData.flagsCapacity = newCapacity;
        g_ArgsData.flags = newFlags;
    }

    Flag* flag = &g_ArgsData.flags[g_ArgsData.flagCount];
    flag->name = name;

    if (abbreviation) {
        flag->abbreviation = abbreviation;
    } else {
        flag->abbreviation = NULL;
    }

    if (description) {
        flag->description = description;
    } else {
        flag->description = NULL;
    }

    flag->hasParameter = false;

    flag->isSet = false;

    g_ArgsData.flagCount++;
    return ARGPARSE_ERROR_SUCCESS;
};

// Register parameter for flag. ex. "5" in "--volume 5"
// Arguments:
// flagName: full name of flag to be assosciated with.
// name: name of parameter, shown in help message.
// dataType: data type. one of ARG_TYPE_INT, ARG_TYPE_FLOAT or ARG_TYPE_STRING.
ArgParseError ArgParse_RegisterFlagParameter(const char* flagName, const char* name,
                                    ArgType dataType) {
    Flag* flag = findFlagByName(flagName);
    if (!flag) return error(ARGPARSE_ERROR_UNKNOWN_FLAG, "Cannot set flag parameter to %s as the flag does not exist.", flagName);

    if (flag->hasParameter) return error(ARGPARSE_ERROR_PARAMETER_ALREADY_REGISTERED, "A parameter '%s' is already registered for the flag '%s'.", flag->parameter.name, flagName);

    flag->parameter.name = name;

    flag->parameter.isSet = false;
    flag->parameter.dataType = dataType;

    return ARGPARSE_ERROR_SUCCESS;

}

// Register variadic parameter. ex. "source.mp4" in "clipcutter --volume 5 source.mp4". Only one variadic parameter can be registered per program, but multiple parameters can be supplied by user, like: "clipcutter source.mp4 source2.wav"
// Arguments:
// name: name of variadic parameter, shown in help message.
ArgParseError ArgParse_RegisterVariadicParameter(const char* name) {
    if (g_ArgsData.VariadicName) {
        return error(ARGPARSE_ERROR_DUPLICATE, "Only one variadic may be defined.");
    }

    g_ArgsData.VariadicName = name;

    return ARGPARSE_ERROR_SUCCESS;
};

// for arguments that don't take option parameters.
bool ArgParse_IsFlagSet(const char* flagName) {
    Flag* flag =  findFlagByName(flagName);
    if (!flag) return false;
    return flag->isSet;
}

// get pointer to value of the argument, NULL if not set
// If someone calls GetValueInt on a flag registered as ARG_TYPE_STRING, that
// should return NULL (or assert, in a debug build) should all return NULL if
// ArgParse_Parse hasn't been done yet.
int* ArgParse_GetValueInt(const char* argName) {
    Flag* flag =  findFlagByName(argName);
    if (!flag) return NULL;

    if (!flag->hasParameter || !flag->parameter.isSet || flag->parameter.dataType != ARG_TYPE_INT) return NULL;

    return &flag->parameter.value.intVal;
}

char* ArgParse_GetValueStr(const char* argName) {
    Flag* flag =  findFlagByName(argName);
    if (!flag) return NULL;

    if (!flag->hasParameter || !flag->parameter.isSet || flag->parameter.dataType != ARG_TYPE_STRING) return NULL;

    return flag->parameter.value.strVal;
}

float* ArgParse_GetValueFloat(const char* argName) {
    Flag* flag =  findFlagByName(argName);
    if (!flag) return NULL;

    if (!flag->hasParameter || !flag->parameter.isSet || flag->parameter.dataType != ARG_TYPE_FLOAT) return NULL;

    return &flag->parameter.value.floatVal;
}

// register the last option, that can take any number of parameters.
// returns an array of char*
char** ArgParse_GetVariadicValues(int* outCount) {
    if (!g_ArgsData.hasParsed) return NULL;

    if (!g_ArgsData.VariadicName) {
        return NULL;
    }

    *outCount = g_ArgsData.VariadicValuesCount;
    return g_ArgsData.VariadicValues;
};

// programTitle is recommended to be the program name and version
void ArgParse_ShowHelpMessage() {
    char* helpStr = ArgParse_GetHelpMessage();
    printf("%s", helpStr);
    free(helpStr);

}


// Returned char* must be freed.
char* ArgParse_GetHelpMessage() {
    SB sb;
    SB_init(&sb, 512);

    const char* prog = g_ArgsData.executableName ? g_ArgsData.executableName : "program";
    SB_appendf(&sb, "Usage: %s [OPTIONS]", prog);
    if (g_ArgsData.VariadicName) SB_appendf(&sb, " [%s...]", g_ArgsData.VariadicName);
    SB_appendf(&sb, "\n\nOptions:\n");

    // first pass: figure out column width for alignment
    int maxLeftWidth = 0;
    for (int i = 0; i < g_ArgsData.flagCount; i++) {
        Flag* f = &g_ArgsData.flags[i];
        int w = (int)strlen(f->name) + 6; // "--name" + padding
        if (f->abbreviation)
            w += 4; // "-x, "
        if (f->hasParameter)
            w += (int)strlen(f->parameter.name) + 3; // " <NAME>"
        if (w > maxLeftWidth)
            maxLeftWidth = w;
    }

    for (int i = 0; i < g_ArgsData.flagCount; i++) {
        Flag* f = &g_ArgsData.flags[i];

        SB left;
        SB_init(&left, 256);
        SB_appendf(&left, "  ");
        if (f->abbreviation)
            SB_appendf(&left, "-%c, ", f->abbreviation);
        SB_appendf(&left, "--%s", f->name);
        if (f->hasParameter)
            SB_appendf(&left, " <%s>", f->parameter.name);

        SB_appendf(&sb, "%-*s", maxLeftWidth + 2, left.buf);
        if (f->description)
            SB_appendf(&sb, "%s", f->description);
        SB_appendf(&sb, "\n");

        free(left.buf);
    }

    return sb.buf; // caller owns this, must free()
}

void setParameterValue(FlagParameter* parameter, char* argParameterVal) {
    switch (parameter->dataType) {
        case ARG_TYPE_INT:
            parameter->value.intVal = atoi(argParameterVal);
            break;
        case ARG_TYPE_FLOAT:
            parameter->value.floatVal = atof(argParameterVal);
            break;
        case ARG_TYPE_STRING:
        default:
            parameter->value.strVal = argParameterVal;
            break;
    }
}

ArgParseError parseFlag(Flag* flag, int argc, char** argv, int* i) {
    if (flag->hasParameter) {
        char* argParameterVal = NULL;
        bool missingParameterVal = false;
        if (*i+1 == argc) { // if last argument
            missingParameterVal = true;
        } else {

            argParameterVal = argv[*i+1];

            if (strncmp(argParameterVal, "-", 1) == 0) {
                missingParameterVal = true;
            }
        }

        if (missingParameterVal) {
            return error(ARGPARSE_ERROR_USER_MISSING_PARAMETER, "Missing parameter for flag '%s'. Please see usage.", flag->name);
        }

        setParameterValue(&flag->parameter, argParameterVal);
        flag->parameter.isSet = true;
        *i = *i + 1; // skip the parameter on next loop iteration
    }

    flag->isSet = true;

    return ARGPARSE_ERROR_SUCCESS;
}

const char* basenameOf(const char* path) {
    const char* slash = strrchr(path, '/');
    const char* backslash = strrchr(path, '\\');
    const char* last = slash;
    if (backslash && (!last || backslash > last)) last = backslash;
    return last ? last + 1 : path;
}

// Parse argv and argc so that the getter functions can be used.
// Make sure this is called after registering flags or else the getter functions will not work.
// If any error is returned it should be displayed to the user.
ArgParseError ArgParse_Parse(int argc, char** argv) {
    g_ArgsData.executableName = (char*) basenameOf(argv[0]);
    // i=1 to start at arguments rather than program path
    for (int i=1; i < argc; i++) {
        char* arg = argv[i];

        Flag* flag = NULL;
        if (strncmp(arg, "-", 1) == 0) {
            if (strncmp(arg, "--", 2) == 0) {
                flag = findFlagByName(arg+2);

                    if (!flag) {
                        return error(ARGPARSE_ERROR_USER_UNKNOWN_ARGUMENT, "Argument '%s' is not recognised. Please see usage.", argv[i]);
                    }

                ArgParseError ret = parseFlag(flag, argc, argv, &i);
                if (ret != ARGPARSE_ERROR_SUCCESS)
                    return ret;
            } else { // abbreviated arguments like "-a -b -c" or "-abc"
                for (size_t x=1; x < strlen(arg); x++) {
                    char abbreviation = *(arg+x);
                    flag = findFlagByAbbreviation(abbreviation);

                    if (!flag) {
                        return error(ARGPARSE_ERROR_USER_UNKNOWN_ARGUMENT, "Argument abbreviation '%c' in '%s' is not recognised. Please see usage.", abbreviation, argv[i]);
                    }

                    ArgParseError ret = parseFlag(flag, argc, argv, &i);
                    if (ret != ARGPARSE_ERROR_SUCCESS)
                        return ret;
                }
            }

        } else { // Variatic argument(s)
            if (!g_ArgsData.VariadicName) {
                return error(ARGPARSE_ERROR_USER_UNKNOWN_VARIADIC, "Cannot parse argument '%s'. This program does not accept variadic arguments.", arg);
            }

            if (!g_ArgsData.VariadicValues) {
                g_ArgsData.VariadicValues = (char**) malloc(sizeof(char*) * (argc-1)); // it can't be more than argc-1
                if (!g_ArgsData.VariadicValues) return error(ARGPARSE_ERROR_MEMORY, "Insufficient memory.");
            }

            g_ArgsData.VariadicValues[g_ArgsData.VariadicValuesCount] = arg;
            g_ArgsData.VariadicValuesCount++;

        }

    }

    g_ArgsData.hasParsed = true;
    return ARGPARSE_ERROR_SUCCESS;
}


void ArgParse_Free() {
    free(g_ArgsData.flags);

    free(g_ArgsData.VariadicValues);

    free(g_ArgsData.lastErr);
}
