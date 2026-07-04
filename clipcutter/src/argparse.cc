// Argument parser library by AsciiJakob 2026-07-01
//
// if one hyphen is followed by two or more letters it may mean two options are
// being specified, or it may mean the second and subsequent letters are a
// parameter (such as filename or date) for the first option
//
//
// Built-in usage help and man pages commonly employ a small syntax to describe
// the valid command form:
// angle brackets for required parameters: ping <hostname>
// square brackets for optional parameters: mkdir [-p] <dirname>
// ellipses for repeated items: cp <source1> [source2…] <dest>
// vertical bars for choice of items: netstat {-t|-u}


#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include "argparse.h"

typedef union {
    char* strVal;
    int intVal;
    float floatVal;
} ArgValue;

struct FlagParameter {
    char* name;
    ArgValue value;
    ArgType dataType;
    bool isSet;
} typedef FlagParameter;

struct Flag {
    char* name;
    char* abbreviation;
    FlagParameter* parameter; // NULL if flag takes no parameter.
    bool isSet;
} typedef Flag;


// stores all the registered flags
static struct {
    Flag* flags;
    int flagCount;
    int flagsCapacity; // for realloc growth

    char* VariadicName; // NULL, if none has been registered
    char** VariadicValues; 
    int VariadicValuesCount; 
    int VariadicValuesCapacity; 

