#include "pch.h"
#include <cstdarg>
#include <cstdio>

#include "log.h"

char* alloc_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    //─────────────────── get length ───────────────────
    va_list copy;
    va_copy(copy, args);
    int len = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);

    if (len < 0) {
        va_end(args);
        return NULL;
    }

    char* errorBuffer = (char*) _alloca((size_t) len + 1);
    if (!errorBuffer) {
        va_end(args);
        return NULL;
    }

    vsnprintf(errorBuffer, (size_t) len + 1, fmt, args);
    va_end(args);

    log_error(errorBuffer);
    return errorBuffer;
}

void popup_error(const char* title, char* message) {
    if (ImGui::BeginPopupModal(title, NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse)) {
        ImGui::Text("%s", message);
        ImGui::Separator();

        if (ImGui::Button("OK", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        // ImGui::SameLine();
        // if (ImGui::Button("Cancel", ImVec2(120, 0))) {
        //     ImGui::CloseCurrentPopup();
        // }
        ImGui::EndPopup();
    }
}
