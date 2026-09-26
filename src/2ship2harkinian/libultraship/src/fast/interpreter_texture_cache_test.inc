// Included inside namespace Fast. The native probe uses the real cache methods
// with an isolated recording renderer, never the live game's GPU objects.
namespace {
class TextureCacheProbeRenderer final : public GfxRenderingAPI {
  public:
    uint32_t created=0;uint64_t selections=0,mirrorSelections=0,uploads=0,draws=0;
    std::array<uint32_t,SHADER_MAX_TEXTURES> bound{};
    const char* GetName() override{return "texture-cache-probe";}
    int GetMaxTextureSize() override{return 4096;}
    GfxClipParameters GetClipParameters() override{return {false,false};}
    void UnloadShader(ShaderProgram*) override{}
    void LoadShader(ShaderProgram*) override{}
    void ClearShaderCache() override{}
    ShaderProgram* CreateAndLoadNewShader(uint64_t,uint64_t) override{return nullptr;}
    ShaderProgram* LookupShader(uint64_t,uint64_t) override{return nullptr;}
    void ShaderGetInfo(ShaderProgram*,uint8_t* inputs,bool textures[2]) override{*inputs=0;textures[0]=textures[1]=false;}
    uint32_t NewTexture() override{return ++created;}
    void SelectTexture(int tile,uint32_t texture) override{++selections;bound.at(tile)=texture;}
    void UploadTexture(const uint8_t*,uint32_t,uint32_t) override{++uploads;}
    void SetSamplerParameters(int,bool,uint32_t,uint32_t) override{}
    void SetDepthTestAndMask(bool,bool) override{}
    void SetCurrentPrimDepth(float) override{}
    void SetZmodeDecal(bool) override{}
    void SetViewport(int,int,int,int) override{}
    void SetScissor(int,int,int,int) override{}
    void SetUseAlpha(bool) override{}
    void DrawTriangles(float*,size_t,size_t) override{++draws;}
    void Init() override{}
    void OnResize() override{}
    void StartFrame() override{}
    void EndFrame() override{}
    void FinishRender() override{}
    int CreateFramebuffer() override{return 0;}
    void UpdateFramebufferParameters(int,uint32_t,uint32_t,uint32_t,bool,bool,bool,bool) override{}
    void StartDrawToFramebuffer(int,float) override{}
    void CopyFramebuffer(int,int,int,int,int,int,int,int,int,int) override{}
    void ClearFramebuffer(bool,bool) override{}
    void ReadFramebufferToCPU(int,uint32_t,uint32_t,uint16_t*) override{}
    void ResolveMSAAColorBuffer(int,int) override{}
    std::unordered_map<std::pair<float,float>,uint16_t,hash_pair_ff>
        GetPixelDepth(int,const std::set<std::pair<float,float>>&) override{return {};}
    void* GetFramebufferTextureId(int) override{return nullptr;}
    void SelectTextureFb(int framebuffer) override{++mirrorSelections;bound[0]=0xf0000000u|uint32_t(framebuffer);}
    void DeleteTexture(uint32_t) override{}
    void SetTextureFilter(FilteringMode) override{}
    FilteringMode GetTextureFilter() override{return FILTER_THREE_POINT;}
    void SetSrgbMode() override{}
    ImTextureID GetTextureById(int) override{return (ImTextureID)0;}
};
}

void Interpreter::WriteTextureCacheDiagnostics(const char* path) const {
    if(!mTextureCacheDiagnosticsEnabled||!path)return;
    const char* session=std::getenv("MMVR_SESSION_TOKEN");
    const auto& counts=mTextureCacheDiagnostics;
    std::ofstream(path)<<"{\"session\":\""<<(session?session:"")<<"\",\"currentHits\":"<<counts.currentHits
        <<",\"mapHits\":"<<counts.mapHits<<",\"misses\":"<<counts.misses
        <<",\"lookups\":"<<(counts.currentHits+counts.mapHits+counts.misses)
        <<",\"entries\":"<<mTextureCache.map.size()
        <<",\"sharedLightingHits\":"<<mPreparedLightingHits
        <<",\"sharedLightingMisses\":"<<mPreparedLightingMisses<<"}";
}

