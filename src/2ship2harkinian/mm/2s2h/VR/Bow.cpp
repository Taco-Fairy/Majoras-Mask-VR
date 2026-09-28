#ifdef MMVR_ENABLE
#include "Bow.h"
#include "VehicleCollision.h"
#include "Bombchu.h"
#include "Carry.h"
#include "FormPresentation.h"
#include "2s2h/Enhancements/FrameInterpolation/FrameInterpolation.h"
#include "NativeCombat.h"
#include "Interactions.h"
#include "bow_draw.h"
#include "bow_aim.h"
#include "AimReticle.h"
#include "runtime.h"
#include "ui.h"
#include <fstream>
#include <cstring>
extern "C" {
#include "global.h"
#include "objects/object_link_child/object_link_child.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "overlays/actors/ovl_En_Arrow/z_en_arrow.h"
s32 func_808305BC(PlayState*, Player*, ItemId*, ArrowType*);
}
namespace {
mmvr::BowDraw draw;
bool pending = false;
Vec3f shotPosition{};
Vec3s shotRotation{};
float shotPower = 0;
mmvr::Matrix arrowPose{}, reticlePose{};
mmvr::Matrix bowModel{};
Vec3f stringHand{};
float stringPull = 0;
bool bowPoseValid = false;
Player* owner = nullptr;
int scene = -1, action = -1, controller = -1;
uint64_t epoch = 0;
s16 Angle(float value) {
    return static_cast<s16>(std::lround(value * 32768.f / 3.141592654f));
}
bool BowItem(int item) {
    return (item >= ITEM_BOW && item <= ITEM_ARROW_LIGHT) || (item >= ITEM_BOW_FIRE && item <= ITEM_BOW_LIGHT);
}
Vec3f Point(const mmvr::Matrix& m, float x, float y, float z) {
    Vec3f out{};
    float* p = &out.x;
    for (int c = 0; c < 3; ++c)
        p[c] = x * m.m[0][c] + y * m.m[1][c] + z * m.m[2][c] + m.m[3][c];
    return out;
}
// Private, bounded samples: a visible string does not prove input eligibility.
void LogBowGate(PlayState* play, Player* p, const mmvr::TrackingFrame& frame,
                unsigned gate, float distance=-1, float reach=-1, int bg=-1) {
#ifdef MMVR_LOCAL_TEST_TOOLS
    static double next=0;
    static unsigned samples=0;
    if (!p || samples>=600 || frame.timeSeconds<next ||
        p->heldItemAction<PLAYER_IA_BOW || p->heldItemAction>PLAYER_IA_BOW_LIGHT) return;
    next=frame.timeSeconds+.5; ++samples;
    std::ofstream("mmvr-bow-gates.log",std::ios::app)
        << "scene="<<play->sceneId<<" frame="<<play->gameplayFrames<<" gate="<<gate
        <<" physical="<<mmvr::PhysicalActionsAllowed()<<" eligible="<<mmvrgame::InteractionsEligible(play,p)
        <<" msg="<<int(play->msgCtx.msgMode)<<" cs="<<int(p->csAction)<<" script="<<int(play->csCtx.state)
        <<" flags="<<p->stateFlags1<<" ammo="<<int(play->bButtonAmmoPlusOne)
        <<" held="<<(p->heldActor?p->heldActor->id:-1)<<" initialized="<<(p->heldActor?!p->heldActor->init:0)
        <<" trigger="<<frame.triggers[mmvr::SwordController(mmvr::GetSettings())]
        <<" distance="<<distance<<" reach="<<reach<<" bg="<<bg
        <<" scale="<<frame.trackingScale<<" drawing="<<draw.drawing<<" epoch="<<frame.epoch<<"\n";
#endif
}
} // namespace
extern "C" int MMVR_IndependentBow(Player* p) {
    return p && mmvr::FirstPersonRequested() && p->transformation == PLAYER_FORM_HUMAN &&
           mmvr::GetSettings().Get(mmvr::Setting::PhysicalBow) > .5f && p->heldItemAction >= PLAYER_IA_BOW &&
           p->heldItemAction <= PLAYER_IA_BOW_LIGHT;
}
namespace mmvrgame {
bool BowHeld() {
    return gPlayState && MMVR_IndependentBow(GET_PLAYER(gPlayState));
}
void ClearBow() {
    draw.Cancel();
    pending = false;
    bowPoseValid = false;
    stringPull = 0;
    arrowPose = reticlePose = {};
}
void UpdateBow(const mmvr::TrackingFrame& frame, const mmvr::Matrix& view, const mmvr::Matrix& relativeHead,
               const mmvr::Matrix& model) {
    auto* play = gPlayState;
    auto* p = play ? GET_PLAYER(play) : nullptr;
    const auto& settings = mmvr::GetSettings();
    int dominant = mmvr::SwordController(settings), off = 1 - dominant;
    if (!p || owner != p || scene != play->sceneId || epoch != frame.epoch || action != p->heldItemAction ||
        controller != dominant) {
        ClearBow();
        owner = p;
        scene = play ? play->sceneId : -1;
        epoch = frame.epoch;
        action = p ? p->heldItemAction : -1;
        controller = dominant;
    }
    // A blocked shot must not remove the bow's string. Keep a relaxed pose
    // whenever the bow hand is tracked, including scripted vehicle sequences.
    bowModel = model;
    bowPoseValid = BowHeld() && frame.handTracked[off];
    const unsigned gate = (!BowHeld()?1u:0u) | (!mmvr::PhysicalActionsAllowed()?2u:0u) |
        (!InteractionsEligible(play,p)?4u:0u) | (!frame.handTracked[off]?8u:0u) |
        (!frame.handTracked[dominant]?16u:0u) | (!frame.aimValid[off]?32u:0u) |
        (p && p->heldActor && !CarriedObject(p) && !MMVR_BowHasNockedArrow(p)?64u:0u) |
        (play && play->msgCtx.msgMode!=MSGMODE_NONE?128u:0u);
    if (gate) {
        LogBowGate(play,p,frame,gate);
        ClearBow();
        bowPoseValid = BowHeld() && frame.handTracked[off];
        return;
    }
    auto worldPose = [&](const XrPosef& pose) {
        auto m = mmvr::Multiply(mmvr::PoseMatrix(pose), mmvr::InversePose(mmvr::PoseMatrix(frame.origin)));
        m.m[3][0] = (m.m[3][0] - relativeHead.m[3][0]) * 40;
        m.m[3][1] *= 40;
        m.m[3][2] = (m.m[3][2] - relativeHead.m[3][2]) * 40;
        return mmvr::Multiply(m, view);
    };
    auto aim = worldPose(frame.aims[off]);
    auto hand = worldPose(frame.hands[dominant]);
    // Native bow string attaches just behind the authored right-hand grip.
    auto anchor = Point(model, -35, -395, 0), pullHand = Point(hand, 0, 0, 0);
    Vec3f delta{ anchor.x - pullHand.x, anchor.y - pullHand.y, anchor.z - pullHand.z };
    float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z) / 40;
    float backwards = (-aim.m[2][0] * delta.x - aim.m[2][1] * delta.y - aim.m[2][2] * delta.z) / 40;
    auto head = InteractionHead();
    Vec3f from{ head.x, head.y, head.z }, hit;
    CollisionPoly* poly = nullptr;
    int bg = BGCHECK_SCENE;
    float reach = std::sqrt((anchor.x - head.x) * (anchor.x - head.x) + (anchor.y - head.y) * (anchor.y - head.y) +
                            (anchor.z - head.z) * (anchor.z - head.z)) /
                  40;
    // During Cremia's escort the cart surrounds the player. Ignore only the
    // supporting cart, not scene walls or other dynamic scenery.
    Actor* obstructionOwner = &p->actor;
    if (p->actor.floorBgId != BGCHECK_SCENE) {
        auto* support = DynaPoly_GetActor(&play->colCtx, p->actor.floorBgId);
        if (support && VehicleCollisionExcluded(play, p, &support->actor))
            obstructionOwner = &support->actor;
    }
    // The escort writes Link's position directly, so floorBgId can still refer
    // to the road below. The active native cart is authoritative in that case.
    if (play->bButtonAmmoPlusOne > 0 && obstructionOwner == &p->actor) {
        for (auto* actor=play->actorCtx.actorLists[ACTORCAT_NPC].first; actor; actor=actor->next) {
            if (actor->id != ACTOR_OBJ_UM || actor->init || !actor->update) continue;
            if (VehicleCollisionExcluded(play, p, actor)) {
                obstructionOwner=actor;
                break;
            }
        }
    }
    bool valid =
        reach < settings.Get(mmvr::Setting::AimReach)*frame.trackingScale &&
        !BgCheck_EntityLineTest2(&play->colCtx, &from, &anchor, &hit, &poly, true, true, true, true, &bg, obstructionOwner);
    ItemId ammoItem; ArrowType ammoType;
    const bool hasArrow = MMVR_BowHasNockedArrow(p) || func_808305BC(play, p, &ammoItem, &ammoType) > 0;
    const bool freeDrawHand = !CarriedObject(p);
    if (!freeDrawHand) { draw.Cancel(); pending=false; }
    bool fire = draw.Update(frame.timeSeconds, frame.epoch, valid && freeDrawHand && hasArrow, frame.triggers[dominant], distance,
                            std::min(backwards, mmvr::LimitedArrowDraw(100) / 40 * frame.trackingScale),
                            settings.Get(mmvr::Setting::BowGrabDistance)*frame.trackingScale, settings.Get(mmvr::Setting::BowMinDraw)*frame.trackingScale,
                            std::min(settings.Get(mmvr::Setting::BowFullDraw), mmvr::LimitedArrowDraw(100) / 40)*frame.trackingScale);
    LogBowGate(play,p,frame,(!valid?256u:0u)|(!freeDrawHand?512u:0u)|(!hasArrow?1024u:0u),distance,reach,bg);
    bowModel = model;
    stringHand = pullHand;
    bowPoseValid = true;
    stringPull = draw.drawing ? draw.pull : 0;
    auto direction = mmvr::CalibrateBowAim(
        (draw.drawing || fire) && distance > .01f ? XrVector3f{ delta.x, delta.y, delta.z }
                                                  : XrVector3f{ -aim.m[2][0], -aim.m[2][1], -aim.m[2][2] },
        settings.Get(mmvr::Setting::BowAimYaw), settings.Get(mmvr::Setting::BowAimPitch));
    const auto stringDirection = direction;
    if (settings.Get(mmvr::Setting::HeadItemAim) > .5f) {
        const auto headAim = worldPose(frame.head);
        direction = { -headAim.m[2][0], -headAim.m[2][1], -headAim.m[2][2] };
    }
    const float visualDraw = mmvr::LimitedArrowDraw(distance * 40 / frame.trackingScale) * frame.trackingScale;
    if (draw.drawing)
        stringHand = { anchor.x - stringDirection.x * visualDraw, anchor.y - stringDirection.y * visualDraw,
                       anchor.z - stringDirection.z * visualDraw };
    arrowPose = draw.drawing && valid ? mmvr::ArrowPose(direction, { stringHand.x, stringHand.y, stringHand.z }, frame.trackingScale)
                                      : mmvr::Matrix{};
    reticlePose = {};
    if (draw.drawing && hasArrow && valid && settings.Get(mmvr::Setting::BowReticle) > .5f) {
        reticlePose = AimReticle(play, p, anchor, direction, head, 4000, 800, true);
    }
    if (fire && distance > .001f) {
        shotPosition = anchor;
        shotRotation = { Angle(-std::atan2(direction.y, std::hypot(direction.x, direction.z))),
                         Angle(std::atan2(direction.x, direction.z)), 0 };
        shotPower = draw.pull;
        pending = true;
    }
}
void ProcessBowInput(PlayState* play) {
    auto* p = GET_PLAYER(play);
    bool enabled = mmvr::FirstPersonRequested() && mmvr::InputFocused() && !mmvr::MenuPaused() &&
                   mmvr::GetSettings().Get(mmvr::Setting::PhysicalBow) > .5f &&
                   p->transformation == PLAYER_FORM_HUMAN && play->pauseCtx.state == PAUSE_STATE_OFF &&
                   play->msgCtx.msgMode == MSGMODE_NONE && p->csAction == PLAYER_CSACTION_NONE &&
                   play->csCtx.state == CS_STATE_IDLE;
    if (!enabled) {
        ClearBow();
        return;
    }
    auto& input = *CONTROLLER1(&play->state);
    const uint16_t buttons[] = { BTN_B, BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT };
    for (int slot = 0; slot < 4; ++slot) {
        int item = Player_GetItemOnButton(play, p, static_cast<EquipSlot>(slot));
        if (BowItem(item)) {
            if ((input.press.button & buttons[slot]) && p->heldItemId != item) {
                ClearBow();
                MMVR_PlayerEquipBow(play, p, item);
            }
            input.press.button &= ~buttons[slot];
            input.cur.button &= ~buttons[slot];
        }
    }
    if (BowHeld()) {
        if (pending) {
            pending = false;
            if (InteractionsEligible(play, p) && mmvr::PhysicalActionsAllowed() &&
                MMVR_FireBow(play, p, &shotPosition.x, &shotRotation.x, shotPower)) {
                mmvr::HapticPulse(mmvr::SwordController(mmvr::GetSettings()), .45f);
                mmvr::HapticPulse(1 - mmvr::SwordController(mmvr::GetSettings()), .3f);
                if (mmvr::GetSettings().Get(mmvr::Setting::SwordDiagnostics) > .5f)
                    std::ofstream("mmvr-combat.log", std::ios::app)
                        << "bow-shot action=" << int(p->heldItemAction) << " draw=" << shotPower
                        << " yaw=" << shotRotation.y << " pitch=" << shotRotation.x << "\n";
            }
        }
    } else
        ClearBow();
}
mmvr::Matrix BowStringPose() {
    if (!BowHeld() || !bowPoseValid)
        return {};
    auto pose = bowModel;
    auto anchor = Point(bowModel, -35, -395, 0);
    for (int c = 0; c < 3; ++c) {
        pose.m[3][c] = (&anchor.x)[c];
        pose.m[1][c] = draw.drawing ? ((&anchor.x)[c] - (&stringHand.x)[c]) / 800.f : bowModel.m[1][c] * .02f;
    }
    // Authored string endpoints are (+/-1010,0,0); center is (0,-800,0).
    // This affine basis keeps both ends on the bow and the center on the draw hand.
    return pose;
}
mmvr::Matrix BowArrowPose() {
    return arrowPose;
}
void UpdateHookshotReticle() {
    auto* play = gPlayState;
    auto* p = play ? GET_PLAYER(play) : nullptr;
    mmvr::Matrix muzzle;
    if (!MMVR_IndependentHookshot(p))
        return;
    reticlePose = {};
    if (mmvr::GetSettings().Get(mmvr::Setting::HookshotReticle) < .5f || !mmvr::PhysicalActionsAllowed() ||
        play->msgCtx.msgMode != MSGMODE_NONE ||
        !TrackedMuzzle(play, p, muzzle))
        return;
    // Native human reticle range is 77600 model units at scale .01.
    reticlePose = AimReticle(play, p, { muzzle.m[3][0], muzzle.m[3][1], muzzle.m[3][2] },
                             { -muzzle.m[2][0], -muzzle.m[2][1], -muzzle.m[2][2] }, InteractionHead(), 776, 776);
}
mmvr::Matrix ItemReticlePose() {
    return reticlePose;
}
void DrawTrackedItems(PlayState* play) {
    // Addresses belong to this display-list frame, never the previously held item.
    mmvr::SetBowArrowMatrix(nullptr);
    mmvr::SetBowStringMatrix(nullptr);
    mmvr::SetItemReticleMatrix(nullptr);
    // Emit the display-list references even if the previous tracking sample
    // could not fire. Their matrices are filled from the current XR sample.
    const bool bow = BowHeld();
    if (!bow && !MMVR_IndependentHookshot(GET_PLAYER(play)) && !FormReticleVisible(GET_PLAYER(play)) &&
        !BombchuReticleVisible(GET_PLAYER(play)))
        return;
    GraphicsContext* __gfxCtx = play->state.gfxCtx;
    ::FrameInterpolation_RecordOpenChild(__FILE__, __LINE__);
    Gfx* refs[3];
    Gfx values[3];
    Graph_OpenDisps(refs, values, __gfxCtx, __FILE__, __LINE__);
    if (bow) {
        // Reuse the game's bow-string resource, with the same physical offhand pose.
        auto* matrix = (Mtx*)GRAPH_ALLOC(play->state.gfxCtx, sizeof(Mtx));
        std::memset(matrix, 0, sizeof(Mtx));
        mmvr::SetBowStringMatrix(matrix);
        Gfx_SetupDL25_Xlu(play->state.gfxCtx);
        gSPMatrix(POLY_XLU_DISP++, matrix, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        bool mirroredString = mmvr::SwordController(mmvr::GetSettings()) == 1;
        if (mirroredString) {
            gSPSetExtraGeometryMode(POLY_XLU_DISP++, G_EX_INVERT_CULLING);
        }
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)object_link_child_DL_017818);
        if (mirroredString) {
            gSPClearExtraGeometryMode(POLY_XLU_DISP++, G_EX_INVERT_CULLING);
        }
        auto* arrow = (Mtx*)GRAPH_ALLOC(__gfxCtx, sizeof(Mtx));
        std::memset(arrow, 0, sizeof(Mtx));
        mmvr::SetBowArrowMatrix(arrow);
        Gfx_SetupDL25_Opa(__gfxCtx);
        gSPMatrix(POLY_OPA_DISP++, arrow, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gameplay_keep_DL_013FF0);
    }
    ::FrameInterpolation_RecordCloseChild();
    Graph_CloseDisps(refs, values, __gfxCtx, __FILE__, __LINE__);
}
void DrawAimReticle(PlayState* play) {
    mmvr::SetItemReticleMatrix(nullptr);
    if (!mmvr::FirstPersonRequested() || !mmvr::PhysicalActionsAllowed() || play->pauseCtx.state != PAUSE_STATE_OFF ||
        play->msgCtx.msgMode != MSGMODE_NONE)
        return;
    auto* p = GET_PLAYER(play);
    if (!BowHeld() && !MMVR_IndependentHookshot(p) && !FormReticleVisible(p) && !BombchuReticleVisible(p))
        return;
    GraphicsContext* __gfxCtx = play->state.gfxCtx;
    ::FrameInterpolation_RecordOpenChild(__FILE__, __LINE__);
    Gfx* refs[3];
    Gfx values[3];
    Graph_OpenDisps(refs, values, __gfxCtx, __FILE__, __LINE__);
    auto* reticle = (Mtx*)GRAPH_ALLOC(__gfxCtx, sizeof(Mtx));
    std::memset(reticle, 0, sizeof(Mtx));
    mmvr::SetItemReticleMatrix(reticle);
    static Vtx cross[8] = { { { { -3, -1, 0 }, 0, { 0, 0 }, { 255, 235, 145, 255 } } },
                            { { { 3, -1, 0 }, 0, { 0, 0 }, { 255, 235, 145, 255 } } },
                            { { { 3, 1, 0 }, 0, { 0, 0 }, { 255, 235, 145, 255 } } },
                            { { { -3, 1, 0 }, 0, { 0, 0 }, { 255, 235, 145, 255 } } },
                            { { { -1, -3, 0 }, 0, { 0, 0 }, { 255, 235, 145, 255 } } },
                            { { { 1, -3, 0 }, 0, { 0, 0 }, { 255, 235, 145, 255 } } },
                            { { { 1, 3, 0 }, 0, { 0, 0 }, { 255, 235, 145, 255 } } },
                            { { { -1, 3, 0 }, 0, { 0, 0 }, { 255, 235, 145, 255 } } } };
    Gfx_SetupDL25_Xlu(__gfxCtx);
    gSPClearGeometryMode(POLY_XLU_DISP++, G_ZBUFFER | G_LIGHTING | G_FOG | G_CULL_BACK | G_CULL_FRONT);
    gSPTexture(POLY_XLU_DISP++, 0, 0, 0, 0, G_OFF);
    gDPSetCombineMode(POLY_XLU_DISP++, G_CC_SHADE, G_CC_SHADE);
    // A single-texture reticle must not read TEXEL1 through a second cycle.
    // That tile belongs to the preceding actor and can crop/corrupt this dot.
    gDPSetCycleType(POLY_XLU_DISP++, G_CYC_1CYCLE);
    gDPSetRenderMode(POLY_XLU_DISP++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);
    gSPMatrix(POLY_XLU_DISP++, reticle, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    if (mmvr::GetSettings().Get(mmvr::Setting::BetaReticle) > .5f) {
        gSPVertex(POLY_XLU_DISP++, (uintptr_t)cross, 8, 0);
        gSP2Triangles(POLY_XLU_DISP++, 0, 1, 2, 0, 0, 2, 3, 0);
        gSP2Triangles(POLY_XLU_DISP++, 4, 5, 6, 0, 4, 6, 7, 0);
    } else {
        static Vtx dot[4] = {
            {{{-3,-3,0},0,{0,2016},{255,255,255,255}}},
            {{{ 3,-3,0},0,{2016,2016},{255,255,255,255}}},
            {{{ 3, 3,0},0,{2016,0},{255,255,255,255}}},
            {{{-3, 3,0},0,{0,0},{255,255,255,255}}}
        };
        gSPTexture(POLY_XLU_DISP++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
        gDPLoadTextureBlock(POLY_XLU_DISP++, gHookshotReticleTex, G_IM_FMT_I, G_IM_SIZ_8b, 64, 64, 0,
                           G_TX_CLAMP, G_TX_CLAMP, 6, 6, G_TX_NOLOD, G_TX_NOLOD);
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 0, 0, 255);
        gDPSetCombineMode(POLY_XLU_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
        gSPVertex(POLY_XLU_DISP++, (uintptr_t)dot, 4, 0);
        gSP2Triangles(POLY_XLU_DISP++, 0, 1, 2, 0, 0, 2, 3, 0);
    }
    ::FrameInterpolation_RecordCloseChild();
    Graph_CloseDisps(refs, values, __gfxCtx, __FILE__, __LINE__);
}

} // namespace mmvrgame
extern "C" void MMVR_DrawAimReticle(PlayState* play) {
    mmvrgame::DrawAimReticle(play);
}
#endif

#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
#include "NativeStateFields.h"
extern "C" void MMVR_VisitVrBowState(MMVR_StateSink* sink) {
    mmvrgame::NativeStateField(sink,"vr/bow/draw",draw);
    mmvrgame::NativeStateField(sink,"vr/bow/pending",pending);
    mmvrgame::NativeStateField(sink,"vr/bow/shotPosition",shotPosition);
    mmvrgame::NativeStateField(sink,"vr/bow/shotRotation",shotRotation);
    mmvrgame::NativeStateField(sink,"vr/bow/shotPower",shotPower);
    mmvrgame::NativeStateField(sink,"vr/bow/arrowPose",arrowPose);
    mmvrgame::NativeStateField(sink,"vr/bow/reticlePose",reticlePose);
    mmvrgame::NativeStateField(sink,"vr/bow/bowModel",bowModel);
    mmvrgame::NativeStateField(sink,"vr/bow/stringHand",stringHand);
    mmvrgame::NativeStateField(sink,"vr/bow/stringPull",stringPull);
    mmvrgame::NativeStateField(sink,"vr/bow/bowPoseValid",bowPoseValid);
    mmvrgame::NativeStateField(sink,"vr/bow/owner",owner);
    mmvrgame::NativeStateField(sink,"vr/bow/scene",scene);
    mmvrgame::NativeStateField(sink,"vr/bow/action",action);
    mmvrgame::NativeStateField(sink,"vr/bow/controller",controller);
    mmvrgame::NativeStateField(sink,"vr/bow/epoch",epoch);
}
#endif

#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
#include "NativeTrackingResume.h"
namespace mmvrgame {
void RebaseBowTracking(const mmvr::TrackingFrame& f) {
    const int dominant=mmvr::SwordController(mmvr::GetSettings());
    draw.Rebase(draw.SampleTime(),f.timeSeconds,f.epoch,
                f.handTracked[dominant]&&f.triggers[dominant]>=.25f);
    auto* play=gPlayState;auto* p=play?GET_PLAYER(play):nullptr;
    owner=p;scene=play?play->sceneId:-1;action=p?p->heldItemAction:-1;
    controller=dominant;epoch=f.epoch;
    // pending/shotPosition/shotRotation are already issued shots, not input.
    bowPoseValid=false;arrowPose=reticlePose={};
}
}
#endif
