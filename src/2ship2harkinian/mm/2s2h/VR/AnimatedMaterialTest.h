#pragma once
#include "2s2h/resource/type/TextureAnimation.h"
#include "2s2h/resource/type/Cutscene.h"
// Opt-in, isolated decoder diagnostic. Uses the production animated-material writer
// and RDP command dispatcher, then checks the selected texture binding (not final rendered pixels).
static void NativeAnimatedMaterialTest(PlayState* play) {
    auto window=std::dynamic_pointer_cast<Fast::Fast3dWindow>(Ship::Context::GetRawInstance()->GetWindow());
    auto interpreter=window->GetInterpreterWeak().lock();
    auto manager=Ship::Context::GetRawInstance()->GetResourceManager();
    std::ofstream out("native-animated-material.log");
    const char* paths[]={"objects/object_dy_obj/gGreatFairyAppearenceTexAnim",
        "objects/gameplay_dangeon_keep/gameplay_dangeon_keep_Matanimheader_01B370"};
    for(auto path:paths){
        auto anim=std::dynamic_pointer_cast<SOH::TextureAnimation>(manager->LoadResource(path));
        auto* params=static_cast<SOH::AnimatedMatTexCycleParams*>(anim->anims[0].params);
        for(int step=0;step<params->keyFrameLength;++step){
            auto* begin=play->state.gfxCtx->polyOpa.p;
            AnimatedMat_DrawStepOpa(play,reinterpret_cast<AnimatedMaterial*>(anim->GetPointer()),step);
            auto* end=play->state.gfxCtx->polyOpa.p;
            interpreter->TestTriangleRunCommands(reinterpret_cast<Fast::F3DGfx*>(begin),size_t(end-begin));
            play->state.gfxCtx->polyOpa.p=begin;
            const char* expectedPath=static_cast<const char*>(params->textureList[params->textureIndexList[step]]);
            auto expected=std::dynamic_pointer_cast<Fast::Texture>(manager->LoadResource(expectedPath));
            Fast::F3DGfx command{};command.words.w0=0xfd100000;command.words.w1=0x08000001;
            interpreter->TestTriangleRunCommands(&command,1);
            const auto& actual=interpreter->mRdp->texture_to_load;
            out<<path<<" step="<<step<<" selected="<<expectedPath<<" addressMatches="<<(actual.addr==expected->ImageData)
                <<" resource="<<(actual.raw_tex_metadata.resource?actual.raw_tex_metadata.resource->GetInitData()->Path:"RAW")
                <<" width="<<actual.raw_tex_metadata.width<<" height="<<actual.raw_tex_metadata.height
                <<" segment="<<reinterpret_cast<const char*>(interpreter->SegAddr(0x08000001))<<"\n";
        }
    }
    const char* scripts[]={
        "scenes/nonmq/Z2_BACKTOWN/Z2_BACKTOWNCutsceneData_0004D4",
        "scenes/nonmq/Z2_CLOCKTOWER/Z2_CLOCKTOWERCutsceneData_000B4C",
        "scenes/nonmq/Z2_CLOCKTOWER/Z2_CLOCKTOWERCutsceneData_0020EC",
        "scenes/nonmq/Z2_CLOCKTOWER/Z2_CLOCKTOWERCutsceneData_0056B8",
        "scenes/nonmq/Z2_CLOCKTOWER/Z2_CLOCKTOWERCutsceneData_006194",
        "scenes/nonmq/Z2_CLOCKTOWER/Z2_CLOCKTOWERCutsceneData_0072BC",
        "scenes/nonmq/Z2_ICHIBA/Z2_ICHIBACutsceneData_000650",
        "scenes/nonmq/Z2_ICHIBA/Z2_ICHIBACutsceneData_0014B0",
        "scenes/nonmq/Z2_ICHIBA/Z2_ICHIBACutsceneData_001670",
        "scenes/nonmq/Z2_ICHIBA/Z2_ICHIBACutsceneData_003BC0",
        "scenes/nonmq/Z2_TOWN/Z2_TOWNCutsceneData_000B70",
        "scenes/nonmq/Z2_TOWN/Z2_TOWNCutsceneData_004378",
        "scenes/nonmq/Z2_TOWN/Z2_TOWNCutsceneData_004A80",
        "scenes/nonmq/Z2_00KEIKOKU/gTerminaFieldIntroFromEastClockTownCs"
    };
    for(auto path:scripts) {
        auto script=std::dynamic_pointer_cast<SOH::Cutscene>(manager->LoadResource(path));
        out << "cutscene=" << path << " loaded=" << bool(script)
            << " titleCard=" << (script && script->showsSceneTitleCard)
            << " classified=" << (script && SOH::Cutscene::IsAreaIntroduction(script->GetPointer())) << "\n";
    }
}
