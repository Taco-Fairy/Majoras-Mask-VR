// Scale only the shield sub-list; keep the original hand, material and collision pose.
#include <libultraship/libultraship.h>
#include <fast/resource/type/DisplayList.h>
#include <libultraship/bridge/resourcebridge.h>
static Gfx* ShieldVisualList(PlayState* play, const void* mesh) {
    const float scale = mmvr::GetSettings().Get(mmvr::Setting::ShieldVisualSize) * .01f;
    if (scale == 1.f || !mesh || !mmvrgame::ShieldRaised() ||
        GET_PLAYER(play)->transformation != PLAYER_FORM_HUMAN ||
        mesh != mmvrgame::TrackedShieldMesh(GET_PLAYER(play))) return (Gfx*)mesh;
    const char* path = (const char*)mesh;
    if (std::strncmp(path, "__OTR__", 7) != 0) return (Gfx*)mesh;
    auto resource = std::dynamic_pointer_cast<Fast::DisplayList>(
        Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource(path + 7));
    if (!resource) return (Gfx*)mesh;
    const auto& commands = resource->Instructions;
    // Hash commands occupy two words. Skip payloads so their bytes are not opcodes.
    size_t shieldIndex = commands.size();
    for (size_t i = 0; i < commands.size(); ++i) {
        const auto op = commands[i].words.w0 >> 24;
        if (op == G_DL_OTR_HASH && i + 1 < commands.size()) {
            const uint64_t hash = (uint64_t(commands[i+1].words.w0) << 32) | commands[i+1].words.w1;
            const char* name = ResourceGetNameByCrc(hash);
            if (name && (std::strstr(name, "/gLinkHumanHerosShieldDL") ||
                         std::strstr(name, "/gLinkHumanMirrorShieldDL"))) shieldIndex = i;
        }
        if ((op == G_DL_OTR_HASH || op == G_MARKER || op == G_VTX_OTR_HASH || op == G_SETTIMG_OTR_HASH) && i+1 < commands.size()) ++i;
    }
    // Unknown replacement meshes retain their original geometry rather than scaling the hand.
    if (shieldIndex == commands.size()) return (Gfx*)mesh;
    Matrix_Push();
    Matrix_Scale(scale, scale, scale, MTXMODE_NEW);
    auto* size = Matrix_Finalize(play->state.gfxCtx);
    Matrix_Pop();
    auto* output = (Gfx*)GRAPH_ALLOC(play->state.gfxCtx, (commands.size()+2)*sizeof(Gfx));
    auto* dst = output;
    for (size_t i = 0; i < commands.size(); ++i) {
        if (i == shieldIndex) gSPMatrix(dst++, size, G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
        *dst++ = commands[i];
        if (i == shieldIndex+1) gSPPopMatrix(dst++, G_MTX_MODELVIEW);
    }
    return output;
}
