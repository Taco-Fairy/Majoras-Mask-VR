#pragma once
extern "C" {
#include "global.h"
#include "overlays/actors/ovl_Obj_Um/z_obj_um.h"
}
namespace mmvrgame {
// A vehicle surrounding its rider must not obstruct the physical bow or its
// hands. Only exclude the occupied boat or active escort cart; retain
// all other scene/prop collision and native dialogue/cutscene input gates.
inline bool VehicleCollisionExcluded(PlayState* play, Player* player, Actor* actor) {
    if (!play || !player || !actor || !actor->update || actor->init) return false;
    if (actor->id == ACTOR_BG_INGATE)
        return player->actor.floorBgId != BGCHECK_SCENE &&
               DynaPoly_GetActor(&play->colCtx, player->actor.floorBgId) == reinterpret_cast<DynaPolyActor*>(actor);
    if (actor->id != ACTOR_OBJ_UM || play->bButtonAmmoPlusOne <= 0) return false;
    const auto* cart=reinterpret_cast<const ObjUm*>(actor);
    return cart->type == OBJ_UM_TYPE_MILK_RUN_MINIGAME && (cart->flags & OBJ_UM_FLAG_PLAYING_MINIGAME);
}
inline Actor* ActiveEscortCart(PlayState* play, Player* player) {
    if (!play || !player || play->bButtonAmmoPlusOne <= 0) return nullptr;
    for (auto* actor=play->actorCtx.actorLists[ACTORCAT_NPC].first; actor; actor=actor->next)
        if (actor->id == ACTOR_OBJ_UM && VehicleCollisionExcluded(play, player, actor)) return actor;
    return nullptr;
}
} // namespace mmvrgame
