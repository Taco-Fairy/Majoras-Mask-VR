#include "FairyComfort.h"
#include "NativeActions.h"
#include "PresentationOptions.h"
#include "ScenePresentation.h"
#ifdef MMVR_ENABLE
#include "2s2h/VR/DebugRoom.h"
#endif
#ifdef MMVR_ENABLE
#include "Bow.h"
#include "TownAudio.h"
#include "Masks.h"
#include "ItemUse.h"
#include "Bottle.h"
#include "Camera.h"
#include "NativeTrackingResume.h"
#include "NativeStateBackend.h"
#include "Interactions.h"
#include "NativeClimbing.h"
#include "NativeCombat.h"
#include "NativeForms.h"
#include "ui.h"
#include "updater.h"
#include "presentation.h"
#include "runtime.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "ship/Context.h"
#include "ship/config/Config.h"
#include "fast/Fast3dGui.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <fstream>
extern "C" {
#include "global.h"
#include "overlays/gamestates/ovl_file_choose/z_file_select.h"
void MMVR_PlayerEquipSword(PlayState*, Player*, ItemId);
}
#include "NativeMenuChecks.h"
#include "NativeTest.h"
#include "DebugMenu.h"
#include "NativeOptions.h"
namespace {
std::atomic<unsigned> instrumentPad{ 0 };
constexpr const char* FrameName = "MMVR/ClockTowerFrame";
const char* SlotKeys[] = { "gVR.Slot.Up", "gVR.Slot.Right", "gVR.Slot.Down", "gVR.Slot.Left", "gVR.Slot.TopLeft", "gVR.Slot.TopRight", "gVR.Slot.BottomLeft", "gVR.Slot.BottomRight" };
const char* SlotNames[] = { "Top item", "Right item", "Bottom item", "Left item", "Top left", "Top right", "Bottom left", "Bottom right" };
std::array<std::shared_ptr<Fast::Texture>, mmvr::MaxItemSlots> iconResources;
std::array<ImTextureID, mmvr::MaxItemSlots> icons{};
std::shared_ptr<Fast::Texture> maskResource;
int SlotItem(PlayState* play, int slot) {
    return mmvrgame::WheelSlotItem(play, slot);
}
bool initialized = false, settingsDirty = false;
auto Gui() {
    return std::dynamic_pointer_cast<Fast::Fast3dGui>(Ship::Context::GetRawInstance()->GetWindow()->GetGui());
}
void Save() {
    settingsDirty = true;
}
bool CommitSettings() {
    if (!settingsDirty) return true;
    CVarSave();
    if (!Ship::Context::GetRawInstance()->GetConfig()->LastSaveSucceeded()) return false;
    settingsDirty = false;
    return true;
}
void Change(mmvr::Setting id, float value) {
    value = mmvr::BoundSetting(id, value);
    mmvr::GetSettings().Set(id, value);
    CVarSetFloat(mmvr::SettingDefinitions[size_t(id)].key, value);
    if (id == mmvr::Setting::ViewMode)
        mmvr::ApplyViewMode(int(std::lround(value)));
    if (id == mmvr::Setting::DebugSkipCutscenes) mmvrgame::SyncDebugCutsceneSkips();
    Save();
}
void Assign(int position, int slot) {
    mmvr::SetSlotAssignment(position, slot);
    CVarSetInteger(SlotKeys[position], slot);
    Save();
}
#ifdef MMVR_LOCAL_TEST_TOOLS
#include "SettingsRepairTest.h"
#endif
void InitFrame() {
    Fast::Texture texture;
    texture.Type = Fast::TextureType::RGBA32bpp;
    texture.Width = texture.Height = 128;
    texture.ImageDataSize = 128 * 128 * 4;
    texture.ImageData = new uint8_t[texture.ImageDataSize]{};
    for (int y = 0; y < 128; ++y)
        for (int x = 0; x < 128; ++x) {
            int edge = std::min(std::min(x, y), std::min(127 - x, 127 - y));
            auto* pixel = texture.ImageData + (y * 128 + x) * 4;
            // One reusable texture: brass bevel, teal inner edge, quiet midnight interior.
            const uint8_t brass[4] = { 172, 132, 64, 255 }, light[4] = { 225, 196, 125, 255 },
                          teal[4] = { 40, 112, 117, 240 }, inside[4] = { 10, 23, 38, 210 };
            const auto* color = edge < 3 ? brass : edge < 5 ? light : edge < 9 ? brass : edge < 11 ? teal : inside;
            for (int c = 0; c < 4; ++c)
                pixel[c] = color[c];
        }
    Gui()->LoadGuiTexture(FrameName, texture, "", { 1, 1, 1, 1 });
    initialized = true;
}
void Render(const mmvr::UiDrawFrame& frame) {
    auto gui = Gui();
    if (!gui || !ImGui::GetCurrentContext())
        return;
    if (frame.kind == mmvr::UiKind::Menu && mmvr::GetMenu().tab == mmvr::NativeTab) {
        // Native widgets persist to the same application config when settings close.
        Save();
        mmvrgame::DrawNativeOptions(frame, *gui);
        return;
    }
    ImDrawList list(ImGui::GetDrawListSharedData());
    list._ResetForNewFrame();
    list.PushTextureID(ImGui::GetIO().Fonts->TexID);
    list.PushClipRect({ 0, 0 }, { float(frame.width), float(frame.height) });
    mmvr::presentation::Draw(list, frame, gui->GetTextureByName(FrameName), icons);
    list.PopClipRect();
    list.PopTextureID();
    ImDrawData data;
    data.Valid = true;
    data.DisplayPos = { 0, 0 };
    data.DisplaySize = { float(frame.width), float(frame.height) };
    data.FramebufferScale = { 1, 1 };
    data.AddDrawList(&list);
    gui->RenderVrDrawData(&data);
    if (frame.kind == mmvr::UiKind::Menu && mmvr::GetMenu().tab == mmvr::SystemTab && mmvr::setupGuideVisible)
        { if(!mmvr::setupGuideRendered) std::ofstream("mmvr-startup.log",std::ios::app)<<"setup rendered\n";
          mmvr::setupGuideRendered = true; }
}
} // namespace
extern "C" int MMVR_HideCompanionFairy(PlayState* play, Actor* actor) {
    auto* player = play ? GET_PLAYER(play) : nullptr;
    const bool companion = actor && ((player && player->tatlActor == actor) ||
        (actor->id == ACTOR_DM_CHAR00 && actor->params == 0));
    // Kafei's player struct carries Link's companion pointer. Hide only that
    // companion during Kafei control, and restore her automatically when the
    // native handoff puts Link back in ACTORCAT_PLAYER (including debug trials).
    const bool controlledKafei = player && MMVR_ControlledKafei(player);
    return companion && (controlledKafei || mmvr::GetSettings().Get(mmvr::Setting::HideFairy) > .5f ||
                         MMVR_FairyReplacedByCutscene(play, actor));
}
extern "C" int MMVR_MuteCompanionFairy(Actor* actor) {
    return actor && actor->id == ACTOR_DM_CHAR00 && actor->params == 0 &&
        mmvr::GetSettings().Get(mmvr::Setting::MuteFairy) > .5f;
}
extern "C" int MMVR_HideFairyArrow(void) {
    return mmvr::GetSettings().Get(mmvr::Setting::HideFairyArrow) > .5f;
}
extern "C" int MMVR_MuteFairySound(unsigned short id) {
    if (mmvr::GetSettings().Get(mmvr::Setting::MuteFairy) < .5f) return false;
    // Include positional and UI-triggered companion voices, but preserve
    // collectible/healing fairy sounds, quest messages and dialogue controls.
    switch (id | SFX_FLAG) {
        case NA_SE_EV_WHITE_FAIRY_DASH: case NA_SE_EV_BELL_DASH_NORMAL:
        case NA_SE_EV_BELL_ANGER: case NA_SE_EV_NAVY_VANISH:
        case NA_SE_EV_NAVY_FLY_REBIRTH: case NA_SE_EV_BELL_SPIT:
        case NA_SE_EV_BELL_SIGH: case NA_SE_EV_BELL_BRAKE:
        case NA_SE_EV_WHITE_FAIRY_SHOT_DASH:
        case NA_SE_VO_NAVY_ENEMY: case NA_SE_VO_NAVY_HELLO:
        case NA_SE_VO_NAVY_HEAR: case NA_SE_VO_NAVY_CALL:
        case NA_SE_VO_NA_HELLO_0: case NA_SE_VO_NA_HELLO_1:
        case NA_SE_VO_NA_HELLO_2: case NA_SE_VO_NA_HELLO_3:
        case NA_SE_VO_NA_LISTEN: return true;
        default: return false;
    }
}
extern "C" int MMVR_SkipIntroMaskVisuals(PlayState* play) {
    return play && mmvr::NeedsOwnedFramebuffer() &&
        mmvr::GetSettings().Get(mmvr::Setting::VrCameraCutscenes) > .5f &&
        play->sceneId == SCENE_OPENINGDAN &&
        gSaveContext.save.entrance == ENTRANCE(OPENING_DUNGEON, 0) &&
        SOH::Cutscene::HasOriginalMaskFall(play->csCtx.script);
}
extern "C" int MMVR_IntroMaskSkipTarget(PlayState* play, const void* script, int frame) {
    return frame == 180 && MMVR_SkipIntroMaskVisuals(play) &&
        SOH::Cutscene::HasOriginalMaskFall(script) ? 414 : frame;
}
extern "C" float MMVR_DialogueScale(int background) {
    if (!mmvr::NeedsOwnedFramebuffer()) return 1.f;
    const auto& settings=mmvr::GetSettings();
    return settings.Get(mmvr::Setting::TextBoxSize)*.01f *
        (background ? 1.f : settings.Get(mmvr::Setting::TextSize)*.01f);
}
extern "C" int MMVR_TextAlpha(int alpha) {
    return int(std::lround(std::clamp(alpha,0,255) * (mmvr::NeedsOwnedFramebuffer() ?
        mmvr::GetSettings().Get(mmvr::Setting::TextOpacity) : 1.f)));
}
extern "C" int MMVR_TextBoxAlpha(int alpha) {
    // Match the menu's background-only opacity: glyphs and native fade timing
    // remain unchanged. The value is independent of VR-menu opacity.
    const float opacity = mmvr::NeedsOwnedFramebuffer() ?
        mmvr::GetSettings().Get(mmvr::Setting::TextBoxOpacity) : 1.f;
    return int(std::lround(std::clamp(alpha, 0, 255) * opacity));
}
extern "C" int MMVR_InstrumentButtons(unsigned short* buttons) {
    auto value = instrumentPad.load();
    if (!(value & 0x10000) || (gPlayState && Message_GetState(&gPlayState->msgCtx) == TEXT_STATE_CHOICE))
        return false;
    *buttons = value & 0x800F;
    return true;
}
extern "C" int MMVR_ScriptedInstrumentVisible(void) {
    if (!gPlayState || gPlayState->csCtx.state == CS_STATE_IDLE) return false;
    auto* player = GET_PLAYER(gPlayState);
    // Player_CsAction_14/29 set itemAction and the native model, but never
    // PLAYER_STATE2_USING_OCARINA. Preserve the scripted prop in the VR hand.
    return player && player->itemAction == PLAYER_IA_OCARINA;
}
extern "C" int MMVR_ClearLessonBackground(void) {
    return mmvr::FirstPersonRequested() && MMVR_ScriptedInstrumentVisible();
}
extern "C" int MMVR_InstrumentOverlay(void) {
    if (!mmvr::FirstPersonRequested() || !gPlayState || !GET_PLAYER(gPlayState)) return false;
    if (GET_PLAYER(gPlayState)->stateFlags2 & PLAYER_STATE2_USING_OCARINA) return true;
    const auto mode = gPlayState->msgCtx.msgMode;
    // Scripted song lessons have a playable native prompt without setting the
    // regular item-use flag. Do not claim normal dialogue or demonstrations.
    return MMVR_ScriptedInstrumentVisible() &&
           (mode == MSGMODE_SONG_PROMPT_STARTING || mode == MSGMODE_SONG_PROMPT);
}
extern "C" void MMVR_ApplyGameInput(void* data) {
    auto* input = static_cast<Input*>(data);
    static uint16_t previous = 0;
    const bool controlledKafei = gPlayState && MMVR_ControlledKafei(GET_PLAYER(gPlayState));
    constexpr uint16_t itemButtons = BTN_CUP | BTN_CDOWN | BTN_CLEFT | BTN_CRIGHT;
    if (controlledKafei) {
        input->prev.button &= ~itemButtons;
        input->cur.button &= ~itemButtons;
        input->press.button &= ~itemButtons;
        input->rel.button &= ~itemButtons;
        previous &= ~itemButtons;
    }
    const bool nativeTest = NativeTestEnabled();
    const char* interactiveFlag = std::getenv("MMVR_PERFORMANCE_INTERACTIVE");
    const bool interactive = nativeTest && gPlayState && gSaveContext.gameMode == GAMEMODE_NORMAL &&
                             interactiveFlag && std::strcmp(interactiveFlag, "1") == 0;
    // Hidden automated runs must not consume the user's keyboard/controller
    // buttons, including edges read by fixture helpers before pad injection.
    if (nativeTest && !interactive)
        input->prev.button = input->cur.button = input->press.button = input->rel.button = 0;
    auto pad = nativeTest ? NativeTestInput() : mmvr::ConsumePad();
    if (controlledKafei)
        pad.buttons &= ~itemButtons;
    if (!pad.active ||
        ((!nativeTest || interactive) && Ship::Context::GetRawInstance()->GetWindow()->GetGui()->GetMenuOrMenubarVisible())) {
        previous = 0;
        instrumentPad.store(0);
        return;
    }
    instrumentPad.store(MMVR_InstrumentOverlay() ? 0x10000 | pad.buttons : 0);
    input->prev.button |= previous;
    input->cur.button |= pad.buttons;
    input->press.button |= pad.buttons & ~previous;
    input->rel.button |= previous & ~pad.buttons;
    input->cur.err_no = 0;
    // Idle motion controllers must not erase native gamepad axes in theater
    // or third-person mode. An actively moved VR stick still takes ownership.
    if (nativeTest || mmvr::FirstPersonSelected() || pad.x || pad.y) {
        input->cur.stick_x = pad.x;
        input->cur.stick_y = pad.y;
    }
    if (nativeTest || mmvr::FirstPersonSelected() || pad.rightX || pad.rightY) {
        input->cur.right_stick_x = pad.rightX;
        input->cur.right_stick_y = pad.rightY;
    }
    PadUtils_UpdateRelXY(input);
    previous = pad.buttons;
}
extern "C" void MMVR_PollUpdater();
extern "C" void MMVR_RegisterMenu(void) {
    MMVR_PollUpdater();
    mmvrgame::SyncDebugCutsceneSkips();
    mmvr::SetUiCallbacks(Render, Change, Assign);
    mmvr::GetMenu().commitSettings = CommitSettings;
#if defined(MMVR_STATE_NATIVE_BACKEND)
    mmvrgame::InitializeExactStateMenu();
#endif
    if (!initialized)
        InitFrame();
    static bool setupShown = false;
    const bool startupScreen = !gPlayState || gSaveContext.gameMode == GAMEMODE_TITLE_SCREEN;
    // Version 1 recorded completion before presenting anything. Show the repaired
    // guide once, after XR is focused, at title/file selection rather than gameplay.
    if (!setupShown && CVarGetInteger("gVR.SetupGuideSeen", 0) < 3 && startupScreen &&
        mmvr::InputFocused() && mmvr::RendererBridgeObserved() &&
        !(mmvr::PrivateDebugTools && std::getenv("MMVR_NATIVE_TEST"))) {
        setupShown = true;
        std::ofstream("mmvr-startup.log",std::ios::app)<<"setup requested at title/file screen\n";
        mmvr::setupGuideRendered = false;
        mmvr::setupGuideVisible = true;
        mmvr::OpenSystemSettings();
    }
    // Loss of focus or a scene reset can close UI without user acknowledgment.
    // Keep the welcome pending until an explicit guide button is pressed.
    if (setupShown && !mmvr::setupGuideCompleted && mmvr::setupGuideVisible &&
        !mmvr::GetMenu().open && startupScreen && mmvr::InputFocused())
        mmvr::OpenSystemSettings();
    if (mmvr::setupGuideCompleted) {
        mmvr::setupGuideCompleted = false;
        CVarSetInteger("gVR.SetupGuideSeen", 3);
        std::ofstream("mmvr-startup.log",std::ios::app)<<"setup explicitly dismissed\n";
        settingsDirty = true;
        CommitSettings();
    }
    // Defaults belong in the registry. Never reapply them on load or save-slot
    // changes: all VR preferences and bindings are application-wide CVars.
    static bool wasOpen = false;
    static int previousFile = -1;
    if (settingsDirty && ((wasOpen && !mmvr::GetMenu().open) || previousFile != gSaveContext.fileNum)) {
        CommitSettings();
    }
    wasOpen = mmvr::GetMenu().open;
    previousFile = gSaveContext.fileNum;
    // Explicit local harness only; never active in normal launchers.
    if (const char* smoke = std::getenv("MMVR_SMOKE_FRAMES"); mmvr::PrivateDebugTools && smoke && std::string(smoke) == "120") {
        static unsigned frames = 0;
        if (++frames == 120) {
            std::ofstream("mmvr-smoke.json")
                << "{\"renderCommandFrames\":120,\"menuResourcesInitialized\":true,\"rendererBridgeObserved\":"
                << (mmvr::RendererBridgeObserved() ? "true" : "false") << "}";
            Ship::Context::GetRawInstance()->GetWindow()->Close();
        }
    }
    auto* play = gPlayState;
    auto* player = play ? GET_PLAYER(play) : nullptr;
    mmvr::GetMenu().playerForm = player ? player->transformation : PLAYER_FORM_HUMAN;
    mmvr::GetMenu().canSkipDay = mmvrgame::CanSkipDebugDay(play);
    mmvr::GetMenu().canSkipHours = mmvrgame::CanSkipDebugHours(play);
    mmvr::SetFirstPersonEligibility(!player || ((play->actorCtx.flags & ACTORCTX_FLAG_TELESCOPE_ON) ||
                                                (mmvrgame::FirstPersonFormAllowed(player) &&
                                                 mmvrgame::SceneView(play) == mmvr::SceneView::Player)));
    bool allowed = player && !mmvrgame::NativeViewfinderActive(play) &&
                   !(play->actorCtx.flags & ACTORCTX_FLAG_TELESCOPE_ON) && play->pauseCtx.state == PAUSE_STATE_OFF &&
                   play->csCtx.state == CS_STATE_IDLE && play->transitionTrigger == TRANS_TRIGGER_OFF &&
                   player->csAction == PLAYER_CSACTION_NONE && gSaveContext.save.saveInfo.playerData.health > 0;
    int assignSlot = -1;
    if (play && play->pauseCtx.state == PAUSE_STATE_MAIN && play->pauseCtx.mainState == PAUSE_MAIN_STATE_IDLE &&
        play->pauseCtx.cursorSpecialPos == 0) {
        int page = play->pauseCtx.pageIndex;
        if (page == PAUSE_QUEST && play->pauseCtx.cursorPoint[PAUSE_QUEST] == QUEST_SWORD &&
            SlotItem(play, 48) >= ITEM_SWORD_KOKIRI && SlotItem(play, 48) <= ITEM_SWORD_GILDED)
            assignSlot = 48;
        if (page == PAUSE_ITEM || page == PAUSE_MASK) {
            assignSlot = play->pauseCtx.cursorSlot[page] + (page == PAUSE_MASK ? ITEM_NUM_SLOTS : 0);
            if (assignSlot < 0 || assignSlot >= 48 ||
                gSaveContext.save.saveInfo.inventory.items[assignSlot] == ITEM_NONE)
                assignSlot = -1;
        }
    }
    mmvr::SetAssignmentContext(assignSlot);
    mmvr::SetClimbingContext(MMVR_ClimbingInputContext(play));
    mmvr::SetThrowableContext(allowed && player->transformation == PLAYER_FORM_HUMAN && player->heldActor &&
                              player->heldActor->id == ACTOR_EN_BOM && player->heldActor->parent == &player->actor &&
                              !mmvr::MenuPaused());
    mmvr::SetInputContext((allowed && !(player->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR)) ||
                              mmvrgame::ExchangePromptActive(play),
                          MMVR_InstrumentOverlay());
    mmvr::SetDialogueChoice(play && (Message_GetState(&play->msgCtx) == TEXT_STATE_CHOICE ||
                                   MMVR_SongTimeSelectionActive()));
    mmvrgame::UpdateMaskContext(play);
    int maskItem = mmvr::WornMaskItem();
    if (maskItem >= 0) {
        auto resource = std::dynamic_pointer_cast<Fast::Texture>(
            Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource((const char*)gItemIcons[maskItem],
                                                                                false));
        if (resource != maskResource) {
            Gui()->UnloadTexture("MMVR/HeldMask");
            mmvr::SetMaskIcon(0);
            if (resource) {
                Gui()->LoadGuiTexture("MMVR/HeldMask", *resource, "", { 1, 1, 1, 1 });
                mmvr::SetMaskIcon((uintptr_t)Gui()->GetTextureByName("MMVR/HeldMask"));
            }
            maskResource = resource;
        }
    } else {
        mmvr::SetMaskIcon(0);
        maskResource.reset();
    }
    for (int i = 0; i < mmvr::MaxItemSlots; ++i) {
        int saved = CVarGetInteger(SlotKeys[i], mmvr::GetSlotAssignment(i));
        if (play && saved >= 0 && saved < 48 && gSaveContext.save.saveInfo.inventory.items[saved] == ITEM_NONE)
            saved = -1;
        mmvr::SetSlotAssignment(i, saved);
        int slot = mmvr::DisplaySlotAssignment(i);
        int item = SlotItem(play, slot);
        if (item == ITEM_NONE || item >= 131) {
            icons[i] = 0;
            iconResources[i].reset();
            continue;
        }
        auto resource = std::dynamic_pointer_cast<Fast::Texture>(
            Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource((const char*)gItemIcons[item], false));
        if (resource != iconResources[i]) {
            auto name = "MMVR/Item" + std::to_string(i);
            Gui()->UnloadTexture(name);
            icons[i] = 0;
            if (resource) {
                Gui()->LoadGuiTexture(name, *resource, "", { 1, 1, 1, 1 });
                icons[i] = Gui()->GetTextureByName(name);
            }
            iconResources[i] = resource;
        }
    }
}
extern "C" void MMVR_NativePresentationProbe(PlayState* play) {
    if (!NativeTestEnabled())
        return;
    for (const char* flag : { "MMVR_SCRIPT_TEST", "MMVR_DAMAGE_MATRIX_TEST", "MMVR_POTION_SHOP_TEST", "MMVR_EXCHANGE_TEST", "MMVR_LIFECYCLE_TEST", "MMVR_TOWN_TEST",
                              "MMVR_ARENA_EXPANSION_TEST", "MMVR_FLOWER_TEST", "MMVR_PERFORMANCE_TEST", "MMVR_PERFORMANCE_INTERACTIVE", "MMVR_SCENE_SWEEP",
                              "MMVR_NATIVE_STATE_TEST", "MMVR_RENDER_CADENCE_TEST", "MMVR_KAFEI_DRAW_TEST" }) {
        const char* value = std::getenv(flag);
        if (value && std::strcmp(value, "1") == 0)
            return;
    }
    const char* room = std::getenv("MMVR_DEBUG_TEST");
    if (room && std::string(room) == "1")
        return;
    if (play->gameplayFrames == 60) {
        mmvr::SetNativeTestTracking(true);
        Interface_SetTatlCall(play, TATL_STATE_2A);
        mmvr::RequestNativeCapture("native-tatl-prompt");
    }
    if (play->gameplayFrames == 61) {
        mmvr::SetNativeTestTracking(false);
        Interface_SetTatlCall(play, TATL_STATE_2C);
    }
}
extern "C" int MMVR_NormalPause(void) {
    if (!gPlayState)
        return false;
    auto state = gPlayState->pauseCtx.state;
    return (state >= PAUSE_STATE_OPENING_0 && state <= PAUSE_STATE_SAVEPROMPT) || state == PAUSE_STATE_UNPAUSE_SETUP ||
           state == PAUSE_STATE_UNPAUSE_CLOSE;
}
extern "C" void MMVR_SetScreenScaleCommands(const void* overlay, const void* world) {
    mmvr::SetScreenScaleCommands(overlay, world);
}
extern "C" const void* MMVR_ScreenScaleWorldCommands(void) { return mmvr::ScreenScaleWorldCommands(); }
extern "C" void MMVR_SetMonochromeCommands(const void* overlay, const void* world) {
    mmvr::SetMonochromeCommands(overlay, world);
}
extern "C" void MMVR_SetDialogueCommands(const void* commands, const void* body) {
    mmvr::SetDialogueCommands(commands, body);
}
extern "C" void MMVR_SetPauseCommands(const void* commands) {
    mmvr::SetPauseCommands(commands);
}
extern "C" int MMVR_WorldPause(void) {
    return (mmvr::StereoActive() || NativeTestEnabled()) && MMVR_NormalPause();
}
extern "C" int MMVR_MenuPaused(void) {
#if defined(MMVR_STATE_NATIVE_BACKEND)
    if (mmvrgame::StateTrackingResumePending()) return true;
#endif
    return mmvr::MenuPaused();
}
extern "C" void MMVR_BeforePlayUpdate(PlayState* play) {
#if defined(MMVR_STATE_NATIVE_BACKEND)
    if (mmvrgame::StateTrackingResumePending()) return;
#endif
    if (mmvr::skipDayRequested.exchange(false)) mmvrgame::SkipDebugDay(play);
    if (mmvr::skipTwoHoursRequested.exchange(false)) mmvrgame::SkipDebugHours(play);
    if (mmvr::mainMenuRequested.exchange(false)) {
        settingsDirty = true;
        CommitSettings();
        mmvr::debugReturnRequested = false;
        mmvrgame::ClearItemSelection();
        mmvrgame::ClearBow();
        mmvrgame::ClearCombat();
        mmvrgame::ClearBottle();
        mmvr::ResetCoordinateTracking();
        // Same native transition as the port's file-select command. No implicit save.
        STOP_GAMESTATE(&play->state);
        SET_NEXT_GAMESTATE(&play->state, FileSelect_Init, sizeof(FileSelectState));
        return;
    }
    MMVR_CameraSceneBoundary(play);
    // Changed-only diagnostics distinguish a native lock from a VR pause/focus loss.
    static int previousReason = -1, previousScene = -1;
    const int reason = (mmvr::GetMenu().open ? 1 : 0) | (!mmvr::InputFocused() ? 2 : 0) | (mmvr::MenuPaused() ? 4 : 0) |
                       (MMVR_NormalPause() ? 8 : 0);
    if (reason != previousReason || previousScene != play->sceneId) {
        std::ofstream("mmvr-pause-state.log", std::ios::app)
            << "scene=" << play->sceneId << " frame=" << play->gameplayFrames << " reason=" << reason
            << " transition=" << int(play->transitionMode) << " csAction=" << int(GET_PLAYER(play)->csAction)
            << " flags=" << GET_PLAYER(play)->stateFlags1 << "\n";
        previousReason = reason;
        previousScene = play->sceneId;
    }
    MMVR_DebugRoomUpdate(play);
    MMVR_UpdateTownAudio(play);
    if (NativeTestEnabled())
        mmvrgame::ProcessSwordEquip(play, true);
    mmvrgame::ProcessMasks(play);
    MMVR_ProcessInteractions(play);
    int slot = mmvr::TakeSelectedSlot();
    const bool exchange = mmvrgame::ExchangePromptActive(play);
    if ((MMVR_ItemPresentationActive(GET_PLAYER(play)) && !exchange) || mmvrgame::NativeViewfinderActive(play))
        return;
    if (slot == -2 && mmvr::FirstPersonRequested() && mmvr::InputFocused() && !mmvr::MenuPaused() &&
        play->pauseCtx.state == PAUSE_STATE_OFF && play->csCtx.state == CS_STATE_IDLE &&
        play->msgCtx.msgMode == MSGMODE_NONE && GET_PLAYER(play)->csAction == PLAYER_CSACTION_NONE &&
        !(GET_PLAYER(play)->stateFlags2 & PLAYER_STATE2_USING_OCARINA)) {
        mmvrgame::ClearItemSelection();
        mmvrgame::ClearBow();
        mmvrgame::ClearCombat();
        MMVR_PlayerEmptyHands(play, GET_PLAYER(play));
        return;
    }
    if (slot < 0 || slot > 48 || mmvr::MenuPaused() || play->pauseCtx.state != PAUSE_STATE_OFF)
        return;
    auto* player = GET_PLAYER(play);
    if (!player || (!exchange && player->csAction != PLAYER_CSACTION_NONE) ||
        (player->stateFlags2 & PLAYER_STATE2_USING_OCARINA))
        return;
    int item = SlotItem(play, slot);
    if (item == ITEM_NONE || item >= 131)
        return;
    if (mmvr::FirstPersonRequested() && !mmvrgame::ItemAllowed(player, item)) {
        Audio_PlaySfx(NA_SE_SY_ERROR);
        return;
    }

    if (exchange && slot == 48)
        return; // Equipment is not a native inventory offer.
    if (slot == 48) {
        if (mmvrgame::SelectItem(play, slot, item)) {
            mmvr::ConfirmSelectedItem();
            return;
        }
        if (mmvr::FirstPersonRequested() && mmvr::InputFocused() && item >= ITEM_SWORD_KOKIRI &&
            item <= ITEM_SWORD_GILDED) {
            MMVR_PlayerEquipSword(play, player, static_cast<ItemId>(item));
            mmvr::ConfirmSelectedItem();
        }
        return;
    }
    // Wheel selection assigns and readies equipment; consumables wait for the item trigger.
    BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_C_DOWN) = item;
    C_SLOT_EQUIP(0, EQUIP_SLOT_C_DOWN) = slot;
    Interface_LoadItemIcon(play, EQUIP_SLOT_C_DOWN);
    mmvr::ConfirmSelectedItem();
    if (!mmvr::FirstPersonSelected())
        return; // Native C-down activation comes from the right trigger or gamepad.
    if (mmvrgame::SelectItem(play, slot, item))
        return;
    if (item == ITEM_BOTTLE && mmvr::FirstPersonRequested() && mmvr::InputFocused() &&
        mmvr::GetSettings().Get(mmvr::Setting::PhysicalBottle) > .5f) {
        player->heldItemButton = EQUIP_SLOT_C_DOWN;
        MMVR_PlayerEquipEmptyBottle(play, player);
        return;
    }
    if (item == ITEM_HOOKSHOT && mmvr::FirstPersonRequested() && mmvr::InputFocused()) {
        player->heldItemButton = EQUIP_SLOT_C_DOWN;
        if (Player_IsHoldingHookshot(player))
            MMVR_UseHookshot(play, player);
        else
            MMVR_PlayerEquipHookshot(play, player);
        return;
    }
    auto& input = *CONTROLLER1(&play->state);
    input.press.button |= BTN_CDOWN;
    input.cur.button |= BTN_CDOWN;
    // Sword selection equips; only a qualified physical stroke may request an attack.
    if (item == ITEM_SWORD_GREAT_FAIRY)
        mmvrgame::ProcessSwordEquip(play, mmvr::FirstPersonRequested() && mmvr::InputFocused());
    mmvrgame::ProcessBowInput(play);
}
#endif
