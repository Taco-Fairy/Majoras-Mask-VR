#pragma once
#include <fstream>
#include <cmath>
// Check the previous native Play_Draw, then configure the next one. These are
// production command sources, not calls directly into the compositor helper.
static void NativeScreenFadeLifecycle(PlayState* play,unsigned tick) {
    struct Saved { int fill=0; u8 color[4]{}; int registers[5]{}; u8 world=0,background=0; };
    static Saved saved;
    static bool passed=true;
    static std::ofstream trace("native-screen-fade.log");
    auto matches=[&](float r,float g,float b,float a) {
        auto color=mmvr::ScreenFade();
        trace<<tick<<" rgba="<<color[0]<<","<<color[1]<<","<<color[2]<<","<<color[3]<<"\n"<<std::flush;
        return std::abs(color[0]-r)<.002f && std::abs(color[1]-g)<.002f &&
               std::abs(color[2]-b)<.002f && std::abs(color[3]-a)<.002f;
    };
    if(tick<170 || tick>174)return;
    if(tick==170) {
        saved.fill=play->envCtx.fillScreen;
        std::copy_n(play->envCtx.screenFillColor,4,saved.color);
        saved.registers[0]=R_PLAY_FILL_SCREEN_ON;saved.registers[1]=R_PLAY_FILL_SCREEN_R;
        saved.registers[2]=R_PLAY_FILL_SCREEN_G;saved.registers[3]=R_PLAY_FILL_SCREEN_B;
        saved.registers[4]=R_PLAY_FILL_SCREEN_ALPHA;
        saved.world=play->worldCoverAlpha;saved.background=play->bgCoverAlpha;
        mmvr::SetNativeTestTracking(true);
        R_PLAY_FILL_SCREEN_ON=0;play->worldCoverAlpha=0;play->bgCoverAlpha=0;
        play->envCtx.fillScreen=1;
        play->envCtx.screenFillColor[0]=255;play->envCtx.screenFillColor[1]=play->envCtx.screenFillColor[2]=0;
        play->envCtx.screenFillColor[3]=128;
    } else if(tick==171) {
        float a=128/255.f;passed &= matches(1,0,0,a+a*(1-a));
        play->envCtx.fillScreen=0;play->worldCoverAlpha=128;
    } else if(tick==172) {
        float a=128/255.f;passed &= matches(0,0,0,a+a*(1-a));
        play->worldCoverAlpha=0;play->bgCoverAlpha=255;
    } else if(tick==173) {
        passed &= matches(0,0,0,0);
        play->bgCoverAlpha=0;R_PLAY_FILL_SCREEN_ON=1;
        R_PLAY_FILL_SCREEN_R=R_PLAY_FILL_SCREEN_B=0;R_PLAY_FILL_SCREEN_G=255;R_PLAY_FILL_SCREEN_ALPHA=200;
    } else {
        float a=200/255.f;passed &= matches(0,1,0,a+a*(1-a));
        play->envCtx.fillScreen=saved.fill;std::copy_n(saved.color,4,play->envCtx.screenFillColor);
        R_PLAY_FILL_SCREEN_ON=saved.registers[0];R_PLAY_FILL_SCREEN_R=saved.registers[1];
        R_PLAY_FILL_SCREEN_G=saved.registers[2];R_PLAY_FILL_SCREEN_B=saved.registers[3];R_PLAY_FILL_SCREEN_ALPHA=saved.registers[4];
        play->worldCoverAlpha=saved.world;play->bgCoverAlpha=saved.background;
        mmvr::SetNativeTestTracking(false);MMVR_ResetScreenFade();
        std::ofstream("native-screen-fade.json")<<"{\"passed\":"<<(passed?"true":"false")<<",\"nativeDrawCases\":4}";
    }
}
