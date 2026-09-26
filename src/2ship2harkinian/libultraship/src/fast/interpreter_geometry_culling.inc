// Included inside fast namespace. Archive resources are immutable during one
// native tick; dynamic/segmented vertex storage deliberately uses the normal path.
struct ArchiveGeometryResolver {
    Interpreter& interpreter;
    bool operator()(const F3DGfx* command, mmvr::GeometryLoad& load) const {
        const unsigned op=uint32_t(command->words.w0)>>24;
        const F3DVtx* vertices=nullptr;
        size_t count=0, destination=0;
        if (op==OTR_G_VTX_OTR_HASH) {
            const uintptr_t offset=command->words.w1;
            if (offset>0xFFFFF) return false; // Unowned cached/dynamic pointer.
            count=(command->words.w0>>12)&255;
            const size_t end=(command->words.w0>>1)&127;
            if (end<count) return false;
            destination=end-count;
            const uint64_t hash=(uint64_t(command[1].words.w0)<<32)+command[1].words.w1;
            auto raw=interpreter.ResolveResourceRawCached(hash);
            if (!raw) return false;
            vertices=reinterpret_cast<const F3DVtx*>(static_cast<const char*>(raw)+offset);
        } else if (op==OTR_G_VTX_OTR_FILEPATH) {
            const uintptr_t path=command->words.w1;
            if (!PlausibleHostAddress(path)) return false;
            count=command[1].words.w0; destination=command[1].words.w1>>16;
            auto raw=interpreter.ResolveResourceRawCached(reinterpret_cast<const char*>(path));
            if (!raw) return false;
            vertices=static_cast<const F3DVtx*>(raw)+(command[1].words.w1&0xFFFF);
        } else return false;
        if (!count || destination>=68 || count>68-destination) return false;
        load={vertices,uint16_t(count),uint16_t(destination),2};
        return true;
    }
    static std::array<float,3> Position(const mmvr::GeometryLoad& load, unsigned index) {
        const auto& v=static_cast<const F3DVtx*>(load.vertices)[index].v;
        return {float(v.ob[0]),float(v.ob[1]),float(v.ob[2])};
    }
};
bool Interpreter::CullGeometryRun(F3DGfx*& command) {
    const uintptr_t address=reinterpret_cast<uintptr_t>(command);
    if (address>mGeometryRunBegin && address<mGeometryRunEnd) return false;
    if (!nativeBoundsReplay || mFbActive || !mShaderStack.empty() || !mResolvedResourceCacheEnabled ||
        ucode_handler_index!=ucode_f3dex2 || !mmvr::PerspectivePass() ||
        mmvr::CullingAuditEnabled() || mmvr::CullingReferenceEnabled()) return false;
    if (mmvr::MultiviewActive() &&
        (mRsp==&mStereoRsp || (mmvr::PairedVertexBenchEnabled() && mmvr::pairedVertexPackingReference))) return false;
    mmvr::CullingGuard guard=mmvr::CurrentCullingGuard(), secondGuard{};
    if (mmvr::MultiviewActive() && !mmvr::StereoCullingGuards(guard,secondGuard)) return false;
    if (!guard.active) return false;
    auto found=mGeometryRuns.find(command);
    if (found==mGeometryRuns.end()) {
        if (mGeometryRuns.size()>=16384) return false;
        found=mGeometryRuns.emplace(command,mmvr::CompileGeometryRun(command,ArchiveGeometryResolver{*this})).first;
    }
    const auto& run=found->second;
    if (run.commands) {
        mGeometryRunBegin=address;
        mGeometryRunEnd=reinterpret_cast<uintptr_t>(command+run.commands);
    }
    return CullGeometryRunGuarded(command,run,guard,
                                  mmvr::MultiviewActive()?mStereoRsp.MP_matrix:nullptr,secondGuard);
}
bool Interpreter::CullGeometryRunGuarded(F3DGfx*& command, const mmvr::GeometryRun& run, mmvr::CullingGuard guard,
                                         const float (*secondMatrix)[4], mmvr::CullingGuard secondGuard) {
    if (!run.commands) return false;
    ++mmvr::cullingAudit.packetTests;
    if (secondMatrix) {
        if (!mmvr::GeometryOutsideBothGuards(run,mRsp->MP_matrix,guard,secondMatrix,secondGuard)) return false;
    } else if (!mmvr::GeometryOutsideGuard(run,mRsp->MP_matrix,guard)) return false;
    // A later command may read any loaded vertex, including a previous range
    // partly overwritten by the final load. Replay the minimal ordered writes
    // that reconstruct ALL slots, not just the last vertex command.
    for (const auto& load:run.retained)
        GfxSpVertex(load.count,load.destination,static_cast<const F3DVtx*>(load.vertices));
    ++mmvr::cullingAudit.skippedPackets;
    mmvr::cullingAudit.skippedPacketCommands+=run.triangles;
    mmvr::cullingAudit.skippedPacketVertices+=run.vertices-run.retainedVertices;
    command+=run.commands;
    return true;
}
