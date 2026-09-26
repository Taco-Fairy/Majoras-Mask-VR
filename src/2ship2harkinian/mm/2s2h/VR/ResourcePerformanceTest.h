#include "ResourceLifecycleChecks.h"
#include "ship/resource/ResourceLookupChecks.h"
#pragma once
#include "BirdInterpolationTest.h"
#include <chrono>
#include <algorithm>
#include <array>
#include <fstream>
#include <ship/utils/StrHash64.h>
#include "2s2h/resource/type/Array.h"
#include <fast/resource/ResourceType.h>
#include "ResourceCommandTest.h"
#include "FixedEffectVerticesTest.h"
static void NativeResourcePerformance(){
 VerifyFixedEffectVertices(gPlayState);
 FrameInterpolation_VerifyScratch();
 auto window=std::dynamic_pointer_cast<Fast::Fast3dWindow>(Ship::Context::GetRawInstance()->GetWindow());
 auto interpreter=window->GetInterpreterWeak().lock();
 interpreter->VerifyTextureCacheFastPath();
 interpreter->VerifySharedLightingTickCache();
 interpreter->VerifyColorCombinerCache();
 interpreter->VerifyTrianglePreparation();
 interpreter->VerifyNativeBoundsCulling();
 interpreter->VerifyTextureOwnershipMoves();
 interpreter->VerifyNativeReadbackCapture();
 mmvrtest::VerifyResourceLifecycle();
 interpreter->mRapi->VerifyLensAperture();
 const auto material=interpreter->mColorCombinerPool.begin()->first;
 auto* cached=interpreter->LookupOrCreateColorCombiner(material);
 const auto beforeEntries=interpreter->mColorCombinerPool.size();
 bool sameMaterial=true;std::map<ColorCombinerKey,int> distinct;
 for(int i=0;i<10000;++i){
  ColorCombinerKey key;std::memset(&key,i%256,sizeof(key));
  key.combine_mode=material.combine_mode;key.options=material.options;
  sameMaterial &= interpreter->LookupOrCreateColorCombiner(key)==cached;
  distinct[key]=1;
 }
 ColorCombinerKey other=material;other.options^=SHADER_OPT(FOG);distinct[other]=2;
 other=material;other.combine_mode^=1;distinct[other]=3;
 std::ofstream("native-material-cache.json")<<"{\"sameMaterial\":"<<sameMaterial
  <<",\"stableEntries\":"<<(beforeEntries==interpreter->mColorCombinerPool.size())
  <<",\"distinctInputs\":"<<(distinct.size()==3)<<",\"repeats\":10000}";
 auto manager=Ship::Context::GetRawInstance()->GetResourceManager();

 auto fragments=std::dynamic_pointer_cast<SOH::Array>(manager->LoadResource("objects/object_hanareyama_obj/object_hanareyama_obj_DLArray_004638"));
 bool fragmentShape=fragments&&fragments->ArrayType==SOH::ArrayResourceType::Pointer&&fragments->Pointers.size()==54&&fragments->GetPointerSize()==54*sizeof(const char*);
 bool fragmentTargets=fragmentShape;
 if(fragmentShape)for(const char* path:fragments->Pointers){
  if(!path||std::string_view(path).find("__OTR__objects/object_hanareyama_obj/")!=0){fragmentTargets=false;break;}
  auto resource=manager->LoadResource(path);
  fragmentTargets&=resource&&resource->GetInitData()->Type==static_cast<uint32_t>(Fast::ResourceType::DisplayList)&&resource->GetPointerSize()>0;
 }
 std::ofstream("native-array-pointers.json")<<"{\"shapeValid\":"<<fragmentShape<<",\"all54TargetsLoaded\":"<<fragmentTargets<<",\"archiveResourceVersion\":"<<(fragments?fragments->GetInitData()->ResourceVersion:-1)<<"}";
 const std::string signaturePath=(const char*)gItemIcons[ITEM_BOW];
 const std::string path=signaturePath.starts_with("__OTR__")?signaturePath.substr(7):signaturePath;
 const Ship::ResourceIdentifier id{path,0,nullptr};
 // Exercise actual replay lookup lifetime: same-tick ownership, explicit cache
 // invalidation, next-tick dirty reload, raw pointers and archive-hash identity.
 interpreter->SetResolvedResourceCacheEnabled(true);
 const auto hash=CRC64(path.c_str());
 const char* resolved=interpreter->ResolveResourcePathCached(hash);
 auto first=interpreter->ResolveResourceCached(signaturePath.c_str());
 bool replayIdentity=resolved&&path==resolved&&first;
 for(int i=0;i<10000;++i)replayIdentity&=interpreter->ResolveResourcePathCached(hash)==resolved&&interpreter->ResolveResourceRawCached(hash)==first->GetRawPointer();
 const auto replayEntries=interpreter->mResolvedResourceCache.size();
 interpreter->SetResolvedResourceCacheEnabled(false);
 bool released=interpreter->mResolvedResourceCache.empty()&&interpreter->mResolvedHashPaths.empty()&&interpreter->mResolvedRawHashes.empty();
 // Exercise the real three-map replay lifetime, including the pooled allocator.
 // Each tick must drop ownership even though storage is reused for the next.
 const auto warmedAllocations=interpreter->mReplayCachePool.Allocations();
 const auto ownerCount=first.use_count();bool poolOwnership=true;
 for(int tick=0;tick<250;++tick){
  interpreter->SetResolvedResourceCacheEnabled(true);
  poolOwnership&=interpreter->ResolveResourcePathCached(hash)==resolved;
  poolOwnership&=interpreter->ResolveResourceRawCached(hash)==first->GetRawPointer();
  poolOwnership&=interpreter->ResolveResourceCached(signaturePath.c_str())==first;
  interpreter->SetResolvedResourceCacheEnabled(false);
  poolOwnership&=first.use_count()==ownerCount;
 }
 const bool poolStable=interpreter->mReplayCachePool.Allocations()==warmedAllocations;
 std::ofstream("native-replay-pool.json")<<"{\"ownershipReleased\":"<<poolOwnership<<",\"steadyAllocationsZero\":"<<poolStable
  <<",\"ticks\":250,\"retainedBytes\":"<<interpreter->mReplayCachePool.RetainedBytes()<<"}";
 if(!poolOwnership||!poolStable)throw std::runtime_error("Replay allocator lifetime regression");
 first->Dirty();
 interpreter->SetResolvedResourceCacheEnabled(true);
 auto next=interpreter->ResolveResourceCached(signaturePath.c_str());
 bool replayReload=next&&next!=first&&interpreter->ResolveResourceRawCached(hash)==next->GetRawPointer();
 interpreter->TextureCacheClear();
 bool explicitClear=interpreter->mResolvedResourceCache.empty()&&interpreter->mResolvedHashPaths.empty()&&interpreter->mResolvedRawHashes.empty();
 interpreter->SetResolvedResourceCacheEnabled(false);
 std::ofstream("native-replay-cache.json")<<"{\"sameResource\":"<<replayIdentity<<",\"boundedEntries\":"<<(replayEntries<=2)<<",\"tickRelease\":"<<released<<",\"dirtyReload\":"<<replayReload<<",\"explicitClear\":"<<explicitClear<<"}";

 NativeResourceCommands(interpreter.get(),hash);
 auto original=manager->LoadResource(id);
 bool identity=original!=nullptr&&manager->LoadResourceAsync(id,false,BS::pr::highest).get()==original&&manager->LoadResource(signaturePath)==original;
 constexpr int count=50000;std::array<double,7> before{},after{};bool same=true;
 for(int run=0;run<7;++run)for(int pass=0;pass<2;++pass){
  const bool direct=(pass+run)%2;auto start=std::chrono::steady_clock::now();
  for(int i=0;i<count;++i){auto value=direct?manager->LoadResource(id):manager->LoadResourceAsync(id,false,BS::pr::highest).get();same&=value==original;}
  auto ns=std::chrono::duration<double,std::nano>(std::chrono::steady_clock::now()-start).count()/count;
  (direct?after:before)[run]=ns;
 }
 std::sort(before.begin(),before.end());std::sort(after.begin(),after.end());
 original->Dirty();auto reloaded=manager->LoadResource(id);
 bool reload=reloaded&&reloaded!=original&&manager->LoadResourceAsync(id,false,BS::pr::highest).get()==reloaded;
 Ship::ResourceIdentifier owned{path,0x4d4d5652,nullptr};auto owner=manager->LoadResource(owned);
 bool owners=owner&&owner!=reloaded&&manager->LoadResourceAsync(owned,false,BS::pr::highest).get()==owner;
 bool missing=!manager->LoadResource("__mmvr_missing_performance_probe__");
 std::ofstream("native-resource-performance.json")<<"{\"iterationsPerSample\":"<<count<<",\"samples\":7,\"baselineNs\":"<<before[3]<<",\"directNs\":"<<after[3]<<",\"sameResource\":"<<(identity&&same)<<",\"dirtyReload\":"<<reload<<",\"ownerIsolation\":"<<owners<<",\"missingSafe\":"<<missing<<"}";

 Ship::VerifyResourceLookupChecks();
}
