#include "HudEditor.h"
#ifdef MMVR_ENABLE
#include "runtime.h"
#include "ui.h"
#include <cstdlib>
#include <cstring>
#include "hud_layout.h"
extern "C" {
#include "global.h"
}
#endif
#include "macros.h"
#include "2s2h/ShipInit.hpp"

extern "C" int16_t OTRGetRectDimensionFromLeftEdge(float v);
extern "C" int16_t OTRGetRectDimensionFromRightEdge(float v);

HudEditorElementID hudEditorActiveElement = HUD_EDITOR_ELEMENT_NONE;
HudEditorElementMode hudEditorOverrideNextElemMode = HUD_EDITOR_ELEMENT_MODE_NONE;

// clang-format off
HudEditorElement hudEditorElements[HUD_EDITOR_ELEMENT_MAX] = {
    HUD_EDITOR_ELEMENT(HUD_EDITOR_ELEMENT_B, "B Button", "B", 167, 17, 100, 255, 120, 255, "Buttons.B"),
    HUD_EDITOR_ELEMENT(HUD_EDITOR_ELEMENT_C_LEFT, "C-Left Button", "CLeft", 227, 18, 255, 240, 0, 255, "Buttons.CLeft"),
    HUD_EDITOR_ELEMENT(HUD_EDITOR_ELEMENT_C_DOWN, "C-Down Button", "CDown", 249, 34, 255, 240, 0, 255, "Buttons.CDown"),
    HUD_EDITOR_ELEMENT(HUD_EDITOR_ELEMENT_C_RIGHT, "C-Right Button", "CRight", 271, 18, 255, 240, 0, 255, "Buttons.CRight"),
    HUD_EDITOR_ELEMENT(HUD_EDITOR_ELEMENT_A, "A Button", "A", 191, 18, 100, 200, 255, 255, "Buttons.A"),
    HUD_EDITOR_ELEMENT(HUD_EDITOR_ELEMENT_C_UP, "C-Up Button", "CUp", 254, 16, 255, 240, 0, 255, HUD_EDITOR_NO_COSMETIC),
    HUD_EDITOR_ELEMENT(HUD_EDITOR_ELEMENT_D_PAD, "D-Pad", "DPad", 271, 55, 255, 255, 255, 255, "Buttons.DPad"),
    HUD_EDITOR_ELEMENT(HUD_EDITOR_ELEMENT_MINIMAP, "Minimap", "Minimap", 295, 220, 0, 255, 255, 160, "HUD.Minimap"),
    HUD_EDITOR_ELEMENT(HUD_EDITOR_ELEMENT_START, "Start Button", "Start", 136, 17, 255, 130, 60, 255, "Buttons.Start"),
    HUD_EDITOR_ELEMENT(HUD_EDITOR_ELEMENT_CARROT, "Horse Carrots", "Carrots", 160, 64, 236, 92, 41, 255, HUD_EDITOR_NO_COSMETIC),
    HUD_EDITOR_ELEMENT(HUD_EDITOR_ELEMENT_CLOCK, "Three Day Clock", "Clock", 160, 206, 255, 255, 255, 255, HUD_EDITOR_NO_COSMETIC),
    HUD_EDITOR_ELEMENT(HUD_EDITOR_ELEMENT_HEARTS, "Hearts", "Hearts", 30, 26, 255, 70, 50, 255, "HUD.Hearts"),
    HUD_EDITOR_ELEMENT(HUD_EDITOR_ELEMENT_MAGIC_METER, "Magic", "Magic", 18, 34, 0, 200, 0, 255, "HUD.Magic"),
    HUD_EDITOR_ELEMENT(HUD_EDITOR_ELEMENT_TIMERS, "Timers", "Timers", 26, 46, 255, 255, 255, 255, HUD_EDITOR_NO_COSMETIC),
    HUD_EDITOR_ELEMENT(HUD_EDITOR_ELEMENT_TIMERS_MOON_CRASH, "Timer - Skull Kid", "SkullKidTimer", 115, 200, 255, 255, 255, 255, HUD_EDITOR_NO_COSMETIC),
    HUD_EDITOR_ELEMENT(HUD_EDITOR_ELEMENT_MINIGAME_COUNTER, "Minigames", "Minigames", 20, 67, 255, 255, 255, 255, HUD_EDITOR_NO_COSMETIC),
    HUD_EDITOR_ELEMENT(HUD_EDITOR_ELEMENT_RUPEE_COUNTER, "Rupees", "Rupees", 26, 206, 200, 255, 100, 255, "HUD.RupeeIcon"),
    HUD_EDITOR_ELEMENT(HUD_EDITOR_ELEMENT_KEY_COUNTER, "Keys", "Keys", 26, 190, 255, 255, 255, 255, "HUD.SmallKey"),
    HUD_EDITOR_ELEMENT(HUD_EDITOR_ELEMENT_SKULLTULA_COUNTER, "Skulltulas", "Skulltulas", 26, 190, 255, 255, 255, 255, HUD_EDITOR_NO_COSMETIC),
};
// clang-format on

