#pragma once
#include "Masks.h"
extern "C" { void Player_Action_63(Player*,PlayState*); }
// Isolated copied-save fixture. Production entry remains the wheel's SelectItem.
static void NativeQuickWheelTest(PlayState* play) {
    auto* p=GET_PLAYER(play);
    const auto baseline=*p;
    const auto save=gSaveContext;
    const auto msg=play->msgCtx;
    const auto cs=play->csCtx;
    const auto input=*CONTROLLER1(&play->state);
    const auto settings=mmvr::GetSettings();
    auto* oldInput=sPlayerControlInput;
    sPlayerControlInput=CONTROLLER1(&play->state);
    mmvr::SetNativeTestTracking(true);
    mmvr::ApplyViewMode(2);
    mmvr::GetSettings().Set(mmvr::Setting::FormFirstPerson,1);
    mmvr::GetSettings().Set(mmvr::Setting::WorldScaleCalibration,0);
    mmvr::TrackingFrame f{};
    f.head.orientation.w=f.origin.orientation.w=1;
    for(int h=0;h<2;++h) {
        f.hands[h].orientation.w=f.aims[h].orientation.w=1;
        f.hands[h].position={0,-.4f,-.4f};
        f.handTracked[h]=f.handValid[h]=f.aimValid[h]=true;
    }
    auto view=mmvr::YawPose(0,0,2045,0),head=mmvr::YawPose(0);
    double clock=10000;
    unsigned assertions=0;
    bool passed=true;
    std::ofstream log("native-quick-wheel.log");
    auto check=[&](bool ok,const char* label) {
        ++assertions;passed&=ok;
        if(!ok)log<<"FAIL "<<label<<" form="<<int(p->transformation)<<" item="<<mmvrgame::SelectedItem(play)
            <<" action="<<int(p->itemAction)<<" held="<<int(p->heldItemAction)<<" cs="<<int(p->csAction)<<"\n";
    };
    auto prepare=[&](int slot,int item,int form,int enabled,int left) {
        mmvrgame::ClearItemSelection();mmvrgame::ClearTracking();
        *p=baseline;gSaveContext=save;play->msgCtx=msg;play->csCtx=cs;
        play->msgCtx.msgMode=MSGMODE_NONE;play->csCtx.state=CS_STATE_IDLE;
        p->actor.world.pos={0,2000,0};p->actor.init=nullptr;
        p->transformation=form;gSaveContext.save.playerForm=form;
        p->stateFlags1=p->stateFlags2=p->stateFlags3=0;
        p->csAction=PLAYER_CSACTION_NONE;p->currentMask=PLAYER_MASK_NONE;
        p->heldItemId=ITEM_NONE;p->itemAction=p->heldItemAction=PLAYER_IA_NONE;
        p->heldActor=p->actor.child=nullptr;p->actionFunc=Player_Action_Idle;
        *CONTROLLER1(&play->state)={};
        mmvr::SetInputContext(true,false);
        mmvr::GetSettings().Set(mmvr::Setting::QuickWheelItems,enabled);
        mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,left);
        gSaveContext.save.saveInfo.inventory.items[slot]=item;
        BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_C_DOWN)=item;C_SLOT_EQUIP(0,EQUIP_SLOT_C_DOWN)=slot;
        f.triggers[0]=f.triggers[1]=0;f.epoch++;f.timeSeconds=clock+=1;
        for(int h=0;h<2;++h)f.hands[h].position={0,-.4f,-.4f};
        mmvrgame::RecordTracking(f,view,head);
        mmvrgame::SelectItem(play,slot,item);
        mmvrgame::UpdateMaskContext(play);
        mmvrgame::ProcessItemTrigger(play);
    };
    auto sample=[&](int hand,float trigger,bool mask) {
        f.timeSeconds=clock+=.02;f.triggers[0]=f.triggers[1]=0;f.triggers[hand]=trigger;
        if(mask)mmvr::UpdateMaskTracking(f,true);
        mmvrgame::RecordTracking(f,view,head);mmvrgame::ProcessItemTrigger(play);
    };
    for(int enabled=0;enabled<2;++enabled)for(int left=0;left<2;++left)
        for(int item=ITEM_MASK_DEKU;item<=ITEM_MASK_GIANT;++item) {
            prepare(SLOT_MASK_DEKU,item,PLAYER_FORM_HUMAN,enabled,left);
            int hand=1-left;
            check((mmvr::HeldMaskItem()==item)==bool(enabled),"selection-ready");
            sample(hand,0,true);
            if(enabled) {
                sample(1-hand,1,true);
                check(mmvr::HeldMaskItem()==item,"offhand-does-not-dismiss");
                sample(hand,1,true);
                check(mmvr::HeldMaskItem()<0&&mmvr::TakeMaskUse()<0,"trigger-dismiss");
                sample(hand,0,true);
                check(mmvr::HeldMaskItem()<0,"dismiss-stays-stowed");
                // Reselect, then move into the face slot without pressing trigger.
                mmvrgame::SelectItem(play,SLOT_MASK_DEKU,item);
                mmvrgame::ProcessItemTrigger(play);sample(hand,0,true);
                f.hands[hand].position={0,-.2f,-.25f};sample(hand,0,true);
                f.hands[hand].position={0,-.12f,-.14f};
                for(int i=0;i<6;++i)sample(hand,0,true);
                check(mmvr::TakeMaskUse()==item&&mmvr::HeldMaskItem()<0,"face-use");
                mmvrgame::SelectItem(play,SLOT_MASK_DEKU,item);mmvrgame::ProcessItemTrigger(play);
                mmvrgame::SelectItem(play,SLOT_BOW,ITEM_BOW);
                check(mmvr::HeldMaskItem()<0,"change-item-clears-mask");
            }
        }
    for(int enabled=0;enabled<2;++enabled)for(int left=0;left<2;++left)
        for(int form=0;form<PLAYER_FORM_MAX;++form) {
            prepare(SLOT_OCARINA,ITEM_OCARINA_OF_TIME,form,enabled,left);
            check((p->itemAction==PLAYER_IA_OCARINA)==bool(enabled),"instrument-native-request");
            if(!enabled)continue;
            p->actionFunc=Player_Action_63;p->stateFlags2|=PLAYER_STATE2_USING_OCARINA;
            play->msgCtx.ocarinaAction=OCARINA_ACTION_FREE_PLAY;
            play->msgCtx.ocarinaMode=OCARINA_MODE_ACTIVE;play->msgCtx.msgMode=MSGMODE_OCARINA_PLAYING;
            mmvr::SetInputContext(false,true);
            sample(1-left,0,false);sample(left,1,false);
            check(play->msgCtx.ocarinaMode==OCARINA_MODE_ACTIVE,"instrument-offhand-safe");
            sample(1-left,1,false);
            check(play->msgCtx.ocarinaMode==OCARINA_MODE_END,"instrument-trigger-cancel");
        }
    for(int guard=0;guard<5;++guard) {
        prepare(SLOT_OCARINA,ITEM_OCARINA_OF_TIME,PLAYER_FORM_HUMAN,1,0);
        p->actionFunc=Player_Action_63;p->stateFlags2|=PLAYER_STATE2_USING_OCARINA;
        play->msgCtx.ocarinaAction=OCARINA_ACTION_FREE_PLAY;play->msgCtx.ocarinaMode=OCARINA_MODE_ACTIVE;
        play->msgCtx.msgMode=MSGMODE_OCARINA_PLAYING;
        if(guard==0)p->csAction=PLAYER_CSACTION_16;
        if(guard==1)p->csAction=PLAYER_CSACTION_68;
        if(guard==2)play->msgCtx.msgMode=MSGMODE_SONG_PROMPT;
        if(guard==3)play->msgCtx.ocarinaAction=OCARINA_ACTION_CHECK_NOTIME;
        if(guard==4)mmvr::GetSettings().Set(mmvr::Setting::QuickWheelItems,0);
        mmvr::SetInputContext(false,true);sample(1,0,false);sample(1,1,false);
        check(play->msgCtx.ocarinaMode==OCARINA_MODE_ACTIVE,"lesson-and-disabled-guards");
    }
    mmvrgame::ClearItemSelection();mmvrgame::ClearTracking();
    *p=baseline;gSaveContext=save;play->msgCtx=msg;play->csCtx=cs;
    *CONTROLLER1(&play->state)=input;mmvr::GetSettings()=settings;
    sPlayerControlInput=oldInput;
    log<<(passed?"PASS":"FAIL")<<" quick-wheel assertions="<<assertions<<"\n";
    log.close();
    Ship::Context::GetRawInstance()->GetWindow()->Close();
}
