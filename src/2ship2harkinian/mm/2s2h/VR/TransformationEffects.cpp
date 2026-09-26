#ifdef MMVR_ENABLE
#include "TransformationEffects.h"
#include "NativeTrackingResume.h"
#include <cstring>
#include "Camera.h"
#include "FormAim.h"
#include "FormPresentation.h"
#include "ScenePresentation.h"
#include "mask_effects.h"
#include "form_presentation.h"
#include "runtime.h"
#include "ui.h"
namespace mmvrgame { float SpinWorldDarkening(); }
extern "C" {
#include "global.h"
#include "overlays/actors/ovl_Dm_Stk/z_dm_stk.h"
#include "objects/gameplay_keep/gameplay_keep.h"
void Player_Action_86(Player*, PlayState*);
void Player_Action_87(Player*, PlayState*);
void Player_Action_89(Player*, PlayState*);
void Player_Action_90(Player*, PlayState*);
}
#include "BeanPresentation.inc"
extern "C" int MMVR_TransformationStyle(Player* p) {
    if (!p || !mmvr::FirstPersonRequested())
        return 0;
    if (p->actionFunc == Player_Action_89)
        return 5;
    if (p->actionFunc == Player_Action_90)
        return 6;
    if (p->actionFunc != Player_Action_86 && p->actionFunc != Player_Action_87)
        return 0;
    int target = p->actionFunc == Player_Action_86 ? int(GET_PLAYER_FORM) : int(p->transformation);
    return target == PLAYER_FORM_HUMAN ? 6 : target == PLAYER_FORM_FIERCE_DEITY ? 4 : target;
}
extern "C" void MMVR_UpdateTransformationEffects(PlayState* play) {
    auto* p = play ? GET_PLAYER(play) : nullptr;
    int style = MMVR_TransformationStyle(p);
    float envelope = 0;
    if (style) {
        envelope = p->actionFunc == Player_Action_87 ? std::clamp(1.f - p->av1.actionVar1 / 20.f, 0.f, 1.f)
                                                     : std::clamp(p->av1.actionVar1 / 8.f, 0.f, 1.f);
    }
    auto tint = mmvr::MaskEffectTint(style, envelope * mmvr::GetSettings().Get(mmvr::Setting::MaskEffectOpacity));
    if (!style) tint = {0, 0, 0, mmvrgame::SpinWorldDarkening()};
    // Composite a separate black layer, preserving the transformation color.
    const bool targetDim = p && (p->stateFlags1 & PLAYER_STATE1_Z_TARGETING) &&
        mmvr::GetSettings().Get(mmvr::Setting::LockOnDim) > .5f;
    if (targetDim) {
        const float alpha = 1.f - (1.f - tint[3]) * .85f;
        const float colorWeight = alpha > 0 ? tint[3] * .85f / alpha : 0;
        for (int i = 0; i < 3; ++i) tint[i] *= colorWeight;
        tint[3] = alpha;
    }
    mmvr::SetWorldTint(tint);
}
extern "C" void MMVR_DrawTransformationEffects(PlayState* play) {
    auto* p = GET_PLAYER(play);
    int style = MMVR_TransformationStyle(p);
    if (!style || !MMVR_FirstPersonBody())
        return;
    auto head = mmvrgame::FormHeadPose();
    if (!head.m[3][3])
        return;
    float strength = mmvr::GetSettings().Get(mmvr::Setting::MaskParticlesOpacity);
    if (strength <= 0)
        return;
    const int colors[][3] = { { 130, 215, 255 }, { 255, 110, 70 },  { 90, 160, 255 }, { 100, 230, 115 },
                              { 255, 240, 120 }, { 255, 255, 255 }, { 130, 215, 255 } };
    // Explicit translucent flash material forms colored spirals around the face. These are
    // ordinary world geometry, with a late headset anchor and no HUD-opacity dependency.
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    Matrix_Push();
    static Vtx quad[4] = { { { { -32, -32, 0 }, 0, { 0, 2048 }, { 255, 255, 255, 255 } } },
                           { { { 32, -32, 0 }, 0, { 2048, 2048 }, { 255, 255, 255, 255 } } },
                           { { { 32, 32, 0 }, 0, { 2048, 0 }, { 255, 255, 255, 255 } } },
                           { { { -32, 32, 0 }, 0, { 0, 0 }, { 255, 255, 255, 255 } } } };
    // SetupDL25 uses opaque, fogged two-cycle rendering even on POLY_XLU.
    // Own the complete sprite material so zero-intensity texels preserve the world.
    gDPPipeSync(POLY_XLU_DISP++);
    gDPSetCycleType(POLY_XLU_DISP++, G_CYC_1CYCLE);
    gDPSetRenderMode(POLY_XLU_DISP++, G_RM_AA_ZB_XLU_SURF, G_RM_AA_ZB_XLU_SURF2);
    gSPClearGeometryMode(POLY_XLU_DISP++, G_FOG | G_LIGHTING);
    gSPTexture(POLY_XLU_DISP++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
    gDPSetCombineMode(POLY_XLU_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPLoadTextureBlock(POLY_XLU_DISP++, gFlashTex, G_IM_FMT_I, G_IM_SIZ_8b, 64, 64, 0, G_TX_CLAMP, G_TX_CLAMP, 6, 6,
                        G_TX_NOLOD, G_TX_NOLOD);
    gSPClearGeometryMode(POLY_XLU_DISP++, G_CULL_BOTH);
    gSPClearExtraGeometryMode(POLY_XLU_DISP++, G_EX_INVERT_CULLING);
    // A shared head-relative ribbon joins the dots. Two segments per dot keep
    // the spiral smooth without extra textures, draw passes, or particle objects.
    const double sampleTime = mmvrgame::FormTrackingTime();
    struct TrailPoint {
        float x, y, z, flow, shimmer;
    };
    auto point = [&](float phase) {
        float angle = phase * 12.5663706f + float(std::fmod(sampleTime * 3.6, 6.283185307));
        float flow = float(std::fmod(sampleTime * .7 + phase, 1.0));
        if (style != 6)
            flow = 1 - flow;
        float shimmer = .82f + .18f * std::sin(float(std::fmod(sampleTime * 6.0, 6.283185307)) + phase * 18.8495559f);
        float radius = 9 + flow * 18;
        return TrailPoint{ std::cos(angle) * radius, std::sin(angle) * radius, -10 - flow * 40, flow, shimmer };
    };
    Matrix_Put((MtxF*)&head);
    Matrix_Scale(1.f / 128, 1.f / 128, 1.f / 128, MTXMODE_APPLY);
    Mtx* trailMatrix = Matrix_Finalize(play->state.gfxCtx);
    MtxF trailNative;
    Matrix_Get(&trailNative);
    mmvr::Matrix trailModel;
    std::memcpy(&trailModel, &trailNative, sizeof(trailModel));
    mmvr::SetFormEffectMatrix(trailMatrix, mmvr::Multiply(trailModel, mmvr::InversePose(head)), 3.6f, sampleTime);
    gSPMatrix(POLY_XLU_DISP++, trailMatrix, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gDPPipeSync(POLY_XLU_DISP++);
    gDPSetCombineMode(POLY_XLU_DISP++, G_CC_MODULATEIA, G_CC_MODULATEIA);
    auto* ribbon = static_cast<Vtx*>(GRAPH_ALLOC(play->state.gfxCtx, 48 * 4 * sizeof(Vtx)));
    for (int i = 0; i < 48; ++i) {
        const auto a = point(float(i) / 48), b = point(float(i + 1) / 48);
        if (std::abs(a.flow - b.flow) > .5f)
            continue; // Never bridge the spiral's wrap.
        float dx = b.x - a.x, dy = b.y - a.y, len = std::sqrt(dx * dx + dy * dy);
        if (len < .001f)
            continue;
        float nx = -dy / len * .45f, ny = dx / len * .45f;
        Vtx* v = ribbon + i * 4;
        for (int j = 0; j < 4; ++j) {
            const auto& t = j < 2 ? a : b;
            float side = j % 2 ? -1.f : 1.f;
            v[j] = {};
            v[j].v.ob[0] = s16((t.x + side * nx) * 128);
            v[j].v.ob[1] = s16((t.y + side * ny) * 128);
            v[j].v.ob[2] = s16(t.z * 128);
            // Sample the bright central texture strip; soft edges cross the ribbon.
            v[j].v.tc[0] = j < 2 ? 896 : 1152;
            v[j].v.tc[1] = j % 2 ? 2048 : 0;
            for (int c = 0; c < 3; ++c)
                v[j].v.cn[c] = colors[style][c];
            v[j].v.cn[3] = u8(80 * strength * t.shimmer * std::sin(t.flow * 3.14159265f));
        }
        gSPVertex(POLY_XLU_DISP++, reinterpret_cast<uintptr_t>(v), 4, 0);
        gSP2Triangles(POLY_XLU_DISP++, 0, 1, 2, 0, 1, 3, 2, 0);
    }
    gDPPipeSync(POLY_XLU_DISP++);
    gDPSetCombineMode(POLY_XLU_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    for (int i = 0; i < 24; ++i) {
        float phase = float(i) / 24,
              angle = phase * 12.5663706f + float(std::fmod(mmvrgame::FormTrackingTime() * 3.6, 6.283185307));
        float flow = float(std::fmod(mmvrgame::FormTrackingTime() * .7 + phase, 1.0));
        if (style != 6)
            flow = 1 - flow;
        float distance = 10 + flow * 40, radius = 9 + flow * 18;
        Matrix_Put((MtxF*)&head);
        Matrix_Translate(std::cos(angle) * radius, std::sin(angle) * radius, -distance, MTXMODE_APPLY);
        float shimmer = point(phase).shimmer;
        float size = (.06f + .025f * flow) * (.92f + .08f * shimmer);
        Matrix_Scale(size, size, .06f, MTXMODE_APPLY);
        Mtx* matrix = Matrix_Finalize(play->state.gfxCtx);
        MtxF native;
        Matrix_Get(&native);
        mmvr::Matrix model;
        std::memcpy(&model, &native, sizeof(model));
        auto anchor = head;
        mmvr::SetFormEffectMatrix(matrix, mmvr::Multiply(model, mmvr::InversePose(anchor)), 3.6f,
                                  mmvrgame::FormTrackingTime());
        gSPMatrix(POLY_XLU_DISP++, matrix, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, colors[style][0], colors[style][1], colors[style][2],
                        int(220 * strength * shimmer * std::sin(flow * 3.14159265f)));
        gDPSetEnvColor(POLY_XLU_DISP++, colors[style][0] / 5, colors[style][1] / 4, colors[style][2], 128);
        gSPVertex(POLY_XLU_DISP++, reinterpret_cast<uintptr_t>(quad), 4, 0);
        gSP2Triangles(POLY_XLU_DISP++, 0, 1, 2, 0, 0, 2, 3, 0);
    }
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}
// The curse belongs to Skull Kid, not to a scripted camera that can cut away.
// Resolve the live source every draw: no actor pointer or screen matrix survives it.
extern "C" int MMVR_SkullKidEffectAnchor(PlayState* play, float* position) {
    if (!play || !mmvr::FirstPersonRequested() || mmvrgame::SceneView(play) == mmvr::SceneView::Theater)
        return 0;
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_ITEMACTION].first; actor; actor = actor->next) {
        if (actor->id != ACTOR_DM_STK || actor->params != DM_STK_TYPE_SKULL_KID || !actor->update || !actor->draw)
            continue;
        const auto* skullKid = reinterpret_cast<const DmStk*>(actor);
        if (!skullKid->shouldDraw || skullKid->alpha <= 0) continue;
        // headPos is written by the native animated head limb. Do not invent an
        // anchor before that limb has been rendered or after the actor disappears.
        const auto& head = skullKid->headPos;
        if (!std::isfinite(head.x) || !std::isfinite(head.y) || !std::isfinite(head.z) ||
            (head.x == 0 && head.y == 0 && head.z == 0)) continue;
        position[0] = head.x; position[1] = head.y; position[2] = head.z;
        return 1;
    }
    return -1;
}
extern "C" void* MMVR_FinalizeSongEffect(PlayState* play) {
    auto head = mmvrgame::FormHeadPose();
    const bool tracked = MMVR_FirstPersonBody() && head.m[3][3] &&
        mmvr::GetSettings().Get(mmvr::Setting::ComfortHudEffects) < .5f;
    mmvr::Matrix local{};
    if (tracked) {
        MtxF native; Matrix_Get(&native);
        mmvr::Matrix current, camera;
        std::memcpy(&current, &native, sizeof(current));
        std::memcpy(&camera, &play->billboardMtxF, sizeof(camera));
        const auto eye = GET_ACTIVE_CAM(play)->eye;
        camera.m[3][0]=eye.x; camera.m[3][1]=eye.y; camera.m[3][2]=eye.z; camera.m[3][3]=1;
        local = mmvr::Multiply(current, mmvr::InversePose(camera));
        auto world = mmvr::Multiply(local, head);
        Matrix_Put(reinterpret_cast<MtxF*>(&world));
    }
    auto* matrix = Matrix_Finalize(play->state.gfxCtx);
    if (tracked) mmvr::SetFormEffectMatrix(matrix, local, 0, mmvrgame::FormTrackingTime());
    return matrix;
}
#endif

#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
#include "NativeStateFields.h"
namespace { double plantingStateClock=0; }
extern "C" void MMVR_VisitVrCosmeticState(MMVR_StateSink* sink) {
    plantingStateClock=mmvr::PresentationTime();
    mmvrgame::NativeStateField(sink,"vr/presentation/plantingBean",plantingBean);
    sink->pointer(sink->context,&plantingBean.play,0,"vr/presentation/plantingBean.play");
    mmvrgame::NativeStateField(sink,"vr/presentation/plantingClock",plantingStateClock);
}
namespace mmvrgame {
void RebasePresentationClock(double now) {
    if(plantingBean.active)plantingBean.born+=now-plantingStateClock;
    plantingStateClock=now;
}
}
#endif
