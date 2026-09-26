#pragma once
// Exercise the native soil exchange, watering reaction, growth and riding actions.
static bool NativeBeanTraversalTest(PlayState* play,std::ostream& log){
 ObjBean *soil=nullptr,*plant=nullptr,*ride=nullptr;
 for(auto& list:play->actorCtx.actorLists)for(auto* a=list.first;a;a=a->next)if(a->id==ACTOR_OBJ_BEAN&&a->update&&!a->init){
  if(OBJBEAN_GET_C000(a)==1)soil=reinterpret_cast<ObjBean*>(a);
  else if(OBJBEAN_GET_80(a))ride=reinterpret_cast<ObjBean*>(a);else plant=reinterpret_cast<ObjBean*>(a);
 }
 if(!soil||!plant||!ride)return false;
 auto soilBefore=*soil,plantBefore=*plant,rideBefore=*ride;auto* p=GET_PLAYER(play);auto player=*p;
 auto flags=play->actorCtx.sceneFlags;auto col=play->colChkCtx;
 auto step=[&](ObjBean* b){play->colChkCtx.colATCount=play->colChkCtx.colACCount=play->colChkCtx.colOCCount=0;if(b->dyna.actor.update)b->dyna.actor.update(&b->dyna.actor,play);};
 p->exchangeItemAction=PLAYER_IA_MAGIC_BEANS;soil->dyna.actor.flags|=ACTOR_FLAG_TALK;step(soil);
 bool planted=Flags_GetSwitch(play,49);for(int i=0;i<140;++i)step(soil);
 Actor water{};water.id=ACTOR_OBJ_AQUA;soil->collider.base.ac=&water;soil->collider.base.acFlags|=AC_HIT;step(soil);
 bool watered=Flags_GetSwitch(play,50)&&plant->unk_200;
 for(int i=0;i<65;++i)step(plant);
 bool grew=plant->dyna.actor.draw&&plant->dyna.actor.scale.x>.09f;
 auto origin=ride->dyna.actor.world.pos;ride->dyna.interactFlags|=DYNA_INTERACT_PLAYER_ON_TOP;
 for(int i=0;i<40;++i)step(ride);
 bool moved=Math3D_Vec3fDistSq(&origin,&ride->dyna.actor.world.pos)>100;
 log<<"bean planted="<<planted<<" watered="<<watered<<" grew="<<grew<<" moved="<<moved<<"\n";
 *soil=soilBefore;*plant=plantBefore;*ride=rideBefore;*p=player;play->actorCtx.sceneFlags=flags;play->colChkCtx=col;
 DynaPoly_DisableCollision(play,&play->colCtx.dyna,plant->dyna.bgId);
 return planted&&watered&&grew&&moved;
}
