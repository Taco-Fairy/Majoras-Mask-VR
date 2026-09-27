#pragma once
#include "EncounterAuditCatalog.inc"
// Authored room smoke test: does not fabricate actors or certify combat.
static mmvr::Pad NativeEncounterAudit(PlayState* play, unsigned tick) {
    static const int index=[] { const char* v=std::getenv("MMVR_ENCOUNTER_CASE"); return v?std::atoi(v):-1; }();
    static bool launched=false, requested=false, ready=false, finished=false;
    static unsigned samples=0, roomTicks=0;
    static std::ofstream log("native-encounter.log");
    mmvr::Pad pad; pad.active=true;
    auto finish=[&](const char* status,const char* reason) {
        if(finished)return;
        finished=true;
        log<<status<<" encounter="<<index<<" samples="<<samples<<" reason="<<reason<<'\n'<<std::flush;
        Ship::Context::GetRawInstance()->GetWindow()->Close();
    };
    if(index<0 || index>=ARRAY_COUNT(nativeEncounterRecipes)){finish("FAIL","invalid-index");return pad;}
    const auto& recipe=nativeEncounterRecipes[index];
    if(!launched && tick>=60){
        launched=true; gSaveContext.save.cutsceneIndex=0;
        gSaveContext.save.day=gSaveContext.save.eventDayCount=recipe.day;
        gSaveContext.save.time=recipe.night?CLOCK_TIME(22,0):CLOCK_TIME(12,0);
        gSaveContext.save.isNight=recipe.night;
        play->nextEntrance=recipe.entrance; play->transitionTrigger=TRANS_TRIGGER_START;
        play->transitionType=TRANS_TYPE_FADE_WHITE;
    }
    if(tick>1800){finish("BLOCKED","scene-room-or-actor-prerequisite");return pad;}
    if(!launched || play->sceneId!=recipe.scene || play->transitionTrigger!=TRANS_TRIGGER_OFF ||
        play->transitionMode!=TRANS_MODE_OFF || play->roomCtx.status)return pad;
    if(!ready){
        if(!requested && play->roomCtx.curRoom.num!=recipe.room){
            requested=Room_RequestNewRoom(play,&play->roomCtx,recipe.room)!=0;
            if(!requested)finish("BLOCKED","room-request-rejected");
            return pad;
        }
        if(requested)Room_FinishRoomChange(play,&play->roomCtx);
        // This interior entrance leaves Link beside the exit in a different
        // room. Place him above the verified room-3 Shellblade placement so
        // the native doorway cannot interrupt the observation window.
        if(recipe.actor==ACTOR_EN_SB && recipe.scene==SCENE_PIRATE && recipe.room==3) {
            auto* player=GET_PLAYER(play);
            player->actor.world.pos={4221.f,832.f,-1198.f};
            player->actor.prevPos=player->actor.world.pos;
            player->actor.velocity={};
            player->actor.speed=0;
        }
        ready=true;
        log<<"room="<<int(play->roomCtx.curRoom.num)<<" actor="<<recipe.actor<<'\n'<<std::flush;
    }
    ++roomTicks;
    bool found=false;
    for(int category=0;category<ACTORCAT_MAX;++category)
        for(Actor* actor=play->actorCtx.actorLists[category].first;actor;actor=actor->next){
            if(actor->id!=recipe.actor || !actor->update)continue;
            found=true;
            if(!std::isfinite(actor->world.pos.x)||!std::isfinite(actor->world.pos.y)||!std::isfinite(actor->world.pos.z)){
                finish("FAIL","nonfinite-actor-position");return pad;
            }
        }
    if(found)++samples;
    if(samples>=60)finish("PASS","native-actor-present-60-updates");
    else if(roomTicks>=180)finish("BLOCKED","actor-absent-or-short-lived");
    return pad;
}
