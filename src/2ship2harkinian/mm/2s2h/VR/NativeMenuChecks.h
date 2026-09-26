#pragma once
#include "2s2h/BenGui/UIWidgets.hpp"
#include <stdexcept>
#include <limits>

// Isolated ImGui context: never mutate the player's live menu or config.
extern "C" void MMVR_VerifyNativeWidgets() {
    const char* enabled = std::getenv("MMVR_NATIVE_TEST");
    if (!enabled || std::string(enabled) != "1") return;
    struct ContextGuard {
        ImGuiContext* previous = ImGui::GetCurrentContext();
        ImGuiContext* scratch = ImGui::CreateContext();
        ~ContextGuard() { ImGui::DestroyContext(scratch); ImGui::SetCurrentContext(previous); }
    } context;
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr; io.LogFilename = nullptr;
    io.DisplaySize = {1280, 960}; io.DeltaTime = 1.f / 90.f;
    unsigned char* pixels; int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    unsigned checks = 0;
    auto verify = [&](bool dirty, int actual, int expected) {
        if (dirty || actual != expected) throw std::runtime_error("Native combobox boundary regression");
        ++checks;
    };
    for (const auto& [key, expected] : UIWidgets::ColorValues) {
        const auto& actual = UIWidgets::GetColor(key);
        if (actual.x != expected.x || actual.y != expected.y || actual.z != expected.z || actual.w != expected.w)
            throw std::runtime_error("Native theme color regression");
        ++checks;
    }
    for (int invalid : {-1, 21, 99, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
        const auto& actual = UIWidgets::GetColor(static_cast<UIWidgets::Colors>(invalid));
        const auto& expected = UIWidgets::ColorValues.at(UIWidgets::Colors::LightBlue);
        if (actual.x != expected.x || actual.y != expected.y || actual.z != expected.z || actual.w != expected.w)
            throw std::runtime_error("Native invalid theme fallback regression");
        ++checks;
    }
    for (int frame = 0; frame < 2; ++frame) {
        ImGui::NewFrame();
        ImGui::SetNextWindowSize({1200, 900}); ImGui::Begin("Native widget boundary checks");
        UIWidgets::ComboboxOptions options;
        options.labelPosition = frame ? UIWidgets::LabelPosition::Far : UIWidgets::LabelPosition::Above;
        options.alignment = UIWidgets::ComponentAlignment::Left;
        options.flags = ImGuiComboFlags_WidthFitPreview;
        std::vector<const char*> chars, blankChars{"", ""};
        std::vector<std::string> strings, blankStrings{"", ""};
        std::unordered_map<int,const char*> emptyMap, blankMap{{0,""}};
        const char* blankArray[] = {"", ""};
        int value = 99;
        verify(UIWidgets::Combobox("empty chars", &value, chars, options), value, 99);
        verify(UIWidgets::Combobox("empty strings", &value, strings, options), value, 99);
        verify(UIWidgets::Combobox("empty map", &value, &emptyMap, options), value, 99);
        verify(UIWidgets::ComboboxWithSearch("empty search", &value, &emptyMap, options), value, 99);
        // Evaluate the widget before inspecting value: function-argument order is unspecified.
        auto changed = UIWidgets::Combobox("blank chars", &value, blankChars, options); verify(changed,value,0);
        value = -1; changed = UIWidgets::Combobox("blank strings", &value, blankStrings, options); verify(changed,value,0);
        value = 99; changed = UIWidgets::Combobox("blank array", &value, blankArray, options); verify(changed,value,0);
        value = 99; changed = UIWidgets::Combobox("blank map", &value, &blankMap, options); verify(changed,value,0);
        value = 99; changed = UIWidgets::ComboboxWithSearch("blank search", &value, &blankMap, options); verify(changed,value,0);
        value = 1; changed = UIWidgets::Combobox("valid vector", &value, blankChars, options); verify(changed,value,1);
        value = 1; changed = UIWidgets::Combobox("valid array", &value, blankArray, options); verify(changed,value,1);
        auto bad = static_cast<UIWidgets::Colors>(std::numeric_limits<int>::max());
        UIWidgets::PushStyleMenu(bad); UIWidgets::PopStyleMenu();
        UIWidgets::PushStyleMenuItem(bad); UIWidgets::PopStyleMenuItem();
        UIWidgets::PushStyleButton(bad); UIWidgets::PopStyleButton();
        UIWidgets::PushStyleInput(bad); UIWidgets::PopStyleInput();
        UIWidgets::PushStyleHeader(bad); UIWidgets::PopStyleHeader();
        UIWidgets::PushStyleCheckbox(bad); UIWidgets::PopStyleCheckbox();
        UIWidgets::PushStyleCombobox(bad); UIWidgets::PopStyleCombobox();
        UIWidgets::PushStyleTabs(bad); UIWidgets::PopStyleTabs();
        UIWidgets::PushStyleSlider(bad); UIWidgets::PopStyleSlider();
        checks += 9;
        ImGui::End(); ImGui::Render();
    }
    std::ofstream("native-widget-boundaries.json") << "{\"passed\":true,\"cases\":" << checks << "}";
}
