#pragma once
#include "NativeCombat.h"
#include "2s2h/Enhancements/FrameInterpolation/FrameInterpolation.h"
#include <limits>
#include <fstream>
#include <cstring>
extern "C" {
#include "overlays/actors/ovl_En_M_Thunder/z_en_m_thunder.h"
void EnMThunder_Draw(Actor*, PlayState*);
}

static void NativeSwordChargeTest(PlayState* play) {
    auto* player=GET_PLAYER(play);
    const auto savedPlayer=*player;
    const auto savedSave=gSaveContext;
    const auto settings=mmvr::GetSettings();
    const auto savedFrame=play->gameplayFrames;
    const auto savedMessage=play->msgCtx.msgMode;
    const auto savedPause=play->pauseCtx.state;
    const auto savedCutscene=play->csCtx.state;
    bool passed=true; unsigned checks=0, matrices=0;
    auto require=[&](bool value,const char* why) {
        ++checks; if(!value) { passed=false; std::ofstream("native-sword-charge.log",std::ios::app)<<"FAIL "<<why<<"\n"; }
    };
    std::ofstream("native-sword-charge.log",std::ios::trunc).close();
    const ItemId swords[]={ITEM_SWORD_KOKIRI,ITEM_SWORD_RAZOR,ITEM_SWORD_GILDED,ITEM_SWORD_GREAT_FAIRY};
    mmvr::SetNativeTestTracking(true);mmvr::ApplyViewMode(2);
    mmvr::GetSettings().Set(mmvr::Setting::WorldScaleCalibration,0);
    mmvr::GetSettings().Set(mmvr::Setting::FormFirstPerson,1);
    mmvr::GetSettings().Set(mmvr::Setting::SpinChargeTime,.5f);
    auto lastMatrix=[](Gfx* begin,Gfx* end)->Mtx* {
        Mtx* result=nullptr;
        for(auto* command=begin;command<end;++command)
            if((command->words.w0>>24)==G_MTX) result=reinterpret_cast<Mtx*>(command->words.w1);
        return result;
    };
    for(int sword=0;sword<4;++sword)for(int left=0;left<2;++left)
        for(int great=0;great<2;++great)for(int hz:{72,90,120,200}) {
            *player=savedPlayer;gSaveContext=savedSave;
            player->actor.world.pos={0,2000,0};player->actor.shape.rot={};
            player->stateFlags1=player->stateFlags2=player->stateFlags3=0;
            player->csAction=PLAYER_CSACTION_NONE;player->heldActor=nullptr;
            player->transformation=PLAYER_FORM_HUMAN;
            player->itemAction=player->heldItemAction=PLAYER_IA_NONE;
            play->msgCtx.msgMode=MSGMODE_NONE;play->pauseCtx.state=PAUSE_STATE_OFF;play->csCtx.state=CS_STATE_IDLE;
            mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,left);
            mmvr::SetInputContext(true,false);mmvrgame::ClearTracking();
            MMVR_PlayerEquipSword(play,player,swords[sword]);
            gSaveContext.save.saveInfo.playerData.isMagicAcquired=1;
            gSaveContext.save.saveInfo.playerData.magic=20;gSaveContext.magicState=MAGIC_STATE_IDLE;
            if(great) SET_WEEKEVENTREG(WEEKEVENTREG_RECEIVED_GREAT_SPIN_ATTACK);
            else CLEAR_WEEKEVENTREG(WEEKEVENTREG_RECEIVED_GREAT_SPIN_ATTACK);
            mmvr::TrackingFrame frame{};frame.epoch=81000+checks;
            frame.head.orientation.w=frame.origin.orientation.w=1;
            for(int hand=0;hand<2;++hand) {
                frame.hands[hand].orientation.w=frame.aims[hand].orientation.w=1;
                frame.handTracked[hand]=frame.handValid[hand]=frame.aimValid[hand]=true;
            }
            auto model=mmvr::YawPose(0,0,2032,0);
            for(int row=0;row<3;++row)for(int col=0;col<3;++col)model.m[row][col]*=.01f;
            double pressedAt=0;
            for(int tick=0;tick<=hz;++tick) {
                // Keep the clock's base fixed despite assertion counting.
                frame.timeSeconds=81000+frame.epoch+tick/double(hz);
                if(tick==2) pressedAt=frame.timeSeconds;
                frame.triggers[1-left]=tick>=2?1.f:0.f;
                mmvrgame::RecordTracking(frame,mmvr::YawPose(0,0,2045,0),mmvr::YawPose(0));
                mmvrgame::UpdateSwordDiagnostics(frame,model);
                const auto visual=mmvrgame::TrackedSwordCharge(play);
                const float chargeTime=mmvr::GetSettings().Get(mmvr::Setting::SpinChargeTime);
                // Use the supplied double-precision timestamps. A simplified
                // float tick/hz division crosses the native alpha boundary by
                // one at 200 Hz even when the actual charge is correct.
                const float expectedCharge=tick<2?0.f:std::clamp(float((frame.timeSeconds-pressedAt)/chargeTime),0.f,1.f);
                const auto expected=mmvr::SwordChargeEffect(expectedCharge,great,play->gameplayFrames);
                require(visual.alpha==expected.alpha&&visual.great==expected.great,"charge timing / magic tier");
            }
            require(mmvrgame::TrackedSwordCharge(play).alpha==255,"fully charged glow missing");
            const auto magic=gSaveContext.save.saveInfo.playerData.magic;
            const auto magicState=gSaveContext.magicState;
            const auto atCount=play->colChkCtx.colATCount;
            if(hz==90) for(unsigned pulse=0;pulse<8;++pulse) {
                play->gameplayFrames=pulse;
                const auto visual=mmvrgame::TrackedSwordCharge(play);
                auto identity=mmvr::YawPose(0);
                std::memcpy(&player->leftHandMf,&identity,sizeof(identity));
                EnMThunder native{};native.type=sword;native.chargingAlpha=visual.alpha;native.unk1B0=visual.nativeCharge;
                Matrix_Push();Matrix_Translate(0,0,0,MTXMODE_NEW);
                auto* begin=play->state.gfxCtx->polyXlu.p;
                EnMThunder_Draw(&native.actor,play);
                auto* nativeMatrix=lastMatrix(begin,play->state.gfxCtx->polyXlu.p);
                Matrix_Pop();
                Mtx hand{};
                begin=play->state.gfxCtx->polyXlu.p;
                require(mmvrgame::DrawTrackedSwordCharge(play,&hand,left==0),"tracked charge draw rejected");
                auto* local=lastMatrix(begin,play->state.gfxCtx->polyXlu.p);
                require(nativeMatrix&&local,"charge matrices not emitted");
                if(nativeMatrix&&local) {
                    MtxF a,b;Matrix_MtxToMtxF(nativeMatrix,&a);Matrix_MtxToMtxF(local,&b);
                    bool same=true;
                    for(int i=0;i<16;++i) same&=std::abs(reinterpret_cast<float*>(&a)[i]-reinterpret_cast<float*>(&b)[i])<.001f;
                    require(same,"native sword-local matrix differs");++matrices;
                }
                bool handLoad=false;
                for(auto* command=begin;command<play->state.gfxCtx->polyXlu.p;++command)
                    if((command->words.w0>>24)==G_MTX&&command->words.w1==reinterpret_cast<uintptr_t>(&hand)) handLoad=true;
                require(handLoad,"late hand matrix not referenced");
                mmvr::CameraFrame camera{};camera.active=true;camera.hands[0]=mmvr::YawPose(.7f,pulse*3,10,20);
                mmvr::SetPlayerMatrixRange(nullptr,nullptr,&hand,nullptr);mmvr::SetNativeTestCamera(camera);mmvr::SetNativeTestEye(0);
                mmvr::Matrix late{};
                require(mmvr::OverrideModelMatrix(&hand,late.m)&&std::memcmp(&late,&camera.hands[0],sizeof(late))==0,
                        "current eye hand pose not used");
                mmvr::SetNativeTestCamera({});mmvr::SetPlayerMatrixRange(nullptr,nullptr,nullptr,nullptr);
            }
            if(sword==0&&left==0&&great==0&&hz==90) {
                const int savedFps=CVarGetInteger("gInterpolationFPS",20);
                CVarSetInteger("gInterpolationFPS",120);
                FrameInterpolation_ShouldInterpolateFrame(true);
                FrameInterpolation_ResetHistory();
                Matrix_Push();
                for(int frameIndex=0;frameIndex<4;++frameIndex) {
                    FrameInterpolation_StartRecord();
                    if(frameIndex&1) {
                        Mtx hand{};
                        require(mmvrgame::DrawTrackedSwordCharge(play,&hand,false),"transition charge draw rejected");
                    }
                    // A sibling's parent-path transform must interpolate the
                    // same way while the charge child appears/disappears.
                    Matrix_Push();Matrix_Translate(10.f+frameIndex*20.f,0,0,MTXMODE_NEW);
                    Mtx* sibling=Matrix_Finalize(play->state.gfxCtx);Matrix_Pop();
                    FrameInterpolation_StopRecord();
                    if(frameIndex) for(float alpha:{0.f,.5f,1.f}) {
                        FrameInterpolationScratch scratch;
                        const auto& compiled=FrameInterpolation_Interpolate(alpha,scratch,true);
                        const auto recursive=FrameInterpolation_Interpolate(alpha);
                        const float expected=10.f+(frameIndex-1+alpha)*20.f;
                        auto a=compiled.find(sibling),b=recursive.find(sibling);
                        require(a!=compiled.end()&&b!=recursive.end()&&
                                std::abs(a->second.xw-expected)<.001f&&
                                std::abs(b->second.xw-expected)<.001f,
                                "charge transition disturbed sibling interpolation");
                    }
                }
                FrameInterpolation_ResetHistory();
                Mtx* local=nullptr;Mtx* unprotected=nullptr;MtxF expectedLocal{};
                for(int actorFrame=0;actorFrame<2;++actorFrame) {
                    FrameInterpolation_StartRecord();
                    Vec3s rotation{0,static_cast<s16>(actorFrame*0x4000),0};
                    FrameInterpolation_RecordActorPosRotMatrix();
                    Matrix_SetTranslateRotateYXZ(actorFrame*200.f,actorFrame*300.f,actorFrame*400.f,&rotation);
                    Mtx hand{};auto* start=play->state.gfxCtx->polyXlu.p;
                    require(mmvrgame::DrawTrackedSwordCharge(play,&hand,false),"moving actor charge draw rejected");
                    local=lastMatrix(start,play->state.gfxCtx->polyXlu.p);
                    if(local) Matrix_MtxToMtxF(local,&expectedLocal);
                    Matrix_Push();Matrix_Translate(0,0,0,MTXMODE_NEW);
                    unprotected=Matrix_Finalize(play->state.gfxCtx);Matrix_Pop();
                    FrameInterpolation_StopRecord();
                }
                for(float alpha:{0.f,.5f,1.f}) {
                    FrameInterpolationScratch scratch;
                    const auto& compiled=FrameInterpolation_Interpolate(alpha,scratch,true);
                    const auto recursive=FrameInterpolation_Interpolate(alpha);
                    auto a=compiled.find(local),b=recursive.find(local);
                    bool same=local&&a!=compiled.end()&&b!=recursive.end();
                    if(same)for(int i=0;i<16;++i) {
                        same&=std::abs(reinterpret_cast<const float*>(&a->second)[i]-reinterpret_cast<float*>(&expectedLocal)[i])<.001f;
                        same&=std::abs(reinterpret_cast<const float*>(&b->second)[i]-reinterpret_cast<float*>(&expectedLocal)[i])<.001f;
                    }
                    require(same,"actor motion applied twice to local charge matrix");
                    if(alpha==.5f) {
                        auto negative=compiled.find(unprotected);
                        require(negative!=compiled.end()&&std::abs(negative->second.xx-1.f)>.01f,
                                "actor adjustment negative control inactive");
                    }
                }
                Matrix_Pop();FrameInterpolation_ResetHistory();
                CVarSetInteger("gInterpolationFPS",savedFps);
            }
            require(gSaveContext.save.saveInfo.playerData.magic==magic&&gSaveContext.magicState==magicState&&
                    play->colChkCtx.colATCount==atCount,"visual changed magic / collision ownership");
            gSaveContext.save.saveInfo.playerData.magic=1;
            require(!mmvrgame::TrackedSwordCharge(play).alpha,"insufficient magic glow");
            gSaveContext.save.saveInfo.playerData.magic=magic;
            gSaveContext.magicState=MAGIC_STATE_CONSUME;
            require(!mmvrgame::TrackedSwordCharge(play).alpha,"another magic owner glow");
            gSaveContext.magicState=magicState;gSaveContext.save.saveInfo.playerData.isMagicAcquired=0;
            require(!mmvrgame::TrackedSwordCharge(play).alpha,"unacquired magic glow");
            gSaveContext.save.saveInfo.playerData.isMagicAcquired=1;
            play->msgCtx.msgMode=MSGMODE_TEXT_START;
            require(!mmvrgame::TrackedSwordCharge(play).alpha,"dialogue glow");play->msgCtx.msgMode=MSGMODE_NONE;
            mmvr::ApplyViewMode(1);require(!mmvrgame::TrackedSwordCharge(play).alpha,"third-person duplicate glow");mmvr::ApplyViewMode(2);
            frame.timeSeconds+=1./hz;frame.triggers[1-left]=0;
            mmvrgame::RecordTracking(frame,mmvr::YawPose(0,0,2045,0),mmvr::YawPose(0));mmvrgame::UpdateSwordDiagnostics(frame,model);
            require(!mmvrgame::TrackedSwordCharge(play).alpha,"glow survived release");
            mmvrgame::ClearTracking();require(!mmvrgame::TrackedSwordCharge(play).alpha,"glow survived reset");
        }
    require(!mmvr::SwordChargeEffect(std::numeric_limits<float>::quiet_NaN(),true,0).alpha,"nonfinite charge accepted");
    std::ofstream("native-sword-charge.log",std::ios::app)<<(passed?"PASS":"FAIL")<<" sword-charge assertions="<<checks<<" native-matrices="<<matrices<<"\n";
    *player=savedPlayer;gSaveContext=savedSave;play->gameplayFrames=savedFrame;
    play->msgCtx.msgMode=savedMessage;play->pauseCtx.state=savedPause;play->csCtx.state=savedCutscene;
    mmvr::SetNativeTestCamera({});mmvr::SetNativeTestTracking(false);mmvr::GetSettings()=settings;
    mmvr::ApplyViewMode(int(settings.Get(mmvr::Setting::ViewMode)));mmvrgame::ClearTracking();
    Ship::Context::GetRawInstance()->GetWindow()->Close();
}
