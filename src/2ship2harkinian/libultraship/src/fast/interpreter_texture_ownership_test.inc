// Resource ownership must outlive both the caller and a subsequent texture bind.
void Interpreter::VerifyTextureOwnershipMoves() {
    const char* test=std::getenv("MMVR_NATIVE_TEST");
    if(!test || std::strcmp(test,"1")!=0)return;
    Interpreter probe;
    unsigned checks=0;
    auto check=[&](bool ok){++checks;if(!ok)throw std::runtime_error("Texture ownership regression");};
    auto make=[](){auto t=std::make_shared<Texture>();t->ImageData=new uint8_t[4]{11,22,33,44};
        t->ImageDataSize=4;t->Width=t->Height=1;t->Type=TextureType::RGBA32bpp;return t;};
    RawTexMetadata metadata{};
    metadata.width=19;metadata.height=23;metadata.h_byte_scale=2;metadata.v_pixel_scale=3;
    metadata.type=TextureType::RGBA32bpp;metadata.resource=make();
    std::weak_ptr<Texture> weakA=metadata.resource;
    const void* data=metadata.resource->ImageData;
    probe.GfxDpSetTextureImage(0,3,19,"test",7,metadata,data);
    check(weakA.use_count()==2 && metadata.resource);
    const auto& current=probe.mRdp->texture_to_load;
    check(current.addr==data && current.siz==3 && current.width==19 && current.tex_flags==7);
    check(current.raw_tex_metadata.width==19 && current.raw_tex_metadata.height==23 &&
          current.raw_tex_metadata.h_byte_scale==2 && current.raw_tex_metadata.v_pixel_scale==3 &&
          current.raw_tex_metadata.type==TextureType::RGBA32bpp);
    // Aliasing the destination remains legal: the public parameter owns a copy.
    probe.GfxDpSetTextureImage(0,3,19,"test",7,current.raw_tex_metadata,data);
    check(weakA.use_count()==2 && current.addr==data);
    probe.mRdp->loaded_texture[0].raw_tex_metadata=current.raw_tex_metadata;
    metadata.resource.reset();check(weakA.use_count()==2);
    metadata.resource=make();std::weak_ptr<Texture> weakB=metadata.resource;
    const void* dataB=metadata.resource->ImageData;
    probe.GfxDpSetTextureImage(0,3,19,"test",7,std::move(metadata),dataB);
    check(!metadata.resource && weakB.use_count()==1 && weakA.use_count()==1 && current.addr==dataB);
    probe.GfxDpSetTextureImage(0,3,0,nullptr,0,{},nullptr);
    check(weakB.expired() && !weakA.expired());
    probe.mRdp->loaded_texture[0].raw_tex_metadata={};check(weakA.expired());

    metadata.resource=make();data=metadata.resource->ImageData;
    // Compare the previous value-copy implementation with the real current one.
    auto reference=[&](RawTexMetadata value){
        probe.mRdp->texture_to_load.addr=static_cast<const uint8_t*>(data);
        probe.mRdp->texture_to_load.siz=3;probe.mRdp->texture_to_load.width=19;
        probe.mRdp->texture_to_load.tex_flags=7;
        probe.mRdp->texture_to_load.raw_tex_metadata=value;
    };
    constexpr int iterations=100000;
    std::vector<double> before,after;
    for(int run=0;run<7;++run)for(int pass=0;pass<2;++pass){
        bool candidate=(run+pass)%2;
        auto start=std::chrono::steady_clock::now();
        if(candidate)for(int i=0;i<iterations;++i)
            probe.GfxDpSetTextureImage(0,3,19,"test",7,metadata,data);
        else for(int i=0;i<iterations;++i)reference(metadata);
        double ns=std::chrono::duration<double,std::nano>(std::chrono::steady_clock::now()-start).count()/iterations;
        (candidate?after:before).push_back(ns);
        check(metadata.resource.use_count()==2 && current.addr==data);
    }
    std::sort(before.begin(),before.end());std::sort(after.begin(),after.end());
    const char* token=std::getenv("MMVR_SESSION_TOKEN");
    std::ofstream("native-texture-ownership.json")<<"{\"session\":\""<<(token?token:"")
        <<"\",\"passed\":true,\"checks\":"<<checks<<",\"iterations\":"<<iterations
        <<",\"samples\":7,\"referenceNs\":"<<before[3]<<",\"candidateNs\":"<<after[3]
        <<",\"aliasingAndLoadedTileOwnership\":true}";
}