// Allows specifying an override mode to the next active element.
// Must be called again with HUD_EDITOR_ELEMENT_MODE_NONE when done overriding.
extern "C" void HudEditor_OverrideNextElementMode(HudEditorElementMode mode) {
    hudEditorOverrideNextElemMode = mode;
}

#ifdef MMVR_ENABLE
static bool VrHudActive(){
 const char* nativeTest=std::getenv("MMVR_NATIVE_TEST");
 const bool test=nativeTest&&std::strcmp(nativeTest,"1")==0;
 return mmvr::FirstPersonSelected()&&(mmvr::StereoActive()||test)&&gPlayState&&gPlayState->pauseCtx.state==PAUSE_STATE_OFF&&!mmvr::MenuPaused();
}
extern "C" int MMVR_HudLayout(){return VrHudActive();}
static mmvr::HudGroup VrHudGroup(HudEditorElementID id){
 switch(id){
 case HUD_EDITOR_ELEMENT_HEARTS:case HUD_EDITOR_ELEMENT_MAGIC_METER:case HUD_EDITOR_ELEMENT_TIMERS:case HUD_EDITOR_ELEMENT_MINIGAME_COUNTER:return mmvr::HudGroup::TopLeft;
 case HUD_EDITOR_ELEMENT_C_UP:case HUD_EDITOR_ELEMENT_A:case HUD_EDITOR_ELEMENT_B:return mmvr::HudGroup::Buttons;
 case HUD_EDITOR_ELEMENT_RUPEE_COUNTER:case HUD_EDITOR_ELEMENT_KEY_COUNTER:case HUD_EDITOR_ELEMENT_SKULLTULA_COUNTER:return mmvr::HudGroup::BottomLeft;
 case HUD_EDITOR_ELEMENT_MINIMAP:return mmvr::HudGroup::Minimap;
 case HUD_EDITOR_ELEMENT_CLOCK:return mmvr::HudGroup::Clock;
 case HUD_EDITOR_ELEMENT_START:case HUD_EDITOR_ELEMENT_CARROT:return mmvr::HudGroup::TopCenter;
 case HUD_EDITOR_ELEMENT_TIMERS_MOON_CRASH:return mmvr::HudGroup::BottomCenter;
 default:return mmvr::HudGroup::None;
 }
}
#endif

extern "C" bool HudEditor_ShouldOverrideDraw() {
#ifdef MMVR_ENABLE
    if(hudEditorActiveElement!=HUD_EDITOR_ELEMENT_NONE&&VrHudActive())return true;
#endif
    return hudEditorActiveElement != HUD_EDITOR_ELEMENT_NONE &&
           (hudEditorOverrideNextElemMode != HUD_EDITOR_ELEMENT_MODE_NONE
                ? hudEditorOverrideNextElemMode
                : CVarGetInteger(hudEditorElements[hudEditorActiveElement].modeCvar,
                                 HUD_EDITOR_ELEMENT_MODE_VANILLA)) != HUD_EDITOR_ELEMENT_MODE_VANILLA;
}

extern "C" void HudEditor_SetActiveElement(HudEditorElementID id) {
    hudEditorActiveElement = id;
}

