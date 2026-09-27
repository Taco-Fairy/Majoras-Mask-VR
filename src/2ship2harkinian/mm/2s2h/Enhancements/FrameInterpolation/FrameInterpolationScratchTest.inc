// Included at the end of FrameInterpolation.cpp only in MMVR builds. This
// native fixture records real operations into isolated recordings and restores
// the live recording pair, path pointers, flags, generation, and matrix stack.
#include <array>
#include <chrono>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <set>
#include <stdexcept>

namespace {
using ScratchProbeMap = std::unordered_map<Mtx*,MtxF>;
static_assert(sizeof(MtxF)==64);

bool ScratchProbeEqual(const ScratchProbeMap& a,const ScratchProbeMap& b){
    if(a.size()!=b.size())return false;
    for(const auto& [key,value]:a){
        const auto found=b.find(key);
        if(found==b.end()||std::memcmp(value.mf,found->second.mf,64)!=0)return false;
    }
    return true;
}
MtxF ScratchProbeMatrix(float seed){
    MtxF value{};
    for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)
        value.mf[row][col]=seed+float(row*4+col)*.25f;
    return value;
}
MtxF ScratchProbeBlend(const MtxF& oldValue,const MtxF& newValue,float alpha){
    MtxF value{};const float weight=1.f-alpha;
    for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)
        value.mf[row][col]=weight*oldValue.mf[row][col]+alpha*newValue.mf[row][col];
    return value;
}

// Swap complete recordings instead of copying/reconstructing them. The saved
// path pointers can include &current_recording.root_path; they are not touched
// while the fixture is active and point at that same restored object afterward.
struct ScratchProbeIsolation {
    Recording current,previous;
    std::vector<Path*> paths,expectedPaths;
    const bool recording=is_recording,pending=recording_reset_pending,discontinuous=recording_discontinuous;
    const bool camera=camera_interpolation,wide=interpolate_wider_angles,nextActor=next_is_actor_pos_rot_matrix;
    const bool hasInverse=has_inv_actor_mtx,ignoreInverse=ignore_inv_actor_mtx;
    const size_t ignoreIndex=ignore_inv_actor_mtx_path_index,inverseIndex=inv_actor_mtx_path_index;
    const uint32_t epoch=camera_epoch,previousEpoch=previous_camera_epoch;
    const uint64_t generation=recording_generation;
    const MtxF inverse=inv_actor_mtx;
    const int fps=CVarGetInteger("gInterpolationFPS",20);
    MtxF* matrixTop=Matrix_GetCurrent();
    MtxF matrixValue=*matrixTop;
    const void* currentItems=current_recording.root_path.items.data();
    const void* previousItems=previous_recording.root_path.items.data();
    bool& restored;
    explicit ScratchProbeIsolation(bool& result):restored(result){
        interpolationPlan.Invalidate();
        expectedPaths=current_path;
        std::swap(current,current_recording);std::swap(previous,previous_recording);paths.swap(current_path);
        is_recording=false;recording_reset_pending=false;recording_discontinuous=false;
        camera_interpolation=true;interpolate_wider_angles=false;next_is_actor_pos_rot_matrix=false;
        has_inv_actor_mtx=false;ignore_inv_actor_mtx=false;
        ignore_inv_actor_mtx_path_index=inv_actor_mtx_path_index=0;
        camera_epoch=previous_camera_epoch=0;
        CVarSetInteger("gInterpolationFPS",120);
        // Keep transformations and VR billboard flags in a temporary stack slot.
        Matrix_Push();Matrix_Put(&gIdentityMtxF);
    }
    ~ScratchProbeIsolation(){
        interpolationPlan.Invalidate();
        is_recording=false;
        Matrix_Pop();
        current_path.clear();
        std::swap(current,current_recording);std::swap(previous,previous_recording);paths.swap(current_path);
        recording_reset_pending=pending;recording_discontinuous=discontinuous;camera_interpolation=camera;
        interpolate_wider_angles=wide;next_is_actor_pos_rot_matrix=nextActor;
        has_inv_actor_mtx=hasInverse;ignore_inv_actor_mtx=ignoreInverse;
        ignore_inv_actor_mtx_path_index=ignoreIndex;inv_actor_mtx_path_index=inverseIndex;inv_actor_mtx=inverse;
        camera_epoch=epoch;previous_camera_epoch=previousEpoch;recording_generation=generation;
        CVarSetInteger("gInterpolationFPS",fps);is_recording=recording;
        restored=Matrix_GetCurrent()==matrixTop&&std::memcmp(matrixTop->mf,matrixValue.mf,64)==0&&
            current_path==expectedPaths&&current_recording.root_path.items.data()==currentItems&&
            previous_recording.root_path.items.data()==previousItems&&recording_generation==generation&&
            recording_reset_pending==pending&&recording_discontinuous==discontinuous&&is_recording==recording&&
            camera_interpolation==camera&&camera_epoch==epoch&&previous_camera_epoch==previousEpoch&&
            interpolate_wider_angles==wide&&next_is_actor_pos_rot_matrix==nextActor&&has_inv_actor_mtx==hasInverse&&
            ignore_inv_actor_mtx==ignoreInverse&&ignore_inv_actor_mtx_path_index==ignoreIndex&&
            inv_actor_mtx_path_index==inverseIndex&&std::memcmp(inv_actor_mtx.mf,inverse.mf,64)==0&&
            CVarGetInteger("gInterpolationFPS",20)==fps;
    }
};
struct ScratchProbeMatrixScope {
    ScratchProbeMatrixScope(){Matrix_Push();Matrix_Put(&gIdentityMtxF);}
    ~ScratchProbeMatrixScope(){Matrix_Pop();}
};
}

