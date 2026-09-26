// Only accepted native bean exchanges start this cosmetic arc. Inventory,
// flags, growth and watering remain owned by ObjBean.
#include "BeanPresentation.h"
#include "2s2h/Enhancements/FrameInterpolation/FrameInterpolation.h"
#include "Interactions.h"
extern "C" {
#include "archives/icon_item_static/icon_item_static_yar.h"
}
namespace {
struct PlantingBean {
    PlayState *play = nullptr;
    int scene = -1;
    double born = 0;
    Vec3f from{}, to{};
    bool active = false;
} plantingBean;
} // namespace
extern "C" void MMVR_BeanPlanted(PlayState *play, Actor *soil) {
    plantingBean.active = false;
    if (!play || !soil || !mmvr::FirstPersonRequested())
        return;
    mmvr::Matrix hand;
    if (!mmvrgame::TrackedMaskHand(play, hand, mmvr::SwordController(mmvr::GetSettings())))
        return;
    plantingBean = {
        play, play->sceneId, mmvr::PresentationTime(), {hand.m[3][0], hand.m[3][1], hand.m[3][2]}, soil->world.pos,
        true};
    plantingBean.to.y += 2;
}
extern "C" void MMVR_DrawPlantingBean(PlayState *play) {
#ifdef MMVR_STATE_NATIVE_BACKEND
    if (mmvrgame::StateTrackingResumePending()) return;
#endif
    if (!plantingBean.active)
        return;
    const float t = float((mmvr::PresentationTime() - plantingBean.born) / .65);
    if (play != plantingBean.play || !play || play->sceneId != plantingBean.scene || t < 0 || t >= 1) {
        plantingBean.active = false;
        return;
    }
    Vec3f pos;
    pos.x = plantingBean.from.x + (plantingBean.to.x - plantingBean.from.x) * t;
    pos.z = plantingBean.from.z + (plantingBean.to.z - plantingBean.from.z) * t;
    pos.y = plantingBean.from.y + (plantingBean.to.y - plantingBean.from.y) * t + 18.f * 4 * t * (1 - t);
    static Vtx quad[4] = {{{{-5, -5, 0}, 0, {0, 1024}, {255, 255, 255, 255}}},
                          {{{5, -5, 0}, 0, {1024, 1024}, {255, 255, 255, 255}}},
                          {{{5, 5, 0}, 0, {1024, 0}, {255, 255, 255, 255}}},
                          {{{-5, 5, 0}, 0, {0, 0}, {255, 255, 255, 255}}}};
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    gDPPipeSync(POLY_XLU_DISP++);
    gDPSetCycleType(POLY_XLU_DISP++, G_CYC_1CYCLE);
    gDPSetRenderMode(POLY_XLU_DISP++, G_RM_AA_ZB_XLU_SURF, G_RM_AA_ZB_XLU_SURF2);
    gSPClearGeometryMode(POLY_XLU_DISP++, G_LIGHTING | G_FOG | G_CULL_BACK | G_CULL_FRONT);
    gDPSetCombineMode(POLY_XLU_DISP++, G_CC_MODULATEIA, G_CC_MODULATEIA);
    gDPSetTextureLUT(POLY_XLU_DISP++, G_TT_NONE);
    gSPTexture(POLY_XLU_DISP++, 0xFFFF, 0xFFFF, 0, 0, G_ON);
    gDPLoadTextureBlock(POLY_XLU_DISP++, gItemIconMagicBeansTex, G_IM_FMT_RGBA, G_IM_SIZ_32b, 32, 32, 0, G_TX_CLAMP,
                        G_TX_CLAMP, 5, 5, G_TX_NOLOD, G_TX_NOLOD);
    FrameInterpolation_RecordOpenChild(&plantingBean, 0);
    Matrix_Push();
    Matrix_Translate(pos.x, pos.y, pos.z, MTXMODE_NEW);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    const float shrink = 1.f - .6f * t * t;
    Matrix_Scale(shrink, shrink, shrink, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPVertex(POLY_XLU_DISP++, (uintptr_t)quad, 4, 0);
    gSP2Triangles(POLY_XLU_DISP++, 0, 1, 2, 0, 0, 2, 3, 0);
    Matrix_Pop();
    FrameInterpolation_RecordCloseChild();
    CLOSE_DISPS(play->state.gfxCtx);
}
