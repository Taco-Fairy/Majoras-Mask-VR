// CPU-only differential fixture. It never touches the live interpreter or GPU.
void Interpreter::VerifySharedLightingTickCache() {
    const char* diagnostic=std::getenv("MMVR_NATIVE_TEST");
    if(!diagnostic||std::strcmp(diagnostic,"1")!=0)return;
    Interpreter reference,candidate;
    reference.mSharedScenePreparation=false;
    candidate.mSharedScenePreparation=true;
    candidate.mTextureCacheDiagnosticsEnabled=true;
    reference.mFbActive=candidate.mFbActive=true;
    reference.mActiveFrameBuffer=reference.mFrameBuffers.end();
    candidate.mActiveFrameBuffer=candidate.mFrameBuffers.end();
    reference.mCurDimensions.width=candidate.mCurDimensions.width=320;
    reference.mCurDimensions.height=candidate.mCurDimensions.height=240;
    constexpr size_t count=32;
    std::array<F3DVtx,count> source{};
    for(size_t i=0;i<count;++i){
        source[i].v.ob[0]=short(int(i)*3-45);
        source[i].v.ob[1]=short(int(i)*5-65);
        source[i].v.ob[2]=short(int(i)*7+12);
        source[i].v.tc[0]=short(int(i)*41);
        source[i].v.tc[1]=short(int(i)*-27);
        source[i].n.n[0]=int8_t((int(i)%5)*18-36);
        source[i].n.n[1]=int8_t((int(i)%7)*13-39);
        source[i].n.n[2]=80;
    }
    auto configure=[&](Interpreter& probe,int eye,uint32_t mode){
        auto* rsp=probe.mRsp;
        rsp->geometry_mode=mode;
        rsp->modelview_matrix_stack_size=1;
        rsp->current_num_lights=2;
        rsp->texture_scaling_factor.s=0xffff;
        rsp->texture_scaling_factor.t=0xffff;
        rsp->lights_changed=true;
        std::memset(rsp->modelview_matrix_stack[0],0,sizeof(rsp->modelview_matrix_stack[0]));
        std::memset(rsp->MP_matrix,0,sizeof(rsp->MP_matrix));
        for(int axis=0;axis<4;++axis){
            rsp->modelview_matrix_stack[0][axis][axis]=1.f;
            rsp->MP_matrix[axis][axis]=1.f;
        }
        const float angle=float(eye-1)*.035f;
        rsp->modelview_matrix_stack[0][0][0]=std::cos(angle);
        rsp->modelview_matrix_stack[0][0][2]=std::sin(angle);
        rsp->modelview_matrix_stack[0][2][0]=-std::sin(angle);
        rsp->modelview_matrix_stack[0][2][2]=std::cos(angle);
        rsp->current_lights[0].l.col[0]=180;
        rsp->current_lights[0].l.col[1]=150;
        rsp->current_lights[0].l.col[2]=110;
        if(mode&G_LIGHTING_POSITIONAL){
            rsp->current_lights[0].p.unk3=1;
            rsp->current_lights[0].p.pos[0]=80;
            rsp->current_lights[0].p.pos[1]=12;
            rsp->current_lights[0].p.pos[2]=-20;
        }else{
            rsp->current_lights[0].p.unk3=0;
            rsp->current_lights[0].l.dir[0]=40;
            rsp->current_lights[0].l.dir[1]=30;
            rsp->current_lights[0].l.dir[2]=115;
        }
        rsp->current_lights[1].l.col[0]=40;
        rsp->current_lights[1].l.col[1]=35;
        rsp->current_lights[1].l.col[2]=30;
        rsp->lookat[0].dir[0]=127;rsp->lookat[0].dir[1]=0;rsp->lookat[0].dir[2]=0;
        rsp->lookat[1].dir[0]=0;rsp->lookat[1].dir[1]=127;rsp->lookat[1].dir[2]=0;
    };
    bool exact=true;
    auto compare=[&](int eye,uint32_t mode,int mutation=0,size_t batch=0){
        configure(reference,eye,mode);configure(candidate,eye,mode);
        for(auto* probe:{&reference,&candidate}){
            if(mutation==1)probe->mRsp->current_lights[0].l.col[0]++;
            if(mutation==2)probe->mRsp->lookat[0].dir[0]--;
            if(mutation==3)probe->mRsp->modelview_matrix_stack[0][3][0]+=2.f;
            if(mutation==4)probe->mRsp->current_lights[2].l.col[0]++;
            if(mutation==5)probe->mRsp->lookat[0].dir[0]--;
        }
        reference.mSceneVertexBatch=0;candidate.mSceneVertexBatch=batch;
        reference.GfxSpVertexImpl(count,0,source.data(),true);
        candidate.GfxSpVertexImpl(count,0,source.data(),true);
        for(size_t i=0;i<count;++i)
            exact &= std::memcmp(&reference.mRsp->loaded_vertices[i],&candidate.mRsp->loaded_vertices[i],
                                 offsetof(LoadedVertex,clip_rej)+sizeof(LoadedVertex::clip_rej))==0;
    };
    for(int pass=0;pass<3;++pass)for(int eye=0;eye<3;++eye)compare(eye,G_LIGHTING);
    const bool reused=candidate.mPreparedLightingHits>=6;
    const auto beforeMutation=candidate.mPreparedLightingMisses;
    source[0].n.n[0]++;
    compare(0,G_LIGHTING);
    const bool sourceInvalidated=candidate.mPreparedLightingMisses>beforeMutation;
    source[1].v.ob[0]++;
    source[2].v.tc[0]++;
    compare(0,G_LIGHTING);
    auto clear=[&](){
        for(auto& ways:candidate.mPreparedLighting)for(auto& entry:ways)entry.vertices=nullptr;
        candidate.InvalidateAddressLightingCache();
    };
    clear();compare(0,G_LIGHTING);
    const auto beforeActiveLight=candidate.mPreparedLightingMisses;
    compare(0,G_LIGHTING,1);
    const bool activeLightInvalidated=candidate.mPreparedLightingMisses>beforeActiveLight;
    clear();compare(0,G_LIGHTING|G_TEXTURE_GEN);
    const auto beforeTexgenLookat=candidate.mPreparedLightingMisses;
    compare(0,G_LIGHTING|G_TEXTURE_GEN,2);
    const bool texgenLookatInvalidated=candidate.mPreparedLightingMisses>beforeTexgenLookat;
    clear();compare(0,G_LIGHTING);
    const auto beforeIrrelevant=candidate.mPreparedLightingHits;
    compare(0,G_LIGHTING,4); // light 2 is outside the active [0,2) prefix
    compare(0,G_LIGHTING,5); // lookat is unused without texture generation
    compare(0,G_LIGHTING,3); // directional lights ignore model translation
    const bool irrelevantReused=candidate.mPreparedLightingHits>=beforeIrrelevant+3;
    clear();compare(0,G_LIGHTING|G_LIGHTING_POSITIONAL);
    const auto beforePosition=candidate.mPreparedLightingMisses;
    compare(0,G_LIGHTING|G_LIGHTING_POSITIONAL,3);
    const bool positionInvalidated=candidate.mPreparedLightingMisses>beforePosition;
    compare(1,G_LIGHTING|G_TEXTURE_GEN);
    compare(2,G_LIGHTING|G_LIGHTING_POSITIONAL);
    // A new native tick explicitly discards all retained command-position keys.
    clear();
    const auto beforeTick=candidate.mPreparedLightingMisses;
    compare(0,G_LIGHTING);
    const bool tickInvalidated=candidate.mPreparedLightingMisses>beforeTick;
    // A mesh reached at a different command ordinal must share lighting only
    // when its complete input key and current source bytes still match.
    const bool savedAddressExperiment=mmvr::lightingAddressExperiment;
    const bool savedAddressReference=mmvr::lightingAddressReference;
    mmvr::lightingAddressExperiment=true;mmvr::lightingAddressReference=false;
    clear();compare(0,G_LIGHTING);
    const auto beforeAddressReuse=candidate.mPreparedLightingHits;
    compare(0,G_LIGHTING,0,37);
    const bool addressReused=candidate.mPreparedLightingHits==beforeAddressReuse+1;
    const auto beforeAddressMutation=candidate.mPreparedLightingMisses;
    source[0].n.n[0]++;compare(0,G_LIGHTING,0,38);
    source[1].v.ob[0]++;compare(0,G_LIGHTING,0,39);
    source[2].v.tc[0]++;compare(0,G_LIGHTING,0,40);
    const bool addressSourceInvalidated=candidate.mPreparedLightingMisses==beforeAddressMutation+3;
    const auto beforeAddressLight=candidate.mPreparedLightingMisses;
    compare(0,G_LIGHTING,1,41);
    const bool addressLightInvalidated=candidate.mPreparedLightingMisses==beforeAddressLight+1;
    compare(0,G_LIGHTING|G_TEXTURE_GEN,0,42);
    const auto beforeAddressLookat=candidate.mPreparedLightingMisses;
    compare(0,G_LIGHTING|G_TEXTURE_GEN,2,43);
    const bool addressLookatInvalidated=candidate.mPreparedLightingMisses==beforeAddressLookat+1;
    compare(0,G_LIGHTING|G_LIGHTING_POSITIONAL,0,44);
    const auto beforeAddressPosition=candidate.mPreparedLightingMisses;
    compare(0,G_LIGHTING|G_LIGHTING_POSITIONAL,3,45);
    const bool addressPositionInvalidated=candidate.mPreparedLightingMisses==beforeAddressPosition+1;
    clear();
    const auto beforeAddressTick=candidate.mPreparedLightingMisses;
    compare(0,G_LIGHTING,0,46);
    const bool addressTickInvalidated=candidate.mPreparedLightingMisses==beforeAddressTick+1;
    const auto beforeAddressNewTickReuse=candidate.mPreparedLightingHits;
    compare(0,G_LIGHTING,0,47);
    const bool addressNewTickReused=candidate.mPreparedLightingHits==beforeAddressNewTickReuse+1;
    mmvr::lightingAddressExperiment=savedAddressExperiment;
    mmvr::lightingAddressReference=savedAddressReference;
    if(!exact||!addressReused||!addressSourceInvalidated||!addressLightInvalidated||
       !addressLookatInvalidated||!addressPositionInvalidated||!addressTickInvalidated||!addressNewTickReused)
        throw std::runtime_error("Address lighting cache differential failed");
    // Same three model/eye variants as the real native/left/right replay, then
    // several interpolated replays of the unchanged display list.
    Interpreter prior;
    prior.mSharedScenePreparation=true;
    prior.mFbActive=true;
    prior.mActiveFrameBuffer=prior.mFrameBuffers.end();
    prior.mCurDimensions.width=320;prior.mCurDimensions.height=240;
    // Both paths use the same three-way cache. The prior path discards it at
    // every render; the candidate retains it for a five-render native tick.
    constexpr int rounds=2000;
    auto run=[&](Interpreter& probe,bool retainWithinTick){
        uint64_t checksum=0;
        const auto begin=std::chrono::steady_clock::now();
        for(int frame=0;frame<rounds;++frame){
            if(!retainWithinTick||frame%5==0)
                for(auto& ways:probe.mPreparedLighting)for(auto& entry:ways)entry.vertices=nullptr;
            for(int eye=0;eye<3;++eye){
                configure(probe,eye,G_LIGHTING);
                probe.mSceneVertexBatch=0;
                probe.GfxSpVertexImpl(count,0,source.data(),true);
                checksum+=probe.mRsp->loaded_vertices[frame%count].color.r;
            }
        }
        const auto elapsed=std::chrono::steady_clock::now()-begin;
        return std::pair{std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count(),checksum};
    };
    const auto baseline=run(prior,false);
    const auto optimized=run(candidate,true);
    exact &= baseline.second==optimized.second;
    const char* session=std::getenv("MMVR_SESSION_TOKEN");
    std::ofstream("native-shared-lighting-cache.json")
        <<"{\"session\":\""<<(session?session:"")<<"\",\"exact\":"<<exact
        <<",\"reused\":"<<reused<<",\"sourceInvalidated\":"<<sourceInvalidated
        <<",\"tickInvalidated\":"<<tickInvalidated
        <<",\"activeLightInvalidated\":"<<activeLightInvalidated
        <<",\"texgenLookatInvalidated\":"<<texgenLookatInvalidated
        <<",\"irrelevantReused\":"<<irrelevantReused
        <<",\"positionInvalidated\":"<<positionInvalidated
        <<",\"hits\":"<<candidate.mPreparedLightingHits
        <<",\"misses\":"<<candidate.mPreparedLightingMisses
        <<",\"referenceNs\":"<<baseline.first<<",\"candidateNs\":"<<optimized.first<<"}";
    if(!exact||!reused||!sourceInvalidated||!activeLightInvalidated||!texgenLookatInvalidated||
       !irrelevantReused||!positionInvalidated||!tickInvalidated)
        throw std::runtime_error("Shared lighting tick cache differential failed");
}