extern "C" bool HudEditor_IsActiveElementHidden() {
    return hudEditorActiveElement != HUD_EDITOR_ELEMENT_NONE &&
           (hudEditorOverrideNextElemMode != HUD_EDITOR_ELEMENT_MODE_NONE
                ? hudEditorOverrideNextElemMode
                : CVarGetInteger(hudEditorElements[hudEditorActiveElement].modeCvar,
                                 HUD_EDITOR_ELEMENT_MODE_VANILLA)) == HUD_EDITOR_ELEMENT_MODE_HIDDEN;
}

extern "C" f32 HudEditor_GetActiveElementScale() {
    float vrScale=1.f;
#ifdef MMVR_ENABLE
    if(VrHudActive())vrScale=mmvr::HudElementScale(mmvr::GetSettings().Get(mmvr::Setting::HudSize));
#endif
    return vrScale * ((hudEditorActiveElement != HUD_EDITOR_ELEMENT_NONE &&
            hudEditorOverrideNextElemMode == HUD_EDITOR_ELEMENT_MODE_NONE)
               ? CVarGetFloat(hudEditorElements[hudEditorActiveElement].scaleCvar, 1.0f)
               : 1.0f);
}

extern "C" void HudEditor_ModifyRectPosValuesFromBase(s16 baseX, s16 baseY, s16* rectLeft, s16* rectTop) {
    s16 offsetFromBaseX = *rectLeft - baseX;
    s16 offsetFromBaseY = *rectTop - baseY;
    *rectLeft = baseX + (offsetFromBaseX * HudEditor_GetActiveElementScale());
    *rectTop = baseY + (offsetFromBaseY * HudEditor_GetActiveElementScale());
}

void HudEditor_ModifyRectPosValuesFloat(f32* rectLeft, f32* rectTop) {
    f32 offsetFromBaseX = *rectLeft - hudEditorElements[hudEditorActiveElement].defaultX;
    f32 offsetFromBaseY = *rectTop - hudEditorElements[hudEditorActiveElement].defaultY;
    *rectLeft = CVarGetInteger(hudEditorElements[hudEditorActiveElement].xCvar,
                               hudEditorElements[hudEditorActiveElement].defaultX) +
                (offsetFromBaseX * CVarGetFloat(hudEditorElements[hudEditorActiveElement].scaleCvar, 1.0f));
    *rectTop = CVarGetInteger(hudEditorElements[hudEditorActiveElement].yCvar,
                              hudEditorElements[hudEditorActiveElement].defaultY) +
               (offsetFromBaseY * CVarGetFloat(hudEditorElements[hudEditorActiveElement].scaleCvar, 1.0f));

    if (CVarGetInteger(hudEditorElements[hudEditorActiveElement].modeCvar, HUD_EDITOR_ELEMENT_MODE_VANILLA) ==
        HUD_EDITOR_ELEMENT_MODE_MOVABLE_LEFT) {
        *rectLeft = OTRGetRectDimensionFromLeftEdge(*rectLeft);
    } else if (CVarGetInteger(hudEditorElements[hudEditorActiveElement].modeCvar, HUD_EDITOR_ELEMENT_MODE_VANILLA) ==
               HUD_EDITOR_ELEMENT_MODE_MOVABLE_RIGHT) {
        *rectLeft = OTRGetRectDimensionFromRightEdge(*rectLeft);
    }
#ifdef MMVR_ENABLE
    if(VrHudActive()){
        const auto& settings=mmvr::GetSettings();
        // C-item icons are absent in VR. Put Tatl beneath A/B so maximum corner
        // spread cannot push the prompt past the right edge of the HUD plane.
        if(hudEditorActiveElement==HUD_EDITOR_ELEMENT_C_UP){*rectLeft-=63;*rectTop+=48;}
        // A uses a 45-pixel perspective viewport, not B's 29-pixel rectangle.
        // Align their visual centers: mask, B, A; retain the native action label/shadow.
        if(hudEditorActiveElement==HUD_EDITOR_ELEMENT_A){*rectLeft+=4;*rectTop-=14+R_A_BTN_Y_OFFSET;}
        auto position=mmvr::HudPosition(VrHudGroup(hudEditorActiveElement),*rectLeft,*rectTop,
            settings.Get(mmvr::Setting::HudWidth),settings.Get(mmvr::Setting::HudSize),
            settings.Get(mmvr::Setting::HudHorizontalSpread),settings.Get(mmvr::Setting::HudVerticalSpread));
        *rectLeft=position.x;*rectTop=position.y;
    }
#endif
}

