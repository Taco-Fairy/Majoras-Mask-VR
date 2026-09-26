int catalogPage=0,catalogLastPad=-1;
constexpr int catalogPerPage=8;
constexpr int catalogPages=(ARRAY_COUNT(debugLocations)+catalogPerPage-1)/catalogPerPage;
// A previous interpolated frame can still reference the previous page. Keep
// each page's vertex/command allocations immutable for the lifetime of the lab.
std::array<Mesh,catalogPages> catalogMeshes;
Mesh& CatalogMesh(){return catalogMeshes[catalogPage];}
void BuildCatalogPage(){
 auto& catalog=CatalogMesh();if(!catalog.commands.empty())return;
 for(int i=0;i<catalogPerPage;++i){
  int index=catalogPage*catalogPerPage+i;if(index>=ARRAY_COUNT(debugLocations))break;
  short x=short(2890+(i%4)*440),z=short((i/4)*470-120);
  catalog.Quad({short(x-44),2,short(z-44)},{short(x-44),2,short(z+44)},{short(x+44),2,short(z+44)},{short(x+44),2,short(z-44)},purple);
  std::string name=debugLocations[index].name;for(size_t pos=0;(pos=name.find('3',pos))!=std::string::npos;pos+=5)name.replace(pos,1,"THREE");for(char& c:name){if(c>='a'&&c<='z')c-=32;if(c=='\''||c=='('||c==')'||c=='&'||c=='-')c=' ';}
  size_t split=name.size()>24?name.rfind(' ',24):std::string::npos;
  if(split==std::string::npos){Sign(name.c_str(),x,95,z-64,1,&catalog);}
  else{Sign(name.substr(0,split).c_str(),x,110,z-64,1,&catalog);Sign(name.substr(split+1).c_str(),x,92,z-64,1,&catalog);}
 }
 std::string page="PAGE ";page+=char('A'+catalogPage);Sign(page.c_str(),3600,115,830,2,&catalog);
 for(short x:{short(3150),short(4050)})catalog.Quad({short(x-44),2,836},{short(x-44),2,924},{short(x+44),2,924},{short(x+44),2,836},green);
 catalog.Finish();
}
void UpdateCatalogPads(PlayState* play,Player* player){
 if(play->msgCtx.msgMode!=MSGMODE_NONE||(player->stateFlags1&PLAYER_STATE1_TALKING))return;
 const auto pos=player->actor.world.pos;int pad=-1;
 if(std::abs(pos.y)<12){
  for(int i=0;i<catalogPerPage;++i)if(catalogPage*catalogPerPage+i<ARRAY_COUNT(debugLocations)&&std::abs(pos.x-(2890+(i%4)*440))<44&&std::abs(pos.z-((i/4)*470-120))<44)pad=i;
  if(std::abs(pos.x-3150)<44&&std::abs(pos.z-880)<44)pad=8;
  if(std::abs(pos.x-4050)<44&&std::abs(pos.z-880)<44)pad=9;
 }
 if(pad>=0&&pad!=catalogLastPad){
  if(pad>=8){catalogPage=(catalogPage+(pad==9?1:catalogPages-1))%catalogPages;BuildCatalogPage();Audio_PlaySfx(NA_SE_SY_DECIDE);}
  else MMVR_DebugLocationBegin(play,catalogPage*catalogPerPage+pad);
 }
 catalogLastPad=pad;
}
