// Included by Camera.cpp: native camera and headset presentation share one owner.
#include "view_tools.h"
namespace {
struct ViewToolState {
 PlayState* play=nullptr;int scene=-1;bool scope=false,photo=false,scopeSample=false,photoSample=false;
 mmvr::Matrix entryHead{},scopePose{},photoPose{},photoNative{};float startYaw=0,startPitch=0,scopeYaw=0,scopePitch=0;
 double sampled=0;uint64_t origin=0;
} viewTool;
void ResetViewToolHistory(){viewTool={};mmvr::SetViewTool(0,1,0);}
void ViewToolContext(PlayState* play){
 if(!play||play!=viewTool.play||play->sceneId!=viewTool.scene){viewTool={};viewTool.play=play;viewTool.scene=play?play->sceneId:-1;}
 bool scope=play&&(play->actorCtx.flags&ACTORCTX_FLAG_TELESCOPE_ON)&&play->csCtx.state==CS_STATE_IDLE&&
     mmvr::GetSettings().Get(mmvr::Setting::TelescopeComfort)<.5f;
 bool photo=play&&mmvrgame::NativeViewfinderActive(play);
 if(!scope){viewTool.scope=false;viewTool.scopeSample=false;}
 if(!photo){viewTool.photo=false;viewTool.photoSample=false;viewTool.photoNative={};}
 if(!scope&&!photo)mmvr::SetViewTool(play&&(play->actorCtx.flags&ACTORCTX_FLAG_TELESCOPE_ON)&&
     mmvr::GetSettings().Get(mmvr::Setting::TelescopeComfort)<.5f?2:0,1,0);
}
mmvr::Matrix ScopePose(float yaw,float pitch,Vec3f eye){
 float c=std::cos(yaw),s=std::sin(yaw),cp=std::cos(pitch),sp=std::sin(pitch);
 return {{{-c,0,s,0},{s*sp,cp,c*sp,0},{-s*cp,sp,-c*cp,0},{eye.x,eye.y,eye.z,1}}};
}
}
namespace mmvrgame {
bool ViewToolCamera(const mmvr::TrackingFrame& f,mmvr::CameraFrame& result){
 auto* play=gPlayState;ViewToolContext(play);
 if(mmvr::GetSettings().Get(mmvr::Setting::TelescopeComfort)>.5f)return false;
 if(!play||!mmvr::FirstPersonSelected()||!(play->actorCtx.flags&ACTORCTX_FLAG_TELESCOPE_ON))return false;
 auto* p=GET_PLAYER(play);auto* camera=GET_ACTIVE_CAM(play);if(!p||!camera)return false;
 if(play->csCtx.state!=CS_STATE_IDLE){
  Vec3f d{camera->at.x-camera->eye.x,camera->at.y-camera->eye.y,camera->at.z-camera->eye.z};
  auto pose=ScopePose(std::atan2(d.x,d.z),-std::atan2(d.y,std::hypot(d.x,d.z)),camera->eye);
  result.active=true;result.exclusiveView=true;result.view=mmvr::InversePose(pose);
  result.viewAddress=submittedWorldView?submittedWorldView:play->view.viewingPtr;
  result.projectionZoom=std::tan(Pi/6)/std::tan(std::clamp(play->view.fovy,5.f,120.f)*Pi/360.f);
  mmvr::SetViewTool(2,result.projectionZoom,0);return true;
 }
 if(!viewTool.scope||viewTool.origin!=f.originEpoch){viewTool.scope=true;viewTool.scopeSample=false;viewTool.entryHead=mmvr::PoseMatrix(f.head);viewTool.origin=f.originEpoch;
  viewTool.startYaw=float(s16(p->actor.focus.rot.y-p->actor.shape.rot.y))*Pi/32768.f;viewTool.startPitch=float(p->actor.focus.rot.x)*Pi/32768.f;}
 auto relative=mmvr::Multiply(mmvr::PoseMatrix(f.head),mmvr::InversePose(viewTool.entryHead));
 auto angles=mmvr::BoundScope(viewTool.startYaw+mmvr::PoseYaw(relative),viewTool.startPitch+std::atan2(relative.m[2][1],std::hypot(relative.m[2][0],relative.m[2][2])));
 const float desiredYaw=float(p->actor.shape.rot.y)*Pi/32768.f+angles.yaw;
 // Head-driven mode uses the predicted-display pose without artificial lag.
 viewTool.scopeYaw=desiredYaw;viewTool.scopePitch=angles.pitch;
 viewTool.scopePose=ScopePose(viewTool.scopeYaw,viewTool.scopePitch,camera->eye);viewTool.scopeSample=true;viewTool.sampled=f.timeSeconds;
 result.active=true;result.exclusiveView=true;result.view=mmvr::InversePose(viewTool.scopePose);result.viewAddress=submittedWorldView?submittedWorldView:play->view.viewingPtr;
 result.projectionZoom=std::tan(Pi/6)/std::tan(std::clamp(play->view.fovy,5.f,120.f)*Pi/360.f);
 mmvr::SetViewTool(2,result.projectionZoom,angles.fade);return true;
}
bool PhotoRenderView(mmvr::CameraFrame& frame){
 if(!gPlayState||viewTool.play!=gPlayState||viewTool.scene!=gPlayState->sceneId||!viewTool.photoSample||sPictoState!=PICTO_BOX_STATE_LENS)return false;
 // A photograph has one optical center. Both eye views use that center while
 // framing, matching the native 160x112 crop even for close subjects.
 frame.view=mmvr::InversePose(viewTool.photoPose);frame.exclusiveView=true;return true;
}
void RecordPhotoHead(PlayState* play,const mmvr::Matrix& view,const mmvr::Matrix& head){
 ViewToolContext(play);if(!play||!NativeViewfinderActive(play))return;
 viewTool.photo=true;
 if(sPictoState==PICTO_BOX_STATE_LENS){auto h=head;h.m[3][0]=h.m[3][2]=0;h.m[3][1]*=40;
  viewTool.photoPose=mmvr::Multiply(h,view);viewTool.photoSample=true;}
 mmvr::SetViewTool(sPictoState==PICTO_BOX_STATE_LENS?1:3,1,0);
}
}
extern "C" int MMVR_ScopeEyeOverlay(){
 return mmvr::StereoActive()&&gPlayState&&(gPlayState->actorCtx.flags&ACTORCTX_FLAG_TELESCOPE_ON)&&
     mmvr::GetSettings().Get(mmvr::Setting::TelescopeComfort)<.5f&&mmvr::GetSettings().Get(mmvr::Setting::ComfortHudEffects)<.5f;
}
extern "C" int MMVR_ViewToolOverlay(){return mmvr::FirstPersonRequested()&&gPlayState&&sPictoState==PICTO_BOX_STATE_LENS;}
extern "C" int MMVR_ScopeInput(PlayState* play,Player* p){
 if(mmvr::GetSettings().Get(mmvr::Setting::TelescopeComfort)>.5f)return false;
 if(!play||!p||!mmvr::FirstPersonRequested()||!viewTool.scopeSample||viewTool.play!=play||viewTool.scene!=play->sceneId)return false;
 p->actor.focus.rot.x=Angle(viewTool.scopePitch);p->actor.focus.rot.y=Angle(viewTool.scopeYaw);return true;
}
extern "C" int MMVR_ViewToolCamera(PlayState* play,Camera* camera,float* eye,float* at,float* up){
 ViewToolContext(play);if(!mmvr::FirstPersonSelected()||!play||camera!=GET_ACTIVE_CAM(play))return false;
 mmvr::Matrix pose{};
 if((play->actorCtx.flags&ACTORCTX_FLAG_TELESCOPE_ON)&&viewTool.scopeSample){pose=ScopePose(viewTool.scopeYaw,viewTool.scopePitch,{eye[0],eye[1],eye[2]});}
 else if(mmvrgame::NativeViewfinderActive(play)&&viewTool.photoSample){
  if(sPictoState==PICTO_BOX_STATE_LENS)viewTool.photoNative=viewTool.photoPose;
  pose=viewTool.photoNative.m[3][3]?viewTool.photoNative:viewTool.photoPose;
  const auto& viewport=play->view.viewport;
  mmvr::SetPhotoFraming(play->view.fovy,float(viewport.rightX-viewport.leftX)/std::max(1,viewport.bottomY-viewport.topY));
 }else return false;
 for(int k=0;k<3;++k){eye[k]=pose.m[3][k];at[k]=eye[k]-pose.m[2][k]*1000;up[k]=pose.m[1][k];(&camera->eye.x)[k]=eye[k];(&camera->eyeNext.x)[k]=eye[k];(&camera->at.x)[k]=at[k];(&camera->up.x)[k]=up[k];}
 return true;
}