extern "C" void HudEditor_ModifyRectPosValues(s16* rectLeft, s16* rectTop) {
    f32 newLeft = *rectLeft;
    f32 newTop = *rectTop;

    HudEditor_ModifyRectPosValuesFloat(&newLeft, &newTop);

    *rectLeft = (s16)newLeft;
    *rectTop = (s16)newTop;
}

extern "C" void HudEditor_ModifyRectSizeValues(s16* rectWidth, s16* rectHeight) {
    *rectWidth *= HudEditor_GetActiveElementScale();
    *rectHeight *= HudEditor_GetActiveElementScale();
}

extern "C" void HudEditor_ModifyTextureStepValues(s16* dsdx, s16* dtdy) {
    *dsdx /= HudEditor_GetActiveElementScale();
    *dtdy /= HudEditor_GetActiveElementScale();
}

// Modify matrix values based on the identity matrix (0,0) centered on the screen
extern "C" void HudEditor_ModifyMatrixValues(f32* transX, f32* transY) {
    *transX = ((f32)SCREEN_WIDTH / 2) + *transX;
    *transY = ((f32)SCREEN_HEIGHT / 2) - *transY;

    HudEditor_ModifyRectPosValuesFloat(transX, transY);

    *transX = *transX - ((f32)SCREEN_WIDTH / 2);
    *transY = ((f32)SCREEN_HEIGHT / 2) - *transY;
}

extern "C" void HudEditor_ModifyKaleidoEquipAnimValues(s16* ulx, s16* uly, s16* shrinkRate) {
    // Kaleido values are a multiple of 10 on the identity matrix
    // Normalize them before passing to the modify matrix
    f32 transX = *ulx / 10;
    f32 transY = *uly / 10;

    HudEditor_ModifyMatrixValues(&transX, &transY);

    *ulx = transX * 10;
    *uly = transY * 10;

    float scale = HudEditor_GetActiveElementScale();
    // 320 is the vanilla start size, and 280 is the vanilla end size (or 160 for dpad)
    // So we apply the scale to 280 and subtract to get the shrink rate
    int16_t endAnimSize = hudEditorActiveElement == HUD_EDITOR_ELEMENT_D_PAD ? 160 : 280;
    *shrinkRate = 320 - (s16)(endAnimSize * scale);
}

extern "C" void HudEditor_ModifyDrawValuesFromBase(s16 baseX, s16 baseY, s16* rectLeft, s16* rectTop, s16* rectWidth,
                                                   s16* rectHeight, s16* dsdx, s16* dtdy) {
    HudEditor_ModifyRectPosValuesFromBase(baseX, baseY, rectLeft, rectTop);

    *rectWidth *= HudEditor_GetActiveElementScale();
    *rectHeight *= HudEditor_GetActiveElementScale();
    *dsdx /= HudEditor_GetActiveElementScale();
    *dtdy /= HudEditor_GetActiveElementScale();
}

extern "C" void HudEditor_ModifyDrawValues(s16* rectLeft, s16* rectTop, s16* rectWidth, s16* rectHeight, s16* dsdx,
                                           s16* dtdy) {
    HudEditor_ModifyRectPosValues(rectLeft, rectTop);

    *rectWidth *= HudEditor_GetActiveElementScale();
    *rectHeight *= HudEditor_GetActiveElementScale();
    *dsdx /= HudEditor_GetActiveElementScale();
    *dtdy /= HudEditor_GetActiveElementScale();
}

