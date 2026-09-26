#pragma once
#include <fast/resource/type/DisplayList.h>
#include <fast/resource/type/Vertex.h>
#include <fast/lus_gbi.h>
// Real decoder tests: never draw a synthetic triangle or submit another XR frame.
static void NativeResourceCommands(Fast::Interpreter* interpreter,uint64_t textureHash){
 auto manager=Ship::Context::GetRawInstance()->GetResourceManager();
 const auto rsp=*interpreter->mRsp;
 const auto textureState=interpreter->mRdp->texture_to_load;
 bool modifyBounds=false,triangleBounds=false;
 bool textureAdvance=false,missingAdvance=false,nullVertexAdvance=false,offsetPreserved=false,replacementUsed=false,boundsSafe=false;
 auto command=[](int8_t opcode,uint64_t hash,uintptr_t arg=0){
  std::array<Fast::F3DGfx,3> words{};
  words[0].words.w0=uint32_t(uint8_t(opcode))<<24;words[0].words.w1=arg;
  words[1].words.w0=uint32_t(hash>>32);words[1].words.w1=uint32_t(hash);
  words[2].words.w0=0xdf000000;return words;
 };
 auto texture=command(Fast::OTR_G_SETTIMG_OTR_HASH,textureHash);
 texture[0].words.w0|=G_IM_SIZ_32b<<19;
 const auto originalTexture=texture;
 const char* texturePath=interpreter->ResolveResourcePathCached(textureHash);
 auto expectedTexture=texturePath?manager->LoadResourceProcess(texturePath):nullptr;
 textureAdvance=interpreter->TestStepResourceCommand(texture.data())==2&&
  texture[0].words.w0==originalTexture[0].words.w0&&texture[0].words.w1==originalTexture[0].words.w1&&
  expectedTexture&&interpreter->mRdp->texture_to_load.addr==expectedTexture->GetRawPointer();
 auto missing=command(Fast::OTR_G_SETTIMG_OTR_HASH,0x4d4d565200000badULL);
 missingAdvance=interpreter->TestStepResourceCommand(missing.data())==2;
 auto nullVertex=command(Fast::OTR_G_VTX_OTR_FILEPATH,0);
 nullVertex[1].words.w0=1;
 nullVertexAdvance=interpreter->TestStepResourceCommand(nullVertex.data())==2;
 auto hand=std::dynamic_pointer_cast<Fast::DisplayList>(manager->LoadResource("objects/object_link_child/gLinkHumanLeftHandOpenDL"));
 uint64_t vertexHash=0;
 if(hand)for(size_t i=0;i+1<hand->Instructions.size();++i){
  if(uint8_t(hand->Instructions[i].words.w0>>24)==uint8_t(Fast::OTR_G_VTX_OTR_HASH)){
   vertexHash=(uint64_t(hand->Instructions[i+1].words.w0)<<32)|uint32_t(hand->Instructions[i+1].words.w1);break;
  }
 }
 const char* vertexPath=vertexHash?interpreter->ResolveResourcePathCached(vertexHash):nullptr;
 auto vertices=vertexPath?manager->LoadResource(vertexPath):nullptr;
 auto nativeVertices=std::dynamic_pointer_cast<SOH::Array>(vertices);
 auto packVertices=std::dynamic_pointer_cast<Fast::Vertex>(vertices);
 const bool fixtureLoaded=vertices&&vertices->GetPointerSize()>=sizeof(Fast::F3DVtx)&&
  ((nativeVertices&&nativeVertices->ArrayType==SOH::ArrayResourceType::Vertex)||packVertices);
 if(fixtureLoaded){
  // Identity projection and vertex zero make stale retained pointers observable.
  std::memset(interpreter->mRsp->MP_matrix,0,sizeof(interpreter->mRsp->MP_matrix));
  for(int i=0;i<4;++i)interpreter->mRsp->MP_matrix[i][i]=1;
  interpreter->mRsp->geometry_mode=0;
  auto vertex=command(Fast::OTR_G_VTX_OTR_HASH,vertexHash);
  vertex[0].words.w0|=(1<<12)|(1<<1);
  interpreter->SetResolvedResourceCacheEnabled(true);
  offsetPreserved=interpreter->TestStepResourceCommand(vertex.data())==2&&vertex[0].words.w1==0;
  const auto oldX=interpreter->mRsp->loaded_vertices[0].x;
  // MM's original archive stores vertices in Array resources; model packs may
  // use Fast::Vertex. Preserve the actual resource kind in the replacement.
  std::shared_ptr<Ship::IResource> replacement;
  if(nativeVertices){
   auto copy=std::make_shared<SOH::Array>(vertices->GetInitData());
   copy->ArrayType=SOH::ArrayResourceType::Vertex;copy->ArrayCount=nativeVertices->ArrayCount;
   copy->Vertices=nativeVertices->Vertices;replacement=copy;
  }else{
   auto copy=std::make_shared<Fast::Vertex>(vertices->GetInitData());
   copy->VertexList=packVertices->VertexList;replacement=copy;
  }
  auto* copied=static_cast<Fast::F3DVtx*>(replacement->GetRawPointer());
  copied[0].v.ob[0]=copied[0].v.ob[0]>0?-123:123;
  interpreter->SetResolvedResourceCacheEnabled(false);
  manager->CacheExternalResource(vertexPath,replacement);
  interpreter->SetResolvedResourceCacheEnabled(true);
  replacementUsed=interpreter->TestStepResourceCommand(vertex.data())==2&&vertex[0].words.w1==0&&
   interpreter->mRsp->loaded_vertices[0].x!=oldX;
  auto before=*interpreter->mRsp;
  interpreter->GfxSpVertex(2,std::size(interpreter->mRsp->loaded_vertices)-1,
                         reinterpret_cast<const Fast::F3DVtx*>(replacement->GetRawPointer()));
  boundsSafe=std::memcmp(&before,interpreter->mRsp,sizeof(before))==0;
  interpreter->SetResolvedResourceCacheEnabled(false);
  manager->CacheExternalResource(vertexPath,vertices);
 }
 {
  auto before=*interpreter->mRsp;
  const auto bufferLength=interpreter->mBufVboLen;
  interpreter->GfxSpModifyVertex(65535,G_MWO_POINT_ST,0x12345678);
  modifyBounds=std::memcmp(&before,interpreter->mRsp,sizeof(before))==0;
  const uint8_t invalid=uint8_t(std::size(interpreter->mRsp->loaded_vertices));
  interpreter->GfxSpTri1(invalid,0,0,false);
  interpreter->GfxSpTri1(0,invalid,0,false);
  interpreter->GfxSpTri1(0,0,255,false);
  triangleBounds=std::memcmp(&before,interpreter->mRsp,sizeof(before))==0&&interpreter->mBufVboLen==bufferLength;
 }
 *interpreter->mRsp=rsp;interpreter->mRdp->texture_to_load=textureState;
 std::ofstream("native-resource-commands.json")<<"{\"textureAdvance\":"<<textureAdvance
  <<",\"missingAdvance\":"<<missingAdvance<<",\"nullVertexAdvance\":"<<nullVertexAdvance
  <<",\"modifyBounds\":"<<modifyBounds<<",\"triangleBounds\":"<<triangleBounds
  <<",\"fixtureLoaded\":"<<fixtureLoaded<<",\"offsetPreserved\":"<<offsetPreserved<<",\"replacementUsed\":"<<replacementUsed<<",\"boundsSafe\":"<<boundsSafe<<"}";
}
