#pragma once
namespace mmvrgame { bool TestFullBodyRig(); }
static void NativeFullBodyTest(PlayState* play,unsigned tick) {
    if(tick==60) {
        std::ofstream("native-full-body.log")<<"private native skeleton check\n";
        CVarSetFloat("gVR.FullBody",1);CVarSetFloat("gVR.ViewMode",2);
        mmvr::ApplyViewMode(2);mmvr::GetSettings().Set(mmvr::Setting::FullBody,1);
        mmvr::SetNativeTestTracking(true);
    }
    if(tick==66) {
        mmvrgame::TestFullBodyRig();
        Ship::Context::GetRawInstance()->GetWindow()->Close();
    }
}
