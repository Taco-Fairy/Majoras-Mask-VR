#ifdef MMVR_LOCAL_TEST_TOOLS
extern "C" Gfx* ResourceMgr_LoadGfxByName(const char*);
static int kafeiDrawCheckPhase=0;
extern "C" int MMVR_KafeiDrawCheckPhase() { return kafeiDrawCheckPhase; }
static void VerifyKafeiDrawPalette(PlayState* play, Actor* actor) {
    if (!std::getenv("MMVR_KAFEI_DRAW_TEST")) return;
    if (!MMVR_ControlledKafei((Player*)actor)) {
        if (kafeiDrawCheckPhase==1) {
            kafeiDrawCheckPhase=2;
            std::ofstream("native-kafei-return.json") << "{\"linkRestored\":true}";
        }
        return;
    }
    if (kafeiDrawCheckPhase || !MMVR_FirstPersonBody()) return;
    if (!handSkeletonPalette || handSkeletonCount<=0 || !extraActive[0] || !extraActive[1])
        throw std::runtime_error("Actual Kafei draw did not record its hand palette");
    // Follow the segmented matrix references in the real archive meshes, not a
    // fabricated rigid hand. These are the commands that overrode tracking.
    unsigned loads[2]{};float maxError=0;
    mmvr::CameraFrame sample{};sample.active=true;
    for (int hand=0;hand<2;++hand) {
        auto* mesh=ResourceMgr_LoadGfxByName((const char*)mmvrgame::FormHandMesh((Player*)actor,hand));
        for (int command=0;command<128;++command) {
            const auto opcode=(mesh[command].words.w0>>24)&255;
            if(opcode==G_ENDDL)break;
            if(opcode!=G_MTX)continue;
            const auto address=uintptr_t(mesh[command].words.w1);
            if((address>>24)!=0x0D)throw std::runtime_error("Unexpected Kafei hand matrix segment");
            const unsigned bone=unsigned(address & 0x00FFFFFE)/sizeof(Mtx);
            if(bone>=unsigned(handSkeletonCount))throw std::runtime_error("Kafei hand palette index out of range");
            ++loads[hand];
            const auto* target=&kafeiTestPalettes[hand][bone];
            for(int pose=0;pose<3;++pose) {
                sample.hands[hand]=mmvr::YawPose(.7f*pose,100.f+hand*60+pose*15,80+pose*9,-40+pose*7);
                mmvr::SetNativeTestCamera(sample);mmvr::SetNativeTestEye(.1f);
                mmvr::Matrix result{};
                if(!mmvr::OverrideModelMatrix(target,result.m))throw std::runtime_error("Embedded Kafei matrix was not tracked");
                const auto expected=mmvr::Multiply(kafeiTestLocals[hand][bone],sample.hands[hand]);
                for(int row=0;row<4;++row)for(int col=0;col<4;++col)
                    maxError=std::max(maxError,std::abs(result.m[row][col]-expected.m[row][col]));
            }
        }
    }
    mmvr::SetNativeTestTracking(false);mmvr::SetNativeTestTracking(true);mmvr::SetNativeTestCamera({});
    if(loads[0]<2 || loads[1]<2 || maxError>.001f || !MMVR_HideNativeBodyRender())
        throw std::runtime_error("Actual Kafei skinned hand draw failed");
    std::ofstream("native-kafei-draw.json") << "{\"actualKafeiActor\":true,\"paletteBones\":" << handSkeletonCount
        << ",\"leftMeshMatrixLoads\":" << loads[0] << ",\"rightMeshMatrixLoads\":" << loads[1]
        << ",\"controllerPosesPerLoad\":3,\"maxMatrixError\":" << maxError << ",\"nativeBodyHidden\":true}";
    kafeiDrawCheckPhase=1;
}
#else
extern "C" int MMVR_KafeiDrawCheckPhase() { return 0; }
#endif