bool FrameInterpolation_VerifyScratch(){
    const char* diagnostic=std::getenv("MMVR_NATIVE_TEST");
    if(!diagnostic||std::strcmp(diagnostic,"1")!=0)return true;
    bool restored=false,recorded=true,keySets=true,exactBytes=true,duplicateLastWrite=true;
    bool nodeReuse=true,bucketReuse=true,generationChange=true,pendingEmpty=true,deferredClear=true;
    bool giantBoundary=true,emptyGeneration=true,independentScratch=true,adjustmentMismatch=true;
    unsigned comparisons=0,matrixComparisons=0,nodeChecks=0,bucketChecks=0,duplicateChecks=0,generationChecks=0,mismatchChecks=0;
    size_t warmEntries=0,warmBuckets=0;
    bool compiledPlanReuse=true;
    bool recordingPoolReuse=true;size_t recordingPoolBytes=0;
    {
        ScratchProbeIsolation isolation(restored);
        std::array<Mtx,12> destinations{};
        std::array<int,4> labels{};
        MtxF actorRaw{};
        auto record=[&](int frame,bool reduced,bool giant,bool adjusted=true){
            FrameInterpolation_ShouldInterpolateFrame(true);FrameInterpolation_StartRecord();
            recorded&=is_recording;
            auto direct=ScratchProbeMatrix(float(8+frame*16));
            FrameInterpolation_RecordMatrixMtxFToMtx(&direct,&destinations[0]);
            FrameInterpolation_RecordOpenChild(&labels[0],0);
            Matrix_Push();Matrix_Translate(float(frame*5),float(11+frame),-9.f,MTXMODE_NEW);
            Matrix_RotateYF(float(frame)*.125f,MTXMODE_APPLY);
            Matrix_Scale(giant?10.f:1.f,giant?10.f:1.25f,giant?10.f:.75f,MTXMODE_APPLY);
            FrameInterpolation_RecordMatrixToMtx(&destinations[1],nullptr,0);
            if(!reduced){
                FrameInterpolation_RecordOpenChild(&labels[1],2);
                if(adjusted)FrameInterpolation_RecordActorPosRotMatrix();
                Vec3s rotation{0,s16(frame*1000),0};
                Matrix_SetTranslateRotateYXZ(float(frame*3),7.f,4.f,&rotation);
                Matrix_Scale(giant?10.f:1.f,giant?10.f:1.f,giant?10.f:1.f,MTXMODE_APPLY);
                Matrix_Translate(2.f,3.f,4.f,MTXMODE_APPLY);
                actorRaw=*Matrix_GetCurrent();
                FrameInterpolation_RecordMatrixToMtx(&destinations[2],nullptr,0);
                FrameInterpolation_RecordCloseChild();
            }
            Matrix_Pop();FrameInterpolation_RecordCloseChild();
            // Repeated sibling labels exercise per-label occurrence matching;
            // a repeated destination exercises traversal-order last-write wins.
            if(!reduced)for(int occurrence=0;occurrence<2;++occurrence){
                FrameInterpolation_RecordOpenChild(&labels[2],7);
                auto source=ScratchProbeMatrix(float(32+frame*16+occurrence*4));
                FrameInterpolation_RecordSkinMatrixMtxFToMtx(&source,&destinations[3+occurrence]);
                FrameInterpolation_RecordMatrixMtxFToMtx(&source,&destinations[0]);
                FrameInterpolation_RecordCloseChild();
            }
            FrameInterpolation_RecordOpenChild(&labels[3],reduced?9:8);
            auto extra=ScratchProbeMatrix(float(48+frame*16));
            FrameInterpolation_RecordMatrixMtxFToMtx(&extra,&destinations[reduced?7:5]);
            FrameInterpolation_RecordCloseChild();
            auto last=ScratchProbeMatrix(float(64+frame*16));
            FrameInterpolation_RecordMatrixMtxFToMtx(&last,&destinations[0]);
            FrameInterpolation_StopRecord();
            recorded&=!is_recording&&current_path.size()==1&&current_path[0]==&current_recording.root_path;
        };
        auto fresh=[&](float alpha){ScratchProbeMatrixScope matrices;return FrameInterpolation_Interpolate(alpha);};
        auto reused=[&](float alpha,FrameInterpolationScratch& scratch)->const ScratchProbeMap&{
            ScratchProbeMatrixScope matrices;return FrameInterpolation_Interpolate(alpha,scratch);
        };
        auto compare=[&](float alpha,FrameInterpolationScratch& scratch,const std::set<Mtx*>& expected){
            auto baseline=fresh(alpha);const auto& actual=reused(alpha,scratch);++comparisons;
            std::set<Mtx*> baselineKeys,actualKeys;
            for(const auto& [key,value]:baseline)baselineKeys.insert(key);
            for(const auto& [key,value]:actual)actualKeys.insert(key);
            keySets&=baselineKeys==actualKeys&&actualKeys==expected;
            exactBytes&=ScratchProbeEqual(baseline,actual);matrixComparisons+=unsigned(actual.size());
            return baseline;
        };
        std::set<Mtx*> fullKeys;for(unsigned i=0;i<6;++i)fullKeys.insert(&destinations[i]);
        const std::set<Mtx*> reducedKeys{&destinations[0],&destinations[1],&destinations[7]};
        record(0,false,false);record(1,false,false);
        FrameInterpolationScratch scratch,secondScratch;
        compare(.25f,scratch,fullKeys);
        const auto& warm=reused(.25f,scratch);warmEntries=warm.size();warmBuckets=warm.bucket_count();
        std::map<Mtx*,const MtxF*> addresses;
        for(const auto& [key,value]:warm)addresses[key]=&value;
        const std::array<float,12> alphas{0.f,.125f,.25f,.5f,.75f,1.f,20.f/90.f,40.f/90.f,20.f/120.f,100.f/120.f,.75f,.125f};
        const auto oldLast=ScratchProbeMatrix(64.f),newLast=ScratchProbeMatrix(80.f);
        for(float alpha:alphas){
            auto baseline=compare(alpha,scratch,fullKeys);const auto& actual=reused(alpha,scratch);
            for(const auto& [key,address]:addresses){
                auto entry=actual.find(key);nodeReuse&=entry!=actual.end()&& &entry->second==address;++nodeChecks;
            }
            bucketReuse&=actual.bucket_count()==warmBuckets;++bucketChecks;
            if(alpha==0.f||alpha==.5f||alpha==1.f){
                const auto expected=ScratchProbeBlend(oldLast,newLast,alpha);auto entry=actual.find(&destinations[0]);
                duplicateLastWrite&=entry!=actual.end()&&std::memcmp(entry->second.mf,expected.mf,64)==0;++duplicateChecks;
            }
        }
        const auto& firstMap=reused(.375f,scratch);const auto savedFirst=firstMap;
        const auto& secondMap=reused(.625f,secondScratch);
        independentScratch&=&firstMap!=&secondMap&&ScratchProbeEqual(firstMap,savedFirst)&&ScratchProbeEqual(secondMap,fresh(.625f));
        for(const auto& [key,value]:firstMap){auto found=secondMap.find(key);independentScratch&=found!=secondMap.end()&& &found->second!=&value;}
        ++comparisons;matrixComparisons+=unsigned(secondMap.size());

        const auto planBuilds=interpolationPlan.builds;
        const auto* planStorage=interpolationPlan.items.data();
        compiledPlanReuse&=interpolationPlan.valid&&!interpolationPlan.items.empty()&&
            interpolationPlan.generation==recording_generation;
        for(float alpha:{.125f,.375f,.75f})compare(alpha,scratch,fullKeys);
        compiledPlanReuse&=interpolationPlan.builds==planBuilds&&interpolationPlan.items.data()==planStorage;

        const auto previousGeneration=recording_generation;
        record(2,true,false);
        generationChange&=recording_generation!=previousGeneration;++generationChecks;
        compare(.5f,scratch,reducedKeys);
        const auto& smaller=reused(.5f,scratch);
        generationChange&=smaller.size()==3&&!smaller.contains(&destinations[2])&&!smaller.contains(&destinations[3])&&
            !smaller.contains(&destinations[4])&&!smaller.contains(&destinations[5]);
        compare(0.f,scratch,reducedKeys);compare(1.f,scratch,reducedKeys);

        // This is the same invalidation contract used by a Giant coordinate-
        // scale transition: an already returned map stays usable until the next
        // evaluation, while pending history produces no interpolated matrices.
        const auto& held=reused(.25f,scratch);const auto heldCopy=held;
        const auto beforeReset=recording_generation;
        FrameInterpolation_ResetHistory();
        deferredClear&=ScratchProbeEqual(held,heldCopy);
        FrameInterpolation_ShouldInterpolateFrame(true);
        pendingEmpty&=recording_reset_pending&&recording_generation!=beforeReset;++generationChecks;
        for(float alpha:{0.f,.5f,1.f}){
            compare(alpha,scratch,{});pendingEmpty&=reused(alpha,scratch).empty();
        }
        FrameInterpolation_ResetHistory();
        compare(.5f,scratch,{});pendingEmpty&=reused(.5f,scratch).empty();
        record(3,false,true);
        giantBoundary&=recording_discontinuous&&!recording_reset_pending;
        for(float alpha:{0.f,.5f,1.f}){
            auto baseline=compare(alpha,scratch,fullKeys);
            const auto endpoint=fresh(1.f);
            giantBoundary&=ScratchProbeEqual(baseline,endpoint);
        }
        // Subsequent ordinary records must resume interpolation and replace the
        // discontinuous generation without retaining its larger key set.
        record(4,true,false);compare(.5f,scratch,reducedKeys);
        generationChange&=!recording_discontinuous&&reused(.5f,scratch).size()==3;++generationChecks;

        // The same destination switches between actor-relative and ordinary
        // records in both directions. These spaces cannot be blended: the
        // baseline and scratch must independently match the native current raw
        // matrix captured when recording, for every tested alpha.
        record(5,false,false);
        for(bool adjusted:{false,true}){
            record(adjusted?7:6,false,false,adjusted);
            for(float alpha:{0.f,.5f,1.f}){
                auto baseline=compare(alpha,scratch,fullKeys);
                const auto& actual=reused(alpha,scratch);const auto found=actual.find(&destinations[2]);
                adjustmentMismatch&=found!=actual.end()&&std::memcmp(found->second.mf,actorRaw.mf,64)==0;
                const auto reference=baseline.find(&destinations[2]);
                adjustmentMismatch&=reference!=baseline.end()&&std::memcmp(reference->second.mf,actorRaw.mf,64)==0;
                ++mismatchChecks;
            }
        }

        // Exercise actual recording rebuilds and emitted matrices over100 ticks.
        for(int frame=8;frame<12;++frame) record(frame,false,false);
        const auto warmAllocations=recordingPool.Allocations();
        for(int frame=12;frame<112;++frame){
            record(frame,false,false);compare(.5f,scratch,fullKeys);
        }
        recordingPoolReuse=recordingPool.Allocations()==warmAllocations;
        recordingPoolBytes=recordingPool.RetainedBytes();
        // Same recorded workload and retained output maps in alternating rounds.
        // This isolates tree matching from allocation and from headset FPS.
        std::array<double,7> treeTimes{},planTimes{};
        ScratchProbeMap treeMap,planMap;
        auto measure=[&](bool compiled){
            auto& output=compiled?planMap:treeMap;
            const auto started=std::chrono::steady_clock::now();
            for(int n=0;n<4000;++n){
                ScratchProbeMatrixScope matrices;
                interpolate_into(float(n%6)/6.f,output,compiled);
            }
            return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
        };
        measure(false);measure(true);
        for(unsigned round=0;round<treeTimes.size();++round){
            if(round%2){planTimes[round]=measure(true);treeTimes[round]=measure(false);}
            else {treeTimes[round]=measure(false);planTimes[round]=measure(true);}
            exactBytes&=ScratchProbeEqual(treeMap,planMap);
        }
        std::sort(treeTimes.begin(),treeTimes.end());std::sort(planTimes.begin(),planTimes.end());
        std::ofstream benchmark("native-interpolation-plan-benchmark.json");
        benchmark<<"{\"iterations\":4000,\"rounds\":7,\"treeMs\":"<<treeTimes[3]
            <<",\"planMs\":"<<planTimes[3]<<",\"ratio\":"<<planTimes[3]/treeTimes[3]
            <<",\"sameOutput\":"<<ScratchProbeEqual(treeMap,planMap)<<",\"gameFps\":false}";
        benchmark.close();
        if(!benchmark)throw std::runtime_error("Could not write interpolation-plan benchmark");

        // Item appearance can move balanced stack operations across child paths.
        // Both replay paths must retain every NEW operation, even without an old
        // sample. Dropping the second parent push but replaying a new child's pop
        // previously walked below the stack base on every interpolated frame.
        for(int frame=0;frame<2;++frame){
            FrameInterpolation_ShouldInterpolateFrame(true);FrameInterpolation_StartRecord();
            Matrix_Push();
            if(frame)Matrix_Push();
            FrameInterpolation_RecordOpenChild(&labels[0],42);
            Matrix_Pop();FrameInterpolation_RecordCloseChild();
            if(frame){
                FrameInterpolation_RecordOpenChild(&labels[1],43);
                Matrix_Pop();FrameInterpolation_RecordCloseChild();
            }
            FrameInterpolation_StopRecord();
        }
        for(bool compiled:{false,true})for(int frame=0;frame<120;++frame){
            auto* before=Matrix_GetCurrent();ScratchProbeMap output;
            interpolate_into(float(frame%6)/6.f,output,compiled);
            if(Matrix_GetCurrent()!=before)
                throw std::runtime_error("New item interpolation unbalanced the matrix stack");
        }

        // An authored camera cut must not discard unrelated actor interpolation
        // when the headset remains anchored to the player. Exercise real records.
        for (int frame=0; frame<2; ++frame) {
            FrameInterpolation_SetCameraPolicy(frame==0, true);
            FrameInterpolation_StartRecord();
            auto actor=ScratchProbeMatrix(float(frame*20));
            FrameInterpolation_RecordMatrixMtxFToMtx(&actor,&destinations[0]);
            FrameInterpolation_RecordOpenChild(nullptr,FrameInterpolation_GetCameraEpoch());
            auto camera=ScratchProbeMatrix(float(100+frame*100));
            FrameInterpolation_RecordMatrixMtxFToMtx(&camera,&destinations[1]);
            FrameInterpolation_RecordCloseChild();FrameInterpolation_StopRecord();
        }
        const auto cut=FrameInterpolation_Interpolate(.5f);
        const auto expectedActor=ScratchProbeMatrix(10.f), expectedCamera=ScratchProbeMatrix(200.f);
        if(cut.size()!=2 || std::memcmp(cut.at(&destinations[0]).mf,expectedActor.mf,64) ||
           std::memcmp(cut.at(&destinations[1]).mf,expectedCamera.mf,64))
            throw std::runtime_error("VR camera cut interrupted world interpolation");
        FrameInterpolation_SetCameraPolicy(false,false);FrameInterpolation_StartRecord();
        if(is_recording)throw std::runtime_error("Native theater interpolation policy changed");
        FrameInterpolation_StopRecord();
        FrameInterpolation_ShouldInterpolateFrame(true);FrameInterpolation_StartRecord();FrameInterpolation_StopRecord();
        compare(.5f,scratch,{});emptyGeneration&=reused(.5f,scratch).empty();++generationChecks;
        // References/maps/scratch die here before live recording globals return.
    }
    const bool passed=compiledPlanReuse&&recordingPoolReuse&&recorded&&keySets&&exactBytes&&duplicateLastWrite&&nodeReuse&&bucketReuse&&generationChange&&
        pendingEmpty&&deferredClear&&giantBoundary&&emptyGeneration&&independentScratch&&adjustmentMismatch&&restored&&warmEntries==6;
    const char* session=std::getenv("MMVR_SESSION_TOKEN");
    std::ofstream report("native-interpolation-scratch.json");
    report<<"{\"session\":\""<<(session?session:"")<<"\",\"passed\":"<<passed<<",\"recorded\":"<<recorded
        <<",\"keySets\":"<<keySets<<",\"exactBytes\":"<<exactBytes<<",\"duplicateLastWrite\":"<<duplicateLastWrite
        <<",\"nodeReuse\":"<<nodeReuse<<",\"bucketReuse\":"<<bucketReuse<<",\"generationChange\":"<<generationChange
        <<",\"pendingEmpty\":"<<pendingEmpty<<",\"deferredClear\":"<<deferredClear<<",\"giantBoundary\":"<<giantBoundary
        <<",\"adjustmentMismatch\":"<<adjustmentMismatch<<",\"mismatchChecks\":"<<mismatchChecks
        <<",\"emptyGeneration\":"<<emptyGeneration<<",\"independentScratch\":"<<independentScratch<<",\"restored\":"<<restored
        <<",\"compiledPlanReuse\":"<<compiledPlanReuse
        <<",\"recordingPoolReuse\":"<<recordingPoolReuse<<",\"recordingPoolBytes\":"<<recordingPoolBytes
        <<",\"matrixBytes\":64,\"comparisons\":"<<comparisons<<",\"matrixComparisons\":"<<matrixComparisons
        <<",\"nodeChecks\":"<<nodeChecks<<",\"bucketChecks\":"<<bucketChecks<<",\"duplicateChecks\":"<<duplicateChecks
        <<",\"generationChecks\":"<<generationChecks<<",\"warmEntries\":"<<warmEntries<<",\"warmBuckets\":"<<warmBuckets<<"}";
    report.close();
    if(!report)throw std::runtime_error("Could not write native interpolation-scratch report");
    if(!passed)throw std::runtime_error("Native interpolation-scratch differential regression failed");
    return true;
}
