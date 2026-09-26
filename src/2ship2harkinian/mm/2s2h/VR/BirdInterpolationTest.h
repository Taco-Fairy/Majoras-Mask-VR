#pragma once
#include "2s2h/Enhancements/FrameInterpolation/FrameInterpolation.h"
#include "objects/object_crow/object_crow.h"
static void NativeBirdInterpolation(PlayState* play){
 // The real Guay skeleton is shared by wild birds, the telescope bird and the
 // shooting gallery. Validate its rendered joints without speeding up AI/time.
 auto* gfx=play->state.gfxCtx;auto opa=gfx->polyOpa;auto xlu=gfx->polyXlu;
 const int fps=CVarGetInteger("gInterpolationFPS",20);CVarSetInteger("gInterpolationFPS",120);
 SkelAnime skel{};Vec3s joints[OBJECT_CROW_LIMB_MAX]{},morph[OBJECT_CROW_LIMB_MAX]{};Actor actor{};
 SkelAnime_InitFlex(play,&skel,(FlexSkeletonHeader*)gGuaySkel,(AnimationHeader*)gGuayFlyAnim,joints,morph,OBJECT_CROW_LIMB_MAX);
 for(int frame=0;frame<2;++frame){
  SkelAnime_Update(&skel);FrameInterpolation_ShouldInterpolateFrame(true);FrameInterpolation_StartRecord();
  FrameInterpolation_RecordOpenChild(&actor,0);Matrix_Push();Matrix_Translate(0,0,0,MTXMODE_NEW);
  SkelAnime_DrawFlexOpa(play,skel.skeleton,skel.jointTable,skel.dListCount,nullptr,nullptr,&actor);
  Matrix_Pop();FrameInterpolation_RecordCloseChild();FrameInterpolation_StopRecord();
 }
 auto a=FrameInterpolation_Interpolate(0),b=FrameInterpolation_Interpolate(1);
 unsigned moving=0,checks=0;bool smooth=true;
 for(const auto& [address,matrix]:b){auto old=a.find(address);if(old==a.end()){smooth=false;continue;}
  float delta=0;for(int i=0;i<16;++i)delta+=std::abs((&matrix.mf[0][0])[i]-(&old->second.mf[0][0])[i]);
  moving+=delta>.001f;
 }
 for(int rate:{90,120})for(int render=1;render<rate/20.f;++render){
  float alpha=float(render)*20/rate;auto step=FrameInterpolation_Interpolate(alpha);++checks;
  for(const auto& [address,matrix]:b){if(!a.contains(address)||!step.contains(address)){smooth=false;continue;}
   for(int i=0;i<16;++i){float expected=(&a.at(address).mf[0][0])[i]*(1-alpha)+(&matrix.mf[0][0])[i]*alpha;
    smooth&=std::abs((&step.at(address).mf[0][0])[i]-expected)<.0001f;}
  }
 }
 std::ofstream("native-bird-interpolation.json")<<"{\"movingJoints\":"<<moving<<",\"intermediateFrames\":"<<checks<<",\"smooth\":"<<smooth<<"}";
 CVarSetInteger("gInterpolationFPS",fps);gfx->polyOpa=opa;gfx->polyXlu=xlu;
}
