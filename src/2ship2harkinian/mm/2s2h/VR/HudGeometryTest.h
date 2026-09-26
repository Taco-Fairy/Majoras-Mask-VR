#pragma once
#include "hud_layout.h"
#include "2s2h/BenGui/HudEditor.h"
#include "PauseWheel.h"
extern "C" void Interface_SetPerspectiveView(PlayState*,s32,s32,s32,s32);
static void NativeHudGeometry(PlayState* play){
 auto settings=mmvr::GetSettings();std::ofstream log("native-hud-geometry.json");log<<"[";bool first=true;
 for(float size:{.5f,1.f,1.5f})for(float width:{.6f,1.6f,3.f})for(float spread:{0.f,100.f,300.f})for(float vertical:{0.f,100.f,300.f}){
  mmvr::GetSettings().Set(mmvr::Setting::HudSize,size);mmvr::GetSettings().Set(mmvr::Setting::HudHorizontalSpread,spread);
  mmvr::GetSettings().Set(mmvr::Setting::HudVerticalSpread,vertical);mmvr::GetSettings().Set(mmvr::Setting::HudWidth,width);
  auto rect=[&](HudEditorElementID element,float x,float y,float w,float h){
   HudEditor_SetActiveElement(element);s16 ds=512,dt=512;HudEditor_ModifyDrawValuesPrecise(&x,&y,&w,&h,&ds,&dt);return std::array<float,4>{x,y,w,h};};
  auto left=rect(HUD_EDITOR_ELEMENT_MAGIC_METER,18,42,8,16),mid=rect(HUD_EDITOR_ELEMENT_MAGIC_METER,26,42,96,16),right=rect(HUD_EDITOR_ELEMENT_MAGIC_METER,122,42,8,16);
  bool joined=std::abs(left[0]+left[2]-mid[0])<.001f&&std::abs(mid[0]+mid[2]-right[0])<.001f;
  bool hearts=true,rupees=true;float scale=mmvr::HudElementScale(size);
  for(int row=0;row<2;++row)for(int i=0;i<9;++i){auto a=rect(HUD_EDITOR_ELEMENT_HEARTS,24.56f+10*i,20.56f+10*row,10.88f,10.88f),b=rect(HUD_EDITOR_ELEMENT_HEARTS,34.56f+10*i,20.56f+10*row,10.88f,10.88f);hearts&=std::abs((b[0]-a[0])-10*scale)<=.251f;}
  for(int i=0;i<2;++i){auto a=rect(HUD_EDITOR_ELEMENT_RUPEE_COUNTER,32+6*i,206,8,16),b=rect(HUD_EDITOR_ELEMENT_RUPEE_COUNTER,38+6*i,206,8,16);rupees&=std::abs((b[0]-a[0])-6*scale)<=.251f;}
  bool axes=true;
  for(auto group:{mmvr::HudGroup::TopLeft,mmvr::HudGroup::Buttons,mmvr::HudGroup::BottomLeft,mmvr::HudGroup::Minimap,mmvr::HudGroup::Clock}){
   auto anchor=mmvr::GroupAnchor(group);
   auto base=mmvr::HudPosition(group,anchor.x,anchor.y,width,size,0,0);
   auto horizontalOnly=mmvr::HudPosition(group,anchor.x,anchor.y,width,size,spread,0);
   auto verticalOnly=mmvr::HudPosition(group,anchor.x,anchor.y,width,size,0,vertical);
   axes &= std::abs(horizontalOnly.y-base.y)<.001f && std::abs(verticalOnly.x-base.x)<.001f;
  }
  auto clock=rect(HUD_EDITOR_ELEMENT_CLOCK,160,206,0,0);
  auto mask=rect(HUD_EDITOR_ELEMENT_B,143,21,20,20),b=rect(HUD_EDITOR_ELEMENT_B,167,17,29,29);
  auto a=rect(HUD_EDITOR_ELEMENT_A,190,23+R_A_BTN_Y_OFFSET,45,45);
  bool cluster=std::abs((b[0]-mask[0])-24*scale)<=.26f && std::abs((a[0]+a[2]/2)-(b[0]+b[2]/2)-35*scale)<=.51f;
  bool centered=std::abs(clock[0]-160)<.001f;
  if(!first)log<<",";first=false;log<<"{\"size\":"<<size<<",\"spread\":"<<spread<<",\"vertical\":"<<vertical<<",\"width\":"<<width<<",\"axesIndependent\":"<<axes<<",\"clockCentered\":"<<centered<<",\"buttonCluster\":"<<cluster<<",\"magicJoined\":"<<joined<<",\"heartSpacing\":"<<hearts<<",\"rupeeSpacing\":"<<rupees<<"}";
 }
 log<<"]";
 // The perspective A-button viewport must use the same canvas as B/mask rects.
 mmvr::GetSettings().Set(mmvr::Setting::HudSize,1);
 mmvr::GetSettings().Set(mmvr::Setting::HudWidth,1.6f);
 mmvr::GetSettings().Set(mmvr::Setting::HudHorizontalSpread,100);
 mmvr::GetSettings().Set(mmvr::Setting::HudVerticalSpread,100);
 HudEditor_SetActiveElement(HUD_EDITOR_ELEMENT_A);
 s16 x=190,y=23+R_A_BTN_Y_OFFSET,w=45,h=45,ds=512,dt=512;
 HudEditor_ModifyDrawValues(&x,&y,&w,&h,&ds,&dt);
 Interface_SetPerspectiveView(play,23+R_A_BTN_Y_OFFSET,68+R_A_BTN_Y_OFFSET,190,235);
 const auto& viewport=play->interfaceCtx.viewport;
 std::ofstream("native-hud-viewport.json") << "{\"sameCanvas\":" <<
  (viewport.leftX==x && viewport.rightX==x+w && viewport.topY==y && viewport.bottomY==y+h)
  << ",\"left\":"<<viewport.leftX<<",\"expectedLeft\":"<<x<<"}";
 HudEditor_SetActiveElement(HUD_EDITOR_ELEMENT_NONE);mmvr::GetSettings()=settings;
}
