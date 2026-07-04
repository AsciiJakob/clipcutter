#ifndef ARGPARSE_H
#define ARGPARSE_H

typedef enum { ARG_TYPE_STRING, ARG_TYPE_INT, ARG_TYPE_FLOAT } ArgType;
typedef enum {
    ARGPARSE_ERROR_SUCCESS,
    ARGPARSE_ERROR_DUPLICATE,
    ARGPARSE_ERROR_MEMORY,
    ARGPARSE_ERROR_ABBREVIATION_LENGTH,
    ARGPARSE_ERROR_UNKNOWN_FLAG,
    ARGPARSE_ERROR_PARAMETER_ALREADY_REGISTERED,
    ARGPARSE_ERROR_USER_UNKNOWN_ARGUMENT,
    ARGPARSE_ERROR_USER_UNKNOWN_VARIADIC,
    ARGPARSE_ERROR_USER_MISSING_PARAMETER
} ArgParseError;

char* ArgParse_GetErrorStr();

ArgParseError ArgParse_Parse(int argc, char** argv);

ArgParseError ArgParse_RegisterFlag(const char* name, const char abbreviation);

ArgParseError ArgParse_RegisterFlagParameter(const char* flagName, const char* name,
                                    ArgType dataType);
ArgParseError ArgParse_RegisterVariadicParameter(const char* name);

bool ArgParse_IsFlagSet(const char* flagName);
int* ArgParse_GetValueInt(const char* argName);
char* ArgParse_GetValueStr(const char* argName);
float* ArgParse_GetValueFloat(const char* argName);
char** ArgParse_GetVariadicValues(int* outCount);
void ArgParse_ShowHelpMessage();

#endif
