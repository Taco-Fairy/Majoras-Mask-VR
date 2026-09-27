#pragma once
extern "C" {
#include "overlays/actors/ovl_Obj_Spidertent/z_obj_spidertent.h"
#include "overlays/actors/ovl_Obj_Syokudai/z_obj_syokudai.h"
void ObjSyokudai_Init(Actor*,PlayState*);
void ObjSyokudai_Update(Actor*,PlayState*);
void ObjSyokudai_Destroy(Actor*,PlayState*);
s32 func_80B30480(ObjSpidertent*,PlayState*,Vec3f*);
void ObjSpidertent_Init(Actor*,PlayState*);
void ObjSpidertent_Destroy(Actor*,PlayState*);
void ObjSpidertent_Update(Actor*,PlayState*);
void func_808388B8(PlayState*,Player*,PlayerTransformation);
void Player_InitItemActionWithAnim(PlayState*,Player*,PlayerItemAction);
}
static void NativeFlameHotfixTest(PlayState* play) {
 auto* p=GET_PLAYER(play);auto baseline=*p;auto saved=gSaveContext;auto settings=mmvr::GetSettings();
 bool flame=true,web=true,miss=true,unlit=true;
 mmvr::SetNativeTestTracking(true);mmvr::GetSettings().Set(mmvr::Setting::PhysicalSword,1);
 for(int left=0;left<2;++left){
  *p=baseline;p->actor.world.pos={0,2000,0};p->actor.velocity={};p->stateFlags1=p->stateFlags2=p->stateFlags3=0;
  p->heldActor=p->actor.child=nullptr;p->transformation=PLAYER_FORM_HUMAN;
  p->itemAction=p->heldItemAction=PLAYER_IA_NONE;p->heldItemId=ITEM_DEKU_STICK;
  p->nextModelGroup=Player_ActionToModelGroup(p,PLAYER_IA_DEKU_STICK);
  Player_InitItemActionWithAnim(play,p,PLAYER_IA_DEKU_STICK);p->unk_B28=120;p->unk_B0C=1;
  mmvrgame::ClearTracking();mmvrgame::ClearCombat();mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,left);
  mmvr::TrackingFrame f{};f.head.orientation.w=f.origin.orientation.w=1;f.epoch=29000+left;f.timeSeconds=29000+left;
  for(int h=0;h<2;++h){f.hands[h].orientation.w=f.aims[h].orientation.w=1;f.hands[h].position={h?.2f:-.2f,-.25f,-.4f};f.handTracked[h]=f.handValid[h]=f.aimValid[h]=true;}
  auto view=mmvr::YawPose(0,0,2045,0),head=mmvr::YawPose(0);mmvrgame::RecordTracking(f,view,head);
  auto model=mmvr::TrackedHandModel(f,view,head,0,1-left,mmvr::GetSettings());
  mmvrgame::UpdateSwordDiagnostics(f,model);
  Vec3f expected{};for(int k=0;k<3;++k)(&expected.x)[k]=model.m[3][k]+MMVR_NativeSwordLength(p)*model.m[0][k]+267.2f*model.m[1][k]-33.82f*model.m[2][k];
  flame&=Math3D_Vec3fDistSq(&expected,&p->meleeWeaponInfo[0].tip)<.001f;
  ObjSpidertent tent{};tent.collider.elements=tent.colliderElements;tent.collider.count=6;
  Vec3f a{expected.x-30,expected.y-30,expected.z},b{expected.x+30,expected.y-30,expected.z},c{expected.x,expected.y+30,expected.z},hit{};
  for(auto& element:tent.colliderElements)Math3D_TriNorm(&element.dim,&a,&b,&c);
  web&=func_80B30480(&tent,play,&hit)!=0;
  p->meleeWeaponInfo[0].tip.z+=20;miss&=func_80B30480(&tent,play,&hit)==0;
  p->meleeWeaponInfo[0].tip=expected;p->unk_B28=0;unlit&=func_80B30480(&tent,play,&hit)==0;
 }
 bool nativeMeshes=true;
 for(int type=0;type<2;++type){
  *p=baseline;p->actor.world.pos={0,2000,0};p->stateFlags1=p->stateFlags2=p->stateFlags3=0;
  p->heldActor=p->actor.child=nullptr;p->transformation=PLAYER_FORM_HUMAN;p->heldItemId=ITEM_DEKU_STICK;
  p->nextModelGroup=Player_ActionToModelGroup(p,PLAYER_IA_DEKU_STICK);Player_InitItemActionWithAnim(play,p,PLAYER_IA_DEKU_STICK);p->unk_B0C=1;p->unk_B28=120;
  ObjSpidertent tent{};tent.dyna.actor.id=ACTOR_OBJ_SPIDERTENT;tent.dyna.actor.update=ObjSpidertent_Update;tent.dyna.actor.params=0x7F00|type;tent.dyna.actor.world.pos={0,2000,0};
  Flags_UnsetSwitch(play,127);ObjSpidertent_Init(&tent.dyna.actor,play);DynaPoly_UpdateContext(play,&play->colCtx.dyna);
  auto& tri=tent.collider.elements[0].dim;Vec3f center{},normal=tri.plane.normal;
  for(int k=0;k<3;++k)(&center.x)[k]=((&tri.vtx[0].x)[k]+(&tri.vtx[1].x)[k]+(&tri.vtx[2].x)[k])/3;
  // Exercise the actor's actual six-triangle ignition mesh. Some web models
  // intentionally have no solid polygon beneath a given ignition triangle.
  for(int index=0;index<6;++index){
   auto& face=tent.collider.elements[index].dim;Vec3f flamePoint{};
   for(int k=0;k<3;++k)(&flamePoint.x)[k]=((&face.vtx[0].x)[k]+(&face.vtx[1].x)[k]+(&face.vtx[2].x)[k])/3;
   p->meleeWeaponInfo[0].tip=flamePoint;Vec3f burn{};
   nativeMeshes&=func_80B30480(&tent,play,&burn)!=0;
  }
  ObjSpidertent_Destroy(&tent.dyna.actor,play);DynaPoly_UpdateContext(play,&play->colCtx.dyna);
 }
 bool torchLighting=true;
 *p=baseline;p->heldItemAction=PLAYER_IA_DEKU_STICK;
 ObjSyokudai torch{};torch.actor.id=ACTOR_OBJ_SYOKUDAI;torch.actor.params=0x287F;torch.actor.world.pos={0,2000,0};
 ObjSyokudai_Init(&torch.actor,play);
 p->meleeWeaponInfo[0].tip={0,2067,0};p->unk_B28=0;
 ObjSyokudai_Update(&torch.actor,play);torchLighting&=p->unk_B28>0;
 torch.snuffTimer=0;ObjSyokudai_Update(&torch.actor,play);torchLighting&=torch.snuffTimer!=0;
 ObjSyokudai_Destroy(&torch.actor,play);
 *p=baseline;gSaveContext=saved;
 R_PLAY_FILL_SCREEN_ON=-20;R_PLAY_FILL_SCREEN_ALPHA=100;
 func_808388B8(play,p,PLAYER_FORM_DEKU);
 bool reset=R_PLAY_FILL_SCREEN_ON==0&&R_PLAY_FILL_SCREEN_ALPHA==0;
 std::ofstream("native-flame-hotfix.json")<<"{\"flameTip\":"<<flame<<",\"webIgnites\":"<<web<<",\"webMiss\":"<<miss<<",\"unlitRejected\":"<<unlit<<",\"transformFadeReset\":"<<reset<<",\"torchLighting\":"<<torchLighting<<",\"nativeMeshes\":"<<nativeMeshes<<"}";
 *p=baseline;gSaveContext=saved;mmvr::GetSettings()=settings;
 Ship::Context::GetRawInstance()->GetWindow()->Close();
}
