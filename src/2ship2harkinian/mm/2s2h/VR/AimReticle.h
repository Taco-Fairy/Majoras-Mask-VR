#pragma once
#include "bow_aim.h"
extern "C" {
#include "global.h"
}
namespace mmvrgame {
inline mmvr::Matrix AimReticle(PlayState* play,Player* p,Vec3f anchor,XrVector3f direction,XrVector3f head,float range,float emptyDistance,bool projectile=false){
 mmvr::Matrix result{};
  Vec3f endpoint{anchor.x+direction.x*range,anchor.y+direction.y*range,anchor.z+direction.z*range};
  CollisionPoly* targetPoly=nullptr;int targetBg=BGCHECK_SCENE;Vec3f targetHit;
  bool surfaceHit=projectile ? BgCheck_ProjectileLineTest(&play->colCtx,&anchor,&endpoint,&targetHit,&targetPoly,true,true,true,true,&targetBg) : BgCheck_EntityLineTest2(&play->colCtx,&anchor,&endpoint,&targetHit,&targetPoly,true,true,true,true,&targetBg,&p->actor);
  if(surfaceHit)endpoint=targetHit;
  else endpoint={anchor.x+direction.x*emptyDistance,anchor.y+direction.y*emptyDistance,anchor.z+direction.z*emptyDistance};
  // Reticles are depth-independent; lift their center clear of the struck surface.
  auto basis=mmvr::ArrowPose(direction,{0,0,0});float size=mmvr::ReticleWorldScale(std::sqrt(SQ(endpoint.x-head.x)+SQ(endpoint.y-head.y)+SQ(endpoint.z-head.z)));
  result=mmvr::YawPose(0,endpoint.x,endpoint.y,endpoint.z);
  for(int k=0;k<3;++k){result.m[0][k]=basis.m[2][k]*size*100;result.m[1][k]=basis.m[1][k]*size*100;result.m[2][k]=basis.m[0][k]*size*100;}
  if(surfaceHit&&targetPoly){
   Vec3f v[3];CollisionPoly_GetVerticesByBgId(targetPoly,targetBg,&play->colCtx,v);
   Vec3f a{v[1].x-v[0].x,v[1].y-v[0].y,v[1].z-v[0].z},b{v[2].x-v[0].x,v[2].y-v[0].y,v[2].z-v[0].z};
   XrVector3f normal{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};
   float length=std::sqrt(SQ(normal.x)+SQ(normal.y)+SQ(normal.z));
   if(length>.0001f){float sign=normal.x*direction.x+normal.y*direction.y+normal.z*direction.z>0?-1.f:1.f;
    normal.x*=sign/length;normal.y*=sign/length;normal.z*=sign/length;mmvr::LiftReticle(result,normal);}
  }
 return result;
}
}
