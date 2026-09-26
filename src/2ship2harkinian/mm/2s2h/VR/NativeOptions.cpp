#ifdef MMVR_ENABLE
#include "NativeOptions.h"
#include "NamedTab.h"
#include "ui.h"
#include "presentation.h"
#include "2s2h/BenGui/BenMenu.h"
#include "2s2h/Enhancements/Audio/AudioEditor.h"
#include "2s2h/Rando/VRMenu.h"
#include "fast/Fast3dGui.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <limits>
#include <fstream>
#include <stdexcept>

namespace mmvrgame {
namespace {
constexpr const char* Categories[] = { "Audio", "Gameplay", "Cheats", "Difficulty", "Randomizer" };
struct Panel {
    ImGuiContext* context = nullptr;
    ImFontAtlas* fonts = nullptr;
    uint64_t session = std::numeric_limits<uint64_t>::max(), frame = 0;
    int category = -1;
    bool pointerMode = false, previousConfirm = false, previousBack = false, previousCollapse = false;
    bool upperCase = false, keyboard = false, inputReady = false;
    int keyboardRow = 0, keyboardColumn = 0, navX = 0, navY = 0;
    float keyboardRepeat = 0;
    ImGuiKey queuedKey = ImGuiKey_None, heldKey = ImGuiKey_None;
    ImVec2 pointer{512, 300};
} panel;

// Only this context owns native-menu navigation, popups and text editing. The
// desktop ImGui frame and the existing VR draw-list panels retain their state.
struct ContextScope {
    ImGuiContext* previous = ImGui::GetCurrentContext();
    ImFontAtlas* atlas = previous->IO.Fonts;
    bool locked = atlas->Locked;
    ~ContextScope() { ImGui::SetCurrentContext(previous); atlas->Locked = locked; }
};
void ResetPanel(ImGuiContext* parent, uint64_t session) {
    if (panel.context) ImGui::DestroyContext(panel.context);
    panel = {};
    panel.session = session;
    panel.fonts = parent->IO.Fonts;
    panel.context = ImGui::CreateContext(panel.fonts);
    // CreateContext restores the existing context when one was already active.
    ImGui::SetCurrentContext(panel.context);
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags = ImGuiConfigFlags_NavEnableGamepad | ImGuiConfigFlags_NavEnableKeyboard;
    io.BackendFlags |= ImGuiBackendFlags_HasGamepad | (parent->IO.BackendFlags & ImGuiBackendFlags_RendererHasVtxOffset);
    io.FontDefault = parent->IO.FontDefault;
    io.FontGlobalScale = 1.35f;
    ImGui::GetStyle() = parent->Style;
    ImGui::GetStyle().WindowRounding = 5;
    ImGui::GetStyle().FramePadding = {10, 7};
    ImGui::GetStyle().ItemSpacing = {10, 10};
    Rando::ResetVrRandomizerMenu();
}
bool KeyboardHit(ImVec2 p) {
    return panel.keyboard && p.x >= 64 && p.x < 960 && p.y >= 779 && p.y < 1015;
}
bool WritableTextActive() {
    auto* context = ImGui::GetCurrentContext();
    auto* state = ImGui::GetInputTextState(context->ActiveId);
    return context->ActiveId != 0 && state && !(state->Flags & ImGuiInputTextFlags_ReadOnly);
}
void SyncKeyboard(const mmvr::NativeMenuInput& input) {
    const bool active = WritableTextActive();
    if (active && !panel.keyboard) {
        panel.keyboardRow = panel.keyboardColumn = panel.navX = panel.navY = 0;
        panel.keyboardRepeat = 0;
        panel.previousConfirm = input.confirm; // Opening A must not type a key.
        panel.pointer = {102, 800};
    }
    panel.keyboard = active;
}
int KeyboardColumns(int row) { return row == 4 ? 4 : 11; }
void NavigateKeyboard(const mmvr::NativeMenuInput& input, float dt) {
    const int x = input.navigateX > .55f ? 1 : input.navigateX < -.55f ? -1 : 0;
    const int y = input.navigateY > .55f ? -1 : input.navigateY < -.55f ? 1 : 0;
    panel.keyboardRepeat -= dt;
    if ((x || y) && (x != panel.navX || y != panel.navY || panel.keyboardRepeat <= 0)) {
        panel.keyboardRow = std::clamp(panel.keyboardRow + y, 0, 4);
        panel.keyboardColumn = std::clamp(panel.keyboardColumn + x, 0, KeyboardColumns(panel.keyboardRow)-1);
        panel.keyboardRepeat = x != panel.navX || y != panel.navY ? .35f : .10f;
    }
    panel.navX=x; panel.navY=y;
}
void FeedInput(const mmvr::NativeMenuInput& input) {
    auto& io = ImGui::GetIO();
    const float dt = std::clamp(input.delta, .001f, .1f);
    io.DeltaTime = dt;
    const float pointerMagnitude = std::hypot(input.pointerX, input.pointerY);
    const float navMagnitude = std::hypot(input.navigateX, input.navigateY);
    if (navMagnitude > .45f) panel.pointerMode = false;
    if (pointerMagnitude > .20f) {
        panel.pointerMode = true;
        panel.pointer.x = std::clamp(panel.pointer.x + input.pointerX * 750.f * dt, 42.f, 982.f);
        panel.pointer.y = std::clamp(panel.pointer.y - input.pointerY * 750.f * dt, 184.f, panel.keyboard ? 1026.f : 674.f);
    }
    if (panel.keyboard && !panel.pointerMode) NavigateKeyboard(input, dt);
    io.AddMousePosEvent(panel.pointerMode ? panel.pointer.x : -FLT_MAX,
                        panel.pointerMode ? panel.pointer.y : -FLT_MAX);
    // The drawn keyboard never takes ActiveId from the text field it edits.
    io.AddMouseButtonEvent(0, panel.pointerMode && !KeyboardHit(panel.pointer) && input.confirm);
    io.AddKeyEvent(ImGuiKey_GamepadFaceDown, !panel.pointerMode && !panel.keyboard && input.confirm);
    // FaceUp is ImGui's text-input activation; A must also enter InputText.
    io.AddKeyEvent(ImGuiKey_GamepadFaceUp, !panel.pointerMode && !panel.keyboard && input.confirm);
    io.AddKeyEvent(ImGuiKey_GamepadFaceRight, !panel.keyboard && input.back);
    if (panel.keyboard && input.back && !panel.previousBack) panel.queuedKey=ImGuiKey_Enter;
    io.AddKeyEvent(ImGuiKey_GamepadDpadUp, !panel.pointerMode && !panel.keyboard && input.navigateY > .55f);
    io.AddKeyEvent(ImGuiKey_GamepadDpadDown, !panel.pointerMode && !panel.keyboard && input.navigateY < -.55f);
    io.AddKeyEvent(ImGuiKey_GamepadDpadLeft, !panel.pointerMode && !panel.keyboard && input.navigateX < -.55f);
    io.AddKeyEvent(ImGuiKey_GamepadDpadRight, !panel.pointerMode && !panel.keyboard && input.navigateX > .55f);
    // Deliver a complete key-down frame, then release on the following frame.
    // Otherwise a virtual tap can arrive as down+up in the same input batch.
    if (panel.heldKey != ImGuiKey_None) io.AddKeyEvent(panel.heldKey, false);
    panel.heldKey = panel.queuedKey;
    panel.queuedKey = ImGuiKey_None;
    // Multiline fields use their own Ctrl+Enter convention to finish editing.
    auto* textState=ImGui::GetInputTextState(ImGui::GetCurrentContext()->ActiveId);
    const bool ctrlEnter=panel.heldKey==ImGuiKey_Enter && textState &&
        (textState->Flags & ImGuiInputTextFlags_Multiline) && !(textState->Flags & ImGuiInputTextFlags_CtrlEnterForNewLine);
    io.AddKeyEvent(ImGuiMod_Ctrl, ctrlEnter);
    if (panel.heldKey != ImGuiKey_None) io.AddKeyEvent(panel.heldKey, true);
}
void DrawKeyboard(const mmvr::NativeMenuInput& input) {
    if (!panel.keyboard) return;
    auto* draw = ImGui::GetForegroundDrawList();
    draw->AddRectFilled({56, 772}, {968, 1022}, IM_COL32(9, 22, 36, 255), 6);
    auto key = [&](const char* label, ImVec2 pos, ImVec2 size, int row, int column) {
        const bool hover = panel.pointerMode ?
            panel.pointer.x >= pos.x && panel.pointer.x < pos.x + size.x &&
            panel.pointer.y >= pos.y && panel.pointer.y < pos.y + size.y :
            panel.keyboardRow == row && panel.keyboardColumn == column;
        draw->AddRectFilled(pos, {pos.x + size.x, pos.y + size.y},
                            hover ? IM_COL32(52, 123, 128, 255) : IM_COL32(29, 57, 72, 255), 4);
        const auto text = ImGui::CalcTextSize(label);
        draw->AddText({pos.x + (size.x-text.x)*.5f, pos.y+(size.y-text.y)*.5f}, IM_COL32_WHITE, label);
        return hover && input.confirm && !panel.previousConfirm;
    };
    const char* rows[] = {"1234567890-", "qwertyuiop_", "asdfghjkl./", "zxcvbnm,:@?"};
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; rows[y][x]; ++x) {
            char text[] = {rows[y][x], 0};
            if (panel.upperCase && text[0] >= 'a' && text[0] <= 'z') text[0] -= 'a' - 'A';
            if (key(text, {64.f + x*81.f, 779.f+y*47.f}, {76, 42}, y, x)) ImGui::GetIO().AddInputCharacter(text[0]);
        }
    }
    if (key(panel.upperCase ? "lowercase" : "UPPERCASE", {64, 967}, {180,42}, 4, 0)) panel.upperCase = !panel.upperCase;
    if (key("Space", {250,967}, {280,42}, 4, 1)) ImGui::GetIO().AddInputCharacter(' ');
    if (key("Backspace", {536,967}, {220,42}, 4, 2)) panel.queuedKey=ImGuiKey_Backspace;
    if (key("Enter", {762,967}, {192,42}, 4, 3)) panel.queuedKey=ImGuiKey_Enter;
}
void Contents(Fast::Fast3dGui& gui) {
    auto native = std::dynamic_pointer_cast<BenGui::BenMenu>(gui.GetMenu());
    if (!native) { ImGui::TextWrapped("2Ship options are still initializing."); return; }
    if (panel.category < 0) {
        ImGui::TextWrapped("Choose a group. These are the native 2Ship settings.");
        for (int i=0;i<5;++i) {
            if (ImGui::Button(Categories[i], {-1,52})) { panel.category=i; ImGui::SetScrollY(0); }
        }
        ImGui::TextWrapped("Use the left stick to navigate. Use the right stick as a pointer for lists, dragging and the keyboard.");
        return;
    }
    if (ImGui::Button("Back to 2Ship")) { panel.category=-1; return; }
    ImGui::SameLine(); ImGui::TextUnformatted(Categories[panel.category]);
    ImGui::Separator();
    switch (panel.category) {
        case 0:
            if (ImGui::BeginTabBar("Audio pages")) {
                if (BeginNamedTab("Volumes")) { native->DrawVrSection("Settings","Audio"); ImGui::EndTabItem(); }
                if (BeginNamedTab("Music and sounds")) { DrawVrAudioEditor(); ImGui::EndTabItem(); }
                ImGui::EndTabBar();
            }
            break;
        case 1:
            ImGui::TextWrapped("Native button shortcuts use their original buttons. VR physical controls and the item wheel remain available.");
            native->DrawVrSection("Enhancements","Gameplay"); break;
        case 2: native->DrawVrSection("Enhancements","Cheats"); break;
        case 3: native->DrawVrSection("Enhancements","Difficulty Options"); break;
        case 4: Rando::DrawVrRandomizerMenu(); break;
    }
}
} // namespace
static void BuildNativeOptions(const mmvr::UiDrawFrame& frame, Fast::Fast3dGui& gui, bool submit) {
    ContextScope scope;
    auto& menu = mmvr::GetMenu();
    if (!panel.context || panel.fonts != scope.atlas || panel.session != menu.nativeSession)
        ResetPanel(scope.previous, menu.nativeSession);
    ImGui::SetCurrentContext(panel.context);
    auto input = menu.nativeInput;
    // Entering a tab must never reuse the button press that selected it.
    if (!panel.inputReady) {
        panel.inputReady = !input.confirm && !input.back && !input.collapse;
        input.confirm = input.back = input.collapse = false;
    }
    // A menu image may be submitted more than once; consume XR input only once.
    if (panel.frame != input.frame || panel.frame == 0) {
        auto& io = ImGui::GetIO();
        io.DisplaySize = {float(frame.width), float(frame.height)};
        SyncKeyboard(input);
        const bool back = input.back && !panel.previousBack;
        const bool collapse = input.collapse && !panel.previousCollapse;
        if ((back && !panel.keyboard && panel.context->OpenPopupStack.empty() && !panel.context->ActiveId) || collapse) {
            ImGui::ClearActiveID();
            if (!panel.context->OpenPopupStack.empty()) ImGui::ClosePopupToLevel(0, true);
            panel.keyboard = false;
            if (panel.category >= 0) { panel.category=-1; Rando::ResetVrRandomizerMenu(); }
            else if (back) menu.nativeCloseRequested=true;
        }
        FeedInput(input);
        ImGui::NewFrame();
        std::array<ImTextureID, mmvr::MaxItemSlots> noIcons{};
        mmvr::presentation::Draw(*ImGui::GetBackgroundDrawList(), frame, nullptr, noIcons);
        ImGui::SetNextWindowPos({48,190});
        // Keep the editor viewport fixed: shrinking it can clip/deactivate the
        // very text field that opened the keyboard. Keys live below the menu.
        ImGui::SetNextWindowSize({928, 480});
        ImGui::SetNextWindowBgAlpha(0.f);
        constexpr auto flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground;
        ImGui::Begin("##VR2Ship", nullptr, flags);
        if (panel.frame==0) ImGui::SetWindowFocus();
        Contents(gui);
        ImGui::End();
        SyncKeyboard(input);
        DrawKeyboard(input);
        if (panel.pointerMode) {
            auto* draw=ImGui::GetForegroundDrawList();
            draw->AddCircleFilled(panel.pointer, 5.f, IM_COL32(248,211,119,255));
            draw->AddCircle(panel.pointer, 6.f, IM_COL32(0,0,0,255));
        }
        ImGui::Render();
        panel.frame = input.frame;
        panel.previousConfirm = input.confirm;
        panel.previousBack = input.back;
        panel.previousCollapse = input.collapse;
    }
    if (submit) {
        auto* data = ImGui::GetDrawData();
        // GPU backends belong to the host context. The private context owns only
        // widget state; rendering its draw data must use the initialized backend.
        ImGui::SetCurrentContext(scope.previous);
        gui.RenderVrDrawData(data);
    }
}
void DrawNativeOptions(const mmvr::UiDrawFrame& frame, Fast::Fast3dGui& gui) {
    BuildNativeOptions(frame, gui, true);
}
#include "NativeTextChecks.inl"
#include "NativeOptionsChecks.inl"
} // namespace mmvrgame
#endif
