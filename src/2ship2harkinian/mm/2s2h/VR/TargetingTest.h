#pragma once
extern "C" {void Player_UpdateZTargeting(Player*,PlayState*);extern Input* sPlayerControlInput;}
static void NativeTargetingTest(PlayState* play,const Player& baseline,std::ostream& log){
 auto* p=GET_PLAYER(play);auto saved=*p;auto save=gSaveContext;auto input=*CONTROLLER1(&play->state);auto* control=sPlayerControlInput;auto attention=play->actorCtx.attention;auto cs=play->csCtx.state;
 sPlayerControlInput=CONTROLLER1(&play->state);play->csCtx.state=CS_STATE_IDLE;
 Actor a{},b{};for(auto* actor:{&a,&b}){actor->update=[](Actor*,PlayState*){};actor->flags=ACTOR_FLAG_ATTENTION_ENABLED|ACTOR_FLAG_HOSTILE;actor->xyzDistToPlayerSq=100;actor->attentionRangeType=ATTENTION_RANGE_0;}
 auto prepare=[&](){*p=baseline;p->stateFlags1=p->stateFlags2=p->stateFlags3=0;p->csAction=PLAYER_CSACTION_NONE;p->currentMask=PLAYER_MASK_NONE;p->focusActor=p->autoLockOnActor=nullptr;p->zTargetActiveTimer=0;p->heldItemAction=PLAYER_IA_NONE;*sPlayerControlInput={};play->actorCtx.attention.tatlHoverActor=&a;play->actorCtx.attention.arrowHoverActor=nullptr;};
 auto tick=[&](bool held,bool press){sPlayerControlInput->cur.button=held?BTN_Z:0;sPlayerControlInput->press.button=press?BTN_Z:0;Player_UpdateZTargeting(p,play);};
 log<<",\"tapTargeting\":[";
 for(int setting=0;setting<2;++setting){
  prepare();gSaveContext.options.zTargetSetting=setting;tick(true,true);bool acquired=p->focusActor==&a;
  for(int i=0;i<25;++i)tick(false,false);bool retained=p->focusActor==&a&&p->zTargetActiveTimer>=5;
  play->actorCtx.attention.arrowHoverActor=&b;tick(true,true);bool cycled=p->focusActor==&b;tick(false,false);
  play->actorCtx.attention.tatlHoverActor=&b;play->actorCtx.attention.arrowHoverActor=nullptr;tick(true,true);bool released=!p->focusActor;
  prepare();tick(true,true);for(int i=0;i<25;++i)tick(true,false);bool held=p->focusActor==&a;
  a.update=nullptr;tick(false,false);bool deadReleased=!p->focusActor;a.update=[](Actor*,PlayState*){};
  prepare();tick(true,true);for(int i=0;i<25;++i)tick(false,false);a.xyzDistToPlayerSq=1e10f;tick(false,false);bool rangeReleased=!p->focusActor;a.xyzDistToPlayerSq=100;
  prepare();tick(true,true);play->csCtx.state=CS_STATE_RUN;tick(false,false);bool cinematicReleased=!p->focusActor;play->csCtx.state=CS_STATE_IDLE;
  prepare();play->actorCtx.attention.tatlHoverActor=nullptr;tick(true,true);for(int i=0;i<25;++i)tick(false,false);
  bool noTargetReleased=!p->focusActor&&p->zTargetActiveTimer>=5&&(p->stateFlags1&PLAYER_STATE1_PARALLEL);
  tick(true,true);tick(false,false);noTargetReleased&=p->zTargetActiveTimer==0&&!(p->stateFlags1&PLAYER_STATE1_PARALLEL);
  if(setting)log<<",";log<<"{\"acquired\":"<<acquired<<",\"retained\":"<<retained<<",\"cycled\":"<<cycled<<",\"released\":"<<released<<",\"held\":"<<held<<",\"deadReleased\":"<<deadReleased<<",\"rangeReleased\":"<<rangeReleased<<",\"cinematicReleased\":"<<cinematicReleased<<",\"noTargetReleased\":"<<noTargetReleased<<",\"savePreserved\":"<<(gSaveContext.options.zTargetSetting==setting)<<"}";
 }
 prepare();gSaveContext.options.zTargetSetting=1;mmvr::SetNativeTestTracking(false);tick(true,true);for(int i=0;i<25;++i)tick(false,false);bool desktopHold=!p->focusActor;mmvr::SetNativeTestTracking(true);
 log<<"],\"desktopHoldPreserved\":"<<desktopHold;
 *p=saved;gSaveContext=save;*CONTROLLER1(&play->state)=input;sPlayerControlInput=control;play->actorCtx.attention=attention;play->csCtx.state=cs;
}
