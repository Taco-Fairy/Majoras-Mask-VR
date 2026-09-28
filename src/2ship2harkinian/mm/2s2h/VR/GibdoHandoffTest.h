#pragma once
extern "C" {
#include "overlays/actors/ovl_En_Talk_Gibud/z_en_talk_gibud.h"
}
// Each recipe is an actual well placement. Consumption/switch/death are never injected.
static mmvr::Pad NativeGibdoHandoff(PlayState* play,unsigned tick) {
    struct Recipe { int room,params,item,slot,amount; bool bottle; };
    static const Recipe recipes[]={
        {0,672,ITEM_POTION_BLUE,SLOT_BOTTLE_1,1,true},
        {0,657,ITEM_MAGIC_BEANS,SLOT_MAGIC_BEANS,5,false},
        {2,610,ITEM_SPRING_WATER,SLOT_BOTTLE_1,1,true},
        {1,643,ITEM_FISH,SLOT_BOTTLE_1,1,true},
        {5,532,ITEM_BUG,SLOT_BOTTLE_1,1,true},
        {1,629,ITEM_DEKU_NUT,SLOT_DEKU_NUT,10,false},
        {3,566,ITEM_BOMB,SLOT_BOMB,10,false},
        {3,583,ITEM_HOT_SPRING_WATER,SLOT_BOTTLE_1,1,true},
        {6,520,ITEM_BIG_POE,SLOT_BOTTLE_1,1,true},
        {7,489,ITEM_MILK_BOTTLE,SLOT_BOTTLE_1,1,true}};
    static const int index=std::atoi(std::getenv("MMVR_GIBDO_HANDOFF"));
    static int phase=0,age=0,done=-1;static bool roomRequested=false,sawPrompt=false;
    static std::ofstream log("native-gibdo-handoff.log");
    mmvr::Pad pad;pad.active=true;
    auto finish=[&](bool ok,const char* why){log<<(ok?"PASS":"FAIL")<<" Gibdo handoff "<<index<<" "<<why<<"\n"<<std::flush;Ship::Context::GetRawInstance()->GetWindow()->Close();};
    if(index<0||index>=10){finish(false,"invalid-case");return pad;}
    const auto& c=recipes[index];auto* p=GET_PLAYER(play);
    if(!phase) {
        if(tick<30)return pad;
        gSaveContext.save.day=gSaveContext.save.eventDayCount=1;gSaveContext.save.time=CLOCK_TIME(12,0);gSaveContext.save.isNight=false;
        gSaveContext.save.playerForm=PLAYER_FORM_HUMAN;gSaveContext.save.equippedMask=PLAYER_MASK_GIBDO;
        gSaveContext.save.saveInfo.inventory.items[SLOT_MASK_GIBDO]=ITEM_MASK_GIBDO;
        gSaveContext.save.saveInfo.inventory.items[c.slot]=c.item;
        if(!c.bottle)AMMO(c.item)=20;
        BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_C_DOWN)=c.item;C_SLOT_EQUIP(0,EQUIP_SLOT_C_DOWN)=c.slot;
        gSaveContext.buttonStatus[EQUIP_SLOT_C_DOWN]=BTN_ENABLED;
        memset(gSaveContext.cycleSceneFlags,0,sizeof(gSaveContext.cycleSceneFlags));
        gSaveContext.nextCutsceneIndex=gSaveContext.cutsceneTrigger=gSaveContext.respawnFlag=0;gSaveContext.save.cutsceneIndex=0;
        play->nextEntrance=ENTRANCE(BENEATH_THE_WELL,0);play->transitionTrigger=TRANS_TRIGGER_START;play->transitionType=TRANS_TYPE_FADE_BLACK;
        phase=1;return pad;
    }
    if(play->sceneId!=SCENE_REDEAD||play->transitionTrigger!=TRANS_TRIGGER_OFF||play->transitionMode!=TRANS_MODE_OFF||play->roomCtx.status)return pad;
    if(phase==1) {
        if(!roomRequested&&play->roomCtx.curRoom.num!=c.room){roomRequested=Room_RequestNewRoom(play,&play->roomCtx,c.room)!=0;return pad;}
        if(roomRequested)Room_FinishRoomChange(play,&play->roomCtx);
        phase=2;
    }
    EnTalkGibud* npc=nullptr;
    for(auto* a=play->actorCtx.actorLists[ACTORCAT_ENEMY].first;a;a=a->next)
        if(a->id==ACTOR_EN_TALK_GIBUD&&a->params==c.params&&a->update)npc=(EnTalkGibud*)a;
    if(phase==2) {
        static int settle=0;if(++settle<45)return pad;
        if(!npc){if(++age>60)finish(false,"actor-absent");return pad;}
        p->actor.world.pos=npc->actor.world.pos;p->actor.world.pos.x+=70*Math_SinS(npc->actor.shape.rot.y);p->actor.world.pos.z+=70*Math_CosS(npc->actor.shape.rot.y);p->actor.prevPos=p->actor.home.pos=p->actor.world.pos;
        p->actor.shape.rot.y=p->actor.world.rot.y=static_cast<s16>(npc->actor.shape.rot.y+0x8000);p->actor.velocity={};p->actor.speed=0;
        p->currentMask=PLAYER_MASK_GIBDO;mmvrgame::SelectItem(play,c.slot,c.item);phase=3;age=0;
    }
    ++age;mmvr::SetNativeTestTracking(true);mmvr::ApplyViewMode(2);
    mmvr::TrackingFrame f{};f.head.orientation.w=f.origin.orientation.w=1;f.timeSeconds=tick/20.;f.epoch=1;
    mmvr::SetNativeTestCamera(mmvrgame::TestCameraFrame(f));
    const auto state=Message_GetState(&play->msgCtx);sawPrompt|=state==TEXT_STATE_PAUSE_MENU;
    if(age%8==0)pad.buttons=state==TEXT_STATE_PAUSE_MENU?BTN_CDOWN:BTN_A;
    if(age%30==0)log<<"age="<<age<<" npc="<<(npc!=nullptr)<<" distance="<<(npc?npc->actor.xzDistToPlayer:-1)<<" mask="<<int(p->currentMask)<<" msg="<<int(state)<<" text="<<play->msgCtx.currentTextId<<" switch="<<Flags_GetSwitch(play,(c.params>>4)&255)<<"\n"<<std::flush;
    const bool consumed=c.bottle?gSaveContext.save.saveInfo.inventory.items[c.slot]==ITEM_BOTTLE:AMMO(c.item)==20-c.amount;
    if(sawPrompt&&!npc&&consumed&&Flags_GetSwitch(play,(c.params>>4)&255)&&!(p->stateFlags1&(PLAYER_STATE1_20|PLAYER_STATE1_20000000))&&done<0)done=age;
    if(done>=0&&age-done>40)finish(true,"consumed-once-switch-set-control-restored");
    else if(age>1000)finish(false,"timeout");
    return pad;
}