extern "C" void HudEditor_ModifyDrawValuesPrecise(f32* left,f32* top,f32* width,f32* height,s16* dsdx,s16* dtdy){
#ifdef MMVR_ENABLE
 if(VrHudActive()){
  const float scale=HudEditor_GetActiveElementScale();
  HudEditor_ModifyRectPosValuesFloat(left,top);
  // Quantize shared edges once, rather than flooring each segment's position and width separately.
  float right=std::round((*left+*width*scale)*4.f)*.25f,bottom=std::round((*top+*height*scale)*4.f)*.25f;
  *left=std::round(*left*4.f)*.25f;*top=std::round(*top*4.f)*.25f;
  *width=right-*left;*height=bottom-*top;*dsdx=std::lround(*dsdx/scale);*dtdy=std::lround(*dtdy/scale);return;
 }
#endif
 s16 l=*left,t=*top,w=*width,h=*height;
 HudEditor_ModifyDrawValues(&l,&t,&w,&h,dsdx,dtdy);*left=l;*top=t;*width=w;*height=h;
}
extern "C" void HudEditor_ModifyDrawValuesFromBasePrecise(s16 baseX,s16 baseY,f32* left,f32* top,f32* width,f32* height,s16* dsdx,s16* dtdy){
 // Timer paths keep their established base-relative layout.
 s16 l=*left,t=*top,w=*width,h=*height;
 HudEditor_ModifyDrawValuesFromBase(baseX,baseY,&l,&t,&w,&h,dsdx,dtdy);*left=l;*top=t;*width=w;*height=h;
}

const char* modeNames[] = {
    "Vanilla (4:3)", "Hidden", "Movable (Align Center)", "Movable (Align Left)", "Movable (Align Right)",
};

const char* presetNames[] = {
    "Vanilla (4:3)",
    "Hidden",
    "Widescreen",
};

static CosmeticOption& HudEditor_GetCosmeticOption(const char* cosmeticOptionId) {
    return cosmeticOptions.at(cosmeticOptionId);
}

namespace HudEditor {
enum Presets {
    VANILLA,
    HIDDEN,
    WIDESCREEN,
};
};

