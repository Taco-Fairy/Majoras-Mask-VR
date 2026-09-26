/*
 * File: z_obj_funen.c
 * Overlay: ovl_Obj_Funen
 * Description: Unused(?) Stone Tower smoke
 */

#ifdef MMVR_ENABLE
#include "visibility.h"
#endif
#include "z_obj_funen.h"
#include "objects/object_funen/object_funen.h"

#define FLAGS (ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED)

void ObjFunen_Init(Actor* thisx, PlayState* play);
void ObjFunen_Draw(Actor* thisx, PlayState* play);

ActorProfile Obj_Funen_Profile = {
    /**/ ACTOR_OBJ_FUNEN,
    /**/ ACTORCAT_PROP,
    /**/ FLAGS,
    /**/ OBJECT_FUNEN,
    /**/ sizeof(ObjFunen),
    /**/ ObjFunen_Init,
    /**/ Actor_Noop,
    /**/ Actor_Noop,
    /**/ ObjFunen_Draw,
};

f32 D_80A198D0[] = { 0.1f, 0.024390244f };

void ObjFunen_Init(Actor* thisx, PlayState* play) {
    ObjFunen* this = (ObjFunen*)thisx;

    Actor_SetScale(&this->actor, D_80A198D0[this->actor.params & 1]);
}

void ObjFunen_Draw(Actor* thisx, PlayState* play) {
    s32 temp;
    s32 pad[2];

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    Matrix_RotateYS((s16)(Camera_GetCamDirYaw(GET_ACTIVE_CAM(play)) - 0x8000), MTXMODE_APPLY);

    {
        Mtx* smokeMatrix=Matrix_Finalize(play->state.gfxCtx);
#ifdef MMVR_ENABLE
        MtxF smokeTransform;Matrix_Get(&smokeTransform);
        MMVR_SetYawBillboardMatrix(smokeMatrix,((s16)(Camera_GetCamDirYaw(GET_ACTIVE_CAM(play)) - 0x8000))*(3.14159265359f/32768.f),smokeTransform.mf[3][0],smokeTransform.mf[3][1],smokeTransform.mf[3][2]);
#endif
        gSPMatrix(POLY_XLU_DISP++,smokeMatrix,G_MTX_NOPUSH|G_MTX_LOAD|G_MTX_MODELVIEW);
    }

    temp = -(play->gameplayFrames & 0x7FFFFFFF) & 0x7F;

    gSPSegment(POLY_XLU_DISP++, 0x08,
               Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, temp, 0x20, 0x20, 1, 0, temp, 0x20, 0x20, 0, -1, 0, -1));
    gSPDisplayList(POLY_XLU_DISP++, gStoneTowerSmokeDL);
    CLOSE_DISPS(play->state.gfxCtx);
}
