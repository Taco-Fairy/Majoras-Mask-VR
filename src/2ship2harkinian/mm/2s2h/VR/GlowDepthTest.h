#pragma once
// Inspect the commands produced by the real native light draw, rather than a
// duplicate VR-only material. This fixture never submits its temporary list.
static bool NativeGlowDepthTest(PlayState* play) {
    const auto settings = mmvr::GetSettings();
    auto* gfx = play->state.gfxCtx;
    const auto savedXlu = gfx->polyXlu;
    auto* savedLights = play->lightCtx.listHead;
    LightInfo info{};
    Lights_PointGlowSetInfo(&info, 0, 100, 0, 255, 180, 80, 100);
    info.params.point.drawGlow = true;
    LightNode light{};
    light.info = &info;
    play->lightCtx.listHead = &light;
    bool valid = true;
    Matrix_Push();
    for (int mode : {0, 2}) {
        mmvr::GetSettings().Set(mmvr::Setting::ViewMode, float(mode));
        mmvr::ApplyViewMode(mode);
        const bool firstPerson = MMVR_FirstPersonBody() != 0;
        valid &= firstPerson == (mode == 2);
        Gfx* begin = gfx->polyXlu.p;
        Lights_DrawGlow(play);
        unsigned depthModes = 0, depthGeometry = 0, matrices = 0;
        for (Gfx* command = begin; command < gfx->polyXlu.p; ++command) {
            const unsigned op = (command->words.w0 >> 24) & 0xff;
            if (op == G_MTX) ++matrices;
            if (op == G_GEOMETRYMODE && (command->words.w1 & G_ZBUFFER)) ++depthGeometry;
            if (op == G_SETOTHERMODE_L && (command->words.w1 & Z_CMP)) {
                ++depthModes;
                // Cloud alpha blending is retained, with depth comparison only.
                valid &= !(command->words.w1 & Z_UPD);
                valid &= command->words.w1 == (G_RM_ZB_CLD_SURF | G_RM_ZB_CLD_SURF2);
            }
        }
        valid &= matrices == 1;
        valid &= depthModes == unsigned(firstPerson) && depthGeometry == unsigned(firstPerson);
        gfx->polyXlu = savedXlu;
    }
    Matrix_Pop();
    play->lightCtx.listHead = savedLights;
    mmvr::GetSettings() = settings;
    mmvr::ApplyViewMode(int(settings.Get(mmvr::Setting::ViewMode)));
    return valid;
}
