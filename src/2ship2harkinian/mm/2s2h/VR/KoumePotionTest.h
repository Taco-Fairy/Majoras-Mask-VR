#pragma once
extern "C" {
#include "overlays/actors/ovl_En_Tru/z_en_tru.h"
}
// Isolated real-scene handoff regression; do not replace native action functions.
static mmvr::Pad NativeKoumePotionTest(PlayState* play,unsigned tick) {
    static const bool physical=std::getenv("MMVR_KOUME_PHYSICAL")!=nullptr;
    static const bool preoffer=std::getenv("MMVR_KOUME_PREOFFER")!=nullptr;
    static int phase=0,age=0,completedAge=-1;static bool sawDrink=false,sawEmpty=false;
    static std::ofstream log("native-koume-potion.log");
    mmvr::Pad pad;pad.active=true;auto* p=GET_PLAYER(play);
    if(!phase){
        if(tick<30)return pad;
        gSaveContext.save.day=gSaveContext.save.eventDayCount=1;gSaveContext.save.time=CLOCK_TIME(12,0);
        gSaveContext.save.isNight=false;gSaveContext.nextCutsceneIndex=gSaveContext.cutsceneTrigger=gSaveContext.respawnFlag=0;
        gSaveContext.save.cutsceneIndex=0;CLEAR_WEEKEVENTREG(WEEKEVENTREG_SAVED_KOUME);SET_WEEKEVENTREG(WEEKEVENTREG_TALKED_KOUME_INJURED);
        gSaveContext.save.playerForm=PLAYER_FORM_HUMAN;
        gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_1]=ITEM_POTION_RED;
        BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_C_DOWN)=ITEM_POTION_RED;C_SLOT_EQUIP(0,EQUIP_SLOT_C_DOWN)=SLOT_BOTTLE_1;
        gSaveContext.buttonStatus[EQUIP_SLOT_C_DOWN]=BTN_ENABLED;
        play->nextEntrance=ENTRANCE(WOODS_OF_MYSTERY,0);play->transitionTrigger=TRANS_TRIGGER_START;play->transitionType=TRANS_TYPE_FADE_BLACK;
        phase=1;log<<"enter woods\n"<<std::flush;return pad;
    }
    if(play->sceneId!=SCENE_26SARUNOMORI||play->transitionTrigger!=TRANS_TRIGGER_OFF||play->transitionMode!=TRANS_MODE_OFF)return pad;
    static bool requestedRoom=false, roomReady=false;
    if(!roomReady){
        if(play->roomCtx.status)return pad;
        if(!requestedRoom&&play->roomCtx.curRoom.num!=7){requestedRoom=Room_RequestNewRoom(play,&play->roomCtx,7)!=0;return pad;}
        if(requestedRoom)Room_FinishRoomChange(play,&play->roomCtx);
        p->actor.world.pos={-1762.f,0.f,64.f};p->actor.prevPos=p->actor.world.pos;roomReady=true;
    }
    ++age;mmvr::ApplyViewMode(2);mmvr::SetNativeTestTracking(true);
    mmvr::TrackingFrame frame{};frame.head.orientation.w=frame.origin.orientation.w=1;frame.timeSeconds=tick/20.;frame.epoch=1;
    for(int h=0;h<2;++h){frame.hands[h].orientation.w=frame.aims[h].orientation.w=1;
        frame.handValid[h]=frame.handTracked[h]=frame.aimValid[h]=true;
        frame.hands[h].position={h?.12f:-.12f,-.1f,-.4f};}
    if(physical&&phase==2&&((preoffer&&age==20)||(!preoffer&&Message_GetState(&play->msgCtx)==TEXT_STATE_PAUSE_MENU&&age%8==0)))frame.triggers[1]=1;
    mmvr::SetNativeTestCamera(mmvrgame::TestCameraFrame(frame));
    EnTru* npc=nullptr;for(auto* a=play->actorCtx.actorLists[ACTORCAT_NPC].first;a;a=a->next)if(a->id==ACTOR_EN_TRU&&a->update)npc=(EnTru*)a;
    if(phase==1&&age>=45){
        if(!npc){log<<"FAIL missing Koume\n"<<std::flush;Ship::Context::GetRawInstance()->GetWindow()->Close();return pad;}
        p->actor.world.pos=npc->actor.world.pos;p->actor.world.pos.z+=35;
        npc->actor.shape.rot.y=npc->actor.world.rot.y=0;p->actor.prevPos=p->actor.world.pos;
        p->actor.shape.rot.y=p->actor.world.rot.y=static_cast<s16>(0x8000);
        mmvrgame::SelectItem(play,SLOT_BOTTLE_1,ITEM_POTION_RED);
        phase=2;age=0;log<<"ready actor="<<npc->actor.params<<"\n"<<std::flush;
    }
    if(phase==2){
        if(age==20&&!preoffer)pad.buttons=BTN_A;
        if(preoffer&&age<22)pad.buttons|=BTN_Z;
        auto state=Message_GetState(&play->msgCtx);
        if(state==TEXT_STATE_PAUSE_MENU){
#ifdef __ANDROID__
            // Live XR polling owns motion triggers between native ticks. Supply the
            // equivalent native offer here to exercise the actual Quest renderer.
            if(age%8==0)pad.buttons=BTN_CDOWN;
#else
            if(!physical&&age%8==0)pad.buttons=BTN_CDOWN;
#endif
        }
        else if(age%8==0&&(!preoffer||state!=TEXT_STATE_NONE))pad.buttons=BTN_A;
        if(npc&&npc->unk_364>=3)sawDrink=true;
        sawEmpty|=gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_1]==ITEM_BOTTLE;
        if(age%10==0)log<<"age="<<age<<" text="<<play->msgCtx.currentTextId<<" msg="<<int(state)<<" stage="<<(npc?npc->unk_364:-1)<<" anim="<<(npc?npc->animIndex:-1)<<" heldButton="<<int(p->heldItemButton)<<" bottle="<<int(gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_1])<<"\n"<<std::flush;
        if(CHECK_WEEKEVENTREG(WEEKEVENTREG_SAVED_KOUME)&&sawDrink&&sawEmpty&&completedAge<0)completedAge=age;
        if(completedAge>=0&&age-completedAge>=120){log<<"PASS Koume potion consumed and rescue completed\n"<<std::flush;Ship::Context::GetRawInstance()->GetWindow()->Close();}
        if(age>1800){log<<"FAIL handoff timeout\n"<<std::flush;Ship::Context::GetRawInstance()->GetWindow()->Close();}
    }
    return pad;
}
