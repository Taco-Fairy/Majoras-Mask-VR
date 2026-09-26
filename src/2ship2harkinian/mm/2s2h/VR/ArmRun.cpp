#ifdef MMVR_ENABLE
#include "ArmRun.h"
#include "NativeForms.h"
#include "NativeClimbing.h"
#include "arm_run.h"
#include "runtime.h"
#include "ui.h"
#include <chrono>
#include <fstream>
extern "C" {
#include "global.h"
}
namespace {
mmvr::ArmRunGesture gesture;
Player* owner = nullptr;
int scene = -1, form = -1;
double sampleTime = 0;
std::chrono::steady_clock::time_point sampled;
// Private-preview sampling exposes qualification and actual movement without
// logging at render cadence or changing the player's HUD.
void Diagnose(const mmvr::TrackingFrame& frame, Player* p, bool eligible) {
    if constexpr (!mmvr::PrivateDebugTools) return;
    static auto previous = std::chrono::steady_clock::time_point{};
    auto now = std::chrono::steady_clock::now();
    if (now - previous < std::chrono::seconds(1)) return;
    previous = now;
    static std::ofstream log("mmvr-arm-run.csv", std::ios::app);
    log << frame.timeSeconds << ",form=" << (p ? int(p->transformation) : -1)
        << ",eligible=" << eligible << ",tracked=" << frame.handTracked[0] << frame.handTracked[1]
        << ",ground=" << (p ? !!(p->actor.bgCheckFlags & BGCHECKFLAG_GROUND) : false)
        << ",physical=" << mmvr::PhysicalActionsAllowed()
        << ",boost=" << (eligible ? (gesture.Multiplier(frame.timeSeconds)>1.f ? 1.f+mmvr::GetSettings().Get(mmvr::Setting::PhysicalRunBoost)/100.f : 1.f) : 1.f)
        << ",speed=" << (p ? p->speedXZ : 0.f) << "\n";

}
} // namespace
namespace mmvrgame {
void ClearArmRun() {
    gesture.Reset();
    owner = nullptr;
    sampleTime = 0;
}
void UpdateArmRun(const mmvr::TrackingFrame& frame) {
    auto* play = gPlayState;
    auto* p = play ? GET_PLAYER(play) : nullptr;
    const bool eligible =
        p && mmvr::FirstPersonRequested() && FirstPersonFormAllowed(p) && mmvr::PhysicalActionsAllowed() &&
        !mmvr::MenuPaused() && frame.handValid[0] && frame.handValid[1] && frame.handTracked[0] &&
        frame.handTracked[1] && p->csAction == PLAYER_CSACTION_NONE && play->csCtx.state == CS_STATE_IDLE &&
        play->pauseCtx.state == PAUSE_STATE_OFF && play->msgCtx.msgMode == MSGMODE_NONE &&
        play->transitionTrigger == TRANS_TRIGGER_OFF && !p->rideActor && (p->actor.bgCheckFlags & BGCHECKFLAG_GROUND) &&
        !NativeAbilityOwnsFacing(p) && !MMVR_ClimbingInputContext(play) &&
        !(p->stateFlags1 & (PLAYER_STATE1_TALKING | PLAYER_STATE1_8000000));
    if (!eligible) {
        ClearArmRun();
        Diagnose(frame, p, false);
        return;
    }
    if (owner != p || scene != play->sceneId || form != p->transformation)
        ClearArmRun();
    owner = p;
    scene = play->sceneId;
    form = p->transformation;
    sampleTime = frame.timeSeconds;
    sampled = std::chrono::steady_clock::now();
    std::array<std::array<float, 3>, 2> positions{};
    for (int hand = 0; hand < 2; ++hand)
        positions[hand] = { frame.hands[hand].position.x, frame.hands[hand].position.y, frame.hands[hand].position.z };
    gesture.Update(sampleTime, frame.epoch, frame.originEpoch, eligible, positions);
    Diagnose(frame, p, true);
}
float ArmRunMultiplier() {
    if (!gPlayState || !owner || owner != GET_PLAYER(gPlayState) || scene != gPlayState->sceneId ||
        form != owner->transformation || std::chrono::steady_clock::now() - sampled > std::chrono::milliseconds(150))
        return 1.f;
    return gesture.Multiplier(sampleTime)>1.f
        ? 1.f+mmvr::GetSettings().Get(mmvr::Setting::PhysicalRunBoost)/100.f : 1.f;
}
} // namespace mmvrgame
#endif

