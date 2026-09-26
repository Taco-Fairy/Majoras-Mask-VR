#pragma once
#include "FormAim.h"
#include "Interactions.h"
#include "PlayerBody.h"
#include "GlowDepthTest.h"
#include "FairyCameraTest.h"
extern "C" int MMVR_VerifyHandWorldContacts(Player*);
extern "C" int MMVR_VerifyMountedHands(Player*);
#ifdef MMVR_LOCAL_TEST_TOOLS
extern "C" int MMVR_TestMessageBackground(MessageContext*, int, int);
static bool NativeLessonBackgroundTest() {
    MessageContext message{};message.textBoxType=TEXTBOX_TYPE_0;
    bool ok=true;
    for(int mode : {MSGMODE_SONG_DEMONSTRATION_STARTING, MSGMODE_SONG_DEMONSTRATION,
                   MSGMODE_SONG_DEMONSTRATION_DONE, MSGMODE_SONG_PROMPT_STARTING, MSGMODE_SONG_PROMPT,
                   MSGMODE_SONG_PROMPT_SUCCESS, MSGMODE_SONG_PROMPT_FAIL,
                   MSGMODE_SONG_PROMPT_NOTES_DROP}) {
        message.msgMode=mode;message.ocarinaAction=OCARINA_ACTION_PROMPT_TIME;
        ok &= !MMVR_TestMessageBackground(&message,1,1);
        ok &= MMVR_TestMessageBackground(&message,0,0);
    }
    message.msgMode=MSGMODE_OCARINA_PLAYING;
    for(int action : {OCARINA_ACTION_FREE_PLAY, OCARINA_ACTION_CHECK_NOTIME}) {
        message.ocarinaAction=action;
        ok &= MMVR_TestMessageBackground(&message,1,0);
        ok &= MMVR_TestMessageBackground(&message,0,0);
    }
    message.msgMode=MSGMODE_TEXT_DISPLAYING;message.ocarinaAction=OCARINA_ACTION_PROMPT_TIME;
    ok &= MMVR_TestMessageBackground(&message,1,0); // Zelda's ordinary dialogue remains visible.
    ok &= MMVR_TestMessageBackground(&message,0,0);
    return ok;
}
#else
static bool NativeLessonBackgroundTest() { return true; }
#endif
static void NativeRenderCadenceTest(PlayState* play) {
    auto* p=GET_PLAYER(play);const Player saved=*p;const auto savedFrames=play->gameplayFrames;
    auto settings=mmvr::GetSettings();
    mmvr::SetNativeTestTracking(true);mmvr::SetNativeTestEye(0);
    bool fairyCamera=NativeFairyCameraChecks(play);
    bool lessonBackground=NativeLessonBackgroundTest();
    bool glowDepth=NativeGlowDepthTest(play);
    bool geometry=MMVR_VerifyHandWorldContacts(p),mounted=MMVR_VerifyMountedHands(p),reward=true,guard=true;
    Mtx rewardAddresses[2]{};
    mmvr::SetRewardRange(0,&rewardAddresses[0],&rewardAddresses[1]);
    mmvr::CameraFrame camera;camera.active=true;camera.rewardActive=true;
    const auto native=mmvr::YawPose(.4f,10,20,30);
    for(int sample=0;sample<9;++sample) {
        auto target=mmvr::YawPose(float(sample)*.03f,40+sample*.3f,50+sample*.7f,60-sample*.2f);
        camera.rewardCorrection=mmvr::Multiply(mmvr::InversePose(native),target);
        mmvr::SetNativeTestCamera(camera);auto output=native;
        reward&=mmvr::OverrideModelMatrix(&rewardAddresses[0],output.m,native.m);
        for(int r=0;r<4;++r)for(int c=0;c<4;++c)reward&=std::abs(output.m[r][c]-target.m[r][c])<.0001f;
    }
    mmvr::SetRewardRange(0,nullptr,nullptr);
    p->transformation=PLAYER_FORM_DEKU;p->stateFlags1=PLAYER_STATE1_400000;
    p->actor.world.pos={0,2000,0};p->actor.shape.rot.y=(s16)0x8000;
    const auto reference=mmvr::YawPose(0,0,2000+mmvrgame::FormEyeHeight(p),0);
    auto lower=mmvr::YawPose(0,0,reference.m[3][1]-8,-5);
    for(int i=0;i<3;++i)lower.m[i][i]=.01f;
    Matrix_Push();Matrix_Put((MtxF*)&lower);MMVR_RecordDekuGuard(play,p);
    ++play->gameplayFrames;
    auto upper=lower;upper.m[3][1]+=6;
    Matrix_Put((MtxF*)&upper);MMVR_RecordDekuGuard(play,p);Matrix_Pop();
    Mtx guardAddress{};MMVR_BindDekuGuard(&guardAddress);
    mmvr::TrackingFrame tracking{};tracking.head.orientation.w=tracking.origin.orientation.w=1;
    for(int sample=0;sample<=4;++sample) {
        tracking.visualAlpha=sample*.25f;
        auto head=mmvr::PoseMatrix({{std::sin(sample*.05f),0,0,std::cos(sample*.05f)},{0,0,0}});
        mmvrgame::RecordFormTracking(tracking,reference,head);
        mmvrgame::RecordBodyTracking(tracking,reference,head);
        camera={};camera.active=true;camera.dekuGuard=mmvrgame::DekuGuardPose(play,p);
        mmvr::SetNativeTestCamera(camera);
        auto local=lower;local.m[3][1]+=6*tracking.visualAlpha;
        auto expected=mmvr::Multiply(mmvr::Multiply(local,mmvr::InversePose(reference)),mmvrgame::FormHeadPose());
        auto output=upper;guard&=mmvr::OverrideModelMatrix(&guardAddress,output.m,upper.m);
        for(int r=0;r<4;++r)for(int c=0;c<4;++c)guard&=std::abs(output.m[r][c]-expected.m[r][c])<.001f;
    }
    *p=saved;play->gameplayFrames=savedFrames;mmvr::GetSettings()=settings;
    std::ofstream report("native-render-cadence.json");
    report<<"{\"fairyCamera\":"<<(fairyCamera?"true":"false")<<",\"lessonBackground\":"<<(lessonBackground?"true":"false")<<",\"glowDepth\":"<<(glowDepth?"true":"false")<<",\"handWorldGeometry\":"<<(geometry?"true":"false")<<",\"rewardRenderAnchor\":"<<(reward?"true":"false")
          <<",\"mountedHands\":"<<(mounted?"true":"false")<<",\"dekuLocalInterpolation\":"<<(guard?"true":"false")<<"}";report.close();
    std::_Exit(fairyCamera&&lessonBackground&&glowDepth&&geometry&&mounted&&reward&&guard?0:2);
}
