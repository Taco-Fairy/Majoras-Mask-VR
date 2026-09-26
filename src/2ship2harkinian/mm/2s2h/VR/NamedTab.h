#pragma once
#include "imgui.h"

namespace mmvrgame {
// Show the full label before the selected tab submits any child widgets.
inline bool BeginNamedTab(const char* label) {
    const bool open = ImGui::BeginTabItem(label, nullptr, ImGuiTabItemFlags_NoTooltip);
    if (ImGui::IsItemHovered() || ImGui::IsItemFocused()) {
        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0, 0, 0, 1));
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 28.f);
        ImGui::TextUnformatted(label);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
        ImGui::PopStyleColor();
    }
    return open;
}
}
