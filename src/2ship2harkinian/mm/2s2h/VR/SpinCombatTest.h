#pragma once
extern "C" void Magic_Update(PlayState*);
extern "C" void ObjGrass_UpdateGrass(ObjGrass*, PlayState*);
extern "C" void ObjGrass_ProcessColliders(ObjGrass*, PlayState*);
extern "C" {
#include "overlays/actors/ovl_En_Kusa2/z_en_kusa2.h"
void EnKusa2_Init(Actor*,PlayState*);
void EnKusa2_Destroy(Actor*,PlayState*);
void EnKusa2_Update(Actor*,PlayState*);
s32 func_80A5BFD8(EnKusa2*,PlayState*);
}
// Exercise native magical volume at natural hand height, with actual Keaton actors.
static void NativeSpinCombatTest(PlayState* play,const Player& baseline,std::ostream& log){
 auto* p=GET_PLAYER(play);auto saved=*p;auto save=gSaveContext;auto settings=mmvr::GetSettings();auto collision=play->colChkCtx;auto input=*CONTROLLER1(&play->state);
 log<<",\"chargedSpins\":[";
 for(int nativeHz:{20,30})for(int tier=1;tier<=2;++tier)for(int automatic=0;automatic<2;++automatic)for(int left=0;left<2;++left){
  *p=baseline;gSaveContext=save;*CONTROLLER1(&play->state)={};p->actor.world.pos={0,2000,0};p->actor.shape.rot={};p->stateFlags1=p->stateFlags2=p->stateFlags3=0;p->csAction=PLAYER_CSACTION_NONE;p->heldActor=nullptr;p->itemAction=p->heldItemAction=PLAYER_IA_NONE;p->actor.bgCheckFlags|=BGCHECKFLAG_GROUND;
  mmvr::GetSettings().Set(mmvr::Setting::SpinChargeTime,tier==2?1.f:2.f);mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,left);mmvr::GetSettings().Set(mmvr::Setting::TriggerSpinTurn,automatic);mmvrgame::ClearTracking();MMVR_PlayerEquipSword(play,p,ITEM_SWORD_GILDED);
  gSaveContext.save.saveInfo.playerData.isMagicAcquired=1;gSaveContext.save.saveInfo.playerData.magic=20;gSaveContext.magicState=MAGIC_STATE_IDLE;CLEAR_WEEKEVENTREG(WEEKEVENTREG_DRANK_CHATEAU_ROMANI);SET_WEEKEVENTREG(WEEKEVENTREG_RECEIVED_GREAT_SPIN_ATTACK);
  EnDekubaba enemy{};enemy.actor.id=ACTOR_EN_DEKUBABA;enemy.actor.update=EnDekubaba_Update;enemy.actor.world.pos={0,2000,0};enemy.actor.home.pos=enemy.actor.world.pos;
  EnDekubaba_Init(&enemy.actor,play);enemy.actor.colChkInfo.health=8;enemy.collider.base.colMaterial=COL_MATERIAL_HIT0;enemy.collider.base.acFlags&=~AC_HARD;
  for(auto& e:enemy.colliderElements){e.dim.worldSphere.center={30,2024,0};e.dim.worldSphere.radius=5;e.base.acElemFlags|=ACELEM_ON;}
  mmvr::TrackingFrame f{};f.head.orientation.w=f.origin.orientation.w=1;f.epoch=91000+left;
  for(int h=0;h<2;++h){f.hands[h].orientation.w=f.aims[h].orientation.w=1;f.handValid[h]=f.handTracked[h]=f.aimValid[h]=true;}
  auto view=mmvr::YawPose(0,0,2045,0),head=mmvr::YawPose(0);int early=0,hits=0;bool flags=true;int effects=0,volumeFrames=0;float turn=0;int coverage=0,expectedCoverage=0;
  std::array<Actor,420> contactOwners{}, reachOwners{};int reachHits=0,reachChecks=0,expiredHits=0;
  ColliderCylinderInit coverageInit={{COL_MATERIAL_NONE,AT_NONE,AC_ON|AC_TYPE_PLAYER,OC1_NONE,OC2_NONE,COLSHAPE_CYLINDER},
   {ELEM_MATERIAL_UNK0,{0,0,0},{0xFFFFFFFF,0,0},ATELEM_NONE,ACELEM_ON,OCELEM_NONE},{2,4,0,{0,0,0}}};
  ColliderCylinder coverageCollider;Collider_InitAndSetCylinder(play,&coverageCollider,&contactOwners[0],&coverageInit);
  ColliderCylinder reachCollider;Collider_InitAndSetCylinder(play,&reachCollider,&reachOwners[0],&coverageInit);
  // Fixed world-space dense patch: ten groups exceed both native pool limits.
  // Unlike the blade-following diagnostic, these targets never move with it.
  ObjGrass patch{}; patch.actor.id=ACTOR_OBJ_GRASS; patch.actor.update=[](Actor*,PlayState*){}; patch.activeGrassGroups=10;
  for(int g=0;g<10;++g) {
   auto& group=patch.grassGroups[g];group.count=10;
   for(int e=0;e<10;++e) {
    const float angle=(g*10+e)*6.28318530718f/100.f;
    auto& element=group.elements[e];float radius=tier==2?95.f:75.f;
    element.pos={std::cos(angle)*radius,2000,std::sin(angle)*radius};element.dropTable=0x10;
    group.homePos.x+=element.pos.x/10;group.homePos.y=2000;group.homePos.z+=element.pos.z/10;
   }
  }
  auto grassInit=coverageInit;grassInit.dim.radius=6;grassInit.dim.height=44;
  for(auto& collider:patch.grassElemColliders) Collider_InitAndSetCylinder(play,&collider.collider,&patch.actor,&grassInit);
  int grassCut=0,firstSpinGrass=0;
  std::array<EnKusa2,11> keaton{};int keatonCut=0,keatonOutside=0;
  for(int k=0;k<11;++k) {
   auto& grass=keaton[k];grass.actor.id=ACTOR_EN_KUSA2;grass.actor.params=1;
   grass.actor.update=EnKusa2_Update;
   float angle=k*6.28318530718f/8.f;
   const float radius=k<8?80.f:k==9?float(tier==2?150:120):0.f;
   grass.actor.world.pos={std::cos(angle)*radius,100,std::sin(angle)*radius};
   EnKusa2_Init(&grass.actor,play);
   // Keep real init/collider/native cut behavior but place the isolated actors
   // on this test's elevated plane, clear of unrelated arena scenery.
   grass.actor.update=EnKusa2_Update;grass.actor.world.pos.y=k==10?2140.f:2000.f;
   Collider_UpdateCylinder(&grass.actor,&grass.collider);
  }
  std::vector<Actor*> old;for(auto* a=play->actorCtx.actorLists[ACTORCAT_ITEMACTION].first;a;a=a->next)old.push_back(a);
  for(int i=0;i<420;++i){
   f.timeSeconds=91000+left*5+i/90.;int hand=1-left;float z=automatic?25.f:i<130?-25:std::min(25.f,-25+(i-130)*2.f);f.hands[hand].position={0,-.5f,z/40};f.triggers[hand]=((i>=5&&i<130)||(i>=200&&i<325))?1:0;
   mmvrgame::RecordTracking(f,view,head);turn+=mmvrgame::AdvanceSpinTurn(f);auto model=mmvr::Multiply(mmvr::YawPose(0,0,2032,z),mmvr::YawPose(turn));for(int row=0;row<3;++row)for(int col=0;col<3;++col)model.m[row][col]*=.01f;mmvrgame::UpdateSwordDiagnostics(f,model);
   if(i==0 || i*nativeHz/90 != (i-1)*nativeHz/90){
    if(automatic) {
     ObjGrass_ProcessColliders(&patch,play);
     if(i==234)for(int g=0;g<10;++g)for(int e=0;e<10;++e)firstSpinGrass+=(patch.grassGroups[g].elements[e].flags&OBJ_GRASS_ELEM_REMOVED)!=0;
    }
    CollisionCheck_ClearContext(play,&play->colChkCtx);
    if(automatic) ObjGrass_UpdateGrass(&patch,play);
    for(auto& grass:keaton) if(grass.actor.update) CollisionCheck_SetAC(play,&play->colChkCtx,&grass.collider.base);
    Collider_ResetJntSphAC(play,&enemy.collider.base);CollisionCheck_ResetDamage(&enemy.actor.colChkInfo);enemy.collider.base.acFlags|=AC_ON;CollisionCheck_SetAC(play,&play->colChkCtx,&enemy.collider.base);
    bool checkCoverage=i>=132 && i<=225;
    if(checkCoverage) {
     auto* blade=mmvrgame::MeleeDebugCollider();
     if(blade){
      const auto& q=reinterpret_cast<ColliderQuad*>(blade)->dim.quad;
      Vec3f middle{};for(auto& v:q){middle.x+=v.x*.25f;middle.y+=v.y*.25f;middle.z+=v.z*.25f;}
      coverageCollider.base.actor=&contactOwners[i];contactOwners[i].update=[](Actor*,PlayState*){};
      Collider_ResetCylinderAC(play,&coverageCollider.base);
      coverageCollider.dim.pos={(s16)middle.x,(s16)(middle.y-2),(s16)middle.z};
      CollisionCheck_SetAC(play,&play->colChkCtx,&coverageCollider.base);++expectedCoverage;
     }
    }
    const bool checkReach=(i>=150 && i<=171), checkExpired=(i>=234 && i<=258);
    if(checkReach || checkExpired) {
     const float reach=tier==2?10000.f:7500.f;
     Vec3f point{model.m[3][0]+model.m[0][0]*reach+model.m[1][0]*230.f,
                model.m[3][1]+model.m[0][1]*reach+model.m[1][1]*230.f,
                model.m[3][2]+model.m[0][2]*reach+model.m[1][2]*230.f};
     Collider_ResetCylinderAC(play,&reachCollider.base);reachCollider.base.actor=&reachOwners[i];
     reachOwners[i].update=[](Actor*,PlayState*){};
     reachCollider.dim.pos={(s16)point.x,(s16)(point.y-2),(s16)point.z};
     CollisionCheck_SetAC(play,&play->colChkCtx,&reachCollider.base);
     if(checkReach)++reachChecks;
    }
    Magic_Update(play);
    mmvrgame::ProcessCombatInput(play);
    for(auto* a=play->actorCtx.actorLists[ACTORCAT_ITEMACTION].first;a;a=a->next)if(a->id==ACTOR_EN_M_THUNDER&&a->update&&std::find(old.begin(),old.end(),a)==old.end()){
     if(a->init){a->init(a,play);a->init=nullptr;}auto before=play->colChkCtx.colATCount;a->update(a,play);
     if(play->colChkCtx.colATCount>before)++volumeFrames;++effects;
    }
    MMVR_FilterAttackCollisions(play);CollisionCheck_AT(play,&play->colChkCtx);MMVR_AfterAttackCollision(play);CollisionCheck_Damage(play,&play->colChkCtx);
    for(int k=0;k<11;++k)if(keaton[k].actor.update && func_80A5BFD8(&keaton[k],play)) {
     if(k<9)++keatonCut;else ++keatonOutside;
    }
    if(checkReach && (reachCollider.base.acFlags&AC_HIT))++reachHits;
    if(checkExpired && (reachCollider.base.acFlags&AC_HIT))++expiredHits;
    if(checkCoverage && (coverageCollider.base.acFlags&AC_HIT))++coverage;
    if(enemy.collider.base.acFlags&AC_HIT){if(i<130)++early;else ++hits;flags&=p->meleeWeaponQuads[0].elem.atDmgInfo.dmgFlags==DMG_SPIN_ATTACK&&enemy.actor.colChkInfo.damage==3;EnDekubaba_UpdateDamage(&enemy,play);}
   }
  }
  if(automatic) {
   ObjGrass_ProcessColliders(&patch,play);
   for(int g=0;g<10;++g)for(int e=0;e<10;++e)grassCut+=(patch.grassGroups[g].elements[e].flags&OBJ_GRASS_ELEM_REMOVED)!=0;
  }
  for(auto& collider:patch.grassElemColliders)Collider_DestroyCylinder(play,&collider.collider);
  for(auto& grass:keaton) EnKusa2_Destroy(&grass.actor,play);
  bool magic=gSaveContext.magicState==MAGIC_STATE_IDLE&&gSaveContext.save.saveInfo.playerData.magic==16;
  if(nativeHz!=20||tier>1||left||automatic)log<<",";log<<"{\"nativeHz\":"<<nativeHz<<",\"left\":"<<left<<",\"automatic\":"<<automatic<<",\"turn\":"<<turn<<",\"early\":"<<early<<",\"hits\":"<<hits<<",\"nativeDamage\":"<<flags<<",\"magic\":"<<magic<<",\"effects\":"<<effects<<",\"nativeVolumeFrames\":"<<volumeFrames<<",\"keatonCut\":"<<keatonCut<<",\"keatonOutside\":"<<keatonOutside<<",\"coverage\":"<<coverage<<",\"expectedCoverage\":"<<expectedCoverage<<",\"tier\":"<<tier<<",\"reachHits\":"<<reachHits<<",\"reachChecks\":"<<reachChecks<<",\"firstSpinGrass\":"<<firstSpinGrass<<",\"grassCut\":"<<grassCut<<",\"expiredHits\":"<<expiredHits<<"}";
  for(auto* a=play->actorCtx.actorLists[ACTORCAT_ITEMACTION].first;a;){auto* next=a->next;if(a->id==ACTOR_EN_M_THUNDER&&std::find(old.begin(),old.end(),a)==old.end())Actor_Delete(&play->actorCtx,a,play);a=next;}
  Collider_DestroyCylinder(play,&coverageCollider);Collider_DestroyCylinder(play,&reachCollider);
  EnDekubaba_Destroy(&enemy.actor,play);
 }
 log<<"]";*p=saved;gSaveContext=save;mmvr::GetSettings()=settings;play->colChkCtx=collision;*CONTROLLER1(&play->state)=input;mmvrgame::ClearTracking();
}
