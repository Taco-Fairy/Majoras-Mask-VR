#ifdef MMVR_ENABLE
#include "Swimming.h"
#include "FormAim.h"
#include "runtime.h"
#include "swim_direction.h"
extern "C" {
#include "global.h"
void Player_Action_56(Player*, PlayState*);
void Player_Action_57(Player*, PlayState*);
void Player_Action_58(Player*, PlayState*);
void Player_Action_59(Player*, PlayState*);
}
namespace {
s16 Angle(float x) {
    return s16(int32_t(std::remainder(x, 6.283185307f) * 32768.f / 3.141592654f));
}
} // namespace
extern "C" float MMVR_SwimEyeHeight(Player* p, float modelHeight, float standingHeight) {
    if (!p || p->transformation != PLAYER_FORM_ZORA || !(p->stateFlags1 & PLAYER_STATE1_8000000) ||
        (p->currentBoots >= PLAYER_BOOTS_ZORA_UNDERWATER && (p->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) ||
        !std::isfinite(modelHeight) || modelHeight <= -120.f || modelHeight >= 160.f)
        return standingHeight;
    const auto id = mmvr::Setting::ZoraEyeHeight;
    return modelHeight + mmvr::GetSettings().Get(id) - mmvr::SettingDefinitions[size_t(id)].initial;
}
extern "C" float MMVR_SwimMovementScale(PlayState* play, Player* p, int vertical) {
    if (!play || play != gPlayState || !p || p != GET_PLAYER(play) || !mmvrgame::FormTrackingReady(p) ||
        (p->transformation != PLAYER_FORM_HUMAN && p->transformation != PLAYER_FORM_ZORA) ||
        !(p->stateFlags1 & PLAYER_STATE1_8000000) || p->currentBoots >= PLAYER_BOOTS_ZORA_UNDERWATER)
        return 1.f;
    const bool dash = p->transformation == PLAYER_FORM_ZORA && p->actionFunc == Player_Action_56;
    const bool dive = p->actionFunc == Player_Action_59 && p->av1.actionVar1 == 0 && p->av2.actionVar2 != 0;
    const bool surface = p->actionFunc == Player_Action_57 || p->actionFunc == Player_Action_58;
    if (!(dash || dive || (!vertical && surface))) return 1.f;
    return mmvr::GetSettings().Get(mmvr::Setting::SwimSpeed) * .01f;
}
extern "C" int MMVR_SwimAim(PlayState* play, Player* p, short* yaw, short* pitch) {
    if (!play || play != gPlayState || !p || p != GET_PLAYER(play) || !yaw || !pitch ||
        (p->transformation != PLAYER_FORM_HUMAN && p->transformation != PLAYER_FORM_ZORA) ||
        !(p->stateFlags1 & PLAYER_STATE1_8000000) || !mmvrgame::FormTrackingReady(p) ||
        mmvr::GetSettings().Get(mmvr::Setting::HeadSwim) < .5f)
        return false;
    auto direction =
        mmvr::HeadSwimDirection(mmvrgame::FormHeadPose(), mmvr::GetSettings().Get(mmvr::Setting::SwimPitchLimit));
    if (!direction.valid)
        return false;
    *yaw = Angle(direction.yaw);
    *pitch = Angle(direction.pitch);
    return true;
}
extern "C" int MMVR_SwimDive(PlayState* play, Player* p, float* horizontal, short* yaw, float* vertical) {
    // Keep the native limited dive and automatic recovery. Only its intentional
    // descent is steered; surfacing, water currents and world collision stay native.
    short pitch;
    if (!p || !horizontal || !vertical || *vertical >= 0 || p->actionFunc != Player_Action_59 ||
        p->av1.actionVar1 != 0 || p->av2.actionVar2 == 0 || !MMVR_SwimAim(play, p, yaw, &pitch))
        return false;
    float speed = -*vertical;
    *horizontal = Math_CosS(pitch) * speed;
    *vertical = -Math_SinS(pitch) * speed;
    p->unk_AAA = pitch;
    return true;
}
#endif
