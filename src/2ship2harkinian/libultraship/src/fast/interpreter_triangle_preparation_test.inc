// Included after the encoded Tri2/quad handlers, inside namespace Fast.
// CPU-only differential execution: each side owns its interpreter and recording
// renderer. The live game's GPU, resources, command stack and display list stay untouched.
namespace {
struct TriangleProbeEvent {
    std::string kind;
    std::vector<uint64_t> args;
    std::vector<uint8_t> bytes;
    bool operator==(const TriangleProbeEvent&) const = default;
};
struct TriangleProbeShader {
    uint64_t id0=0,id1=0;
    CCFeatures features{};
};
class TriangleProbeRenderer final : public GfxRenderingAPI {
  public:
    std::vector<TriangleProbeEvent> events;
    std::map<std::pair<uint64_t,uint64_t>,TriangleProbeShader> shaders;
    std::array<uint32_t,SHADER_MAX_TEXTURES> bound{};
    std::map<int,bool> framebufferInverted;
    uint64_t shaderQueries=0,clipQueries=0;
    uint32_t created=0;
    int currentTile=0,currentFramebuffer=0;
    bool readbackEnabled=false;
    uint16_t readbackColor=0;
    uint64_t readbackCalls=0;
    int lastReadbackFramebuffer=-1;
    uint32_t lastReadbackWidth=0,lastReadbackHeight=0;
    bool zeroToOne=false;
    static uint64_t FloatBits(float value){uint32_t bits;std::memcpy(&bits,&value,sizeof(bits));return bits;}
    void Event(const char* name,std::initializer_list<uint64_t> args={}){events.push_back({name,args,{}});}
    static TriangleProbeShader* Shader(ShaderProgram* p){return reinterpret_cast<TriangleProbeShader*>(p);}
    void ShaderEvent(const char* name,ShaderProgram* p){
        auto* value=Shader(p);Event(name,{uint64_t(value!=nullptr),value?value->id0:0,value?value->id1:0});
    }
    const char* GetName() override{return "triangle-preparation-probe";}
    int GetMaxTextureSize() override{return 64;}
    GfxClipParameters GetClipParameters() override{++clipQueries;return {zeroToOne,framebufferInverted[currentFramebuffer]};}
    void UnloadShader(ShaderProgram* p) override{ShaderEvent("unload",p);}
    void LoadShader(ShaderProgram* p) override{ShaderEvent("load",p);}
    void ClearShaderCache() override{Event("clear-shaders");shaders.clear();}
    ShaderProgram* CreateAndLoadNewShader(uint64_t id0,uint64_t id1) override{
        auto& value=shaders[{id0,id1}];value.id0=id0;value.id1=id1;
        gfx_cc_get_features(id0,id1,&value.features);
        Event("create-shader",{id0,id1});return reinterpret_cast<ShaderProgram*>(&value);
    }
    ShaderProgram* LookupShader(uint64_t id0,uint64_t id1) override{
        auto it=shaders.find({id0,id1});return it==shaders.end()?nullptr:reinterpret_cast<ShaderProgram*>(&it->second);
    }
    void ShaderGetInfo(ShaderProgram* p,uint8_t* inputs,bool textures[2]) override{
        ++shaderQueries;const auto& f=Shader(p)->features;*inputs=uint8_t(f.numInputs);
        textures[0]=f.usedTextures[0];textures[1]=f.usedTextures[1];
    }
    uint32_t NewTexture() override{Event("new-texture",{++created});return created;}
    bool TextureBindingMatches(int tile,uint32_t id) const override{return bound.at(tile)==id;}
    void SelectTexture(int tile,uint32_t texture) override{currentTile=tile;bound.at(tile)=texture;Event("select",{uint64_t(tile),texture});}
    void UploadTexture(const uint8_t* rgba,uint32_t width,uint32_t height) override{
        Event("upload",{uint64_t(currentTile),width,height});
        events.back().bytes.assign(rgba,rgba+size_t(width)*height*4);
    }
    void SetSamplerParameters(int tile,bool linear,uint32_t s,uint32_t t) override{Event("sampler",{uint64_t(tile),linear,s,t});}
    void SetDepthTestAndMask(bool test,bool mask) override{Event("depth",{test,mask});}
    void SetCurrentPrimDepth(float depth) override{Event("prim-depth",{FloatBits(depth)});}
    void SetZmodeDecal(bool decal) override{Event("decal",{decal});}
    void SetViewport(int x,int y,int w,int h) override{Event("viewport",{uint64_t(x),uint64_t(y),uint64_t(w),uint64_t(h)});}
    void SetScissor(int x,int y,int w,int h) override{Event("scissor",{uint64_t(x),uint64_t(y),uint64_t(w),uint64_t(h)});}
    void SetUseAlpha(bool alpha) override{Event("alpha",{alpha});}
    void DrawTriangles(float* data,size_t length,size_t triangles) override{
        Event("draw",{length,triangles});const auto* bytes=reinterpret_cast<const uint8_t*>(data);
        events.back().bytes.assign(bytes,bytes+length*sizeof(float));
    }
    void Init() override{Event("init");}
    void OnResize() override{Event("resize");}
    void StartFrame() override{Event("start-frame");}
    void EndFrame() override{Event("end-frame");}
    void FinishRender() override{Event("finish-render");}
    int CreateFramebuffer() override{Event("new-framebuffer");return int(framebufferInverted.size()+1);}
    void UpdateFramebufferParameters(int id,uint32_t w,uint32_t h,uint32_t msaa,bool inverted,bool target,bool depth,bool extract) override{
        framebufferInverted[id]=inverted;Event("framebuffer-parameters",{uint64_t(id),w,h,msaa,inverted,target,depth,extract});
    }
    void StartDrawToFramebuffer(int id,float scale) override{currentFramebuffer=id;Event("framebuffer",{uint64_t(id),FloatBits(scale)});}
    void CopyFramebuffer(int a,int b,int x0,int y0,int x1,int y1,int u0,int v0,int u1,int v1) override{
        Event("copy-framebuffer",{uint64_t(a),uint64_t(b),uint64_t(x0),uint64_t(y0),uint64_t(x1),uint64_t(y1),uint64_t(u0),uint64_t(v0),uint64_t(u1),uint64_t(v1)});
    }
    void ClearFramebuffer(bool color,bool depth) override{Event("clear-framebuffer",{color,depth});}
    void ReadFramebufferToCPU(int fb,uint32_t width,uint32_t height,uint16_t* pixels) override{
        if(!readbackEnabled){Event("unexpected-readback");return;}
        ++readbackCalls;lastReadbackFramebuffer=fb;lastReadbackWidth=width;lastReadbackHeight=height;
        Event("readback",{uint64_t(fb),width,height});
        if(pixels)std::fill_n(pixels,size_t(width)*height,readbackColor);
    }
    void ResolveMSAAColorBuffer(int a,int b) override{Event("resolve",{uint64_t(a),uint64_t(b)});}
    std::unordered_map<std::pair<float,float>,uint16_t,hash_pair_ff> GetPixelDepth(int,const std::set<std::pair<float,float>>&) override{return {};}
    void* GetFramebufferTextureId(int) override{return nullptr;}
    void SelectTextureFb(int id) override{bound[0]=0xf0000000u|uint32_t(id);Event("select-framebuffer",{uint64_t(id)});}
    void DeleteTexture(uint32_t id) override{Event("delete-texture",{id});}
    void SetTextureFilter(FilteringMode mode) override{Event("filter",{uint64_t(mode)});}
    FilteringMode GetTextureFilter() override{return FILTER_THREE_POINT;}
    void SetSrgbMode() override{Event("srgb");}
    ImTextureID GetTextureById(int) override{return (ImTextureID)0;}
    void SetLensMask(int mode) override{Event("lens",{uint64_t(mode)});}
};
// Compare each triangle's exact vertex bytes and effective draw state. A
// material-batched path intentionally changes draw boundaries and redundant
// setter count; those differences must not hide a state/attribute difference.
struct TriangleDrawSample {
    std::vector<TriangleProbeEvent> state;
    std::vector<uint8_t> bytes;
    bool operator==(const TriangleDrawSample&) const = default;
};
static std::vector<TriangleDrawSample> TriangleDrawSamples(const TriangleProbeRenderer& backend) {
    std::map<std::string,TriangleProbeEvent> state;
    auto bound=backend.bound;
    std::vector<TriangleDrawSample> result;
    for(const auto& event:backend.events) {
        if(event.kind=="draw") {
            const size_t triangles=event.args[1];
            if(!triangles || event.bytes.size()%triangles)throw std::runtime_error("Invalid fixture draw");
            const size_t bytes=event.bytes.size()/triangles;
            for(size_t i=0;i<triangles;++i) {
                TriangleDrawSample sample;
                for(const auto& [key,value]:state)sample.state.push_back(value);
                sample.bytes.assign(event.bytes.begin()+i*bytes,event.bytes.begin()+(i+1)*bytes);
                result.push_back(std::move(sample));
            }
        } else if(event.kind=="sampler") {
            const auto slot=event.args[0];
            state["sampler-slot-"+std::to_string(slot)]=event;
            auto object=event;object.args[0]=bound.at(slot);
            state["sampler-object-"+std::to_string(object.args[0])]=object;
        } else if(event.kind=="select") {
            bound.at(event.args[0])=uint32_t(event.args[1]);
            state["texture-"+std::to_string(event.args[0])]=event;
        } else if(event.kind=="load" || event.kind=="create-shader") {
            auto shader=event;
            if(event.kind=="create-shader")shader.args.insert(shader.args.begin(),1);
            shader.kind="shader";state["shader"]=std::move(shader);
        } else if(event.kind=="depth" || event.kind=="prim-depth" || event.kind=="decal" ||
                  event.kind=="viewport" || event.kind=="scissor" || event.kind=="alpha" ||
                  event.kind=="framebuffer" || event.kind=="lens" || event.kind=="filter") state[event.kind]=event;
    }
    return result;
}
static bool TrianglePayloadsEqual(const TriangleProbeRenderer& a,const TriangleProbeRenderer& b) {
    std::vector<uint8_t> left,right;
    std::vector<TriangleProbeEvent> uploadA,uploadB;
    for(const auto& e:a.events){if(e.kind=="draw")left.insert(left.end(),e.bytes.begin(),e.bytes.end());else if(e.kind=="upload")uploadA.push_back(e);}
    for(const auto& e:b.events){if(e.kind=="draw")right.insert(right.end(),e.bytes.begin(),e.bytes.end());else if(e.kind=="upload")uploadB.push_back(e);}
    return left==right && uploadA==uploadB;
}
enum TriangleProbeStyle : unsigned {
    ProbeTexture=1,ProbeDual=2,ProbeHd=4,ProbeFog=8,ProbeBlend=16,ProbeGray=32,
    ProbeAlpha=64,ProbeLod=128,ProbePrimDepth=256,ProbeDecal=512,ProbeMaskOptions=1024,ProbeCustomShader=2048
};
struct TriangleProbeCase {
    const char* name;
    int encoding=0;
    unsigned seed=0,style=0,reject=0,cull=0;
    bool negativeW=false,invertCull=false,alias=false,transition=false,rectangle=false,invalid=false,zeroToOne=false,shared=false;
};
struct TriangleProbeResult {
    const char* name;
    bool events=false,bytes=false,state=false,queries=false,expected=false;
    uint64_t accepted=0,prepared=0,reused=0,aliasFallbacks=0;
};
}

