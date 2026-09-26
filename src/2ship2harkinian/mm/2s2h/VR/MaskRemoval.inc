// Cosmetic removal prop: owned inventory never becomes a collectible actor.
// Kept separate from the gesture state, which must clear while native form swaps run.
namespace {
struct RemovedMask {
 int item=-1,scene=-1,age=0,settled=0,hand=1;Player* owner=nullptr;
 bool returningHuman=false,released=false,bounced=false;int pendingAge=0;float velocity=0;
 mmvr::Matrix pose{};
} removedMask;
void BeginMaskRemoval(PlayState* play,Player* p,int item){
 removedMask={};
 if(!mmvr::FirstPersonRequested()||item<ITEM_MASK_DEKU||item>ITEM_MASK_FIERCE_DEITY||p->transformation==PLAYER_FORM_HUMAN)return;
 removedMask.hand=mmvr::HeldMaskController();
 if(!mmvrgame::TrackedMaskHand(play,removedMask.pose,removedMask.hand))return;
 removedMask.item=item;removedMask.scene=play->sceneId;removedMask.owner=p;
 std::ofstream("mmvr-mask-removal.log",std::ios::app)<<"begin item="<<item<<" hand="<<removedMask.hand<<"\n";
}
void UpdateRemovedMask(PlayState* play){
 auto& m=removedMask;if(m.item<0)return;
 auto* p=play?GET_PLAYER(play):nullptr;
 if(!p||p!=m.owner||play->sceneId!=m.scene||play->transitionTrigger!=TRANS_TRIGGER_OFF||gSaveContext.save.saveInfo.playerData.health<=0||!mmvr::FirstPersonRequested()){m={};return;}
 if(mmvr::MenuPaused()||play->pauseCtx.state!=PAUSE_STATE_OFF||!mmvr::InputFocused())return;
 if(!m.released){
  if (++m.pendingAge > 300) { m={}; return; }
  if (m.pendingAge > 10 && !MMVR_LocalTransformation(p) && p->itemAction == p->heldItemAction &&
      p->transformation != PLAYER_FORM_HUMAN) { m={}; return; }
  mmvr::Matrix hand;if(mmvrgame::TrackedMaskHand(play,hand,m.hand))m.pose=hand;
  // The native post-transformation mask-off sequence supplies the release timing.
  if(p->transformation==PLAYER_FORM_HUMAN){m.returningHuman=true;if(++m.age>=8){m.released=true;std::ofstream("mmvr-mask-removal.log",std::ios::app)<<"release item="<<m.item<<" pos="<<m.pose.m[3][0]<<","<<m.pose.m[3][1]<<","<<m.pose.m[3][2]<<"\n";}}
  return;
 }
 Vec3f from{m.pose.m[3][0],m.pose.m[3][1]+3,m.pose.m[3][2]},to=from,hit{};CollisionPoly* poly=nullptr;int bg=BGCHECK_SCENE;
 m.velocity=std::max(-12.f,m.velocity-.9f);to.y=m.pose.m[3][1]+m.velocity-1;
 if(BgCheck_EntityLineTest2(&play->colCtx,&from,&to,&hit,&poly,false,true,false,true,&bg,&p->actor)){
  m.pose.m[3][1]=hit.y+1;
  if(!m.bounced){m.bounced=true;std::ofstream("mmvr-mask-removal.log",std::ios::app)<<"land item="<<m.item<<" y="<<hit.y<<"\n";m.velocity=std::min(2.5f,-m.velocity*.22f);Audio_PlaySfx(NA_SE_IT_SET_TRANSFORM_MASK);}
  else{m.velocity=0;if(++m.settled>45)m={};}
 }else m.pose.m[3][1]+=m.velocity;
 if(++m.age>180)m={}; // no floor / unloaded room cannot retain a prop indefinitely
}
void DrawRemovedMask(PlayState* play){
 const auto& m=removedMask;const auto* model=mmvrgame::FindMaskModel(m.item);
 if(!model||m.scene!=play->sceneId||m.owner!=GET_PLAYER(play))return;
 auto pose=m.pose;float scale=40*mmvr::GetSettings().Get(mmvr::Setting::MaskSize)/model->height;
 for(int i=0;i<3;++i)for(int c=0;c<3;++c)pose.m[i][c]*=scale;
 for(int c=0;c<3;++c)for(int i=0;i<3;++i)pose.m[3][c]-=(&model->bottom.x)[i]*pose.m[i][c];
 MtxF matrix;std::memcpy(&matrix,&pose,sizeof(matrix));
 ::FrameInterpolation_RecordOpenChild(&removedMask,0);Matrix_Push();Matrix_Put(&matrix);mmvrgame::DrawMaskModel(play,m.item);Matrix_Pop();::FrameInterpolation_RecordCloseChild();
}
}
extern "C" int MMVR_TrackedMaskRemoval(PlayState* play,Player* p){return removedMask.item>=0&&removedMask.owner==p&&play&&removedMask.scene==play->sceneId&&!removedMask.released;}
