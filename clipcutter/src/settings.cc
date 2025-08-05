#include "pch.h"
#include "app.h"

void registerOpenWith() {
    LONG result;
    HKEY hKey;

    // register the application command path
    const char* appCommandKey = "Software\\Classes\\Applications\\clipcutter.exe\\shell\\open\\command";
    const char* command = "\"Z:\\Programming\\c\\clipcutter_sdl3\\build\\clipcutter\\clipcutter.exe\" \"%1\"";

    result = RegCreateKeyExA(
        HKEY_CURRENT_USER,
        appCommandKey,
        0,
        NULL,
        REG_OPTION_NON_VOLATILE,
        KEY_WRITE,
        NULL,
        &hKey,
        NULL
    );

    if (result != ERROR_SUCCESS) {
        log_error("Failed to create application command key. Error: %ld\n", result);
    }

    result = RegSetValueExA(
        hKey,
        NULL,
        0,
        REG_SZ,
        (const BYTE*)command,
        (DWORD)(strlen(command) + 1)
    );

    RegCloseKey(hKey);

    if (result != ERROR_SUCCESS) {
        log_error("Failed to set application command. Error: %ld\n", result);
    }

    log_info("Application command registered.\n");

    char openWithListKey[256];
    char progidsKey[256];
    for (size_t i=0; i < NUM_SUPPORTED_FORMATS; i++) {
        const char* file_format = SUPPORTED_FILE_FORMATS[i];

        log_info("Creating OpenWithList and OpenWithProgids for format %s", file_format)

        // add executable to OpenWithList
        {
            // const char* openWithListKey = "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.mp4\\OpenWithList";

            sprintf(openWithListKey, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\%s\\OpenWithList", (char*) file_format);

            result = RegCreateKeyExA(
                HKEY_CURRENT_USER,
                openWithListKey,
                0,
                NULL,
                REG_OPTION_NON_VOLATILE,
                KEY_WRITE,
                NULL,
                &hKey,
                NULL
            );

            if (result == ERROR_SUCCESS) {
                const char* valueName = "a"; // windows uses "a", "b", etc.
                const char* exeName = "clipcutter.exe";

                result = RegSetValueExA(
                    hKey,
                    valueName,
                    0,
                    REG_SZ,
                    (const BYTE*)exeName,
                    (DWORD)(strlen(exeName) + 1)
                );

                RegCloseKey(hKey);

                if (result == ERROR_SUCCESS) {
                    log_info("OpenWithList updated.\n");
                } else {
                    log_error("Failed to update OpenWithList. Error: %ld\n", result);
                }
            } else {
                log_info("Failed to open OpenWithList key. Error: %ld\n", result);
            }

        }


        // add to OpenWithProgids
        {
            // const char* progidsKey = "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.mp4\\OpenWithProgids";
            sprintf(progidsKey, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\%s\\OpenWithProgids", (char*) file_format);

            result = RegCreateKeyExA(
                HKEY_CURRENT_USER,
                progidsKey,
                0,
                NULL,
                REG_OPTION_NON_VOLATILE,
                KEY_WRITE,
                NULL,
                &hKey,
                NULL
            );

            if (result == ERROR_SUCCESS) {
                const char* progidName = "Applications\\clipcutter.exe";

                result = RegSetValueExA(
                    hKey,
                    progidName,
                    0,
                    REG_NONE,
                    NULL,
                    0
                );

                RegCloseKey(hKey);

                if (result == ERROR_SUCCESS) {
                    log_info("OpenWithProgids updated.\n");
                } else {
                    log_error("Failed to update OpenWithProgids. Error: %ld\n", result);
                }
            } else {
                log_error("Failed to open OpenWithProgids key. Error: %ld\n", result);
            }
        }


    }
}

void deleteKey(HKEY root, const char* path) {
    LONG res = RegDeleteKeyA(root, path);
    if (res == ERROR_SUCCESS) {
        log_info("Deleted key: %s\n", path);
    } else if (res == ERROR_FILE_NOT_FOUND) {
        log_info("Key not found: %s\n", path);
    } else {
        log_error("Failed to delete key %s (Error %ld)\n", path, res);
    }
};
void deleteValue(HKEY root, const char* path, const char* name) {
    HKEY hKey;
    if (RegOpenKeyExA(root, path, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        LONG res = RegDeleteValueA(hKey, name);
        if (res == ERROR_SUCCESS) {
            log_info("Deleted value '%s' from %s\n", name, path);
        } else if (res == ERROR_FILE_NOT_FOUND) {
            log_info("Value '%s' not found in %s\n", name, path);
        } else {
            log_error("Failed to delete value '%s' (Error %ld)\n", name, res);
        }
        RegCloseKey(hKey);
    } else {
        log_error("Cannot open key: %s\n", path);
    }
};

void unregisterOpenWith() {
    const char* commandKey     = "Software\\Classes\\Applications\\clipcutter.exe\\shell\\open\\command";
    const char* openKey        = "Software\\Classes\\Applications\\clipcutter.exe\\shell\\open";
    const char* shellKey       = "Software\\Classes\\Applications\\clipcutter.exe\\shell";
    const char* appKey         = "Software\\Classes\\Applications\\clipcutter.exe";

    deleteKey(HKEY_CURRENT_USER, commandKey);
    deleteKey(HKEY_CURRENT_USER, openKey);
    deleteKey(HKEY_CURRENT_USER, shellKey);
    deleteKey(HKEY_CURRENT_USER, appKey);

    char openWithListKey[256];
    char progidsKey[256];
    for (size_t i=0; i < NUM_SUPPORTED_FORMATS; i++) {
        const char* file_format = SUPPORTED_FILE_FORMATS[i];

        sprintf(openWithListKey, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\%s\\OpenWithList", (char*) file_format);
        sprintf(progidsKey, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\%s\\OpenWithProgids", (char*) file_format);

        deleteValue(HKEY_CURRENT_USER, openWithListKey, "a");

        deleteValue(HKEY_CURRENT_USER, progidsKey, "Applications\\clipcutter.exe");
    }
}

void Settings_DrawSettings(App* app) {
    #if CC_PLATFORM_WINDOWS
    ImGui::Text("Register ClipCutter in \"open with\" context menu for video files?");

    if (ImGui::Button("Register")) {
        registerOpenWith();
    }

    if (ImGui::Button("Unregister")) {
        unregisterOpenWith();
    }
    #endif

    ImGui::InputText("Default export path", app->exportPath, 1024);
    ImGui::InputFloat("Snapping precision", &app->timeline.snappingPrecision);


    if (ImGui::Button("Close")) {
        ImGui::CloseCurrentPopup();
    }
}
