#pragma once
// Private authored-scene reproduction. Drive the ordinary stick into the intact
// covering rock; do not register colliders or substitute a synthetic player.
extern "C" {
#include "overlays/actors/ovl_Obj_Bombiwa/z_obj_bombiwa.h"
#include "overlays/actors/ovl_En_Bom/z_en_bom.h"
int MMVR_ChooseOCOverflowSlot(PlayState*, CollisionCheckContext*, Collider*);
void MMVR_ClearACOverflow(CollisionCheckContext*);
int MMVR_CollisionACCount(CollisionCheckContext*);
Collider* MMVR_CollisionACAt(CollisionCheckContext*,int);
void MMVR_RemoveACOverflow(CollisionCheckContext*,Collider*);
void MMVR_PrioritizeAC(CollisionCheckContext*,Collider*);
}
static bool NativeACOverflowChecks(PlayState* play,std::ofstream& log) {
    std::array<ColliderCylinder,573> colliders{};
    CollisionCheckContext context{};
    int checks=0,failures=0;
    auto check=[&](bool ok,const char* name) {++checks;failures+=!ok;log<<"damage-queue "<<name<<"="<<ok<<'\n';};
    for(int i=0;i<572;++i) {
        colliders[i].base.shape=COLSHAPE_CYLINDER;
        check(CollisionCheck_SetAC(play,&context,&colliders[i].base)==i,"append");
    }
    colliders[572].base.shape=COLSHAPE_CYLINDER;
    check(CollisionCheck_SetAC(play,&context,&colliders[572].base)==-1,"bounded-capacity");
    check(context.colACCount==60&&MMVR_CollisionACCount(&context)==572,"native-layout-retained");
    check(CollisionCheck_SetAC(play,&context,&colliders[571].base)==571&&MMVR_CollisionACCount(&context)==572,"duplicate");
    auto* displaced=context.colAC[59];
    MMVR_PrioritizeAC(&context,&colliders[571].base);
    check(context.colAC[59]==&colliders[571].base&&MMVR_CollisionACAt(&context,571)==displaced,"shield-priority-preserves-target");
    MMVR_RemoveACOverflow(&context,&colliders[60].base);
    check(MMVR_CollisionACCount(&context)==571,"remove-stale-shield");
    check(!MMVR_CollisionACAt(nullptr,0)&&!MMVR_CollisionACAt(&context,-1)&&!MMVR_CollisionACAt(&context,572),"index-guards");
    context.sacFlags=SAC_ON;
    check(MMVR_CollisionACCount(&context)==60&&CollisionCheck_SetAC(play,&context,&colliders[572].base)==-1,"sac-unchanged");
    CollisionCheck_DestroyContext(play,&context);
    context.sacFlags=0;
    check(MMVR_CollisionACCount(&context)==60,"destroy-clears-owner");
    mmvr::SetNativeTestTracking(false);
    check(CollisionCheck_SetAC(play,&context,&colliders[60].base)==-1,"flat-cap-unchanged");
    mmvr::SetNativeTestTracking(true);
    CollisionCheck_ClearContext(play,&context);
    for(int i=0;i<60;++i) CollisionCheck_SetAC(play,&context,&colliders[i].base);
    Actor target{},attacker{};
    ColliderCylinderInit targetInit={{COL_MATERIAL_NONE,AT_NONE,AC_ON|AC_TYPE_PLAYER,OC1_NONE,OC2_NONE,COLSHAPE_CYLINDER},
        {ELEM_MATERIAL_UNK0,{0,0,0},{DMG_SWORD,0,0},ATELEM_NONE,ACELEM_ON,OCELEM_NONE},{5,20,0,{0,2000,0}}};
    ColliderCylinderInit attackInit={{COL_MATERIAL_NONE,AT_ON|AT_TYPE_PLAYER,AC_NONE,OC1_NONE,OC2_NONE,COLSHAPE_CYLINDER},
        {ELEM_MATERIAL_UNK0,{DMG_SWORD,0,3},{0,0,0},ATELEM_ON,ACELEM_NONE,OCELEM_NONE},{5,20,0,{0,2000,0}}};
    ColliderCylinder defense{},attack{};
    Collider_InitAndSetCylinder(play,&defense,&target,&targetInit);
    Collider_InitAndSetCylinder(play,&attack,&attacker,&attackInit);
    // Null actor updates are rejected by registration; use the live player's
    // update pointer without invoking it on these isolated collider owners.
    target.update=attacker.update=GET_PLAYER(play)->actor.update;
    check(CollisionCheck_SetAC(play,&context,&defense.base)==60,"damage-target-in-overflow");
    CollisionCheck_SetAT(play,&context,&attack.base);
    CollisionCheck_AT(play,&context);CollisionCheck_Damage(play,&context);
    check((defense.base.acFlags&AC_HIT)&&(attack.base.atFlags&AT_HIT)&&target.colChkInfo.damage==3,"native-hit-and-damage");
    Collider_DestroyCylinder(play,&defense);Collider_DestroyCylinder(play,&attack);
    CollisionCheck_ClearContext(play,&context);
    check(!MMVR_CollisionACCount(&context)&&!MMVR_CollisionACAt(&context,60),"frame-clear");
    log<<"damage-queue-checks="<<checks<<" failures="<<failures<<std::endl;
    return !failures;
}
static bool NativeOCOverflowChecks(PlayState* play, std::ofstream& log) {
    auto* p=GET_PLAYER(play);
    std::array<Actor,51> actors{};
    std::array<ColliderCylinder,51> cylinders{};
    CollisionCheckContext context{};
    int failures=0, checks=0;
    auto check=[&](bool ok,const char* name) {
        ++checks; failures+=!ok; log<<"queue "<<name<<"="<<ok<<'\n';
    };
    for (int i=0;i<51;++i) {
        actors[i].update=p->actor.update;
        auto& c=cylinders[i];
        c.base.actor=&actors[i]; c.base.shape=COLSHAPE_CYLINDER;
        c.base.ocFlags1=OC1_ON|OC1_TYPE_ALL; c.base.ocFlags2=OC2_TYPE_2;
        c.elem.ocElemFlags=OCELEM_ON;
        c.dim={12,70,0,{s16(p->actor.world.pos.x+1000+i*20),s16(p->actor.world.pos.y),s16(p->actor.world.pos.z)}};
        if(i<50) check(CollisionCheck_SetOC(play,&context,&c.base)==i,"native-append");
    }
    check(CollisionCheck_SetOC(play,&context,&p->cylinder.base)>=0,"late-player-retained");
    auto& closeCollider=cylinders[50];
    closeCollider.dim.pos.x=s16(p->actor.world.pos.x+60); closeCollider.dim.radius=55;
    check(CollisionCheck_SetOC(play,&context,&closeCollider.base)>=0,"late-closeCollider-rock-retained");
    bool playerPresent=false,nearPresent=false;
    for(auto* c:context.colOC) {playerPresent|=c==&p->cylinder.base;nearPresent|=c==&closeCollider.base;}
    check(context.colOCCount==50&&playerPresent&&nearPresent,"bounded-player-and-cover");
    check(CollisionCheck_SetOC(play,&context,&cylinders[49].base)==-1,"distant-rejected");
    check(CollisionCheck_SetOC(play,&context,&closeCollider.base)>=0&&context.colOCCount==50,"duplicate-no-growth");
    context.sacFlags=SAC_ON;
    check(CollisionCheck_SetOC(play,&context,&cylinders[0].base)==-1,"sac-unchanged");
    context.sacFlags=0;
    actors[0].update=nullptr;
    check(CollisionCheck_SetOC(play,&context,&cylinders[0].base)==-1,"dead-actor-rejected");
    // Large solids are ranked by their surface even when their actor is far away.
    auto large=cylinders[1]; large.dim.pos.x=s16(p->actor.world.pos.x+500); large.dim.radius=500;
    check(MMVR_ChooseOCOverflowSlot(play,&context,&large.base)>=0,"large-surface");
    Sphere16 ball{{s16(p->actor.world.pos.x+10),s16(p->actor.world.pos.y+20),s16(p->actor.world.pos.z)},20};
    ColliderSphere sphere{}; sphere.base=closeCollider.base; sphere.base.shape=COLSHAPE_SPHERE; sphere.dim.worldSphere=ball;
    check(MMVR_ChooseOCOverflowSlot(play,&context,&sphere.base)>=0,"sphere");
    ColliderJntSphElement element{}; element.dim.worldSphere=ball;
    ColliderJntSph joint{}; joint.base=closeCollider.base; joint.base.shape=COLSHAPE_JNTSPH; joint.count=1; joint.elements=&element;
    check(MMVR_ChooseOCOverflowSlot(play,&context,&joint.base)>=0,"joint-sphere");
    ColliderTrisElement triangle{};
    for(auto& v:triangle.dim.vtx) v=p->actor.world.pos;
    ColliderTris tris{}; tris.base=closeCollider.base; tris.base.shape=COLSHAPE_TRIS; tris.count=1; tris.elements=&triangle;
    check(MMVR_ChooseOCOverflowSlot(play,&context,&tris.base)>=0,"triangles");
    ColliderQuad quad{}; quad.base=closeCollider.base; quad.base.shape=COLSHAPE_QUAD;
    for(auto& v:quad.dim.quad) v=p->actor.world.pos;
    check(MMVR_ChooseOCOverflowSlot(play,&context,&quad.base)>=0,"quad");
    check(closeCollider.dim.radius==55&&closeCollider.dim.height==70&&closeCollider.base.ocFlags1==(OC1_ON|OC1_TYPE_ALL),"dimensions-flags-unchanged");
    mmvr::SetNativeTestTracking(false);
    check(!MMVR_WideVisibility()&&MMVR_ChooseOCOverflowSlot(play,&context,&large.base)==-1,"flat-cap-unchanged");
    mmvr::SetNativeTestTracking(true);
    log<<"queue-checks="<<checks<<" failures="<<failures<<std::endl;
    return failures==0;
}
static mmvr::Pad NativeGrottoCollisionTest(PlayState* play, unsigned tick) {
    static bool started=false, paired=false, moved=false, contact=false;
    static unsigned age=0;
    static ObjBombiwa* rock=nullptr;
    static Vec3f start{};
    static EnBom* bomb=nullptr;
    static bool exploded=false;
    static std::ofstream log("native-grotto-collision.log");
    mmvr::Pad pad; pad.active=true;
    mmvr::ApplyViewMode(2); mmvr::SetNativeTestTracking(true);
    auto finish=[&](bool ok,const char* reason) {
        log<<(ok?"PASS":"FAIL")<<" grotto collision paired="<<paired<<" moved="<<moved
           <<" contact="<<contact<<" reason="<<reason<<std::endl;
        Ship::Context::GetRawInstance()->GetWindow()->Close();
    };
    if(tick==50&&(!NativeOCOverflowChecks(play,log)||!NativeACOverflowChecks(play,log))) {finish(false,"queue-checks");return pad;}
    if(!started&&tick>=60) {
        for(int i=0;i<ARRAY_COUNT(debugLocations);++i)
            if(debugLocations[i].interactionPreset==16) {started=MMVR_DebugLocationBegin(play,i)!=0;break;}
    }
    if(!started||play->sceneId!=SCENE_00KEIKOKU||play->transitionTrigger!=TRANS_TRIGGER_OFF||
       play->transitionMode!=TRANS_MODE_OFF||play->roomCtx.status) {
        if(rock) finish(false,"entered-hole-through-intact-rock");
        else if(tick>1200) finish(false,"portal-timeout");
        return pad;
    }
    auto* p=GET_PLAYER(play);
    if(++age==45) {
        start=p->actor.world.pos;
        for(auto& list:play->actorCtx.actorLists) for(auto* a=list.first;a;a=a->next)
            if(a->id==ACTOR_OBJ_BOMBIWA&&a->update&&!a->init&&
               std::hypot(start.x-a->world.pos.x,start.z-a->world.pos.z)<180.f)
                rock=reinterpret_cast<ObjBombiwa*>(a);
        paired=rock!=nullptr;
        if(!paired) {finish(false,"missing-cover");return pad;}
    }
    if(!rock) return pad;
    bool rockLive=false;
    for(auto& list:play->actorCtx.actorLists) for(auto* a=list.first;a;a=a->next)
        rockLive|=a==&rock->actor&&a->update;
    if(!rockLive) {finish(paired&&moved&&contact&&exploded,"completed");return pad;}
    const float distance=std::hypot(p->actor.world.pos.x-rock->actor.world.pos.x,
                                    p->actor.world.pos.z-rock->actor.world.pos.z);
    bool playerQueued=false,rockQueued=false;
    for(int i=0;i<play->colChkCtx.colOCCount;++i) {
        playerQueued|=play->colChkCtx.colOC[i]==&p->cylinder.base;
        rockQueued|=play->colChkCtx.colOC[i]==&rock->collider.base;
    }
    if(age>=45) log<<"tick="<<age<<" pos="<<p->actor.world.pos.x<<","<<p->actor.world.pos.y
        <<","<<p->actor.world.pos.z<<" distance="<<distance<<" body="<<p->cylinder.dim.pos.y
        <<"+"<<p->cylinder.dim.yShift<<"/"<<p->cylinder.dim.height<<" radius="<<p->cylinder.dim.radius
        <<" rock="<<rock->collider.dim.pos.y<<"+"<<rock->collider.dim.yShift<<"/"<<rock->collider.dim.height
        <<" oc="<<playerQueued<<","<<rockQueued<<","<<play->colChkCtx.colOCCount
        <<" flags="<<int(p->cylinder.base.ocFlags1)<<" draw="<<(p->actor.draw!=nullptr)<<std::endl;
    moved|=std::hypot(p->actor.world.pos.x-start.x,p->actor.world.pos.z-start.z)>10.f;
    contact|=distance<=rock->collider.dim.radius+p->cylinder.dim.radius+3.f;
    if(age<190&&distance<rock->collider.dim.radius-5.f) {finish(false,"crossed-native-cover-radius");return pad;}
    if(age>=50&&age<170) {
        const s16 yaw=Math_Vec3f_Yaw(&p->actor.world.pos,&rock->actor.world.pos)-Camera_GetInputDirYaw(GET_ACTIVE_CAM(play));
        pad.x=static_cast<int8_t>(-85.f*Math_SinS(yaw));
        pad.y=static_cast<int8_t>(85.f*Math_CosS(yaw));
    }
    if(age==190) {
        auto pos=rock->actor.world.pos;
        bomb=reinterpret_cast<EnBom*>(Actor_Spawn(&play->actorCtx,play,ACTOR_EN_BOM,
            pos.x,pos.y+10,pos.z,0,0,0,BOMB_TYPE_BODY));
        if(!bomb) {finish(false,"bomb-spawn");return pad;}
    }
    if(bomb) {
        bool bombLive=false;
        for(auto& list:play->actorCtx.actorLists) for(auto* a=list.first;a;a=a->next)
            bombLive|=a==&bomb->actor&&a->update;
        if(bombLive&&!bomb->actor.init) {
            if(age==195) bomb->timer=1;
            exploded|=bomb->actor.params==BOMB_TYPE_EXPLOSION;
            bool ac=false,at=false;
            for(int i=0;i<MMVR_CollisionACCount(&play->colChkCtx);++i) ac|=MMVR_CollisionACAt(&play->colChkCtx,i)==&rock->collider.base;
            for(int i=0;i<play->colChkCtx.colATCount;++i) at|=play->colChkCtx.colAT[i]==&bomb->collider2.base;
            log<<"blast age="<<age<<" timer="<<bomb->timer<<" type="<<bomb->actor.params
               <<" queued="<<ac<<","<<at<<" counts="<<play->colChkCtx.colACCount<<","<<play->colChkCtx.colATCount
               <<" flags="<<int(rock->collider.base.acFlags)<<" bombPos="<<bomb->actor.world.pos.x<<","<<bomb->actor.world.pos.y<<","<<bomb->actor.world.pos.z<<std::endl;
        }
    }
    if(age==250) finish(false,"intact-after-live-bomb");
    return pad;
}