void HudEditorWindow::DrawElement() {
    static HudEditor::Presets preset = HudEditor::Presets::VANILLA;
    if (UIWidgets::Combobox("Preset", &preset, presetNames)) {
        for (int i = HUD_EDITOR_ELEMENT_B; i < HUD_EDITOR_ELEMENT_MAX; i++) {
            CVarClear(hudEditorElements[i].xCvar);
            CVarClear(hudEditorElements[i].yCvar);
            CVarClear(hudEditorElements[i].scaleCvar);
            CVarClear(hudEditorElements[i].modeCvar);
            // Also clear cosmetic colors for elements with mappings
            if (hudEditorElements[i].cosmeticOptionId != nullptr) {
                CosmeticOption& cosmeticElement = HudEditor_GetCosmeticOption(hudEditorElements[i].cosmeticOptionId);
                CVarClear(cosmeticElement.colorCvar);
                CVarClear(cosmeticElement.colorChangedCvar);
                ShipInit::Init(cosmeticElement.colorCvar);
                ShipInit::Init(cosmeticElement.colorChangedCvar);
            }
        }

        switch (preset) {
            case HudEditor::Presets::HIDDEN: {
                for (int i = HUD_EDITOR_ELEMENT_B; i < HUD_EDITOR_ELEMENT_MAX; i++) {
                    CVarSetInteger(hudEditorElements[i].modeCvar, HUD_EDITOR_ELEMENT_MODE_HIDDEN);
                }
                break;
            }
            case HudEditor::Presets::WIDESCREEN: {
                CVarSetInteger(hudEditorElements[HUD_EDITOR_ELEMENT_B].modeCvar, HUD_EDITOR_ELEMENT_MODE_MOVABLE_RIGHT);
                CVarSetInteger(hudEditorElements[HUD_EDITOR_ELEMENT_C_LEFT].modeCvar,
                               HUD_EDITOR_ELEMENT_MODE_MOVABLE_RIGHT);
                CVarSetInteger(hudEditorElements[HUD_EDITOR_ELEMENT_C_DOWN].modeCvar,
                               HUD_EDITOR_ELEMENT_MODE_MOVABLE_RIGHT);
                CVarSetInteger(hudEditorElements[HUD_EDITOR_ELEMENT_C_RIGHT].modeCvar,
                               HUD_EDITOR_ELEMENT_MODE_MOVABLE_RIGHT);
                CVarSetInteger(hudEditorElements[HUD_EDITOR_ELEMENT_A].modeCvar, HUD_EDITOR_ELEMENT_MODE_MOVABLE_RIGHT);
                CVarSetInteger(hudEditorElements[HUD_EDITOR_ELEMENT_C_UP].modeCvar,
                               HUD_EDITOR_ELEMENT_MODE_MOVABLE_RIGHT);
                CVarSetInteger(hudEditorElements[HUD_EDITOR_ELEMENT_D_PAD].modeCvar,
                               HUD_EDITOR_ELEMENT_MODE_MOVABLE_RIGHT);
                CVarSetInteger(hudEditorElements[HUD_EDITOR_ELEMENT_MINIMAP].modeCvar,
                               HUD_EDITOR_ELEMENT_MODE_MOVABLE_RIGHT);
                CVarSetInteger(hudEditorElements[HUD_EDITOR_ELEMENT_START].modeCvar,
                               HUD_EDITOR_ELEMENT_MODE_MOVABLE_RIGHT);
                CVarSetInteger(hudEditorElements[HUD_EDITOR_ELEMENT_CARROT].modeCvar,
                               HUD_EDITOR_ELEMENT_MODE_MOVABLE_43);
                CVarSetInteger(hudEditorElements[HUD_EDITOR_ELEMENT_CLOCK].modeCvar,
                               HUD_EDITOR_ELEMENT_MODE_MOVABLE_43);
                CVarSetInteger(hudEditorElements[HUD_EDITOR_ELEMENT_HEARTS].modeCvar,
                               HUD_EDITOR_ELEMENT_MODE_MOVABLE_LEFT);
                CVarSetInteger(hudEditorElements[HUD_EDITOR_ELEMENT_MAGIC_METER].modeCvar,
                               HUD_EDITOR_ELEMENT_MODE_MOVABLE_LEFT);
                CVarSetInteger(hudEditorElements[HUD_EDITOR_ELEMENT_TIMERS].modeCvar,
                               HUD_EDITOR_ELEMENT_MODE_MOVABLE_LEFT);
                CVarSetInteger(hudEditorElements[HUD_EDITOR_ELEMENT_TIMERS_MOON_CRASH].modeCvar,
                               HUD_EDITOR_ELEMENT_MODE_MOVABLE_43);
                CVarSetInteger(hudEditorElements[HUD_EDITOR_ELEMENT_MINIGAME_COUNTER].modeCvar,
                               HUD_EDITOR_ELEMENT_MODE_MOVABLE_LEFT);
                CVarSetInteger(hudEditorElements[HUD_EDITOR_ELEMENT_RUPEE_COUNTER].modeCvar,
                               HUD_EDITOR_ELEMENT_MODE_MOVABLE_LEFT);
                CVarSetInteger(hudEditorElements[HUD_EDITOR_ELEMENT_KEY_COUNTER].modeCvar,
                               HUD_EDITOR_ELEMENT_MODE_MOVABLE_LEFT);
                CVarSetInteger(hudEditorElements[HUD_EDITOR_ELEMENT_SKULLTULA_COUNTER].modeCvar,
                               HUD_EDITOR_ELEMENT_MODE_MOVABLE_LEFT);
                break;
            }
        }
        Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
    }

    for (int i = HUD_EDITOR_ELEMENT_B; i < HUD_EDITOR_ELEMENT_MAX; i++) {
        ImGui::PushID(hudEditorElements[i].name);
        ImGui::SeparatorText(hudEditorElements[i].name);

        // Color picker - only enabled if this element has a cosmetic counterpart
        bool hasCosmeticMapping = hudEditorElements[i].cosmeticOptionId != nullptr;

        if (hasCosmeticMapping) {
            CosmeticOption& cosmeticElement = HudEditor_GetCosmeticOption(hudEditorElements[i].cosmeticOptionId);
            bool colorChanged = CVarGetInteger(cosmeticElement.colorChangedCvar, false);
            float defaultColor[4] = { cosmeticElement.defaultR / 255.0f, cosmeticElement.defaultG / 255.0f,
                                      cosmeticElement.defaultB / 255.0f, cosmeticElement.defaultA / 255.0f };
            float color[4] = { defaultColor[0], defaultColor[1], defaultColor[2], defaultColor[3] };

            if (colorChanged) {
                Color_RGBA8 changedColor = CVarGetColor(cosmeticElement.colorCvar, {});
                color[0] = (float)changedColor.r / 255;
                color[1] = (float)changedColor.g / 255;
                color[2] = (float)changedColor.b / 255;
                color[3] = (float)changedColor.a / 255;
            }

            if (ImGui::ColorEdit3("Color", color, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel)) {
                Color_RGBA8 colorSelected;
                colorSelected.r = static_cast<uint8_t>(color[0] * 255.0f);
                colorSelected.g = static_cast<uint8_t>(color[1] * 255.0f);
                colorSelected.b = static_cast<uint8_t>(color[2] * 255.0f);
                colorSelected.a = static_cast<uint8_t>(255.0f);

                CVarSetColor(cosmeticElement.colorCvar, colorSelected);
                CVarSetInteger(cosmeticElement.colorChangedCvar, true);
                ShipInit::Init(cosmeticElement.colorCvar);
                ShipInit::Init(cosmeticElement.colorChangedCvar);
                Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
            }
            ImGui::SameLine();
            if (ImGui::Button(ICON_FA_REFRESH)) {
                CVarClear(cosmeticElement.colorCvar);
                CVarClear(cosmeticElement.colorChangedCvar);
                ShipInit::Init(cosmeticElement.colorCvar);
                ShipInit::Init(cosmeticElement.colorChangedCvar);
                Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
            }
        } else {
            // Disabled color picker for elements without cosmetic mappings
            ImGui::BeginDisabled();
            float defaultColor[4] = { hudEditorElements[i].defaultR / 255.0f, hudEditorElements[i].defaultG / 255.0f,
                                      hudEditorElements[i].defaultB / 255.0f, hudEditorElements[i].defaultA / 255.0f };
            ImGui::ColorEdit3("Color", defaultColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("%s", "Color customization is not yet available for this element.");
            }
            ImGui::SameLine();
            ImGui::Button(ICON_FA_REFRESH);
            ImGui::EndDisabled();
        }
        ImGui::SameLine();
        if (UIWidgets::CVarCombobox("Mode", hudEditorElements[i].modeCvar, modeNames,
                                    { .labelPosition = UIWidgets::LabelPosition::None })) {
            CVarClear(hudEditorElements[i].xCvar);
            CVarClear(hudEditorElements[i].yCvar);
            CVarClear(hudEditorElements[i].scaleCvar);
        }
        if (CVarGetInteger(hudEditorElements[i].modeCvar, HUD_EDITOR_ELEMENT_MODE_VANILLA) >=
            HUD_EDITOR_ELEMENT_MODE_MOVABLE_43) {
            if (ImGui::BeginTable("##table", 3,
                                  ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_NoBordersInBody |
                                      ImGuiTableFlags_SizingStretchSame)) {
                ImGui::TableNextColumn();
                UIWidgets::CVarSliderInt("X", hudEditorElements[i].xCvar,
                                         {
                                             .showAdjustmentButtons = false,
                                             .format = "X: %d",
                                             .min = -10,
                                             .max = 330,
                                             .defaultValue = hudEditorElements[i].defaultX,
                                             .labelPosition = UIWidgets::LabelPosition::None,
                                         });
                ImGui::TableNextColumn();
                UIWidgets::CVarSliderInt("Y", hudEditorElements[i].yCvar,
                                         {
                                             .showAdjustmentButtons = false,
                                             .format = "Y: %d",
                                             .min = -10,
                                             .max = 250,
                                             .defaultValue = hudEditorElements[i].defaultY,
                                             .labelPosition = UIWidgets::LabelPosition::None,
                                         });
                ImGui::TableNextColumn();
                UIWidgets::CVarSliderFloat("Scale", hudEditorElements[i].scaleCvar,
                                           {
                                               .showAdjustmentButtons = false,
                                               .format = "Scale: %.2f",
                                               .min = 0.25f,
                                               .max = 4.0f,
                                               .defaultValue = 1.0f,
                                               .labelPosition = UIWidgets::LabelPosition::None,
                                           });
                ImGui::EndTable();
            }
        }
        ImGui::PopID();
    }
}
