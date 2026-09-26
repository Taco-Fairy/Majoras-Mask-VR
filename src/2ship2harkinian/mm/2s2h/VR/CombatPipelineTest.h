#pragma once
#include "fast/Fast3dWindow.h"
#include "visibility.h"
#include "Bottle.h"
#include "bow_aim.h"
extern "C" {
#include "overlays/actors/ovl_En_Dekubaba/z_en_dekubaba.h"
#include "overlays/actors/ovl_En_Arrow/z_en_arrow.h"
#include "overlays/actors/ovl_En_Elf/z_en_elf.h"
#include "overlays/actors/ovl_Obj_Grass/z_obj_grass.h"
#include "overlays/actors/ovl_En_Kusa/z_en_kusa.h"
#include "overlays/actors/ovl_Arms_Hook/z_arms_hook.h"
#include "overlays/actors/ovl_Boss_Hakugin/z_boss_hakugin.h"
void BossHakugin_DrawMalfunctionEffects(BossHakugin*,PlayState*);
#include "overlays/actors/ovl_En_Poh/z_en_poh.h"
#include "overlays/actors/ovl_Obj_Aqua/z_obj_aqua.h"
#include "overlays/actors/ovl_En_Po_Sisters/z_en_po_sisters.h"
void func_80B2F37C(Actor*,PlayState*);
void ObjAqua_Draw(Actor*,PlayState*);
void EnPoSisters_Draw(Actor*,PlayState*);

void EnKusa_Init(Actor*,PlayState*);void EnKusa_Update(Actor*,PlayState*);void EnKusa_Destroy(Actor*,PlayState*);
void ArmsHook_Update(Actor*,PlayState*);void Player_ProcessItemButtons(Player*,PlayState*);
void EnDekubaba_Init(Actor*,PlayState*);void EnDekubaba_Destroy(Actor*,PlayState*);
void EnDekubaba_Update(Actor*,PlayState*);void EnDekubaba_UpdateDamage(EnDekubaba*,PlayState*);
void ObjGrass_InitDraw(ObjGrass*,PlayState*);void EnArrow_Update(Actor*,PlayState*);void func_8088A594(EnArrow*,PlayState*);
}
#include "ItemUseTest.h"
#include "ExchangeTest.h"
#include "ThrowJumpTest.h"
#include "ItemsFeedbackTest.h"
#include "BottleCampaignTest.h"
#include "GestureNativeTest.h"
#include "FormAbilitiesTest.h"
#include "FinCombatTest.h"
#include "MotionPickupTest.h"
#include "DeityCombatTest.h"
#include "TargetingTest.h"
#include "TrackedBodyTest.h"
#include "NativeActionsTest.h"
#include "ShieldReflectionTest.h"
#include "SpinCombatTest.h"
static void TestActorUpdate(Actor*,PlayState*){}
static void NativeCombatPipeline(PlayState* play){
 auto* p=GET_PLAYER(play);Player saved=*p;auto initialSave=gSaveContext;auto initialInput=*CONTROLLER1(&play->state);auto context=play->colChkCtx;auto settings=mmvr::GetSettings();
 // Player_UpdateCommon normally points this at a stack-local filtered Input.
 // This fixture calls native item functions between updates, so bind a live
 // input explicitly rather than reading the previous update's expired stack.
 auto* previousNativeInput=sPlayerControlInput;
 sPlayerControlInput=CONTROLLER1(&play->state);
 mmvr::SetNativeTestTracking(true);mmvr::GetSettings().Set(mmvr::Setting::PhysicalSword,1);mmvr::GetSettings().Set(mmvr::Setting::SwordDiagnostics,1);
 p->actor.world.pos={0,2000,0};p->actor.shape.rot={};p->stateFlags1=0;p->stateFlags2=0;p->stateFlags3=0;p->heldActor=nullptr;
 const ItemId swords[]={ITEM_SWORD_KOKIRI,ITEM_SWORD_RAZOR,ITEM_SWORD_GILDED,ITEM_SWORD_GREAT_FAIRY};
 std::ofstream log("native-combat-pipeline.json");log<<"{\"swords\":[";
 auto frame=mmvr::TrackingFrame{};frame.origin.orientation.w=frame.head.orientation.w=1;frame.epoch=100;
 for(int h=0;h<2;++h){frame.hands[h].orientation.w=frame.aims[h].orientation.w=1;frame.handValid[h]=frame.handTracked[h]=frame.aimValid[h]=true;}
 auto view=mmvr::YawPose(0,0,2045,0),head=mmvr::YawPose(0);
 for(int sword=0;sword<4;++sword)for(int miss=0;miss<2;++miss){
  mmvrgame::ClearTracking();MMVR_PlayerEquipSword(play,p,swords[sword]);
  EnDekubaba enemy{};enemy.actor.id=ACTOR_EN_DEKUBABA;enemy.actor.update=EnDekubaba_Update;enemy.actor.world.pos={0,2000,0};enemy.actor.home.pos=enemy.actor.world.pos;
  EnDekubaba_Init(&enemy.actor,play);enemy.actor.colChkInfo.health=8;enemy.collider.base.colMaterial=COL_MATERIAL_HIT0;enemy.collider.base.acFlags&=~AC_HARD;
  float length=MMVR_NativeSwordLength(p)*.01f;
  for(auto& element:enemy.colliderElements){element.dim.worldSphere.center={(s16)(length+(miss?12:-3)),2024,0};element.dim.worldSphere.radius=5;element.base.acElemFlags|=ACELEM_ON;}
  int contacts=0;float damage=0;
  for(int sample=0;sample<105;++sample){
   frame.timeSeconds=10+sword*4+miss*2+sample/90.0;float z=sample<30?-25:std::min(25.f,-25+(sample-30)*2.0f);
   frame.hands[mmvr::SwordController(mmvr::GetSettings())].position={0,-.5f,z/40};
   mmvrgame::RecordTracking(frame,view,head);
   auto model=mmvr::YawPose(0,0,2020,z);for(int k=0;k<3;++k)model.m[k][k]=.01f;
   mmvrgame::UpdateSwordDiagnostics(frame,model);
   mmvrgame::RecordTracking(frame,view,head);mmvrgame::UpdateSwordDiagnostics(frame,model); // Second late-latch pass at the same predicted display time.
   // Native animation is allowed to reset its WeaponInfo every tick.
   p->meleeWeaponInfo[1].active=false;
   if(sample%3==0){
    CollisionCheck_ClearContext(play,&play->colChkCtx);Collider_ResetJntSphAC(play,&enemy.collider.base);CollisionCheck_ResetDamage(&enemy.actor.colChkInfo);
    enemy.collider.base.acFlags|=AC_ON;CollisionCheck_SetAC(play,&play->colChkCtx,&enemy.collider.base);
    mmvrgame::ProcessCombatInput(play);MMVR_FilterAttackCollisions(play);CollisionCheck_AT(play,&play->colChkCtx);MMVR_AfterAttackCollision(play);CollisionCheck_Damage(play,&play->colChkCtx);
    if(enemy.collider.base.acFlags&AC_HIT){++contacts;damage=enemy.actor.colChkInfo.damage;EnDekubaba_UpdateDamage(&enemy,play);}
   }
  }
  if(sword||miss)log<<",";log<<"{\"weapon\":"<<sword+1<<",\"miss\":"<<miss<<",\"contacts\":"<<contacts<<",\"damage\":"<<damage<<",\"health\":"<<int(enemy.actor.colChkInfo.health)<<"}";
  EnDekubaba_Destroy(&enemy.actor,play);
 }
 log<<"],\"shield\":[";
 mmvrgame::ClearTracking();p->currentShield=PLAYER_SHIELD_HEROS_SHIELD;mmvr::GetSettings().Set(mmvr::Setting::PhysicalShield,1);
 frame.timeSeconds=40;frame.grips[1-mmvr::SwordController(mmvr::GetSettings())]=1;mmvrgame::RecordTracking(frame,view,head);
 auto shield=mmvr::YawPose(0,0,2030,14.39f);for(int k=0;k<3;++k)shield.m[k][k]=.01f;
 for(int miss=0;miss<2;++miss){
  mmvrgame::UpdateShield(frame,shield);CollisionCheck_ClearContext(play,&play->colChkCtx);Collider_ResetCylinderAC(play,&p->cylinder.base);
  Collider_UpdateCylinder(&p->actor,&p->cylinder);CollisionCheck_SetAC(play,&play->colChkCtx,&p->cylinder.base);
  Actor attacker{};attacker.id=ACTOR_EN_DEKUBABA;attacker.update=TestActorUpdate;
  ColliderCylinderInit init={{COL_MATERIAL_NONE,AT_ON|AT_TYPE_ENEMY,AC_NONE,OC1_NONE,OC2_NONE,COLSHAPE_CYLINDER},
   {ELEM_MATERIAL_UNK0,{DMG_SWORD,0,4},{0,0,0},ATELEM_ON,ACELEM_NONE,OCELEM_NONE},{10,8,0,{0,2026,(s16)(miss?-8:8)}}};
  ColliderCylinder attack;Collider_InitAndSetCylinder(play,&attack,&attacker,&init);CollisionCheck_SetAT(play,&play->colChkCtx,&attack.base);
  MMVR_FilterAttackCollisions(play);CollisionCheck_AT(play,&play->colChkCtx);
  bool blocked=p->shieldQuad.base.acFlags&AC_BOUNCED;MMVR_AfterAttackCollision(play);
  if(miss)log<<",";log<<"{\"miss\":"<<miss<<",\"blocked\":"<<blocked<<",\"bodyHit\":"<<bool(p->cylinder.base.acFlags&AC_HIT)<<"}";
  Collider_DestroyCylinder(play,&attack);
 }
 log<<"],\"grass\":";
 ObjGrass grass{};grass.activeGrassGroups=1;grass.grassGroups[0].count=2;grass.grassGroups[0].homePos={20000,0,20000};
 grass.grassGroups[0].elements[1].flags=OBJ_GRASS_ELEM_REMOVED;
 ObjGrass_InitDraw(&grass,play);log<<bool((grass.grassGroups[0].flags&OBJ_GRASS_GROUP_DRAW)&&(grass.grassGroups[0].elements[0].flags&OBJ_GRASS_ELEM_DRAW)&&grass.grassGroups[0].elements[0].alpha==255&&!(grass.grassGroups[0].elements[1].flags&OBJ_GRASS_ELEM_DRAW));
 log<<",\"grassCut\":";
 mmvrgame::ClearTracking();MMVR_PlayerEquipSword(play,p,ITEM_SWORD_GILDED);
 EnKusa bush{};bush.actor.id=ACTOR_EN_KUSA;bush.actor.update=EnKusa_Update;bush.actor.world.pos={0,0,0};EnKusa_Init(&bush.actor,play);
 bush.actor.world.pos={25,2020,0};bush.actor.xzDistToPlayer=25;bool grassCut=false;
 for(int sample=0;sample<100&&!grassCut;++sample){
  frame.timeSeconds=42+sample/90.0;frame.epoch=190;float z=sample<25?-25:std::min(25.f,-25+(sample-25)*1.2f);frame.hands[1].position={0,-.5f,z/40};
  mmvrgame::RecordTracking(frame,view,head);auto model=mmvr::YawPose(0,0,2020,z);for(int k=0;k<3;++k)model.m[k][k]=.01f;
  mmvrgame::UpdateSwordDiagnostics(frame,model);mmvrgame::UpdateSwordDiagnostics(frame,model);
  if(sample%3==0){CollisionCheck_ClearContext(play,&play->colChkCtx);Collider_UpdateCylinder(&bush.actor,&bush.collider);CollisionCheck_SetAC(play,&play->colChkCtx,&bush.collider.base);
   mmvrgame::ProcessCombatInput(play);MMVR_FilterAttackCollisions(play);CollisionCheck_AT(play,&play->colChkCtx);MMVR_AfterAttackCollision(play);CollisionCheck_Damage(play,&play->colChkCtx);
   bool contact=(bush.collider.base.acFlags&AC_HIT)!=0;EnKusa_Update(&bush.actor,play);grassCut=contact&&(bush.actor.update==nullptr||bush.isCut);
  }
 }
 EnKusa_Destroy(&bush.actor,play);log<<grassCut;
 log<<",\"bow\":[";
 auto savedSave=gSaveContext;auto savedInput=*CONTROLLER1(&play->state);mmvr::GetSettings().Set(mmvr::Setting::PhysicalBow,1);
 const int bowItems[]={ITEM_BOW,ITEM_ARROW_FIRE,ITEM_ARROW_ICE,ITEM_ARROW_LIGHT};
 for(int variant=0;variant<4;++variant){
  mmvrgame::ClearTracking();mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,variant%2);int dominant=mmvr::SwordController(mmvr::GetSettings());
  MMVR_PlayerEquipBow(play,p,bowItems[variant]);BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_C_DOWN)=bowItems[variant];AMMO(ITEM_BOW)=20;
  gSaveContext.magicState=MAGIC_STATE_IDLE;gSaveContext.save.saveInfo.playerData.magic=48;
  frame.grips[0]=frame.grips[1]=0;
  auto model=mmvr::YawPose(0,.35f,2028.95f,0);for(int k=0;k<3;++k)model.m[k][k]=.01f;
  bool stringTracks=true;bool prematureAmmo=false;
  for(int sample=0;sample<95;++sample){
   frame.timeSeconds=50+variant*2+sample/90.0;frame.epoch=200+variant;
   float pull=sample<18?0.f:std::min(.45f,(sample-18)/90.f);float trigger=sample>=10&&sample<75?1.f:0.f;
   frame.triggers[dominant]=trigger;frame.triggers[1-dominant]=0;
   frame.hands[dominant].position={0,-.5f,pull};frame.hands[1-dominant].position={0,-.5f,0};
   frame.aims[1-dominant]=frame.hands[1-dominant];
   auto& input=*CONTROLLER1(&play->state);input.cur.button=trigger>0?(dominant?BTN_CDOWN:BTN_Z):0;input.press.button=sample==12?input.cur.button:0;
   mmvrgame::RecordTracking(frame,view,head);mmvrgame::UpdateBow(frame,view,head,model);mmvrgame::UpdateBow(frame,view,head,model);
   if(sample>=20&&sample<75){auto string=mmvrgame::BowStringPose();stringTracks&=std::abs(-800*string.m[1][2]+string.m[3][2]-pull*40)<.01f;}
   if(sample%3==0)mmvrgame::ProcessBowInput(play);
   if(sample<75&&AMMO(ITEM_BOW)!=20)prematureAmmo=true;
  }
  Actor* arrow=nullptr;for(auto* a=play->actorCtx.actorLists[ACTORCAT_ITEMACTION].first;a;a=a->next)if(a->id==ACTOR_EN_ARROW&&a->update&&((EnArrow*)a)->vrReleasePower>0){arrow=a;break;}
  bool launched=false;int type=-1;float speed=0;int yaw=0;
  if(arrow){if(arrow->init){arrow->init(arrow,play);arrow->init=nullptr;}func_8088A594((EnArrow*)arrow,play);launched=arrow->parent==nullptr&&arrow->speed>0;type=arrow->params;speed=arrow->speed;yaw=arrow->world.rot.y;Actor_Kill(arrow);}
  if(variant)log<<",";log<<"{\"variant\":"<<variant<<",\"hand\":"<<dominant<<",\"ammo\":"<<int(AMMO(ITEM_BOW))<<",\"earlyAmmo\":"<<prematureAmmo<<",\"launched\":"<<launched<<",\"type\":"<<type<<",\"speed\":"<<speed<<",\"yaw\":"<<yaw<<",\"stringTracks\":"<<stringTracks<<"}";
 }
 gSaveContext=savedSave;*CONTROLLER1(&play->state)=savedInput;log<<"]";
 mmvrgame::ClearTracking();p->heldActor=nullptr;mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,0);
 BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_C_DOWN)=ITEM_HOOKSHOT;C_SLOT_EQUIP(0,EQUIP_SLOT_C_DOWN)=SLOT_HOOKSHOT;p->heldItemButton=EQUIP_SLOT_C_DOWN;
 std::ofstream hookDiagnostic("native-hook-state.json");
 auto hookState=[&](const char* name){hookDiagnostic<<"\""<<name<<"\":{\"item\":"<<int(p->itemAction)<<",\"held\":"<<int(p->heldItemAction)<<",\"form\":"<<int(p->transformation)<<",\"actors\":"<<int(play->actorCtx.totalLoadedActors)<<",\"actor\":"<<bool(p->heldActor)<<",\"flags1\":"<<p->stateFlags1<<",\"flags3\":"<<p->stateFlags3<<"}";};
 hookDiagnostic<<"{";hookState("before");
 MMVR_PlayerEquipHookshot(play,p);hookDiagnostic<<",";hookState("after");auto* hook=p->heldActor;bool hookFree=false,hookFired=false;
 bool hookStable=true;for(int tick=0;tick<60;++tick){Player_ProcessItemButtons(p,play);hookStable&=p->heldItemAction==PLAYER_IA_HOOKSHOT&&!(p->stateFlags3&PLAYER_STATE3_START_CHANGING_HELD_ITEM);}
 hookDiagnostic<<",";hookState("stable");hookDiagnostic<<"}";hookDiagnostic.flush();
 if(hook){if(hook->init){hook->init(hook,play);hook->init=nullptr;}frame.timeSeconds=70;frame.epoch=300;
  frame.aims[1].orientation={0,.70710678f,0,.70710678f};mmvrgame::RecordTracking(frame,view,head);
  hookFree=MMVR_IndependentHookshot(p)&&p->unk_AA5==PLAYER_UNKAA5_0;
  // X must fire the held hook even when native C-right has a bow assignment.
  BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_C_RIGHT)=ITEM_BOW;C_SLOT_EQUIP(0,EQUIP_SLOT_C_RIGHT)=SLOT_BOW;
  auto hookInput=*CONTROLLER1(&play->state);CONTROLLER1(&play->state)->cur.button=CONTROLLER1(&play->state)->press.button=BTN_CRIGHT;
  MMVR_ProcessInteractions(play);hookFired=MMVR_IndependentHookshot(p)&&p->heldActor==nullptr&&hook->parent==nullptr;
  *CONTROLLER1(&play->state)=hookInput;
  if(hookFired){ArmsHook_Update(hook,play);hookFired=hook->speed>0&&std::abs(hook->world.rot.y+16384)<2;}
  Actor_Kill(hook);p->heldActor=nullptr;p->actor.child=nullptr;
 }
 MMVR_PlayerEmptyHands(play,p);log<<",\"hookshot\":{\"free\":"<<hookFree<<",\"fired\":"<<hookFired<<",\"empty\":"<<(p->heldItemAction==PLAYER_IA_NONE)<<"}";
 log<<",\"hookStable\":"<<hookStable;
 log<<",\"billboards\":";
 auto window=std::dynamic_pointer_cast<Fast::Fast3dWindow>(Ship::Context::GetRawInstance()->GetWindow());
 auto interpreter=window->GetInterpreterWeak().lock();auto rsp=*interpreter->mRsp;auto* replacements=interpreter->mCurMtxReplacements;bool framebuffer=interpreter->mFbActive;
 std::unordered_map<Mtx*,MtxF> none;interpreter->mCurMtxReplacements=&none;interpreter->mFbActive=false;
 bool billboardOk=true;Mtx parent{},billboard{};auto basis=mmvr::YawPose(.3f);auto parentMatrix=mmvr::YawPose(0,123,45,-67);
 Matrix_MtxFToMtx((MtxF*)&parentMatrix,&parent);Matrix_MtxFToMtx((MtxF*)&basis,&billboard);
 MMVR_SetBillboardMatrix(&billboard,&basis.m[0][0],0,0,0);
 for(float yaw:{0.f,1.5707963f,3.14159265f,-1.5707963f}){
  mmvr::SetNativeTestEye(yaw);interpreter->GfxSpMatrix(G_MTX_LOAD,(int32_t*)&parent);
  // This is the previously missed native global billboard G_MTX_MUL path.
  interpreter->GfxSpMatrix(G_MTX_MUL,(int32_t*)&billboard);
  auto expected=mmvr::YawPose(yaw,123,45,-67);auto& actual=interpreter->mRsp->modelview_matrix_stack[interpreter->mRsp->modelview_matrix_stack_size-1];
  for(int row=0;row<4;++row)for(int col=0;col<4;++col)billboardOk&=std::abs(actual[row][col]-expected.m[row][col])<.002f;
  // Use the game's actual last-drawn segment-1 yaw matrix, not a fixture
  // registration. This catches omission of the second native billboard slot.
  billboardOk &= play->billboardMtx != nullptr;
  if (play->billboardMtx) {
   interpreter->GfxSpMatrix(G_MTX_LOAD,(int32_t*)&parent);
   interpreter->GfxSpMatrix(G_MTX_MUL,(int32_t*)(play->billboardMtx+1));
   auto& cylindrical=interpreter->mRsp->modelview_matrix_stack[interpreter->mRsp->modelview_matrix_stack_size-1];
   for(int row=0;row<4;++row)for(int col=0;col<4;++col)billboardOk&=std::abs(cylindrical[row][col]-expected.m[row][col])<.002f;
  }
 }
 // Smoke uses a yaw-only billboard, including a singular Z scale. Preserve
 // interpolation's center and scale while replacing the game's camera yaw.
 Mtx smokeAddress{};
 for(float nativeYaw:{-.9f,.2f})for(float eyeYaw:{-.4f,1.2f,2.7f}){
  auto smoke=mmvr::YawPose(nativeYaw,80,140,-200);for(int k=0;k<3;++k){smoke.m[0][k]*=.1f;smoke.m[1][k]*=.2f;smoke.m[2][k]=0;}
  MMVR_SetYawBillboardMatrix(&smokeAddress,nativeYaw,80,140,-200);mmvr::SetNativeTestEye(eyeYaw);
  auto expected=mmvr::YawPose(eyeYaw,80,140,-200);for(int k=0;k<3;++k){expected.m[0][k]*=.1f;expected.m[1][k]*=.2f;expected.m[2][k]=0;}
  billboardOk&=mmvr::OverrideBillboardMatrix(&smokeAddress,smoke.m);
  for(int row=0;row<4;++row)for(int col=0;col<4;++col)billboardOk&=std::abs(smoke.m[row][col]-expected.m[row][col])<.002f;
 }
 // Exercise Goht's actual steam draw path: manually registering a fixture
 // matrix would fail to catch an omitted actor-side registration.
 {
  auto boss=std::make_unique<BossHakugin>();
  auto* gfx=play->state.gfxCtx;
  const auto savedXlu=gfx->polyXlu;
  Matrix_Push();
  for(float scale:{.15f,.65f}){
   auto& steam=boss->malfunctionEffects[0][0];
   steam.pos={71,143,-207};steam.scaleXY=scale;steam.alpha=190;steam.timer=17;
   Gfx* begin=gfx->polyXlu.p;
   BossHakugin_DrawMalfunctionEffects(boss.get(),play);
   unsigned matrixCount=0;
   for(Gfx* command=begin;command<gfx->polyXlu.p;++command){
    if(((command->words.w0>>24)&0xff)!=G_MTX)continue;
    auto* address=reinterpret_cast<Mtx*>(command->words.w1);
    MtxF native;Matrix_MtxToMtxF(address,&native);++matrixCount;
    for(float eyeYaw:{-.8f,.45f,2.4f}){
     mmvr::SetNativeTestEye(eyeYaw);
     mmvr::Matrix corrected;std::memcpy(&corrected,&native,sizeof(native));
     billboardOk&=mmvr::OverrideBillboardMatrix(address,corrected.m);
     auto expected=mmvr::YawPose(eyeYaw,71,143,-207);
     for(int k=0;k<3;++k){expected.m[0][k]*=scale;expected.m[1][k]*=scale;}
     for(int row=0;row<4;++row)for(int col=0;col<4;++col)
      billboardOk&=std::abs(corrected.m[row][col]-expected.m[row][col])<.002f;
    }
   }
   billboardOk&=matrixCount==1;
  }
  Matrix_Pop();gfx->polyXlu=savedXlu;
 }
 // Exercise actual native effect draws, preserving the authored materials.
 {
  auto* gfx=play->state.gfxCtx;const auto savedXlu=gfx->polyXlu,savedOpa=gfx->polyOpa;
  Matrix_Push();
  for(int effect=0;effect<3;++effect)for(float scale:{.015f,.065f}){
   Matrix_Translate(71,143,-207,MTXMODE_NEW);Matrix_Scale(scale,scale,scale,MTXMODE_APPLY);
   Gfx* begin=gfx->polyXlu.p;
   if(effect==0){auto actor=std::make_unique<EnPoh>();actor->unk_197=190;func_80B2F37C(&actor->actor,play);}
   else if(effect==1){ObjAqua actor{};actor.alpha=190;ObjAqua_Draw(&actor.actor,play);}
   else{
    auto actor=std::make_unique<EnPoSisters>();actor->actor.scale={scale*2,scale*2,scale*2};
    actor->fireCount=1;actor->firePos[0]={71,143,-207};actor->poSisterFlags=1<<7;
    SkelAnime_Init(play,&actor->skelAnime,(SkeletonHeader*)gPoeSistersSkel,(AnimationHeader*)gPoeSistersSwayAnim,nullptr,nullptr,POE_SISTERS_LIMB_MAX);
    EnPoSisters_Draw(&actor->actor,play);SkelAnime_Free(&actor->skelAnime,play);
   }
   unsigned matrixCount=0;
   for(Gfx* command=begin;command<gfx->polyXlu.p;++command){
    if(((command->words.w0>>24)&0xff)!=G_MTX)continue;
    auto* address=reinterpret_cast<Mtx*>(command->words.w1);MtxF native;Matrix_MtxToMtxF(address,&native);++matrixCount;
    for(float eyeYaw:{-.8f,.45f,2.4f}){
     mmvr::SetNativeTestEye(eyeYaw);mmvr::Matrix corrected;std::memcpy(&corrected,&native,sizeof(native));
     billboardOk&=mmvr::OverrideBillboardMatrix(address,corrected.m);
     auto expected=mmvr::YawPose(eyeYaw,71,143,-207);
     for(int row=0;row<3;++row)for(int col=0;col<3;++col)expected.m[row][col]*=scale;
     for(int row=0;row<4;++row)for(int col=0;col<4;++col)billboardOk&=std::abs(corrected.m[row][col]-expected.m[row][col])<.002f;
    }
   }
   billboardOk&=matrixCount==1;
  }
  Matrix_Pop();gfx->polyXlu=savedXlu;gfx->polyOpa=savedOpa;
 }
 log<<billboardOk;
 *interpreter->mRsp=rsp;interpreter->mCurMtxReplacements=replacements;interpreter->mFbActive=framebuffer;
 log<<",\"trackedBlade\":[";
 // Use exactly the camera's calibrated controller -> model transform. Both
 // handedness choices, every sword, no deliberate rests between strokes.
 for(int sword=0;sword<4;++sword)for(int left=0;left<2;++left){
  mmvrgame::ClearTracking();mmvr::GetSettings()=settings;mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,left);mmvr::GetSettings().Set(mmvr::Setting::PhysicalSword,1);
  p->heldActor=nullptr;p->stateFlags1=p->stateFlags2=p->stateFlags3=0;p->heldItemAction=p->itemAction=PLAYER_IA_NONE;
  MMVR_PlayerEquipSword(play,p,swords[sword]);int hand=mmvr::SwordController(mmvr::GetSettings());
  auto point=[](const mmvr::Matrix& m,float x,float y){return Vec3f{x*m.m[0][0]+y*m.m[1][0]+m.m[3][0],x*m.m[0][1]+y*m.m[1][1]+m.m[3][1],x*m.m[0][2]+y*m.m[1][2]+m.m[3][2]};};
  frame.epoch=400+sword*2+left;frame.hands[hand].orientation={0,0,0,1};frame.hands[hand].position={0,-.45f,-.35f};
  frame.triggers[0]=frame.triggers[1]=frame.grips[0]=frame.grips[1]=0;
  auto basis=mmvr::YawPose(left?1.2f:0.f,0,2045,0);
  auto centerModel=mmvr::TrackedHandModel(frame,basis,head,0,hand,mmvr::GetSettings());
  auto center=point(centerModel,(350+MMVR_NativeSwordLength(p))*.5f,230);
  Actor target{};target.id=ACTOR_EN_KUSA;target.update=TestActorUpdate;target.world.pos=center;
  ColliderCylinderInit init={{COL_MATERIAL_NONE,AT_NONE,AC_ON|AC_TYPE_PLAYER,OC1_NONE,OC2_NONE,COLSHAPE_CYLINDER},
   {ELEM_MATERIAL_UNK0,{0,0,0},{DMG_SWORD,0,0},ATELEM_NONE,ACELEM_ON,OCELEM_NONE},{4,8,0,{(s16)center.x,(s16)(center.y-4),(s16)center.z}}};
  ColliderCylinder cylinder;Collider_InitAndSetCylinder(play,&cylinder,&target,&init);
  int contacts=0,wrongDamage=0;double lastContact=-100;bool cooldown=true;
  for(int sample=0;sample<270;++sample){
   frame.timeSeconds=100+sword*8+left*4+sample/90.;frame.hands[hand].position.x=.3f*std::sin(sample/90.f*6.2831853f*2.5f);
   mmvrgame::RecordTracking(frame,basis,head);auto model=mmvr::TrackedHandModel(frame,basis,head,0,hand,mmvr::GetSettings());
   mmvrgame::UpdateSwordDiagnostics(frame,model);mmvrgame::UpdateSwordDiagnostics(frame,model);
   if(sample%3==0){
    CollisionCheck_ClearContext(play,&play->colChkCtx);Collider_ResetCylinderAC(play,&cylinder.base);CollisionCheck_ResetDamage(&target.colChkInfo);CollisionCheck_SetAC(play,&play->colChkCtx,&cylinder.base);
    mmvrgame::ProcessCombatInput(play);MMVR_FilterAttackCollisions(play);CollisionCheck_AT(play,&play->colChkCtx);MMVR_AfterAttackCollision(play);CollisionCheck_Damage(play,&play->colChkCtx);
    if(cylinder.base.acFlags&AC_HIT){++contacts;wrongDamage+=target.colChkInfo.damage!=sword+1;cooldown&=frame.timeSeconds-lastContact>=.24;lastContact=frame.timeSeconds;}
   }
  }
  if(sword||left)log<<",";log<<"{\"weapon\":"<<sword+1<<",\"left\":"<<left<<",\"contacts\":"<<contacts<<",\"wrongDamage\":"<<wrongDamage<<",\"cooldown\":"<<cooldown<<"}";
  Collider_DestroyCylinder(play,&cylinder);
 }
 log<<"]";
 // Held solid geometry and billboards receive one identical late-pose translation.
 mmvr::CameraFrame held;held.active=held.heldActorActive=true;held.heldActorCorrection=mmvr::YawPose(0,20,30,40);
 mmvr::SetNativeTestCamera(held);mmvr::SetNativeTestEye(.9f);
 Mtx heldAddress[2]{};mmvr::SetHeldActorRange(&heldAddress[0],&heldAddress[2]);
 auto bomb=mmvr::YawPose(.3f,1,2,3);auto sprite=mmvr::YawPose(.3f,-8,-10,-12),cap=sprite; // Deliberately stale interpolated pose.
 MMVR_SetBillboardMatrix(&heldAddress[0],&basis.m[0][0],1,2,3);
 mmvr::OverrideModelMatrix(&heldAddress[0],sprite.m,bomb.m);mmvr::OverrideModelMatrix(&heldAddress[1],cap.m,bomb.m);
 bool heldSprite=true;for(int k=0;k<3;++k)heldSprite&=std::abs(sprite.m[3][k]-cap.m[3][k])<.001f&&std::abs(sprite.m[3][k]-(k+1)*11.f-10.f)<.001f;
 log<<",\"heldSprite\":"<<heldSprite;mmvr::SetNativeTestCamera({});mmvr::SetHeldActorRange(nullptr,nullptr);
 // A catch requires a qualified tracked scoop, a native offer and an empty assigned bottle.
 auto bottleSave=gSaveContext;*p=saved;p->actor.world.pos={0,2000,0};p->stateFlags1=p->stateFlags2=p->stateFlags3=0;p->heldActor=nullptr;p->heldItemAction=p->itemAction=PLAYER_IA_NONE;
 mmvr::GetSettings()=settings;mmvr::GetSettings().Set(mmvr::Setting::PhysicalBottle,1);mmvrgame::ClearTracking();
 BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_C_DOWN)=ITEM_BOTTLE;C_SLOT_EQUIP(0,EQUIP_SLOT_C_DOWN)=SLOT_BOTTLE_1;gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_1]=ITEM_BOTTLE;p->heldItemButton=EQUIP_SLOT_C_DOWN;
 MMVR_PlayerEquipEmptyBottle(play,p);int bottleHand=mmvr::SwordController(mmvr::GetSettings());frame.epoch=550;
 frame.hands[bottleHand].orientation={0,0,0,1};frame.hands[bottleHand].position={0,-.5f,-.3f};
 auto bottleModel=mmvr::TrackedHandModel(frame,view,head,0,bottleHand,mmvr::GetSettings());
 Actor fairy{};fairy.id=ACTOR_EN_ELF;fairy.params=FAIRY_PARAMS(FAIRY_TYPE_2,false,0);fairy.update=TestActorUpdate;
 for(int k=0;k<3;++k)(&fairy.world.pos.x)[k]=mmvrgame::BottleMouth.x*bottleModel.m[0][k]+mmvrgame::BottleMouth.y*bottleModel.m[1][k]+mmvrgame::BottleMouth.z*bottleModel.m[2][k]+bottleModel.m[3][k];
 fairy.xzDistToPlayer=1000;bool caught=false,restCaught=false;
 for(int sample=0;sample<85&&!caught;++sample){
  frame.timeSeconds=200+sample/90.;frame.hands[bottleHand].position.x=sample<20?-.3f:std::min(.3f,-.3f+(sample-20)*.018f);
  mmvrgame::RecordTracking(frame,view,head);bottleModel=mmvr::TrackedHandModel(frame,view,head,0,bottleHand,mmvr::GetSettings());mmvrgame::UpdateBottle(frame,bottleModel);
  if(sample%3==0)caught=Actor_OfferGetItem(&fairy,play,GI_MAX,80,60)!=0;
  if(sample<20)restCaught|=caught;
 }
 log<<",\"bottle\":{\"caught\":"<<caught<<",\"restCaught\":"<<restCaught<<",\"inventory\":"<<(gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_1]==ITEM_FAIRY)<<",\"parent\":"<<(fairy.parent==&p->actor)<<",\"fullBlocked\":"<<!MMVR_IndependentBottle(p)<<"}";
 gSaveContext=bottleSave;
 NativeItemUseTest(play,saved,log);
 NativeExchangeTest(play,saved,log);
 NativeThrowJumpTest(play,saved,log);
 NativeSpinCombatTest(play,saved,log);NativeItemsFeedbackTest(play,saved,log);NativeBottleCampaignTest(play,saved,log);
 NativeGestureTest(play,saved,log);NativeFormAbilitiesTest(play,saved,log);NativeFinCombatTest(play,saved,log);NativeMotionPickupTest(play,saved,log);NativeDeityTest(play,saved,log);NativeDeityTriggerTest(play,saved,log);NativeTargetingTest(play,saved,log);NativeTrackedBodyTest(play,saved,log);NativeActionTest(play,saved,log);NativeShieldReflectionTest(play,saved,log);
 log<<"}";log.flush();
 sPlayerControlInput=previousNativeInput;
 mmvrgame::ClearTracking();*p=saved;gSaveContext=initialSave;*CONTROLLER1(&play->state)=initialInput;play->colChkCtx=context;mmvr::GetSettings()=settings;mmvr::SetNativeTestTracking(false);
 for(int slot=0;slot<4;++slot)Interface_LoadItemIcon(play,slot);
}