void Interpreter::WriteTrianglePreparationDiagnostics(const char* path) const {
    if(!mTrianglePreparationDiagnosticsEnabled||!path)return;
    const char* session=std::getenv("MMVR_SESSION_TOKEN");const auto& c=mTrianglePreparationDiagnostics;
    std::ofstream(path)<<"{\"session\":\""<<(session?session:"")<<"\",\"pairCommands\":"<<c.pairCommands
        <<",\"submitted\":"<<c.submitted<<",\"accepted\":"<<c.accepted<<",\"rejected\":"<<(c.submitted-c.accepted)
        <<",\"preparations\":"<<c.preparations<<",\"reuses\":"<<c.reuses<<",\"aliasFallbacks\":"<<c.aliasFallbacks<<"}";
}

void Interpreter::VerifyTrianglePreparation() {
    const char* diagnostic=std::getenv("MMVR_NATIVE_TEST");
    if(!diagnostic||std::strcmp(diagnostic,"1")!=0)return;
    // Encoded handlers consult the active microcode's cull constants. Restore
    // this one global even on a diagnostic failure; no live command stack used.
    struct RestoreUcode{UcodeHandlers saved=ucode_handler_index;~RestoreUcode(){ucode_handler_index=saved;}} restoreUcode;
    std::array<uint8_t,64> texA{},texB{};
    for(size_t i=0;i<texA.size();++i){texA[i]=uint8_t(i*17+3);texB[i]=uint8_t(i*11+9);}
    std::vector<TriangleProbeCase> cases{
        {"direct"},{"tri2_f3dex",1},{"tri2_f3dex2",2},{"quad_f3dex",3},{"quad_f3dex2",4},
        {"buffer_254",0,254},{"buffer_255",0,255},{"first_rejected",0,0,0,1},
        {"second_rejected",0,0,0,2},{"both_rejected",0,0,0,3},
        {"cull_front",0,0,0,0,1},{"cull_back",0,0,0,0,2},{"cull_both",0,0,0,0,3},
        {"negative_w",0,0,0,0,2,true},{"inverted_culling",0,0,0,0,2,false,true},
        {"rgba_texture",2,0,ProbeTexture},{"dual_textures",2,0,ProbeTexture|ProbeDual},
        {"hd_shift_mirror_clamp",2,0,ProbeTexture|ProbeDual|ProbeHd},
        {"fog_grayscale_alpha",2,0,ProbeTexture|ProbeDual|ProbeFog|ProbeGray|ProbeAlpha},
        {"blend_color_fog",2,0,ProbeTexture|ProbeBlend|ProbeAlpha},
        {"per_triangle_lod",2,0,ProbeLod|ProbeAlpha},
        {"primitive_depth_decal",2,0,ProbePrimDepth|ProbeDecal},
        {"shared_texture_sampler_conflict",2,0,ProbeTexture|ProbeDual,0,0,false,false,true},
        {"custom_sampler_fallback",2,0,ProbeTexture|ProbeDual|ProbeCustomShader,0,0,false,false,true},
        {"shader_mask_options",2,0,ProbeTexture|ProbeMaskOptions},
        {"context_and_layout_changes",2,0,ProbeTexture,0,0,false,false,false,true},
        {"rectangle_singles",0,0,ProbeTexture,0,0,false,false,false,false,true},
        {"invalid_first_index",0,0,0,0,0,false,false,false,false,false,true},
        {"dx_clip_depth",2,0,ProbeTexture|ProbeFog,0,0,false,false,false,false,false,false,true},
    };
    // Shared vertices must be tested under the expensive layouts and fallback
    // conditions too, not just under an untextured quad.
    for(auto spec:std::vector<TriangleProbeCase>{{"shared_textured",2,0,ProbeTexture|ProbeDual|ProbeHd},
         {"shared_fog_alpha",2,0,ProbeTexture|ProbeFog|ProbeGray|ProbeAlpha},
         {"shared_lod_unequal_w",2,0,ProbeLod|ProbeAlpha},
         {"shared_flush",2,255,ProbeTexture|ProbeDual},
         {"shared_alias",2,0,ProbeTexture|ProbeDual,0,0,false,false,true}}){spec.shared=true;cases.push_back(spec);}
    std::vector<TriangleProbeResult> results;
    uint64_t preparations=0,reuses=0,aliasFallbacks=0;bool isolated=true;
    bool triangleRunsEqual=true; uint64_t runPreparations=0,runReferencePreparations=0;
    bool structuralRunsEqual=true;
    uint64_t structuralReferencePreparations=0,structuralCandidatePreparations=0;
    double runReferenceMs=0,runCandidateMs=0;
    for(const auto& spec:cases){
        ucode_handler_index=(spec.encoding==1||spec.encoding==3)?ucode_f3dex:ucode_f3dex2;
        TriangleProbeRenderer referenceBackend,fastBackend;
        Interpreter reference,fast;
        std::array<uint8_t,4096> referenceUpload{},fastUpload{};
        reference.mRapi=&referenceBackend;fast.mRapi=&fastBackend;
        reference.mTexUploadBuffer=referenceUpload.data();fast.mTexUploadBuffer=fastUpload.data();
        auto setup=[&](Interpreter& owner,TriangleProbeRenderer& backend){
            backend.zeroToOne=spec.zeroToOne;
            owner.mTextureCache.map.reserve(TEXTURE_CACHE_MAX_SIZE);
            owner.mNativeDimensions={0,0,320,240};owner.mCurDimensions.width=320;owner.mCurDimensions.height=240;
            owner.mRdp->viewport={3,233,310,230};owner.mRdp->scissor={5,225,300,215};
            owner.mRdp->viewport_or_scissor_changed=true;
            owner.mRsp->geometry_mode=G_ZBUFFER;
            if(spec.cull==1)owner.mRsp->geometry_mode|=get_attr(CULL_FRONT);
            if(spec.cull==2)owner.mRsp->geometry_mode|=get_attr(CULL_BACK);
            if(spec.cull==3)owner.mRsp->geometry_mode|=get_attr(CULL_BOTH);
            if(spec.invertCull)owner.mRsp->extra_geometry_mode|=G_EX_INVERT_CULLING;
            owner.mRdp->other_mode_l=Z_CMP|Z_UPD;
            owner.mRdp->other_mode_h=G_TF_BILERP;
            if(spec.style&ProbeAlpha)owner.mRdp->other_mode_l|=(G_BL_CLR_MEM<<20)|(G_BL_1MA<<16);
            if(spec.style&ProbeFog)owner.mRdp->other_mode_l|=uint32_t(G_BL_CLR_FOG)<<30;
            if(spec.style&ProbeBlend)owner.mRdp->other_mode_l|=uint32_t(G_BL_CLR_BL)<<30;
            if(spec.style&ProbeLod)owner.mRdp->other_mode_l|=G_TL_LOD;
            if(spec.style&ProbePrimDepth)owner.mRdp->other_mode_l|=G_ZS_PRIM;
            if(spec.style&ProbeDecal)owner.mRdp->other_mode_l|=ZMODE_DEC;
            owner.mRdp->prim_depth=0x3abc;owner.mRdp->prim_lod_fraction=97;
            owner.mRdp->prim_color={201,57,99,173};owner.mRdp->env_color={15,92,223,114};
            owner.mRdp->fog_color={77,88,99,183};owner.mRdp->blend_color={19,201,71,91};
            owner.mRdp->grayscale=bool(spec.style&ProbeGray);owner.mRdp->grayscale_color={221,41,83,139};
            owner.mRdp->key_center={7,91,23,255};owner.mRdp->key_scale={111,32,53,255};
            owner.mRdp->convert_k[4]=27;owner.mRdp->convert_k[5]=61;
            const uint32_t rgb=(spec.style&ProbeLod)?color_comb(G_CCMUX_PRIMITIVE,8,G_CCMUX_LOD_FRACTION,7):
                (spec.style&ProbeTexture)?color_comb(G_CCMUX_TEXEL0,8,G_CCMUX_SHADE,7):color_comb(0,0,0,G_CCMUX_SHADE);
            const uint32_t alpha=(spec.style&ProbeLod)?alpha_comb(G_ACMUX_SHADE,G_ACMUX_0,G_ACMUX_LOD_FRACTION,G_ACMUX_0):
                (spec.style&ProbeTexture)?alpha_comb(G_ACMUX_TEXEL0,G_ACMUX_0,G_ACMUX_SHADE,G_ACMUX_0):alpha_comb(0,0,0,G_ACMUX_SHADE);
            // Cycle two must consume COMBINED or GenerateCC legitimately
            // removes cycle one; also sample its TEXEL0 (the second texture).
            owner.GfxDpSetCombineMode(rgb,alpha,
                color_comb(G_CCMUX_COMBINED,8,G_CCMUX_TEXEL0,7),
                alpha_comb(G_ACMUX_COMBINED,G_ACMUX_0,G_ACMUX_TEXEL0,G_ACMUX_0));
            if(spec.style&ProbeDual)owner.mRdp->other_mode_h|=G_CYC_2CYCLE;
            for(unsigned i=0;i<2;++i){
                auto& tile=owner.mRdp->texture_tile[i];tile.tmem_index=uint8_t(i);
                tile.fmt=G_IM_FMT_RGBA;tile.siz=G_IM_SIZ_16b;tile.line_size_bytes=8;
                tile.uls=0;tile.ult=0;tile.lrs=12;tile.lrt=12;
                auto& loaded=owner.mRdp->loaded_texture[i];
                loaded.addr=i&&!spec.alias?texB.data():texA.data();
                loaded.line_size_bytes=loaded.full_image_line_size_bytes=8;
                loaded.size_bytes=loaded.orig_size_bytes=32;
                owner.mRdp->textures_changed[i]=true;
                if(spec.style&ProbeHd){
                    loaded.raw_tex_metadata.h_byte_scale=2;loaded.raw_tex_metadata.v_pixel_scale=2;
                    tile.cms=G_TX_MIRROR|G_TX_CLAMP;tile.cmt=G_TX_MIRROR|G_TX_CLAMP;
                    tile.lrs=8;tile.lrt=8;tile.shifts=1;tile.shiftt=12;
                }
            }
            // A custom shader with identical sampler keys must share one cache node;
            // differing sampler keys intentionally create distinct textures.
            if(spec.alias && !(spec.style&ProbeCustomShader))owner.mRdp->texture_tile[1].cms=G_TX_MIRROR;
            if(spec.style&ProbeCustomShader)owner.mShaderStack.push(17);
            // Tests shader-option/attribute preparation without loading a pack.
            if(spec.style&ProbeMaskOptions)owner.mRdp->loaded_texture[0].masked=true;
            for(unsigned i=0;i<6;++i){
                const unsigned corner=i%3;const float w=(i<3)?4000.f:5500.f;
                auto& v=owner.mRsp->loaded_vertices[i];
                v.x=(corner==1?.55f:-.45f)*w;v.y=(corner==2?.6f:-.35f)*w;
                v.z=.2f*w;v.w=w;v.u=float(23+i*47);v.v=float(31+i*29);
                v.color={uint8_t(17+i*19),uint8_t(231-i*23),uint8_t(37+i*7),uint8_t(49+i*31)};
                v.clip_rej=((i<3&&(spec.reject&1))||(i>=3&&(spec.reject&2)))?1:0;
            }
            if(spec.shared)owner.mRsp->loaded_vertices[2].w=5100.f;
            if(spec.negativeW)owner.mRsp->loaded_vertices[0].w*=-1.f;
            for(unsigned i=0;i<spec.seed;++i)owner.GfxSpTri1(0,1,2,false);
            backend.events.clear();backend.shaderQueries=backend.clipQueries=0;
            owner.mTrianglePreparationDiagnostics={};
        };
        if ((spec.style & ProbeTexture) && !spec.seed && !spec.transition && !spec.rectangle && !spec.shared) {
            // Exercise real repeated image-load requests, including changes that
            // MUST split draws. Check effective state for every emitted triangle.
            for (int scenario=0; scenario<6; ++scenario) {
                Interpreter oldPath,newPath;TriangleProbeRenderer oldGpu,newGpu;
                std::array<uint8_t,4096> oldUpload{},newUpload{};
                oldPath.mRapi=&oldGpu;newPath.mRapi=&newGpu;
                oldPath.mTexUploadBuffer=oldUpload.data();newPath.mTexUploadBuffer=newUpload.data();
                setup(oldPath,oldGpu);setup(newPath,newGpu);
                oldPath.mTextureBatchEnabled=false;newPath.mTextureBatchEnabled=true;
                auto run=[&](Interpreter& owner,TriangleProbeRenderer& gpu) {
                    for(int repeat=0;repeat<64;++repeat) {
                        if(repeat==24) {
                            if(scenario==1)owner.mRdp->loaded_texture[0].addr=texB.data();
                            if(scenario==2)owner.mRdp->texture_tile[0].cms=G_TX_MIRROR;
                            if(scenario==3){owner.Flush();gpu.SelectTextureFb(7);}
                            if(scenario==4){owner.Flush();owner.TextureCacheDelete(texA.data());}
                            if(scenario==5)owner.mShaderStack.push(17);
                        }
                        owner.mRdp->textures_changed[0]=owner.mRdp->textures_changed[1]=true;
                        owner.GfxSpTri1(0,1,2,false);
                    }
                    owner.Flush();
                };
                run(oldPath,oldGpu);run(newPath,newGpu);
                const bool equal=TriangleDrawSamples(oldGpu)==TriangleDrawSamples(newGpu) && TrianglePayloadsEqual(oldGpu,newGpu);
                const auto count=[](const TriangleProbeRenderer& gpu){return std::count_if(gpu.events.begin(),gpu.events.end(),[](const auto& e){return e.kind=="draw";});};
                const auto before=count(oldGpu),after=count(newGpu);
                static std::ofstream boundaryReport("native-texture-boundaries.jsonl");
                boundaryReport << "{\"case\":\""<<spec.name<<"\",\"scenario\":"<<scenario
                    <<",\"referenceDraws\":"<<before<<",\"candidateDraws\":"<<after<<",\"equal\":"<<equal<<"}\n"<<std::flush;
                if(!equal || after>before || (!spec.alias && !(spec.style&ProbeCustomShader) && scenario==0 && after>=before))
                    throw std::runtime_error("Texture boundary differential failed: "+std::string(spec.name)+" scenario "+std::to_string(scenario));
            }
        }
        setup(reference,referenceBackend);setup(fast,fastBackend);
        if (std::string_view(spec.name)=="shared_textured" || std::string_view(spec.name)=="shared_fog_alpha" ||
            std::string_view(spec.name)=="shared_lod_unequal_w" || std::string_view(spec.name)=="shared_alias") {
            TriangleProbeRenderer plainGpu,packedGpu;
            Interpreter plain,packed;
            std::array<uint8_t,4096> plainUpload{},packedUpload{};
            plain.mRapi=&plainGpu;packed.mRapi=&packedGpu;
            plain.mTexUploadBuffer=plainUpload.data();packed.mTexUploadBuffer=packedUpload.data();
            setup(plain,plainGpu);setup(packed,packedGpu);
            const bool projectionCase = std::string_view(spec.name) == "shared_textured";
            for (auto* owner:{&plain,&packed}) {
                owner->mTextureCacheDiagnosticsEnabled=true;
                owner->mTriangleTestPaired=true;
                owner->mTriangleRunActive=true;
                if (projectionCase) owner->mRsp->geometry_mode |= get_attr(CULL_BACK);
                for (unsigned i=0;i<6;++i) {
                    auto& right=owner->mStereoRsp.loaded_vertices[i];
                    right=owner->mRsp->loaded_vertices[i];
                    right.x+=right.w*.13f;right.y-=right.w*.07f;
                    right.u+=19;right.v-=11;right.color.a^=0x55;
                }
                for (unsigned i=0;i<3;++i) owner->mStereoRsp.loaded_vertices[i].clip_rej=1;
            }
            plain.mTriangleVertexPackingEnabled=false;
            plain.mTriangleProjectionEnabled=false;
            auto emitBoth=[&] {plain.GfxSpTri1(0,1,2,false);packed.GfxSpTri1(0,1,2,false);};
            // The first triangle rejects the right eye. Its collapsed payload
            // must not seed the later accepted-eye cache.
            emitBoth();
            const bool rightUncached=packed.mTriangleVertices[0].generation[1]!=packed.mTriangleVertexGeneration;
            const bool rightProjectionUncached=!projectionCase ||
                packed.mTriangleVertices[0].projectionGeneration[1]!=packed.mTriangleVertexGeneration;
            for (auto* owner:{&plain,&packed})
                for (unsigned i=0;i<3;++i)owner->mStereoRsp.loaded_vertices[i].clip_rej=0;
            emitBoth();emitBoth();
            const bool rightProjectionCached=!projectionCase ||
                (packed.mTriangleVertices[0].projectionGeneration[1]==packed.mTriangleVertexGeneration &&
                 packed.mTriangleVertices[0].screenX[1]==
                     packed.mStereoRsp.loaded_vertices[0].x/packed.mStereoRsp.loaded_vertices[0].w);
            for (auto* owner:{&plain,&packed})
                for (unsigned i=0;i<3;++i)owner->mRsp->loaded_vertices[i].clip_rej=1;
            emitBoth();
            for (auto* owner:{&plain,&packed})
                for (unsigned i=0;i<3;++i)owner->mRsp->loaded_vertices[i].clip_rej=0;
            emitBoth();
            // Vertex edit preserves material preparation but invalidates both
            // eye records before subsequent triangles.
            for (auto* owner:{&plain,&packed}) {
                owner->GfxSpModifyVertex(0,G_MWO_POINT_ST,0x01900320);
                owner->mStereoRsp.loaded_vertices[0].x += owner->mStereoRsp.loaded_vertices[0].w*.05f;
                owner->mStereoRsp.loaded_vertices[0].y -= owner->mStereoRsp.loaded_vertices[0].w*.04f;
                owner->BeginTriangleCommand(false,true);
                owner->BeginTriangleCommand(true);
            }
            const bool rightProjectionInvalidated=!projectionCase ||
                packed.mTriangleVertices[0].projectionGeneration[1]!=packed.mTriangleVertexGeneration;
            emitBoth();plain.Flush();packed.Flush();
            const bool equal=rightUncached && rightProjectionUncached && rightProjectionCached &&
                rightProjectionInvalidated && TrianglePayloadsEqual(plainGpu,packedGpu) &&
                TriangleDrawSamples(plainGpu)==TriangleDrawSamples(packedGpu);
            static std::ofstream pairedReport("native-paired-vertex-packing.jsonl");
            pairedReport << "{\"case\":\"" << spec.name << "\",\"equal\":" << equal
                         << ",\"rightRejectUncached\":" << rightUncached
                         << ",\"rightProjectionUncached\":" << rightProjectionUncached
                         << ",\"rightProjectionCached\":" << rightProjectionCached
                         << ",\"rightProjectionInvalidated\":" << rightProjectionInvalidated
                         << "}\n" << std::flush;
            if (!equal) throw std::runtime_error("Paired vertex packing differential failed: "+std::string(spec.name));
            // Relative CPU-only measurement; never interpreted as headset FPS.
            auto measure=[&](Interpreter& owner,TriangleProbeRenderer& gpu) {
                const auto start=std::chrono::steady_clock::now();
                for (int repeat=0;repeat<500;++repeat) {
                    owner.GfxSpTri1(0,1,2,false);owner.Flush();gpu.events.clear();
                }
                return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            };
            measure(plain,plainGpu);measure(packed,packedGpu);
            double plainMs=0,packedMs=0;
            for (int round=0;round<4;++round) {
                if(round&1){packedMs+=measure(packed,packedGpu);plainMs+=measure(plain,plainGpu);}
                else {plainMs+=measure(plain,plainGpu);packedMs+=measure(packed,packedGpu);}
            }
            pairedReport << "{\"case\":\"" << spec.name << "_cpu\",\"plainMs\":" << plainMs/4
                         << ",\"packedMs\":" << packedMs/4 << "}\n" << std::flush;
        }
        reference.mMaterialBatchEnabled=false;
        reference.mTextureBatchEnabled=false;fast.mTextureBatchEnabled=false;
        const std::array<uint8_t,6> indices=spec.shared?std::array<uint8_t,6>{0,1,2,2,3,0}:spec.encoding==3?std::array<uint8_t,6>{0,1,2,0,2,3}:
            std::array<uint8_t,6>{uint8_t(spec.invalid?255:0),1,2,3,4,5};
        auto emit=[&](Interpreter& owner,bool optimized){
            if(spec.rectangle){owner.GfxDrawRectangle(0,0,80,60);return;}
            if(!optimized){
                owner.GfxSpTri1(indices[0],indices[1],indices[2],false);
                owner.GfxSpTri1(indices[3],indices[4],indices[5],false);return;
            }
            if(!spec.encoding){owner.GfxSpTri2(indices[0],indices[1],indices[2],indices[3],indices[4],indices[5]);return;}
            F3DGfx command{};F3DGfx* cursor=&command;
            CommandInterpreterScope scope(&owner);
            if(spec.encoding==1||spec.encoding==2){
                const auto opcode=spec.encoding==1?F3DEX_G_TRI2:F3DEX2_G_TRI2;
                command.words.w0=(uint32_t(uint8_t(opcode))<<24)|(uint32_t(indices[0])<<17)|(uint32_t(indices[1])<<9)|(uint32_t(indices[2])<<1);
                command.words.w1=(uint32_t(indices[3])<<17)|(uint32_t(indices[4])<<9)|(uint32_t(indices[5])<<1);
                gfx_tri2_handler_f3dex(&cursor);
            }else if(spec.encoding==3){
                command.words.w0=uint32_t(uint8_t(F3DEX_G_QUAD))<<24;
                command.words.w1=(uint32_t(3*2)<<24)|(uint32_t(0*2)<<16)|(uint32_t(1*2)<<8)|uint32_t(2*2);
                gfx_quad_handler_f3dex(&cursor);
            }else{
                command.words.w0=(uint32_t(uint8_t(F3DEX2_G_QUAD))<<24)|(uint32_t(0*2)<<16)|(uint32_t(1*2)<<8)|uint32_t(2*2);
                command.words.w1=(uint32_t(3*2)<<16)|(uint32_t(4*2)<<8)|uint32_t(5*2);
                gfx_quad_handler_f3dex2(&cursor);
            }
        };
        emit(reference,false);emit(fast,true);
        if(spec.transition){
            auto change=[&](Interpreter& owner,TriangleProbeRenderer& backend){
                owner.Flush();owner.GfxSpExtraGeometryMode(0,G_EX_LENS_REVEAL);
                backend.UpdateFramebufferParameters(1,160,120,1,true,true,true,true);
                owner.SetFrameBuffer(1,1.f);
                owner.mRdp->viewport={1,117,150,110};owner.mRdp->scissor={2,115,145,100};
                owner.mRdp->viewport_or_scissor_changed=true;
                owner.mRdp->grayscale=true;owner.mRdp->grayscale_color={44,91,122,149};
                owner.mRdp->other_mode_l|=(uint32_t(G_BL_CLR_FOG)<<30)|(G_BL_CLR_MEM<<20)|(G_BL_1MA<<16);
                owner.mRdp->other_mode_h|=G_CYC_2CYCLE;
                owner.mRdp->texture_tile[0].shifts=2;owner.mRdp->texture_tile[1].shiftt=1;
                owner.mRdp->textures_changed[0]=owner.mRdp->textures_changed[1]=true;
                owner.mShaderStack.push(17);
            };
            change(reference,referenceBackend);change(fast,fastBackend);
            emit(reference,false);emit(fast,true);
            reference.GfxSpExtraGeometryMode(G_EX_LENS_REVEAL,G_EX_LENS_HIDE);
            fast.GfxSpExtraGeometryMode(G_EX_LENS_REVEAL,G_EX_LENS_HIDE);
            reference.ResetFrameBuffer();fast.ResetFrameBuffer();
            emit(reference,false);emit(fast,true);
            reference.GfxSpExtraGeometryMode(G_EX_LENS_HIDE,0);fast.GfxSpExtraGeometryMode(G_EX_LENS_HIDE,0);
        }
        reference.Flush();fast.Flush();
        const auto& a=reference.mTrianglePreparationDiagnostics;const auto& b=fast.mTrianglePreparationDiagnostics;
        TriangleProbeResult result{spec.name};
        result.events=spec.alias ? TriangleDrawSamples(referenceBackend)==TriangleDrawSamples(fastBackend) : referenceBackend.events==fastBackend.events;
        // Keep byte comparison explicit in the report, including draw split and upload payloads.
        std::vector<TriangleProbeEvent> referenceData,fastData;
        for(const auto& e:referenceBackend.events)if(e.kind=="draw"||e.kind=="upload")referenceData.push_back(e);
        for(const auto& e:fastBackend.events)if(e.kind=="draw"||e.kind=="upload")fastData.push_back(e);
        result.bytes=spec.alias ? TrianglePayloadsEqual(referenceBackend,fastBackend) : referenceData==fastData;
        result.state=a.submitted==b.submitted&&a.accepted==b.accepted&&reference.mBufVboLen==fast.mBufVboLen&&
            reference.mBufVboNumTris==fast.mBufVboNumTris&&reference.mRdp->viewport_or_scissor_changed==fast.mRdp->viewport_or_scissor_changed&&
            reference.mRdp->textures_changed[0]==fast.mRdp->textures_changed[0]&&reference.mRdp->textures_changed[1]==fast.mRdp->textures_changed[1]&&
            reference.mRenderingState.depth_test_and_mask==fast.mRenderingState.depth_test_and_mask&&
            reference.mRenderingState.decal_mode==fast.mRenderingState.decal_mode&&reference.mRenderingState.alpha_blend==fast.mRenderingState.alpha_blend&&
            reference.mColorCombinerPool.size()==fast.mColorCombinerPool.size()&&reference.mTextureCache.map.size()==fast.mTextureCache.map.size()&&
            referenceBackend.bound==fastBackend.bound&&referenceBackend.currentFramebuffer==fastBackend.currentFramebuffer;
        for(unsigned i=0;i<SHADER_MAX_TEXTURES;++i){
            auto* left=reference.mRenderingState.mTextures[i];auto* right=fast.mRenderingState.mTextures[i];
            result.state&=bool(left)==bool(right);
            if(left&&right)result.state&=left->first==right->first&&left->second.texture_id==right->second.texture_id&&
                left->second.linear_filter==right->second.linear_filter&&left->second.cms==right->second.cms&&left->second.cmt==right->second.cmt;
        }
        // Rectangle viewport adjustment performs one additional legitimate
        // clip query outside triangle preparation on both paths.
        const uint64_t extraClipQueries=spec.rectangle?1:0;
        result.queries=referenceBackend.shaderQueries==a.preparations&&referenceBackend.clipQueries==a.preparations+extraClipQueries&&
            fastBackend.shaderQueries==b.preparations&&fastBackend.clipQueries==b.preparations+extraClipQueries&&
            a.preparations==a.accepted&&b.preparations+b.reuses==b.accepted;
        const uint64_t commands=spec.transition?3:1;
        result.expected=spec.rectangle?(b.pairCommands==0&&b.reuses==0&&b.preparations==2):b.pairCommands==commands;
        if(spec.reject||spec.invalid)result.expected&=b.accepted==uint64_t(spec.reject==3?0:1)&&b.preparations==b.accepted&&b.reuses==0;
        if(spec.alias)result.expected&=(spec.style&ProbeCustomShader) ? (b.aliasFallbacks==1&&b.preparations==2&&b.reuses==0) : (b.aliasFallbacks==0&&b.preparations==1&&b.reuses==1);
        else if(!spec.cull&&!spec.reject&&!spec.invalid&&!spec.rectangle)result.expected&=b.preparations==commands&&b.reuses==commands;
        if(spec.seed){
            std::vector<uint64_t> splits;for(const auto& event:fastBackend.events)if(event.kind=="draw")splits.push_back(event.args[1]);
            result.expected&=spec.seed==254?splits==std::vector<uint64_t>{256}:splits==std::vector<uint64_t>{256,1};
        }
        if(spec.reject==3)result.expected&=fastBackend.events.empty()&&fastBackend.shaderQueries==0;
        if(spec.cull==1||spec.cull==3||(spec.cull==2&&spec.invertCull))
            result.expected&=b.accepted==0&&b.preparations==0&&fastBackend.events.empty();
        if (std::string_view(spec.name)=="custom_sampler_fallback") {
            const auto& textures=fast.mRenderingState.mTextures;
            const bool bothBound=textures[0]!=nullptr && textures[1]!=nullptr;
            const bool sameTexture=bothBound && textures[0]==textures[1];
            static std::ofstream customReport("native-custom-alias-state.json");
            const auto* comb=fast.mPrevCombiner==fast.mColorCombinerPool.end() ? nullptr : &fast.mPrevCombiner->second;
            customReport << "{\"used0\":" << (comb ? comb->usedTextures[0] : 0)
                         << ",\"used1\":" << (comb ? comb->usedTextures[1] : 0)
                         << ",\"customShader\":" << !fast.mShaderStack.empty()
                         << ",\"bothBound\":" << bothBound << ",\"sameTexture\":" << sameTexture
                         << ",\"bound0\":" << fastBackend.bound[0] << ",\"bound1\":" << fastBackend.bound[1]
                         << ",\"prepared\":" << b.preparations << ",\"reused\":" << b.reuses
                         << ",\"aliasFallbacks\":" << b.aliasFallbacks << "}\n" << std::flush;
        }
        result.accepted=b.accepted;result.prepared=b.preparations;result.reused=b.reuses;result.aliasFallbacks=b.aliasFallbacks;
        preparations+=b.preparations;reuses+=b.reuses;aliasFallbacks+=b.aliasFallbacks;
        isolated&=reference.mRapi==&referenceBackend&&fast.mRapi==&fastBackend;
        results.push_back(result);
        if (!spec.rectangle && !spec.transition) {
            referenceBackend.events.clear();fastBackend.events.clear();
            reference.mTrianglePreparationDiagnostics={};fast.mTrianglePreparationDiagnostics={};
            reference.mTriangleRunEnabled=false;
            ucode_handler_index=ucode_f3dex2;
            std::array<F3DGfx,49> sequence{};
            for (size_t i=0;i<sequence.size();++i) {
                auto& c=sequence[i];
                c.words.w0=(uint32_t(uint8_t(F3DEX2_G_TRI2))<<24)|(uint32_t(indices[0])<<17)|(uint32_t(indices[1])<<9)|(uint32_t(indices[2])<<1);
                c.words.w1=(uint32_t(indices[3])<<17)|(uint32_t(indices[4])<<9)|(uint32_t(indices[5])<<1);
            }
            sequence[16].words.w0=uint32_t(uint8_t(RDP_G_SETENVCOLOR))<<24;
            sequence[16].words.w1=0x238abd79;
            sequence[32].words.w0=uint32_t(uint8_t(F3DEX2_G_LOAD_UCODE))<<24|uint32_t(ucode_f3dex2);
            sequence[32].words.w1=0;
            reference.TestTriangleRunCommands(sequence.data(),sequence.size());
            fast.TestTriangleRunCommands(sequence.data(),sequence.size());
            reference.Flush();fast.Flush();
            const bool runPayloadsEqual=spec.alias ? TriangleDrawSamples(referenceBackend)==TriangleDrawSamples(fastBackend) :
                referenceBackend.events==fastBackend.events;
            const bool runCountsEqual=reference.mTrianglePreparationDiagnostics.accepted==fast.mTrianglePreparationDiagnostics.accepted;
            const bool runInactive=!reference.mTriangleRunActive && !fast.mTriangleRunActive;
            static std::ofstream runReport("native-triangle-runs.jsonl");
            auto traceRun=[&](const char* stage,bool equal) {
                runReport << "{\"case\":\"" << spec.name << "\",\"stage\":\"" << stage
                          << "\",\"equal\":" << equal << "}\n" << std::flush;
            };
            traceRun("payloads",runPayloadsEqual);
            traceRun("counts",runCountsEqual);
            traceRun("inactive",runInactive);
            triangleRunsEqual &= runPayloadsEqual && runCountsEqual && runInactive;
            if(spec.alias && !(spec.style&ProbeCustomShader)) {
                const auto drawCount=[](const TriangleProbeRenderer& backend){return std::count_if(backend.events.begin(),backend.events.end(),[](const auto& e){return e.kind=="draw";});};
                const auto baselineDraws=drawCount(referenceBackend),batchedDraws=drawCount(fastBackend);
                // Differing sampler keys now own distinct cache textures, so
                // the old per-triangle alias toggle no longer exists to remove.
                const auto& boundTextures=fast.mRenderingState.mTextures;
                const bool sharedNode=boundTextures[0] && boundTextures[0]==boundTextures[1];
                const bool batchingValid=batchedDraws<=baselineDraws && (!sharedNode || batchedDraws<baselineDraws);
                traceRun("material-batching",batchingValid);
                triangleRunsEqual &= batchingValid;
                std::ofstream("native-material-batching.json")<<"{\"referenceDraws\":"<<baselineDraws
                    <<",\"candidateDraws\":"<<batchedDraws<<",\"triangleBytesAndDrawStateEqual\":"
                    <<(TriangleDrawSamples(referenceBackend)==TriangleDrawSamples(fastBackend)?"true":"false")<<"}";
            }
            // A nested list, marker and no-op must retain exactly the same
            // geometry, texture/sampler state and draw ordering. Keep both
            // triangle caches enabled so this isolates structural preservation.
            {
                auto structuralSequence=sequence;
                std::array<F3DGfx,4> child{};
                child[0]=child[2]=sequence[0];
                child[1].words.w0=uint32_t(uint8_t(F3DEX2_G_NOOP))<<24;
                child[3].words.w0=uint32_t(uint8_t(F3DEX2_G_ENDDL))<<24;
                structuralSequence[4].words.w0=uint32_t(uint8_t(F3DEX2_G_NOOP))<<24;
                structuralSequence[4].words.w1=0;
                structuralSequence[8].words.w0=uint32_t(uint8_t(OTR_G_MARKER))<<24;
                structuralSequence[8].words.w1=0;
                structuralSequence[9]={}; // marker's metadata word, not an opcode
                structuralSequence[12].words.w0=uint32_t(uint8_t(F3DEX2_G_DL))<<24;
                structuralSequence[12].words.w1=reinterpret_cast<uintptr_t>(child.data());
                const std::array<uint8_t,4> syncOpcodes{uint8_t(RDP_G_RDPLOADSYNC),uint8_t(RDP_G_RDPPIPESYNC),
                    uint8_t(RDP_G_RDPTILESYNC),uint8_t(RDP_G_RDPFULLSYNC)};
                for (size_t sync=0;sync<syncOpcodes.size();++sync) {
                    // Separate the no-ops with triangles to exercise each
                    // formerly unnecessary material/projection invalidation.
                    auto& command=structuralSequence[20+sync*2];
                    command.words.w0=uint32_t(syncOpcodes[sync])<<24;
                    command.words.w1=0;
                }
                reference.mTriangleRunEnabled=true;
                reference.mStructuralPreparationEnabled=false;
                fast.mStructuralPreparationEnabled=true;
                reference.mTrianglePreparationDiagnostics={};fast.mTrianglePreparationDiagnostics={};
                referenceBackend.events.clear();fastBackend.events.clear();
                // The marker consumes two words in one step; the child adds
                // four commands, including its return, to the parent traversal.
                const auto steps=structuralSequence.size()-1+child.size();
                reference.TestTriangleRunCommands(structuralSequence.data(),steps);
                fast.TestTriangleRunCommands(structuralSequence.data(),steps);
                reference.Flush();fast.Flush();
                const auto before=reference.mTrianglePreparationDiagnostics.preparations;
                const auto after=fast.mTrianglePreparationDiagnostics.preparations;
                const bool equal=TriangleDrawSamples(referenceBackend)==TriangleDrawSamples(fastBackend) &&
                    reference.mTrianglePreparationDiagnostics.accepted==fast.mTrianglePreparationDiagnostics.accepted &&
                    !reference.mTriangleRunActive && !fast.mTriangleRunActive && after<=before;
                traceRun("structural-commands",equal);
                structuralRunsEqual &= equal;
                structuralReferencePreparations+=before;structuralCandidatePreparations+=after;
                reference.mTriangleRunEnabled=false;
                reference.mStructuralPreparationEnabled=true;
            }
            {
                // Exercise every newly classified material dependency through
                // real opcodes, including a viewport change that MUST invalidate
                // and a later VTX which MUST consume the updated RSP inputs.
                auto attributeSequence=sequence;
                alignas(8) std::array<int32_t,16> identity{};
#ifdef GBI_FLOATS
                const float identityFloats[4][4]{{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
                std::memcpy(identity.data(),identityFloats,sizeof(identityFloats));
#else
                identity[0]=0x10000;identity[2]=1;identity[5]=0x10000;identity[7]=1;
#endif
                const std::unordered_map<Mtx*,MtxF> noMatrixOverrides;
                F3DLight light{};
                light.l.col[0]=179;light.l.col[1]=101;light.l.col[2]=53;
                light.l.dir[0]=90;light.l.dir[1]=80;light.l.dir[2]=31;
                F3DVp viewport{};
                viewport.vp.vscale[0]=440;viewport.vp.vscale[1]=380;
                viewport.vp.vtrans[0]=620;viewport.vp.vtrans[1]=470;
                std::array<F3DVtx,6> freshVertices{};
                for(size_t v=0;v<freshVertices.size();++v) {
                    auto& source=freshVertices[v].v;
                    source.ob[0]=v%3==1?1:0;source.ob[1]=v%3==2?1:0;
                    source.tc[0]=int16_t(90+v*21);source.tc[1]=int16_t(70+v*13);
                    source.cn[0]=uint8_t(30+v*20);source.cn[1]=190;source.cn[2]=45;source.cn[3]=220;
                }
                auto setCommand=[&](size_t slot,uint8_t opcode,uint32_t low,uintptr_t word1) {
                    attributeSequence[slot].words.w0=(uint32_t(opcode)<<24)|low;
                    attributeSequence[slot].words.w1=word1;
                };
                setCommand(4,uint8_t(F3DEX2_G_MTX),
                    (get_attr(MTX_LOAD)|get_attr(MTX_PUSH))^F3DEX2_G_MTX_PUSH,reinterpret_cast<uintptr_t>(identity.data()));
                setCommand(6,uint8_t(F3DEX2_G_POPMTX),0,64);
                setCommand(8,uint8_t(F3DEX2_G_MOVEWORD),G_MW_NUMLIGHT<<16,24);
                setCommand(10,uint8_t(F3DEX2_G_MOVEWORD),G_MW_FOG<<16,0x00200040);
                setCommand(12,uint8_t(F3DEX2_G_MOVEMEM),(6<<8)|F3DEX2_G_MV_LIGHT,reinterpret_cast<uintptr_t>(&light));
                setCommand(14,uint8_t(F3DEX2_G_MOVEMEM),F3DEX2_G_MV_VIEWPORT,reinterpret_cast<uintptr_t>(&viewport));
                setCommand(18,uint8_t(RDP_G_SETPRIMCOLOR),127,0x38b36b9d);
                setCommand(20,uint8_t(RDP_G_SETFOGCOLOR),0,0x674395a1);
                setCommand(22,uint8_t(RDP_G_SETBLENDCOLOR),0,0x57ba2977);
                setCommand(24,uint8_t(OTR_G_SETINTENSITY),0,0x593fca81);
                setCommand(26,uint8_t(RDP_G_SETKEYR),0,0x00002781);
                setCommand(28,uint8_t(RDP_G_SETKEYGB),0,0x57276187);
                setCommand(30,uint8_t(RDP_G_SETCONVERT),0,0x03515829);
                setCommand(34,uint8_t(F3DEX2_G_MTX),
                    (get_attr(MTX_LOAD)|get_attr(MTX_PROJECTION))^F3DEX2_G_MTX_PUSH,reinterpret_cast<uintptr_t>(identity.data()));
                setCommand(36,uint8_t(F3DEX2_G_VTX),(6<<12)|(6<<1),reinterpret_cast<uintptr_t>(freshVertices.data()));
                for(auto* owner:{&reference,&fast}) {
                    owner->mCurMtxReplacements=&noMatrixOverrides;
                    owner->mRsp->modelview_matrix_stack_size=1;
                    for(size_t row=0;row<4;++row)for(size_t col=0;col<4;++col) {
                        owner->mRsp->P_matrix[row][col]=row==col?1.f:0.f;
                        owner->mRsp->modelview_matrix_stack[0][row][col]=row==col?1.f:0.f;
                    }
                    owner->mTriangleRunEnabled=true;
                    owner->mTrianglePreparationDiagnostics={};
                }
                reference.mStructuralPreparationEnabled=false;
                referenceBackend.events.clear();fastBackend.events.clear();
                reference.TestTriangleRunCommands(attributeSequence.data(),attributeSequence.size());
                fast.TestTriangleRunCommands(attributeSequence.data(),attributeSequence.size());
                reference.Flush();fast.Flush();
                const auto before=reference.mTrianglePreparationDiagnostics.preparations;
                const auto after=fast.mTrianglePreparationDiagnostics.preparations;
                const bool equal=TriangleDrawSamples(referenceBackend)==TriangleDrawSamples(fastBackend) &&
                    reference.mTrianglePreparationDiagnostics.accepted==fast.mTrianglePreparationDiagnostics.accepted &&
                    !std::memcmp(reference.mRsp->loaded_vertices,fast.mRsp->loaded_vertices,sizeof(reference.mRsp->loaded_vertices)) &&
                    after<=before;
                traceRun("material-attribute-dependencies",equal);
                structuralRunsEqual &= equal;
                structuralReferencePreparations+=before;structuralCandidatePreparations+=after;
                reference.mTriangleRunEnabled=false;
                reference.mStructuralPreparationEnabled=true;
                reference.mCurMtxReplacements=fast.mCurMtxReplacements=nullptr;
            }
            if(!spec.alias && !spec.cull && !spec.reject && !spec.invalid) {
                // ModifyVertex changes packed attributes, not material state.
                // The following triangles must pick up the new UV/color bytes.
                auto vertexSequence=sequence;
                vertexSequence[16].words.w0=(uint32_t(uint8_t(F3DEX2_G_MODIFYVTX))<<24)|(uint32_t(G_MWO_POINT_ST)<<16);
                vertexSequence[16].words.w1=0x01900320;
                referenceBackend.events.clear();fastBackend.events.clear();
                reference.TestTriangleRunCommands(vertexSequence.data(),vertexSequence.size());
                fast.TestTriangleRunCommands(vertexSequence.data(),vertexSequence.size());
                reference.Flush();fast.Flush();
                const bool modifyEqual=referenceBackend.events==fastBackend.events;
                traceRun("modify-vertex",modifyEqual);
                triangleRunsEqual &= modifyEqual;
            }
            // An entirely rejected run never prepares materials, but must
            // still invalidate projection before any later vertex changes.
            if (spec.cull) {
                const auto oldGeneration = fast.mTriangleVertexGeneration;
                fast.BeginTriangleCommand(true);
                fast.GfxSpTri1(0,1,2,false);
                fast.BeginTriangleCommand(false);
                const bool generationAdvanced=fast.mTriangleVertexGeneration != oldGeneration;
                traceRun("generation",generationAdvanced);
                triangleRunsEqual &= generationAdvanced;
                fast.Flush();reference.Flush();
                referenceBackend.events.clear();fastBackend.events.clear();
                for (auto* owner : {&reference,&fast}) {
                    owner->mRsp->loaded_vertices[0].x += 3900.f;
                    owner->mRsp->loaded_vertices[2].y -= 6200.f;
                }
                reference.TestTriangleRunCommands(sequence.data(),sequence.size());
                fast.TestTriangleRunCommands(sequence.data(),sequence.size());
                reference.Flush();fast.Flush();
                const bool cullReplayEqual=referenceBackend.events == fastBackend.events;
                traceRun("cull-replay",cullReplayEqual);
                triangleRunsEqual &= cullReplayEqual;
            }
            runPreparations+=fast.mTrianglePreparationDiagnostics.preparations;
            runReferencePreparations+=reference.mTrianglePreparationDiagnostics.preparations;
            if (spec.cull == 1 || spec.cull == 2) {
                // Incremental benchmark: preparation and packed vertices match;
                // only the candidate reuses vertex projection for culling.
                reference.mTriangleRunEnabled=true;reference.mTriangleProjectionEnabled=false;
                reference.mTrianglePreparationDiagnosticsEnabled=false;fast.mTrianglePreparationDiagnosticsEnabled=false;
                std::vector<double> baseline,candidate;
                auto measure=[&](Interpreter& owner,TriangleProbeRenderer& backend){
                    auto start=std::chrono::steady_clock::now();
                    for(int iteration=0;iteration<250;++iteration){
                        owner.TestTriangleRunCommands(sequence.data(),sequence.size());owner.Flush();backend.events.clear();
                    }
                    return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
                };
                measure(reference,referenceBackend);measure(fast,fastBackend);
                for(int round=0;round<7;++round){
                    if(round%2){candidate.push_back(measure(fast,fastBackend));baseline.push_back(measure(reference,referenceBackend));}
                    else{baseline.push_back(measure(reference,referenceBackend));candidate.push_back(measure(fast,fastBackend));}
                }
                std::sort(baseline.begin(),baseline.end());std::sort(candidate.begin(),candidate.end());
                runReferenceMs+=baseline[3];runCandidateMs+=candidate[3];
            }
        }
    }
    // Compare complete loaded-vertex bytes against the former unconditional
    // positional transform, including ambient/directional/mixed lights, normal
    // texture generation, fog and offscreen/aspect paths.
    bool verticesEqual=true;unsigned vertexCases=0;
    std::array<F3DVtx,8> vertices{};
    for(int i=0;i<8;++i){auto& v=vertices[i].v;v.ob[0]=i*13-50;v.ob[1]=i*7-20;v.ob[2]=i*11-33;v.tc[0]=i*97;v.tc[1]=i*47;v.cn[0]=23+i*17;v.cn[1]=60+i*9;v.cn[2]=100-i*6;v.cn[3]=128+i;}
    for(int pattern=0;pattern<3;++pattern)
    for(int mode=0;mode<32;++mode)for(int lighting=0;lighting<4;++lighting)for(int target=0;target<2;++target){
        auto input = vertices;
        for(int i=0;i<8;++i) for(int c=0;c<3;++c)
            input[i].v.cn[c]=vertices[pattern==0?i:pattern==1?i/2:0].v.cn[c];
        Interpreter reference,fast;
        auto initialize=[&](Interpreter& owner){
            owner.mCurDimensions.width=640;owner.mCurDimensions.height=480;owner.mFbActive=target!=0;owner.mActiveFrameBuffer=owner.mFrameBuffers.end();
            owner.mRsp->geometry_mode=(mode&1?G_LIGHTING:0)|(mode&2?G_LIGHTING_POSITIONAL:0)|(mode&4?G_FOG:0)|(mode&8?G_TEXTURE_GEN:0)|(mode&16?G_TEXTURE_GEN_LINEAR:0);
            owner.mRsp->current_num_lights=lighting==0?1:4;owner.mRsp->lights_changed=true;
            auto matrix=mmvr::YawPose(.3f,13,22,-41);
            std::memcpy(owner.mRsp->MP_matrix,&matrix,sizeof(matrix));
            owner.mRsp->modelview_matrix_stack_size=1;
            std::memcpy(owner.mRsp->modelview_matrix_stack[0],&matrix,sizeof(matrix));
            owner.mRsp->texture_scaling_factor={65535,32767};owner.mRsp->fog_mul=120;owner.mRsp->fog_offset=30;
            owner.mRsp->lookat[0].dir[0]=127;owner.mRsp->lookat[1].dir[1]=127;
            for(int i=0;i<4;++i){auto& light=owner.mRsp->current_lights[i];light={};light.l.col[0]=12+i*10;light.l.col[1]=20+i*8;light.l.col[2]=40+i*3;light.l.dir[0]=30;light.l.dir[1]=60;light.l.dir[2]=95;
                if(i<3&&(lighting==3||(lighting==2&&i==1))){light.p.unk3=1;light.p.unk7=15;light.p.unkE=4;light.p.pos[0]=100+i*20;light.p.pos[1]=80+i*30;light.p.pos[2]=-90+i*10;}
            }
            std::memset(owner.mRsp->loaded_vertices,0,sizeof(owner.mRsp->loaded_vertices));
        };
        initialize(reference);initialize(fast);
        reference.GfxSpVertexImpl(input.size(),0,input.data(),false,false);fast.GfxSpVertex(input.size(),0,input.data());
        verticesEqual&=std::memcmp(reference.mRsp->loaded_vertices,fast.mRsp->loaded_vertices,sizeof(reference.mRsp->loaded_vertices))==0;++vertexCases;
    }
    std::ofstream("native-positional-vertices.json")<<"{\"cases\":"<<vertexCases<<",\"byteEquality\":"<<verticesEqual<<"}";
    if(!verticesEqual)throw std::runtime_error("Unused positional-light optimization changed native vertex output");
    // Opt-in benchmark of the actual vertex path. This runs only in the native
    // differential fixture, alternates order, and never labels CPU timings FPS.
    if (const char* flag = std::getenv("MMVR_BENCHMARK_VERTEX_LIGHTING"); flag && std::string(flag) == "1") {
        std::ofstream report("native-lighting-benchmark.json");
        report << "[";
        bool first = true;
        for (int lightMode = 0; lightMode < 4; ++lightMode) for (int count : {8, 32}) {
            Interpreter probe;
            probe.mCurDimensions.width = 640;
            probe.mCurDimensions.height = 480;
            probe.mActiveFrameBuffer = probe.mFrameBuffers.end();
            probe.mRsp->geometry_mode = G_LIGHTING_POSITIONAL | (lightMode ? G_LIGHTING : 0);
            probe.mRsp->current_num_lights = lightMode <= 1 ? 1 : 4;
            probe.mRsp->modelview_matrix_stack_size = 1;
            auto matrix = mmvr::YawPose(.3f, 13, 22, -41);
            std::memcpy(probe.mRsp->MP_matrix, &matrix, sizeof(matrix));
            std::memcpy(probe.mRsp->modelview_matrix_stack[0], &matrix, sizeof(matrix));
            for (int i = 0; i < 4; ++i) {
                auto& light = probe.mRsp->current_lights[i];
                light = {}; light.l.col[0] = 22; light.l.col[1] = 37; light.l.col[2] = 45;
                light.l.dir[0] = 30; light.l.dir[1] = 60; light.l.dir[2] = 95;
                if (lightMode == 3 && i < 3) {
                    light.p.unk3 = 1; light.p.unk7 = 15; light.p.unkE = 4;
                    light.p.pos[0] = 100 + i * 20; light.p.pos[1] = 80 + i * 30; light.p.pos[2] = -90 + i * 10;
                }
            }
            probe.mRsp->lights_changed = true;
            std::array<F3DVtx, 32> input;
            for (int i = 0; i < 32; ++i) input[i] = vertices[i % vertices.size()];
            probe.GfxSpVertex(count, 0, input.data());
            std::vector<double> reference, candidate;
            double checksum = 0;
            auto sample = [&](bool optimized) {
                auto start = std::chrono::steady_clock::now();
                for (int batch = 0; batch < 15000; ++batch)
                    probe.GfxSpVertexImpl(count, 0, input.data(), optimized);
                auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
                checksum += probe.mRsp->loaded_vertices[count - 1].x;
                (optimized ? candidate : reference).push_back(elapsed);
            };
            for (int round = 0; round < 7; ++round) {
                sample(round % 2 == 0); sample(round % 2 != 0);
            }
            std::sort(reference.begin(), reference.end());
            std::sort(candidate.begin(), candidate.end());
            if (!first) report << ',';
            first = false;
            report << "{\"lightMode\":" << lightMode << ",\"verticesPerCommand\":" << count
                   << ",\"referenceMs\":" << reference[3] << ",\"candidateMs\":" << candidate[3]
                   << ",\"ratio\":" << candidate[3] / reference[3] << ",\"checksum\":" << checksum << '}';
        }
        report << "]";
    }
    if (const char* flag = std::getenv("MMVR_BENCHMARK_VERTEX_LIGHTING"); flag && std::string(flag) == "1") {
        std::ofstream report("native-directional-benchmark.json");
        report << "[";
        bool first = true;
        for (int pattern=0;pattern<3;++pattern) for(int count:{8,32}) {
            Interpreter probe;
            probe.mCurDimensions.width=640;probe.mCurDimensions.height=480;
            probe.mActiveFrameBuffer=probe.mFrameBuffers.end();
            probe.mRsp->geometry_mode=G_LIGHTING;probe.mRsp->current_num_lights=4;
            probe.mRsp->modelview_matrix_stack_size=1;
            auto matrix=mmvr::YawPose(.3f,13,22,-41);
            std::memcpy(probe.mRsp->MP_matrix,&matrix,sizeof(matrix));
            std::memcpy(probe.mRsp->modelview_matrix_stack[0],&matrix,sizeof(matrix));
            for(int i=0;i<4;++i){auto& light=probe.mRsp->current_lights[i];light={};
                light.l.col[0]=22;light.l.col[1]=37;light.l.col[2]=45;
                light.l.dir[0]=30;light.l.dir[1]=60;light.l.dir[2]=95;}
            probe.mRsp->lights_changed=true;
            std::array<F3DVtx,32> input;
            for(int i=0;i<32;++i){input[i]=vertices[i%8];for(int c=0;c<3;++c)
                input[i].v.cn[c]=vertices[pattern==0?i%8:pattern==1?(i/2)%8:0].v.cn[c];}
            probe.GfxSpVertex(count,0,input.data());
            std::vector<double> baseline,candidate;double checksum=0;
            auto sample=[&](bool reuse){auto start=std::chrono::steady_clock::now();
                for(int batch=0;batch<15000;++batch)probe.GfxSpVertexImpl(count,0,input.data(),true,reuse);
                double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
                (reuse?candidate:baseline).push_back(ms);checksum+=probe.mRsp->loaded_vertices[count-1].color.r;};
            for(int round=0;round<7;++round){sample(round%2==0);sample(round%2!=0);}
            std::sort(baseline.begin(),baseline.end());std::sort(candidate.begin(),candidate.end());
            if(!first)report<<',';first=false;
            report<<"{\"pattern\":"<<pattern<<",\"vertices\":"<<count<<",\"baselineMs\":"<<baseline[3]
                  <<",\"candidateMs\":"<<candidate[3]<<",\"ratio\":"<<candidate[3]/baseline[3]<<",\"checksum\":"<<checksum<<'}';
        }
        report<<"]";
    }
    // Ordinary runs do not write diagnostic counters, including the pair path.
    Interpreter gated;TriangleProbeRenderer gatedBackend;gated.mRapi=&gatedBackend;
    gated.mTrianglePreparationDiagnosticsEnabled=false;
    gated.mRsp->loaded_vertices[0].clip_rej=gated.mRsp->loaded_vertices[1].clip_rej=gated.mRsp->loaded_vertices[2].clip_rej=1;
    gated.GfxSpTri2(0,1,2,0,1,2);
    for(unsigned i=0;i<3;++i){gated.mRsp->loaded_vertices[i].clip_rej=0;gated.mRsp->loaded_vertices[i].w=1;}
    gated.GfxSpTri2(0,1,2,0,1,2);gated.Flush();
    const auto& silent=gated.mTrianglePreparationDiagnostics;
    const bool diagnosticsGated=!silent.pairCommands&&!silent.submitted&&!silent.accepted&&!silent.preparations&&!silent.reuses&&!silent.aliasFallbacks;
    bool eventEquality=true,vertexEquality=true,stateEquality=true,queryReduction=true,coverage=true;
    const char* firstFailure="";
    for(const auto& result:results){
        eventEquality&=result.events;vertexEquality&=result.bytes;stateEquality&=result.state;queryReduction&=result.queries;coverage&=result.expected;
        if(!*firstFailure&&(!result.events||!result.bytes||!result.state||!result.queries||!result.expected))firstFailure=result.name;
    }
    std::ofstream("native-structural-preparation.json")
        <<"{\"equal\":"<<(structuralRunsEqual?"true":"false")
        <<",\"referencePreparations\":"<<structuralReferencePreparations
        <<",\"candidatePreparations\":"<<structuralCandidatePreparations<<"}";
    const bool passed=triangleRunsEqual&&structuralRunsEqual&&structuralCandidatePreparations<structuralReferencePreparations&&runPreparations<runReferencePreparations&&eventEquality&&vertexEquality&&stateEquality&&queryReduction&&coverage&&diagnosticsGated&&isolated&&reuses>0&&aliasFallbacks>0;
    const char* session=std::getenv("MMVR_SESSION_TOKEN");
    std::ofstream report("native-triangle-preparation.json");
    report<<"{\"session\":\""<<(session?session:"")<<"\",\"passed\":"<<passed<<",\"cases\":"<<results.size()
        <<",\"eventEquality\":"<<eventEquality<<",\"vertexEquality\":"<<vertexEquality<<",\"stateEquality\":"<<stateEquality
        <<",\"runReferenceMs\":"<<runReferenceMs<<",\"runCandidateMs\":"<<runCandidateMs
        <<",\"triangleRunsEqual\":"<<triangleRunsEqual<<",\"runPreparations\":"<<runPreparations<<",\"runReferencePreparations\":"<<runReferencePreparations
        <<",\"queryReduction\":"<<queryReduction<<",\"coverage\":"<<coverage<<",\"diagnosticsGated\":"<<diagnosticsGated
        <<",\"isolated\":"<<isolated<<",\"preparations\":"<<preparations<<",\"reuses\":"<<reuses<<",\"aliasFallbacks\":"<<aliasFallbacks
        <<",\"firstFailure\":\""<<firstFailure<<"\",\"results\":[";
    for(size_t i=0;i<results.size();++i){
        const auto& r=results[i];if(i)report<<',';
        report<<"{\"name\":\""<<r.name<<"\",\"events\":"<<r.events<<",\"bytes\":"<<r.bytes<<",\"state\":"<<r.state
            <<",\"queries\":"<<r.queries<<",\"expected\":"<<r.expected<<",\"accepted\":"<<r.accepted<<",\"prepared\":"<<r.prepared
            <<",\"reused\":"<<r.reused<<",\"aliasFallbacks\":"<<r.aliasFallbacks<<'}';
    }
    report<<"]}";report.close();
    if(!report)throw std::runtime_error("Could not write native triangle-preparation report");
    if(!passed)throw std::runtime_error("Native triangle-preparation differential regression failed");
}
