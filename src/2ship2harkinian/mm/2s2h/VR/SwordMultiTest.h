#pragma once
// Isolated native diagnostic: real actor hurtboxes, one buffered tracked sweep.
static void NativeSwordMultiTest(PlayState* play) {
    auto* p=GET_PLAYER(play);const Player saved=*p;const auto save=gSaveContext;
    const auto input=*CONTROLLER1(&play->state);const auto context=play->colChkCtx;
    const auto settings=mmvr::GetSettings();auto* previous=sPlayerControlInput;
    sPlayerControlInput=CONTROLLER1(&play->state);
    mmvr::ApplyViewMode(2);mmvr::SetNativeTestTracking(true);
    mmvr::GetSettings().Set(mmvr::Setting::PhysicalSword,1);
    p->actor.world.pos={0,2000,0};p->actor.shape.rot={};p->transformation=PLAYER_FORM_HUMAN;
    p->stateFlags1=p->stateFlags2=p->stateFlags3=0;p->heldActor=nullptr;p->csAction=PLAYER_CSACTION_NONE;
    const auto msg=play->msgCtx.msgMode;const auto cs=play->csCtx.state;
    play->msgCtx.msgMode=MSGMODE_NONE;play->csCtx.state=CS_STATE_IDLE;
    std::ofstream log("native-sword-multi.log");int failures=0;
    auto frame=mmvr::TrackingFrame{};frame.origin.orientation.w=frame.head.orientation.w=1;frame.epoch=800;
    for(int h=0;h<2;++h){frame.hands[h].orientation.w=frame.aims[h].orientation.w=1;frame.handValid[h]=frame.handTracked[h]=frame.aimValid[h]=true;}
    const auto view=mmvr::YawPose(0,0,2045,0),head=mmvr::YawPose(0);
    const ItemId weapons[]={ITEM_SWORD_KOKIRI,ITEM_SWORD_RAZOR,ITEM_SWORD_GILDED,ITEM_SWORD_GREAT_FAIRY};
    for(int weapon=0;weapon<4;++weapon) {
        mmvrgame::ClearTracking();mmvrgame::ClearCombat();MMVR_PlayerEquipSword(play,p,weapons[weapon]);
        EnDekubaba enemies[2]{};int contacts[2]{};
        for(int i=0;i<2;++i) {
            auto& e=enemies[i];e.actor.id=ACTOR_EN_DEKUBABA;e.actor.update=EnDekubaba_Update;
            e.actor.world.pos={0,2000,0};e.actor.home.pos=e.actor.world.pos;
            EnDekubaba_Init(&e.actor,play);e.actor.colChkInfo.health=32;
            e.collider.base.colMaterial=COL_MATERIAL_HIT0;e.collider.base.acFlags&=~AC_HARD;
            for(auto& element:e.colliderElements) {
                element.dim.worldSphere.center={(s16)(MMVR_NativeSwordLength(p)*.01f-3),2024,(s16)(i?8:-8)};
                element.dim.worldSphere.radius=5;element.base.acElemFlags|=ACELEM_ON;
            }
        }
        for(int sample=0;sample<105;++sample) {
            frame.timeSeconds=10+weapon*4+sample/90.0;
            float z=sample<30?-25:std::min(25.f,-25+(sample-30)*2.0f);
            frame.hands[mmvr::SwordController(mmvr::GetSettings())].position={0,-.5f,z/40};
            mmvrgame::RecordTracking(frame,view,head);
            auto model=mmvr::YawPose(0,0,2020,z);for(int k=0;k<3;++k)model.m[k][k]=.01f;
            mmvrgame::UpdateSwordDiagnostics(frame,model);
            if(sample%3==0) {
                CollisionCheck_ClearContext(play,&play->colChkCtx);
                for(auto& e:enemies) {
                    Collider_ResetJntSphAC(play,&e.collider.base);CollisionCheck_ResetDamage(&e.actor.colChkInfo);
                    e.collider.base.acFlags|=AC_ON;CollisionCheck_SetAC(play,&play->colChkCtx,&e.collider.base);
                }
                mmvrgame::ProcessCombatInput(play);MMVR_FilterAttackCollisions(play);
                CollisionCheck_AT(play,&play->colChkCtx);MMVR_AfterAttackCollision(play);CollisionCheck_Damage(play,&play->colChkCtx);
                for(int i=0;i<2;++i) if(enemies[i].collider.base.acFlags&AC_HIT) ++contacts[i];
            }
        }
        log<<"weapon="<<weapon<<" first="<<contacts[0]<<" second="<<contacts[1]<<"\n";
        failures+=(contacts[0]!=1 || contacts[1]!=1);
        for(auto& e:enemies) EnDekubaba_Destroy(&e.actor,play);
    }
    mmvrgame::ClearTracking();mmvrgame::ClearCombat();*p=saved;gSaveContext=save;
    *CONTROLLER1(&play->state)=input;play->colChkCtx=context;mmvr::GetSettings()=settings;
    sPlayerControlInput=previous;play->msgCtx.msgMode=msg;play->csCtx.state=cs;
    log<<"failures="<<failures<<"\n";log.flush();std::_Exit(failures?2:0);
}
