// Included inside namespace Fast. Replays bounded scene-derived vertex inputs
// without a GPU or game state. This is a CPU equivalence and cost check only.
void Interpreter::VerifyCapturedLightingWorkload(const char* path) {
    const char* diagnostic=std::getenv("MMVR_NATIVE_TEST");
    if(!path||!diagnostic||std::strcmp(diagnostic,"1")!=0)
        throw std::runtime_error("Lighting workload probe requires private native-test input");
    std::ifstream input(path,std::ios::binary);
    mmvr::LightingCaptureHeader header{};
    input.read(reinterpret_cast<char*>(&header),sizeof(header));
    const mmvr::LightingCaptureHeader expected{};
    if(!input||std::memcmp(&header,&expected,sizeof(header)))
        throw std::runtime_error("Lighting workload capture header/version mismatch");
    std::vector<mmvr::LightingCaptureRecord> records;
    std::array<uint32_t,10> poseCounts{};
    mmvr::LightingCaptureRecord record{};
    while(input.read(reinterpret_cast<char*>(&record),sizeof(record))){
        if(records.size()>=5120||record.pose>=10||record.batch>=512||record.count==0||record.count>68||
           record.dest>=68||record.count>68-record.dest||record.lightCount<1||record.lightCount>33||
           (!records.empty()&&record.pose<records.back().pose))
            throw std::runtime_error("Lighting workload capture record invalid or unbounded");
        ++poseCounts[record.pose];
        records.push_back(record);
    }
    if(input.gcount()!=0||records.empty())throw std::runtime_error("Lighting workload capture truncated or empty");
    for(auto count:poseCounts)if(count<16||count>512)
        throw std::runtime_error("Lighting workload capture missing or oversized pose");
    using SourceBuffer=std::array<F3DVtx,MAX_VERTICES+4>;
    auto makeProbe=[](){
        auto probe=std::make_unique<Interpreter>();
        probe->mSharedScenePreparation=true;
        probe->mTextureCacheDiagnosticsEnabled=true;
        probe->mFbActive=true;
        probe->mActiveFrameBuffer=probe->mFrameBuffers.end();
        probe->mCurDimensions.width=320;probe->mCurDimensions.height=240;
        return probe;
    };
    auto prepare=[](Interpreter& probe,const mmvr::LightingCaptureRecord& item,F3DVtx* source){
        auto* rsp=probe.mRsp;
        rsp->geometry_mode=item.mode;
        rsp->modelview_matrix_stack_size=1;
        std::memcpy(rsp->modelview_matrix_stack[0],item.model,sizeof(item.model));
        std::memcpy(rsp->MP_matrix,item.mp,sizeof(item.mp));
        std::memcpy(rsp->current_lights,item.lights,sizeof(item.lights));
        std::memcpy(rsp->lookat,item.lookat,sizeof(item.lookat));
        rsp->current_num_lights=uint8_t(item.lightCount);
        rsp->texture_scaling_factor.s=item.scaleS;
        rsp->texture_scaling_factor.t=item.scaleT;
        rsp->fog_mul=item.fogMul;rsp->fog_offset=item.fogOffset;
        rsp->lights_changed=true;
        probe.mSceneVertexBatch=item.batch;
        std::memcpy(source,item.vertices,item.count*sizeof(F3DVtx));
    };
    auto reference=makeProbe(),candidate=makeProbe();
    auto referenceSources=std::make_unique<std::vector<SourceBuffer>>(512);
    auto candidateSources=std::make_unique<std::vector<SourceBuffer>>(512);
    uint32_t previousPose=UINT32_MAX;
    bool exact=true;
    for(const auto& item:records){
        if(item.pose!=previousPose){
            for(auto& ways:reference->mPreparedLighting)for(auto& entry:ways)entry.vertices=nullptr;
            previousPose=item.pose;
        }
        auto* refSource=(*referenceSources)[item.batch].data();
        auto* newSource=(*candidateSources)[item.batch].data();
        prepare(*reference,item,refSource);
        prepare(*candidate,item,newSource);
        mmvr::SetLightingBenchVariant(mmvr::LightingBenchVariant::PriorOneWay,int(item.pose));
        reference->GfxSpVertexImpl(item.count,item.dest,refSource,true);
        mmvr::SetLightingBenchVariant(mmvr::LightingBenchVariant::RetainedThreeWay,int(item.pose));
        candidate->GfxSpVertexImpl(item.count,item.dest,newSource,true);
        for(size_t i=item.dest;i<item.dest+item.count;++i)
            exact&=std::memcmp(&reference->mRsp->loaded_vertices[i],&candidate->mRsp->loaded_vertices[i],
                               offsetof(LoadedVertex,clip_rej)+sizeof(LoadedVertex::clip_rej))==0;
    }
    mmvr::SetLightingBenchVariant(mmvr::LightingBenchVariant::Off,0);
    if(!exact)throw std::runtime_error("Real lighting workload changed loaded vertices");
    // Allocate and touch the same per-batch buffers before measuring. Each
    // measured stream is a new native tick: retained entries expire once at
    // its start, while the original one-way entries expire for every pose.
    constexpr unsigned measuredTicks=3;
    struct Trial{const char* variant;int64_t nanos;uint64_t hits,misses;};
    std::array<Trial,12> trials{};
    for(size_t trial=0;trial<trials.size();++trial){
        const bool prior=trial%4==0||trial%4==3;
        auto probe=makeProbe();
        auto sources=std::make_unique<std::vector<SourceBuffer>>(512);
        auto runTick=[&](){
            for(auto& ways:probe->mPreparedLighting)for(auto& entry:ways)entry.vertices=nullptr;
            uint32_t pose=UINT32_MAX;
            for(const auto& item:records){
                if(prior&&item.pose!=pose)
                    for(auto& ways:probe->mPreparedLighting)for(auto& entry:ways)entry.vertices=nullptr;
                pose=item.pose;
                auto* source=(*sources)[item.batch].data();
                prepare(*probe,item,source);
                mmvr::SetLightingBenchVariant(prior?mmvr::LightingBenchVariant::PriorOneWay:
                    mmvr::LightingBenchVariant::RetainedThreeWay,int(item.pose));
                probe->GfxSpVertexImpl(item.count,item.dest,source,true);
            }
        };
        runTick();
        probe->mPreparedLightingHits=0;
        probe->mPreparedLightingMisses=0;
        const auto start=std::chrono::steady_clock::now();
        for(unsigned tick=0;tick<measuredTicks;++tick)runTick();
        const auto elapsed=std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now()-start).count();
        trials[trial]={prior?"prior":"retained",elapsed/measuredTicks,
            probe->mPreparedLightingHits,probe->mPreparedLightingMisses};
        mmvr::SetLightingBenchVariant(mmvr::LightingBenchVariant::Off,0);
    }
    std::ofstream output("native-lighting-workload-probe.json");
    output<<"{\"exact\":true,\"records\":"<<records.size()<<",\"poses\":[";
    for(size_t i=0;i<poseCounts.size();++i){if(i)output<<',';output<<poseCounts[i];}
    output<<"],\"baseline\":\"original one-way per replay\",\"measure\":\"warmed CPU vertex replay per native tick, no GPU or XR\",\"warmupTicks\":1,\"measuredTicks\":"<<measuredTicks<<",\"trials\":[";
    for(size_t i=0;i<trials.size();++i){if(i)output<<',';const auto& t=trials[i];
        output<<"{\"variant\":\""<<t.variant<<"\",\"ns\":"<<t.nanos
              <<",\"hits\":"<<t.hits<<",\"misses\":"<<t.misses<<'}';}
    output<<"]}";
}
