#pragma once
#include "DebugNativeTriggers.h"
// NativeStateTest.cpp uses this same C-header compatibility wrapper.
extern "C" {
#define this nativeThis
#include "overlays/actors/ovl_Obj_Switch/z_obj_switch.h"
#undef this
}

// Opt-in isolated test: load the authored Woodfall room and let its actual
// crystal actor start csId 1 after a simulated AC collision. No script is
// injected into the cutscene context.
static mmvr::Pad NativeWoodfallCrystalTest(PlayState* play, unsigned tick) {
    static bool started = false, hit = false, passed = false;
    static unsigned sceneTicks = 0;
    static std::ofstream log("native-woodfall-crystal.log");
    mmvr::Pad pad;
    pad.active = true;
    if (!started && tick >= 60) {
        constexpr int woodfallIndex = ARRAY_COUNT(debugNativeTriggers) - 1;
        static_assert(debugNativeTriggers[woodfallIndex].kind == DebugNativeTriggerKind::WoodfallCrystal);
        started = MMVR_DebugNativeTriggerBegin(play, woodfallIndex) != 0;
        log << "portal=" << started << " sourceScene=" << play->sceneId << "\n" << std::flush;
        if (!started) Ship::Context::GetRawInstance()->GetWindow()->Close();
    }
    if (!started || play->sceneId != SCENE_MITURIN || play->transitionTrigger != TRANS_TRIGGER_OFF)
        return pad;
    ++sceneTicks;
    ObjSwitch* crystal = nullptr;
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_SWITCH].first; actor; actor = actor->next) {
        if (actor->id == ACTOR_OBJ_SWITCH && (actor->params & 7) == 3 &&
            ((actor->params >> 8) & 0x7F) == 0x60 && actor->csId == 1) {
            crystal = reinterpret_cast<ObjSwitch*>(actor);
            break;
        }
    }
    if (crystal && !hit && sceneTicks > 30) {
        crystal->colliderJntSph.base.acFlags |= AC_HIT;
        hit = true;
        log << "actor-found=1 csId=" << crystal->dyna.actor.csId << " room=" << int(crystal->dyna.actor.room)
            << " sceneTicks=" << sceneTicks << "\n" << std::flush;
    }
    if (hit && sceneTicks % 15 == 0) {
        const int csId = CutsceneManager_GetCurrentCsId();
        passed |= csId == 1;
        log << "sceneTicks=" << sceneTicks << " csId=" << csId << " csState=" << int(play->csCtx.state)
            << " switch=" << Flags_GetSwitch(play, 0x60) << " passed=" << passed << "\n" << std::flush;
    }
    if (passed || tick > 1200) {
        log << "PASS=" << passed << " actor=" << (crystal != nullptr) << " hit=" << hit << "\n" << std::flush;
        Ship::Context::GetRawInstance()->GetWindow()->Close();
    }
    return pad;
}
