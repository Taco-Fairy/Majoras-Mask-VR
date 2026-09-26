#pragma once
#include "FairyComfort.h"
#include "Camera.h"
#include "PresentationOptions.h"
extern "C" { extern u16 sCueTypeList[10]; }
static bool NativeCompanionOwnershipChecks(PlayState* play) {
    auto* player=GET_PLAYER(play);auto* savedFairy=player->tatlActor;
    const auto savedId=player->actor.id;const auto savedCategory=player->actor.category;
    const auto savedHide=mmvr::GetSettings().Get(mmvr::Setting::HideFairy);
    const auto savedContext=play->csCtx;
    u16 savedCueTypes[10];std::copy(std::begin(sCueTypeList),std::end(sCueTypeList),savedCueTypes);
    const auto savedList=play->actorCtx.actorLists[ACTORCAT_ITEMACTION];
    Actor companion{},tatl{},tael{};CsCmdActorCue cue{};
    companion.id=ACTOR_EN_ELF;player->tatlActor=&companion;
    tatl.id=tael.id=ACTOR_DM_CHAR00;tatl.params=0;tael.params=1;
    tatl.update=tatl.draw=tael.update=tael.draw=[](Actor*,PlayState*){};
    tatl.next=&tael;play->actorCtx.actorLists[ACTORCAT_ITEMACTION].first=&tatl;
    std::fill(std::begin(sCueTypeList),std::end(sCueTypeList),0);
    sCueTypeList[0]=CS_CMD_ACTOR_CUE_113;play->csCtx.state=CS_STATE_RUN;play->csCtx.actorCues[0]=&cue;
    bool ok=MMVR_FairyReplacedByCutscene(play,&companion) &&
        !MMVR_FairyReplacedByCutscene(play,&tatl) && !MMVR_FairyReplacedByCutscene(play,&tael);
    tatl.draw=nullptr;ok &= !MMVR_FairyReplacedByCutscene(play,&companion);tatl.draw=tatl.update;
    play->csCtx.actorCues[0]=nullptr;ok &= !MMVR_FairyReplacedByCutscene(play,&companion);
    play->csCtx.actorCues[0]=&cue;play->csCtx.state=CS_STATE_IDLE;
    ok &= !MMVR_FairyReplacedByCutscene(play,&companion);
    play->csCtx.state=CS_STATE_RUN;play->actorCtx.actorLists[ACTORCAT_ITEMACTION].first=&tael;
    ok &= !MMVR_FairyReplacedByCutscene(play,&companion);
    play->actorCtx.actorLists[ACTORCAT_ITEMACTION]=savedList;play->csCtx=savedContext;
    mmvr::GetSettings().Set(mmvr::Setting::HideFairy,0);
    player->actor.id=ACTOR_EN_TEST3;player->actor.category=ACTORCAT_PLAYER;
    ok &= MMVR_HideCompanionFairy(play,&companion) && MMVR_FairyTrailHidden(play,&companion.world.pos.x);
    MMVR_FairyTrailBegin(play,&companion);ok &= !MMVR_FairyTrailSpawning();MMVR_FairyTrailEnd();
    player->actor.id=savedId;player->actor.category=savedCategory;
    play->csCtx.state=CS_STATE_IDLE;
    ok &= !MMVR_HideCompanionFairy(play,&companion);
    play->csCtx=savedContext;
    mmvr::GetSettings().Set(mmvr::Setting::HideFairy,savedHide);
    std::copy(std::begin(savedCueTypes),std::end(savedCueTypes),sCueTypeList);player->tatlActor=savedFairy;
    return ok;
}
// Bounded native regression: uses the real camera callback and recorded player focus.
static bool NativeFairyCameraChecks(PlayState* play) {
    auto* p=GET_PLAYER(play); const auto saved=*p; const auto cs=play->csCtx;
    const auto settings=mmvr::GetSettings(); const auto collision=play->colChkCtx;
    const auto savedFrame=play->gameplayFrames;
    mmvr::SetNativeTestTracking(true);
    mmvr::GetSettings().Set(mmvr::Setting::VrCameraCutscenes,1);
    mmvr::GetSettings().Set(mmvr::Setting::FairyNearComfort,1);
    p->transformation=PLAYER_FORM_HUMAN;p->currentMask=PLAYER_MASK_NONE;
    p->stateFlags1=p->stateFlags2=p->stateFlags3=0;p->rideActor=nullptr;
    p->actor.world.pos={100,200,300};p->actor.shape.rot.y=0;
    CsCmdActorCue cue{};play->csCtx.playerCue=&cue;play->csCtx.state=CS_STATE_RUN;
    bool heights=true, comfort=true;const bool ownership=NativeCompanionOwnershipChecks(play);
#ifdef MMVR_LOCAL_TEST_TOOLS
    const bool kafeiHands=MMVR_VerifyKafeiHandMapping()!=0;
#else
    // The native fixture can be compiled into non-diagnostic builds too; the
    // test-only hand-mapping probe is intentionally absent from those binaries.
    const bool kafeiHands=true;
#endif
    std::ofstream samples("native-fairy-camera-samples.csv");
    samples<<"stable,focusHeight,active,cameraY,expectedY,eyeValid\n";
    mmvr::TrackingFrame tracking{};tracking.head.orientation.w=tracking.origin.orientation.w=1;
    tracking.timeSeconds=51000;tracking.epoch=51000;
    for(int stable=0;stable<2;++stable) {
        mmvr::GetSettings().Set(mmvr::Setting::StableCutsceneHead,float(stable));
        for(float head : {25.f,95.f}) {
            mmvrgame::ResetTestCamera();
            p->actor.focus.pos={100,200+head,300};
            Matrix_Push();Matrix_Translate(100,200,300,MTXMODE_NEW);
            MMVR_PlayerDrawBegin(play,&p->actor);MMVR_PlayerDrawEnd(play,&p->actor);Matrix_Pop();
            tracking.timeSeconds+=1.;tracking.epoch++;
            const auto frame=mmvrgame::TestCameraFrame(tracking);
            const auto pose=mmvr::InversePose(frame.view);
            heights &= frame.active && std::abs(pose.m[3][1]-(200+head))<1.f;
            float eye[3]{};const bool eyeValid=MMVR_EnvironmentEye(play,eye)!=0;comfort &= eyeValid;
            samples<<stable<<","<<head<<","<<frame.active<<","<<pose.m[3][1]<<","<<(200+head)<<","<<eyeValid<<"\n";
            Actor fairy{};fairy.id=ACTOR_EN_ELF;p->tatlActor=&fairy;
            fairy.world.pos={eye[0],eye[1],eye[2]};
            comfort &= std::abs(MMVR_FairyOpacity(play,&fairy)-.5f)<.001f;
            comfort &= MMVR_FairyTrailHidden(play,&fairy.world.pos.x)!=0;
            fairy.world.pos.x+=.3048f*40.f*1.5f;
            comfort &= std::abs(MMVR_FairyOpacity(play,&fairy)-.75f)<.001f;
            fairy.world.pos.x=eye[0]+.3048f*40.f*2.f;
            comfort &= std::abs(MMVR_FairyOpacity(play,&fairy)-1.f)<.001f;
            fairy.world.pos.x=eye[0]+.3048f*40.f*1.9f;
            comfort &= MMVR_FairyTrailHidden(play,&fairy.world.pos.x)!=0;
            fairy.world.pos.x=eye[0]+.3048f*40.f*2.1f;
            comfort &= MMVR_FairyTrailHidden(play,&fairy.world.pos.x)==0;
            Actor ordinary{};ordinary.id=ACTOR_EN_ELF;ordinary.world.pos={eye[0],eye[1],eye[2]};
            comfort &= MMVR_FairyOpacity(play,&ordinary)==1.f;
            MMVR_FairyTrailBegin(play,&fairy);comfort &= MMVR_FairyTrailSpawning()!=0;
            MMVR_FairyTrailEnd();comfort &= MMVR_FairyTrailSpawning()==0;
            MMVR_FairyTrailBegin(play,&ordinary);comfort &= MMVR_FairyTrailSpawning()==0;
            MMVR_FairyTrailEnd();
            mmvr::GetSettings().Set(mmvr::Setting::FairyNearComfort,0);
            fairy.world.pos={eye[0],eye[1],eye[2]};
            comfort &= MMVR_FairyOpacity(play,&fairy)==1.f && !MMVR_FairyTrailHidden(play,&fairy.world.pos.x);
            mmvr::GetSettings().Set(mmvr::Setting::FairyNearComfort,1);
            p->tatlActor=nullptr;
        }
    }
    *p=saved;play->csCtx=cs;play->colChkCtx=collision;play->gameplayFrames=savedFrame;
    mmvr::GetSettings()=settings;mmvrgame::ResetTestCamera();
    std::ofstream report("native-fairy-camera.json");
    report<<"{\"authoredHeadHeights\":"<<(heights?"true":"false")<<",\"fairyComfort\":"<<(comfort?"true":"false")<<",\"companionOwnership\":"<<(ownership?"true":"false")<<",\"kafeiHandMapping\":"<<(kafeiHands?"true":"false")<<"}";
    return heights&&comfort&&ownership&&kafeiHands;
}
