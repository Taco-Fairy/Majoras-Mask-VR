// CPU-only native command/stack check, independent of the live renderer state.
void Interpreter::VerifyNativeBoundsCulling() {
    const char* diagnostic = std::getenv("MMVR_NATIVE_TEST");
    if (!diagnostic || std::strcmp(diagnostic, "1") != 0) return;
    struct Restore {
        GfxExecStack stack = std::move(g_exec_stack);
        mmvr::CullingAudit audit = mmvr::cullingAudit;
        ~Restore() { g_exec_stack = std::move(stack); mmvr::cullingAudit = audit; }
    } restore;
    Interpreter probe;
    CommandInterpreterScope scope(&probe);
    F3DGfx parent[2]{}, child[2]{};
    unsigned checks = 0;
    auto check = [&](bool ok) { ++checks; if (!ok) throw std::runtime_error("Native bounds command regression"); };
    auto bounds = [&](unsigned first, unsigned last) {
        child[0].words.w0 = (uintptr_t(F3DEX2_G_CULLDL) << 24) | first;
        child[0].words.w1 = last;
    };
    auto begin = [&]() {
        g_exec_stack.start(parent + 1);
        g_exec_stack.call(parent, child);
        probe.mMarkerOn = true;
    };
    bounds(0, 14);
    for (unsigned plane : {1u, 2u, 4u, 8u, 32u}) {
        for (auto& v : probe.mRsp->loaded_vertices) v.clip_rej = plane;
        begin();
        auto& command = g_exec_stack.currCmd();
        check(gfx_cull_native_bounds(&command, false));
        // Do not read command after returning: it was a reference to the popped entry.
        check(g_exec_stack.cmd_stack.size() == 1 && g_exec_stack.currCmd() == parent + 1);
        check(g_exec_stack.gfx_path.empty() && !probe.mMarkerOn);
        for (int visible = 0; visible < 8; ++visible) {
            probe.mRsp->loaded_vertices[visible].clip_rej = 0;
            begin(); auto* before = g_exec_stack.currCmd();
            check(!gfx_cull_native_bounds(&g_exec_stack.currCmd(), false));
            check(g_exec_stack.cmd_stack.size() == 2 && g_exec_stack.currCmd() == before && probe.mMarkerOn);
            probe.mRsp->loaded_vertices[visible].clip_rej = plane;
        }
    }
    for (auto& v : probe.mRsp->loaded_vertices) v.clip_rej = 16;
    begin(); check(!gfx_cull_native_bounds(&g_exec_stack.currCmd(), false)); // Near plane stays conservative.
    for (auto& v : probe.mRsp->loaded_vertices) v.clip_rej = 1;
    for (auto range : {std::pair{1u,14u}, {0u,15u}, {14u,0u}, {0u,65534u}}) {
        bounds(range.first,range.second);begin();
        check(!gfx_cull_native_bounds(&g_exec_stack.currCmd(), false));
        check(g_exec_stack.cmd_stack.size() == 2);
    }
    bounds(0,14);begin();
    g_exec_stack.branch(child);g_exec_stack.branch(child);
    check(g_exec_stack.cmd_stack.size() == 4);
    check(gfx_cull_native_bounds(&g_exec_stack.currCmd(), false));
    check(g_exec_stack.cmd_stack.size() == 1 && g_exec_stack.currCmd() == parent+1 && g_exec_stack.gfx_path.empty());
    g_exec_stack.start(child);probe.mMarkerOn = true;
    check(gfx_cull_native_bounds(&g_exec_stack.currCmd(), false));
    check(g_exec_stack.cmd_stack.empty() && !probe.mMarkerOn);
    begin();auto oldCandidates = mmvr::cullingAudit.candidates;mmvr::cullingAudit.NewReplay();
    check(!gfx_cull_native_bounds(&g_exec_stack.currCmd(), true));
    check(g_exec_stack.cmd_stack.size() == 2 && probe.mMarkerOn && mmvr::cullingAudit.candidates == oldCandidates+1);
    // The ordinary end handler must leave the same parent pointer/path/marker.
    check(gfx_end_dl_handler_common(&g_exec_stack.currCmd()));
    check(g_exec_stack.cmd_stack.size() == 1 && g_exec_stack.currCmd() == parent+1 && !probe.mMarkerOn);
    // Paired native-list return: either visible eye or either safety margin
    // retains the shared list. Exercise the actual return/branch-stack helper.
    const auto pairedGuard=mmvr::MakeCullingGuard({-.8f,.8f,.8f,-.8f},20);
    mmvr::CullingGuard pairedGuards[2]{pairedGuard,pairedGuard};
    auto resetBounds=[&]() {
        bounds(0,14);
        for(unsigned i=0;i<8;++i) {
            for(auto* vertex:{&probe.mRsp->loaded_vertices[i],&probe.mStereoRsp.loaded_vertices[i]}) {
                vertex->x=10;vertex->y=0;vertex->z=0;vertex->w=1;vertex->clip_rej=2;
            }
        }
        begin();
    };
    resetBounds();
    check(gfx_cull_native_bounds_checked(&g_exec_stack.currCmd(),false,pairedGuards));
    check(g_exec_stack.cmd_stack.size()==1 && g_exec_stack.currCmd()==parent+1 && !probe.mMarkerOn);
    for(bool right:{false,true}) {
        resetBounds();
        auto& vertex=right?probe.mStereoRsp.loaded_vertices[0]:probe.mRsp->loaded_vertices[0];
        vertex.x=0;vertex.clip_rej=0;
        check(!gfx_cull_native_bounds_checked(&g_exec_stack.currCmd(),false,pairedGuards));
        check(g_exec_stack.cmd_stack.size()==2 && probe.mMarkerOn);
        // Outside the actual eye but inside its padded field remains visible.
        vertex.x=1.01f;vertex.clip_rej=2;
        check(!gfx_cull_native_bounds_checked(&g_exec_stack.currCmd(),false,pairedGuards));
    }
    resetBounds();pairedGuards[1].active=false;
    check(!gfx_cull_native_bounds_checked(&g_exec_stack.currCmd(),false,pairedGuards));
    pairedGuards[1]=pairedGuard;
    resetBounds();
    for(unsigned i=0;i<8;++i){probe.mStereoRsp.loaded_vertices[i].x=-10;probe.mStereoRsp.loaded_vertices[i].clip_rej=1;}
    g_exec_stack.branch(child);g_exec_stack.branch(child);
    check(gfx_cull_native_bounds_checked(&g_exec_stack.currCmd(),false,pairedGuards));
    check(g_exec_stack.cmd_stack.size()==1 && g_exec_stack.currCmd()==parent+1);
    resetBounds();
    check(!gfx_cull_native_bounds_checked(&g_exec_stack.currCmd(),true,pairedGuards));
    check(g_exec_stack.cmd_stack.size()==2 && probe.mMarkerOn);
    // Real interpreter cache lifetime and immediate reappearance, using native
    // vertex slots and encoded TRI2 commands. No live tracking state is changed.
    F3DGfx packets[65]{};
    for(unsigned i=0;i<64;++i){packets[i].words.w0=0x06000204;packets[i].words.w1=0x00020406;}
    packets[64].words.w0=uintptr_t(F3DEX2_G_ENDDL)<<24;
    const auto guard=mmvr::MakeCullingGuard({-.8f,.8f,.8f,-.8f},20);
    probe.SetResolvedResourceCacheEnabled(true);
    for(auto& v:probe.mRsp->loaded_vertices){v.x=10;v.y=0;v.w=1;v.clip_rej=2;}
    auto* cursor=packets;
    check(probe.CullTrianglePacketGuarded(cursor,guard) && cursor==packets+64);
    const auto loaded=probe.mRsp->loaded_vertices[0];
    probe.mRsp->loaded_vertices[0].x=0;cursor=packets;
    check(!probe.CullTrianglePacketGuarded(cursor,guard) && cursor==packets);
    probe.mRsp->loaded_vertices[0]=loaded;
    probe.SetResolvedResourceCacheEnabled(false);check(probe.mTrianglePackets.empty());
    // Same address, different next tick: no stale skip through a state opcode.
    packets[3].words.w0=uintptr_t(F3DEX2_G_MTX)<<24;
    probe.SetResolvedResourceCacheEnabled(true);cursor=packets;
    check(probe.CullTrianglePacketGuarded(cursor,guard) && cursor==packets+3);
    cursor=packets;check(!probe.CullTrianglePacketGuarded(cursor,{}) && cursor==packets);
    std::ofstream benchmark("native-packet-culling.json");benchmark<<"{\"passed\":true,\"workloads\":[";
    bool first=true;
    for(unsigned length:{3u,8u,32u,64u}){
        for(unsigned i=0;i<64;++i)packets[i].words.w0=0x06000204;
        packets[length].words.w0=uintptr_t(F3DEX2_G_ENDDL)<<24;
        probe.SetResolvedResourceCacheEnabled(false);probe.SetResolvedResourceCacheEnabled(true);
        cursor=packets;check(probe.CullTrianglePacketGuarded(cursor,guard));
        std::vector<double> before,after;
        for(int round=0;round<7;++round)for(int pass=0;pass<2;++pass){
            const bool candidate=(round+pass)%2;
            auto start=std::chrono::steady_clock::now();
            for(int repeat=0;repeat<2000;++repeat){
                probe.BeginTriangleCommand(true);
                cursor=packets;
                if(!candidate || !probe.CullTrianglePacketGuarded(cursor,guard))
                    for(unsigned i=0;i<length;++i)probe.GfxSpTri2(0,1,2,1,2,3);
                probe.BeginTriangleCommand(false);
            }
            auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            (candidate?after:before).push_back(ms);
        }
        std::sort(before.begin(),before.end());std::sort(after.begin(),after.end());
        if(!first)benchmark<<",";first=false;
        benchmark<<"{\"commands\":"<<length<<",\"packets\":2000,\"referenceMs\":"<<before[3]<<",\"candidateMs\":"<<after[3]<<"}";
    }
    benchmark<<"],\"wholeGameFps\":false}";
    probe.SetResolvedResourceCacheEnabled(false);

    // Early archive-run rejection reconstructs every final vertex slot. Verify
    // real lighting/UV/fog/clip values, including partial overwritten ranges.
    F3DVtx raw[3][4]{};
    F3DGfx early[13]{};
    for(unsigned group=0;group<3;++group) {
        for(unsigned v=0;v<4;++v) {
            raw[group][v].v.ob[0]=int16_t(20+group+v);
            raw[group][v].v.ob[1]=int16_t(v);
            raw[group][v].v.cn[0]=uint8_t(group*30+v);
            raw[group][v].v.cn[3]=255;
            raw[group][v].v.tc[0]=int16_t(group*32+v);
        }
        early[group*4].words.w0=0x01000000;
        early[group*4].words.w1=reinterpret_cast<uintptr_t>(raw[group]);
        for(unsigned t=1;t<4;++t){early[group*4+t].words.w0=0x06000204;early[group*4+t].words.w1=0x00020400;}
    }
    early[12].words.w0=uintptr_t(F3DEX2_G_ENDDL)<<24;
    struct FixtureResolver {
        bool operator()(const F3DGfx* c,mmvr::GeometryLoad& load) const {
            if((c->words.w0>>24)!=1)return false;
            load={reinterpret_cast<const void*>(c->words.w1),uint16_t((c->words.w0&255)?3:4),0,1};return true;
        }
        static std::array<float,3> Position(const mmvr::GeometryLoad& load,unsigned i) {
            return ArchiveGeometryResolver::Position(load,i);
        }
    } resolver;
    early[8].words.w0|=1;
    const auto run=mmvr::CompileGeometryRun(early,resolver);
    check(run.commands==12 && run.retainedVertices==7);
    for(uint32_t mode:{0u,uint32_t(G_LIGHTING),uint32_t(G_LIGHTING|G_FOG|G_TEXTURE_GEN)}) {
        Interpreter reference, candidate;
        for(auto* state:{&reference,&candidate}) {
            state->mCurDimensions.width=320;state->mCurDimensions.height=240;
            state->mRsp->geometry_mode=mode;
            state->mRsp->current_num_lights=1;
            state->mRsp->modelview_matrix_stack_size=1;
            for(unsigned a=0;a<4;++a){state->mRsp->MP_matrix[a][a]=1;state->mRsp->modelview_matrix_stack[0][a][a]=1;}
            state->mRsp->current_lights[0].l.col[0]=100;
            state->mRsp->current_lights[0].l.col[1]=120;
            state->mRsp->current_lights[0].l.col[2]=140;
            state->mRsp->lights_changed=true;
        }
        for(unsigned group=0;group<3;++group)reference.GfxSpVertex(group==2?3:4,0,raw[group]);
        auto* current=early;
        check(candidate.CullGeometryRunGuarded(current,run,guard) && current==early+12);
        check(std::memcmp(reference.mRsp->loaded_vertices,candidate.mRsp->loaded_vertices,sizeof(reference.mRsp->loaded_vertices))==0);
        check(reference.mRsp->lights_changed==candidate.mRsp->lights_changed);
        // Exercise the paired rejection gate and verify its retained writes in
        // the primary RSP. Live paired replay verifies the secondary RSP.
        Interpreter paired;
        paired.mCurDimensions.width=320;paired.mCurDimensions.height=240;
        paired.mRsp->geometry_mode=mode;paired.mRsp->current_num_lights=1;
        paired.mRsp->modelview_matrix_stack_size=1;
        for(unsigned a=0;a<4;++a){paired.mRsp->MP_matrix[a][a]=1;paired.mRsp->modelview_matrix_stack[0][a][a]=1;paired.mStereoRsp.MP_matrix[a][a]=1;}
        paired.mRsp->current_lights[0].l.col[0]=100;
        paired.mRsp->current_lights[0].l.col[1]=120;
        paired.mRsp->current_lights[0].l.col[2]=140;
        paired.mRsp->lights_changed=true;
        current=early;
        check(paired.CullGeometryRunGuarded(current,run,guard,paired.mStereoRsp.MP_matrix,guard) && current==early+12);
        check(std::memcmp(reference.mRsp->loaded_vertices,paired.mRsp->loaded_vertices,sizeof(reference.mRsp->loaded_vertices))==0);
        check(reference.mRsp->lights_changed==paired.mRsp->lights_changed);
        paired.mStereoRsp.MP_matrix[3][0]=-22;current=early;
        check(!paired.CullGeometryRunGuarded(current,run,guard,paired.mStereoRsp.MP_matrix,guard) && current==early);
        paired.mStereoRsp.MP_matrix[3][0]=0;current=early;
        check(!paired.CullGeometryRunGuarded(current,run,guard,paired.mStereoRsp.MP_matrix,{}) && current==early);
        candidate.mRsp->MP_matrix[3][0]=-22;current=early;
        check(!candidate.CullGeometryRunGuarded(current,run,guard) && current==early);
    }
    const char* token=std::getenv("MMVR_SESSION_TOKEN");
    std::ofstream("native-native-bounds.json") << "{\"session\":\"" << (token?token:"")
        << "\",\"passed\":true,\"checks\":" << checks
        << ",\"nativeCallAndBranchReturns\":true,\"observedOnlyPreservesStack\":true}";
}