    bool hasParsed;
    char* lastErr;
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

// returns a string explaining the last error that occured. If the error
// came from ArgParse_Parse() then it is appropriate to display this to the user.
char* ArgParse_GetErrorStr() {
    return g_ArgsData.lastErr;
}

Flag* findFlagByAbbreviation(const char* abbreviation) {
    if (!abbreviation) return NULL;

    for (int i=0; i < g_ArgsData.flagCount; i++) {
        Flag* flag = &g_ArgsData.flags[i];
        if (flag->abbreviation && strcmp(abbreviation, flag->abbreviation) == 0) {
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



// returns whether it was succesful or not
ArgParseError ArgParse_RegisterFlag(const char* name, const char* abbreviation) {
    if (findFlagByName(name)) return error(ARGPARSE_ERROR_DUPLICATE, "A flag with the name '%s'is already in use.", name);
    if (abbreviation && findFlagByAbbreviation(abbreviation)) return error(ARGPARSE_ERROR_DUPLICATE, "Abbreviation '%s' is already in use.", abbreviation);

    if (strlen(abbreviation) > 1) return error(ARGPARSE_ERROR_ABBREVIATION_LENGTH, "Abbreviation '%s' is longer than one character.");


    if (g_ArgsData.flagCount+1 > g_ArgsData.flagsCapacity) {
        int newCapacity = g_ArgsData.flagsCapacity == 0 ? 4 : g_ArgsData.flagsCapacity * 2;
        Flag* newFlags = (Flag*) realloc(g_ArgsData.flags, sizeof(Flag) * newCapacity);
        if (!newFlags) return error(ARGPARSE_ERROR_MEMORY, "Insufficient memory.");
        g_ArgsData.flagsCapacity = newCapacity;
        g_ArgsData.flags = newFlags;
    }

    Flag* flag = &g_ArgsData.flags[g_ArgsData.flagCount];
    flag->name = (char*) malloc(strlen(name)+1);
    if (!flag->name) return error(ARGPARSE_ERROR_MEMORY, "Insufficient memory.");
    strcpy(flag->name, name);

    if (abbreviation) {
        flag->abbreviation = (char*) malloc(strlen(abbreviation)+1);
        if (!flag->abbreviation) return error(ARGPARSE_ERROR_MEMORY, "Insufficient memory.");
        strcpy(flag->abbreviation, abbreviation);
    } else {
        flag->abbreviation = NULL;
    }

    flag->parameter = NULL;

    flag->isSet = false;

    g_ArgsData.flagCount++;
    return ARGPARSE_ERROR_SUCCESS;
};

ArgParseError ArgParse_RegisterFlagParameter(const char* flagName, const char* name,
                                    ArgType dataType) {
    Flag* flag = findFlagByName(flagName);
    if (!flag) return error(ARGPARSE_ERROR_UNKNOWN_FLAG, "Cannot set flag parameter to ");

    if (flag->parameter) return error(ARGPARSE_ERROR_PARAMETER_ALREADY_REGISTERED, "A parameter '%s' is already registered for the flag '%s'.");

    flag->parameter = (FlagParameter*) malloc(sizeof(FlagParameter));
    if (!flag->parameter) return error(ARGPARSE_ERROR_MEMORY, "Insufficient memory.");

    flag->parameter->name = (char*) malloc(sizeof(strlen(name)+1));
    if (!flag->parameter->name) {
        free(flag->parameter);
        flag->parameter = NULL;
        return error(ARGPARSE_ERROR_MEMORY, "Insufficient memory.");
    }
    strcpy(flag->parameter->name, name);

    flag->parameter->isSet = false;
    flag->parameter->dataType = dataType;

    return ARGPARSE_ERROR_SUCCESS;

}

ArgParseError ArgParse_RegisterVariadicParameter(const char* name) {
    if (g_ArgsData.VariadicName) {
        return error(ARGPARSE_ERROR_DUPLICATE, "Only one variadic may be defined.");
    }

    if (!(g_ArgsData.VariadicName = (char*) malloc(strlen(name)+1))) {
        return error(ARGPARSE_ERROR_MEMORY, "Insufficient memory.");
    }
    strcpy(g_ArgsData.VariadicName, name);

    return ARGPARSE_ERROR_SUCCESS;
};

// Getters:

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

    if (!flag->parameter || !flag->parameter->isSet || flag->parameter->dataType != ARG_TYPE_INT) return NULL;

    return &flag->parameter->value.intVal;
}

char* ArgParse_GetValueStr(const char* argName) {
    Flag* flag =  findFlagByName(argName);
    if (!flag) return NULL;

    if (!flag->parameter || !flag->parameter->isSet || flag->parameter->dataType != ARG_TYPE_STRING) return NULL;

    return flag->parameter->value.strVal;
}

float* ArgParse_GetValueFloat(const char* argName) {
    Flag* flag =  findFlagByName(argName);
    if (!flag) return NULL;

    if (!flag->parameter || !flag->parameter->isSet || flag->parameter->dataType != ARG_TYPE_FLOAT) return NULL;

    return &flag->parameter->value.floatVal;
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

void ArgParse_ShowHelpMessage() {
    // TODO: add help message
    // make sure it retunrs strings instead of printing
    // iterate through all registered commands and show them as well as their
    // options
    printf("(help message)");
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

// if any error is returned it should be displayed to the user.
ArgParseError ArgParse_Parse(int argc, char** argv) {
    // i=1 to start at arguments rather than program path
    for (int i=1; i < argc; i++) {
        char* arg = argv[i];

        Flag* flag = NULL;
        if (strncmp(arg, "-", 1) == 0) {
            if (strncmp(arg, "--", 2) == 0) {
                flag = findFlagByName(arg+2);

            } else {
                // TODO: I need to handle things like -abc = -a -b -c
                flag = findFlagByAbbreviation(arg+1);

                for (size_t i=1; i < strlen(arg); i++) {
                    

                }
                // if (strlen(arg+1))
            }

            if (!flag) {
                return error(ARGPARSE_ERROR_USER_UNKNOWN_ARGUMENT, "Argument '%s' is not recognised. Please see usage.", arg);
            }


            if (flag->parameter) {
                char* argParameterVal = NULL;
                bool missingParameterVal = false;
                if (i+1 == argc) { // if last argument
                    missingParameterVal = true;
                } else {

                    argParameterVal = argv[i+1];

                    if (strncmp(argParameterVal, "-", 1) == 0) {
                        missingParameterVal = true;
                    }
                }

                if (missingParameterVal) {
                    return error(ARGPARSE_ERROR_USER_MISSING_PARAMETER, "Missing parameter for flag '%s'. Please see usage.", flag->name);
                }

                setParameterValue(flag->parameter, argParameterVal);
                flag->parameter->isSet = true;
                i = i + 1; // skip the parameter on next loop iteration
            }

            flag->isSet = true;

        } else { // Variatic argument(s)
            if (!g_ArgsData.VariadicName) {
                return error(ARGPARSE_ERROR_USER_UNKNOWN_VARIADIC, "Cannot parse argument '%s'. This program does not accept variadic arguments.", arg);
            }

            if (g_ArgsData.VariadicValuesCount+1 > g_ArgsData.VariadicValuesCapacity) {
                int newCapacity = g_ArgsData.VariadicValuesCapacity == 0 ? 4 : g_ArgsData.VariadicValuesCapacity * 2;
                char** n = (char**) realloc(g_ArgsData.VariadicValues, sizeof(char*) * newCapacity);
                if (!n) return error(ARGPARSE_ERROR_MEMORY, "Program ran out of memory.");
                g_ArgsData.VariadicValues = n;
                g_ArgsData.VariadicValuesCapacity = newCapacity;
            }

            char* newStr = (char*) malloc(strlen(arg)+1);
            if (!newStr) error(ARGPARSE_ERROR_MEMORY, "Program ran out of memory.");
            strcpy(newStr, arg);
            g_ArgsData.VariadicValues[g_ArgsData.VariadicValuesCount] = newStr;

            g_ArgsData.VariadicValuesCount++;

        }

    }

    g_ArgsData.hasParsed = true;
    return ARGPARSE_ERROR_SUCCESS;
}


// void ArgParse_Free(void) {
//     for (int i = 0; i < g_ArgsData.flagCount; i++) {
//         free(g_ArgsData.flags[i].name);
//         free(g_ArgsData.flags[i]
//                  .abbreviation); // free(NULL) is safe, no need to check
//         if (g_ArgsData.flags[i].parameter) {
//             free(g_ArgsData.flags[i].parameter->name);
//             if (g_ArgsData.flags[i].parameter->dataType == ARG_TYPE_STRING) {
//                 free(g_ArgsData.flags[i].parameter->value.asString);
//             }
//             free(g_ArgsData.flags[i].parameter);
//         }
//         free(g_ArgsData.flags[i].name); // (only once — watch for this duplicate
//                                         // if you copy-paste)
//     }
//     free(g_ArgsData.flags);
//
//     for (int i = 0; i < g_ArgsData.variadicCount; i++) {
//         free(g_ArgsData.variadicValues[i]);
//     }
//     free(g_ArgsData.variadicValues);
//     free(g_ArgsData.variadicName);
//
//     g_ArgsData = (typeof(g_ArgsData)){0}; // reset to zeroed state
// }
