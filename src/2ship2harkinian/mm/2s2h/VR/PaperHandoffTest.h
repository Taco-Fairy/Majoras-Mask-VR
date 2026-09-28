#pragma once
extern "C" {
#include "overlays/actors/ovl_En_Bjt/z_en_bjt.h"
}
// Native schedule, message callback, paper consumption and first reward.
static mmvr::Pad NativePaperHandoff(PlayState* play,unsigned tick) {
    static const int items[]={ITEM_DEED_LAND,ITEM_DEED_SWAMP,ITEM_DEED_MOUNTAIN,ITEM_DEED_OCEAN,ITEM_LETTER_TO_KAFEI,ITEM_LETTER_MAMA};
    static const int index=std::atoi(std::getenv("MMVR_PAPER_HANDOFF"));
    static int phase=0,age=0,settle=0,done=-1;static bool prompt=false,consumed=false;
    static std::ofstream log("native-paper-handoff.log");
    mmvr::Pad pad;pad.active=true;auto* p=GET_PLAYER(play);
    auto finish=[&](bool ok,const char* why){log<<(ok?"PASS":"FAIL")<<" paper handoff "<<index<<" "<<why<<"\n"<<std::flush;Ship::Context::GetRawInstance()->GetWindow()->Close();};
    if(index<0||index>=6){finish(false,"invalid-case");return pad;}
    const int item=items[index],slot=SLOT(item);
    if(!phase){
        if(tick<30)return pad;
        gSaveContext.save.day=gSaveContext.save.eventDayCount=1;gSaveContext.save.time=CLOCK_TIME(1,0);gSaveContext.save.isNight=true;
        gSaveContext.save.playerForm=PLAYER_FORM_HUMAN;
        CLEAR_WEEKEVENTREG(WEEKEVENTREG_73_08);CLEAR_WEEKEVENTREG(WEEKEVENTREG_90_80);
        gSaveContext.save.saveInfo.inventory.items[slot]=item;
        BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_C_DOWN)=item;C_SLOT_EQUIP(0,EQUIP_SLOT_C_DOWN)=slot;
        gSaveContext.buttonStatus[EQUIP_SLOT_C_DOWN]=BTN_ENABLED;
        gSaveContext.nextCutsceneIndex=gSaveContext.cutsceneTrigger=gSaveContext.respawnFlag=0;gSaveContext.save.cutsceneIndex=0;
        play->nextEntrance=ENTRANCE(STOCK_POT_INN,0);play->transitionTrigger=TRANS_TRIGGER_START;play->transitionType=TRANS_TYPE_FADE_BLACK;
        phase=1;return pad;
    }
    if(play->sceneId!=SCENE_YADOYA||play->transitionTrigger!=TRANS_TRIGGER_OFF||play->transitionMode!=TRANS_MODE_OFF)return pad;
    EnBjt* npc=nullptr;
    for(auto* a=play->actorCtx.actorLists[ACTORCAT_NPC].first;a;a=a->next)if(a->id==ACTOR_EN_BJT&&a->update)npc=(EnBjt*)a;
    if(phase==1){
        if(++settle<45)return pad;
        if(!npc){finish(false,"actor-absent");return pad;}
        // Start in front of the authored toilet, outside its appearance radius.
        // A fixed world +Z offset placed the old fixture behind its wall.
        auto pos=npc->actor.home.pos;const s16 facing=npc->actor.shape.rot.y;
        pos.x+=Math_SinS(facing)*90;pos.z+=Math_CosS(facing)*90;
        Vec3f probe=pos;probe.y+=60;CollisionPoly* floorPoly=nullptr;int floorBg=0;
        float floor=BgCheck_EntityRaycastFloor5(&play->colCtx,&floorPoly,&floorBg,&p->actor,&probe);
        if(!floorPoly||fabsf(floor-pos.y)>20){finish(false,"unsafe-approach-checkpoint");return pad;}
        pos.y=floor;p->actor.world.pos=p->actor.prevPos=p->actor.home.pos=pos;
        p->actor.velocity={};p->actor.speed=0;p->actor.shape.rot.y=p->actor.world.rot.y=p->yaw=static_cast<s16>(facing+0x8000);
        mmvrgame::SelectItem(play,slot,item);MMVR_CameraCoordinateBoundary(play);phase=2;
    }
    ++age;mmvr::SetNativeTestTracking(true);mmvr::ApplyViewMode(2);
    mmvr::TrackingFrame f{};f.head.orientation.w=f.origin.orientation.w=1;f.timeSeconds=tick/20.;f.epoch=1;
    mmvr::SetNativeTestCamera(mmvrgame::TestCameraFrame(f));
    const auto state=Message_GetState(&play->msgCtx);prompt|=state==TEXT_STATE_PAUSE_MENU;
    if(phase==2){
        if(npc&&npc->actor.xzDistToPlayer<60)phase=3;
        else if(npc){
            // Stick input is relative to the active VR camera, not actor facing.
            // Steer toward the NPC through normal movement without teleporting.
            s16 toward=static_cast<s16>(npc->actor.yawTowardsPlayer+0x8000);
            s16 relative=static_cast<s16>(toward-Camera_GetInputDirYaw(GET_ACTIVE_CAM(play)));
            pad.x=static_cast<s8>(-60*Math_SinS(relative));
            pad.y=static_cast<s8>(60*Math_CosS(relative));
        }
    }
    if(phase>=3&&age%8==0)pad.buttons=state==TEXT_STATE_PAUSE_MENU?BTN_CDOWN:BTN_A;
    consumed|=gSaveContext.save.saveInfo.inventory.items[slot]==ITEM_NONE;
    if(age%30==0)log<<"age="<<age<<" distance="<<(npc?npc->actor.xzDistToPlayer:-1)<<" behaviour="<<(npc?npc->behaviour:-1)<<" text="<<play->msgCtx.currentTextId<<" consumed="<<consumed<<"\n"<<std::flush;
    if(prompt&&consumed&&CHECK_WEEKEVENTREG(WEEKEVENTREG_73_08)&&CHECK_WEEKEVENTREG(WEEKEVENTREG_90_80)&&state==TEXT_STATE_NONE&&done<0)done=age;
    if(done>=0&&age-done>40)finish(true,"paper-consumed-native-reward-complete");
    else if(age>1400)finish(false,"timeout");
    return pad;
}