void Interpreter::VerifyTextureCacheFastPath() {
    const char* diagnostic=std::getenv("MMVR_NATIVE_TEST");
    if(!diagnostic||std::strcmp(diagnostic,"1")!=0)return;
    TextureCacheProbeRenderer backend;
    Interpreter probe;probe.mRapi=&backend;
    // Production Init reserves these buckets so LRU map iterators remain valid.
    probe.mTextureCache.map.reserve(TEXTURE_CACHE_MAX_SIZE);
    std::array<uint8_t,TEXTURE_CACHE_MAX_SIZE+2> addresses{};
    std::array<uint8_t,4> palettes{};
    TextureCacheKey base{addresses.data(),{nullptr,nullptr},G_IM_FMT_RGBA,G_IM_SIZ_16b,0,16};
    const bool firstMiss=!probe.TextureCacheLookup(0,base);
    const uint32_t baseId=backend.bound[0];
    const auto firstCounts=probe.mTextureCacheDiagnostics;
    const auto firstSelections=backend.selections;
    bool sameKey=firstMiss&&baseId!=0;
    constexpr unsigned repeats=1000;
    for(unsigned i=0;i<repeats;++i)
        sameKey&=probe.TextureCacheLookup(0,base)&&backend.bound[0]==baseId&&probe.mTextureCache.map.size()==1;
    sameKey&=probe.mTextureCacheDiagnostics.currentHits==firstCounts.currentHits+repeats&&
        probe.mTextureCacheDiagnostics.mapHits==firstCounts.mapHits&&
        probe.mTextureCacheDiagnostics.misses==firstCounts.misses&&backend.selections==firstSelections+repeats;

    // Every field apart from the unchanged address must remain part of identity.
    std::array<TextureCacheKey,6> variants;variants.fill(base);
    variants[0].palette_addrs[0]=palettes.data();variants[1].palette_addrs[1]=palettes.data()+1;
    variants[2].fmt=G_IM_FMT_IA;variants[3].siz=G_IM_SIZ_8b;
    variants[4].palette_index=1;variants[5].size_bytes=32;
    bool keyDifferences=true;std::set<uint32_t> variantIds{baseId};
    for(const auto& key:variants){
        keyDifferences&=!probe.TextureCacheLookup(0,key)&&probe.mRenderingState.mTextures[0]->first==key;
        keyDifferences&=variantIds.insert(backend.bound[0]).second;
    }
    keyDifferences&=probe.mTextureCache.map.size()==variants.size()+1;
    const auto beforeMapHit=probe.mTextureCacheDiagnostics;
    bool mapHit=probe.TextureCacheLookup(0,base)&&backend.bound[0]==baseId&&
        probe.mTextureCacheDiagnostics.mapHits==beforeMapHit.mapHits+1&&
        probe.mTextureCacheDiagnostics.currentHits==beforeMapHit.currentHits;

    TextureCacheKey other=base;other.texture_addr=addresses.data()+1;
    probe.TextureCacheLookup(1,other);
    bool lruRefresh=probe.mTextureCache.lru.back().it->first==other;
    probe.TextureCacheLookup(0,base);
    lruRefresh&=probe.mTextureCache.lru.back().it->first==base;

    // Exercise ImportTexture's real framebuffer-mirror branch. It leaves the
    // current cache node intact, so a following fast hit MUST rebind the texture.
    probe.mRdp->texture_tile[0].tmem_index=0;
    probe.mRdp->texture_tile[0].fmt=base.fmt;probe.mRdp->texture_tile[0].siz=base.siz;
    probe.mRdp->loaded_texture[0].addr=base.texture_addr;
    probe.RegisterFbTexture(base.texture_addr,7);
    auto* beforeMirrorNode=probe.mRenderingState.mTextures[0];
    probe.ImportTexture(0,0,false);
    bool mirrorRebind=backend.mirrorSelections==1&&backend.bound[0]!=baseId&&
        probe.mRenderingState.mTextures[0]==beforeMirrorNode;
    probe.UnregisterFbTexture(base.texture_addr);
    const auto beforeRebind=backend.selections;
    const auto beforeMirrorHit=probe.mTextureCacheDiagnostics.currentHits;
    mirrorRebind&=probe.TextureCacheLookup(0,base)&&backend.bound[0]==baseId&&
        backend.selections==beforeRebind+1&&probe.mTextureCacheDiagnostics.currentHits==beforeMirrorHit+1;

    auto* otherNode=probe.mRenderingState.mTextures[1];
    probe.TextureCacheDelete(base.texture_addr);
    bool deleteReload=probe.mRenderingState.mTextures[0]==nullptr&&
        probe.mRenderingState.mTextures[1]==otherNode&&probe.mTextureCache.map.size()==1;
    for(const auto& entry:probe.mTextureCache.map)deleteReload&=entry.first.texture_addr!=base.texture_addr;
    deleteReload&=!probe.TextureCacheLookup(0,base)&&probe.mRenderingState.mTextures[0]->first==base;
    for(int slot=0;slot<SHADER_MAX_TEXTURES;++slot)probe.TextureCacheLookup(slot,base);
    probe.TextureCacheClear();
    bool clearReload=probe.mTextureCache.map.empty()&&probe.mTextureCache.lru.empty();
    for(auto* current:probe.mRenderingState.mTextures)clearReload&=current==nullptr;
    clearReload&=!probe.TextureCacheLookup(0,base)&&probe.mRenderingState.mTextures[0]->first==base;

    probe.TextureCacheClear();
    probe.TextureCacheLookup(0,base);
    for(size_t i=1;i<TEXTURE_CACHE_MAX_SIZE;++i){
        TextureCacheKey key=base;key.texture_addr=addresses.data()+i;
        probe.TextureCacheLookup(1,key);
    }
    bool eviction=probe.mTextureCache.map.size()==TEXTURE_CACHE_MAX_SIZE&&
        probe.mRenderingState.mTextures[0]&&probe.mRenderingState.mTextures[0]->first==base;
    TextureCacheKey newest=base;newest.texture_addr=addresses.data()+TEXTURE_CACHE_MAX_SIZE;
    eviction&=!probe.TextureCacheLookup(1,newest)&&probe.mRenderingState.mTextures[0]==nullptr&&
        !probe.mTextureCache.map.contains(base)&&probe.mTextureCache.map.size()==TEXTURE_CACHE_MAX_SIZE;
    eviction&=!probe.TextureCacheLookup(0,base)&&probe.mRenderingState.mTextures[0]->first==base;

    bool invariants=probe.mTextureCache.map.size()==probe.mTextureCache.lru.size();
    std::set<uint32_t> liveIds;
    for(const auto& entry:probe.mTextureCache.map){
        invariants&=liveIds.insert(entry.second.texture_id).second;
        invariants&=entry.second.lru_location!=probe.mTextureCache.lru.end()&&
            &*entry.second.lru_location->it==&entry;
    }
    const bool isolated=backend.uploads==0&&backend.draws==0;
    const char* session=std::getenv("MMVR_SESSION_TOKEN");const auto& counts=probe.mTextureCacheDiagnostics;
    std::ofstream("native-texture-cache.json")<<"{\"session\":\""<<(session?session:"")<<"\",\"repeats\":"<<repeats
        <<",\"sameKey\":"<<sameKey<<",\"keyDifferences\":"<<keyDifferences<<",\"mapHit\":"<<mapHit
        <<",\"lruRefresh\":"<<lruRefresh<<",\"mirrorRebind\":"<<mirrorRebind<<",\"deleteReload\":"<<deleteReload
        <<",\"clearReload\":"<<clearReload<<",\"eviction\":"<<eviction<<",\"invariants\":"<<invariants
        <<",\"isolated\":"<<isolated<<",\"currentHits\":"<<counts.currentHits<<",\"mapHits\":"<<counts.mapHits
        <<",\"misses\":"<<counts.misses<<"}";
    if(!sameKey||!keyDifferences||!mapHit||!lruRefresh||!mirrorRebind||!deleteReload||!clearReload||!eviction||!invariants||!isolated)
        throw std::runtime_error("Native texture-cache fast-path regression failed");
}
