#pragma once
#include "2s2h/ShipInit.hpp"
// Existing save-editor time adjustment refreshes EnTest4's bell/day state.
void UpdateGameTime(u16 gameTime);
extern "C" void Interface_NewDay(PlayState* play, s32 day);
namespace mmvrgame {
inline void SyncDebugCutsceneSkips() {
    const bool enabled = mmvr::GetSettings().Get(mmvr::Setting::DebugSkipCutscenes) > .5f;
    static int previous = -1;
    if (previous == int(enabled)) return;
    previous = int(enabled);
    // Use the port's scene-specific skips, which preserve rewards and quest flags.
    // Persist the previous native choices so turning this master option off
    // restores them even after relaunching while the master switch is enabled.
    constexpr const char* keys[] = {
        "gEnhancements.Cutscenes.SkipEntranceCutscenes",
        "gEnhancements.Cutscenes.SkipStoryCutscenes",
        "gEnhancements.Cutscenes.SkipEnemyCutscenes",
        "gEnhancements.Cutscenes.SkipOnePointCutscenes",
        "gEnhancements.Cutscenes.SkipGetItemCutscenes",
        "gEnhancements.Songs.SkipSoTCutscenes",
        "gEnhancements.Songs.SkipSoaringCutscene"
    };
    bool changed = false;
    for (size_t i = 0; i < std::size(keys); ++i) {
        const std::string backup = "gVR.DebugCutsceneBackup.Option" + std::to_string(i);
        if (enabled) {
            if (!CVarGet(backup.c_str())) CVarSetInteger(backup.c_str(), CVarGetInteger(keys[i], 0));
            CVarSetInteger(keys[i], i == 4 ? 3 : 1);
        } else if (CVarGet(backup.c_str())) {
            CVarSetInteger(keys[i], CVarGetInteger(backup.c_str(), 0));
            CVarClear(backup.c_str());
        } else continue;
        ShipInit::Init(keys[i]);
        changed = true;
    }
    if (changed) CVarSave();
}
inline bool CanSkipDebugDay(PlayState* play) {
    return mmvr::PrivateDebugTools && play && GET_PLAYER(play) && gSaveContext.save.day >= 1 && gSaveContext.save.day < 3 &&
        play->transitionTrigger != TRANS_TRIGGER_START && play->transitionMode == TRANS_MODE_OFF &&
        play->pauseCtx.state == PAUSE_STATE_OFF && !Play_InCsMode(play) &&
        Message_GetState(&play->msgCtx) == TEXT_STATE_NONE;
}
inline void SkipDebugDay(PlayState* play) {
    if (!CanSkipDebugDay(play)) return;
    const u16 time = CURRENT_TIME;
    ++gSaveContext.save.day;
    gSaveContext.save.eventDayCount = CURRENT_DAY;
    Interface_NewDay(play, CURRENT_DAY);
    // Same day-change refresh used by the native save editor. Keep progress,
    // position, inventory and time of day; do not reset the three-day cycle.
    Environment_NewDay(&play->envCtx);
    UpdateGameTime(time);
    // A second activation can arrive while the VR menu keeps native Play_Update
    // paused. Preserve the pending reload instead of toggling it back off.
    if (play->numSetupActors < 0) play->numSetupActors = -play->numSetupActors;
}
inline bool CanSkipDebugHours(PlayState* play) {
    if (!mmvr::PrivateDebugTools || !play || !GET_PLAYER(play) || CURRENT_DAY < 1 || CURRENT_DAY > 3 ||
        play->transitionTrigger == TRANS_TRIGGER_START || play->transitionMode != TRANS_MODE_OFF ||
        play->pauseCtx.state != PAUSE_STATE_OFF || Play_InCsMode(play) ||
        Message_GetState(&play->msgCtx) != TEXT_STATE_NONE)
        return false;
    // The Final Day's dawn is the moon-crash boundary, not another playable day.
    return CURRENT_DAY < 3 || CURRENT_TIME < CLOCK_TIME(4, 0) || CURRENT_TIME >= CLOCK_TIME(6, 0);
}
inline void SkipDebugHours(PlayState* play) {
    if (!CanSkipDebugHours(play)) return;
    const u16 before = CURRENT_TIME;
    const u16 after = u16(before + CLOCK_TIME(2, 0));
    const bool dawn = before >= CLOCK_TIME(4, 0) && before < CLOCK_TIME(6, 0);
    // The native time setter toggles numSetupActors when this crosses a day/night
    // boundary. Repeated skips while the menu pauses Play_Update must leave any
    // pending actor reload positive, even if the next skip crosses back.
    const auto night = [](u16 time) { return time > CLOCK_TIME(18, 0) || time < CLOCK_TIME(6, 0); };
    const bool refreshActors = dawn || night(before) != night(after) || play->numSetupActors > 0;
    if (dawn) {
        ++gSaveContext.save.day;
        gSaveContext.save.eventDayCount = CURRENT_DAY;
        Interface_NewDay(play, CURRENT_DAY);
        Environment_NewDay(&play->envCtx);
    }
    UpdateGameTime(after);
    if (refreshActors && play->numSetupActors < 0) play->numSetupActors = -play->numSetupActors;
    gSaveContext.skyboxTime = after;
}
} // namespace mmvrgame
