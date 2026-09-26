#ifdef MMVR_ENABLE
#include "Masks.h"
#include "MaskModels.h"
#include "2s2h/Enhancements/FrameInterpolation/FrameInterpolation.h"
#include <libultraship/libultraship.h>
#include <fast/resource/type/DisplayList.h>
#include <unordered_map>
#include <cstring>
namespace {
struct Trim {
    const char* path;
    size_t index;
    uint32_t w0, w1;
};
struct Visual {
    int item;
    const char* first;
    const char* second;
    bool translucent;
};
#include "MaskGeometry.inc"
struct Filtered {
    std::shared_ptr<Fast::DisplayList> original;
    std::vector<Gfx> commands;
};
std::unordered_map<std::string, Filtered> cache;
const Gfx* MaskList(const char* path) {
    auto source = std::dynamic_pointer_cast<Fast::DisplayList>(
        Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource(path));
    if (!source)
        return nullptr;
    auto& entry = cache[path];
    if (entry.original != source) {
        entry.original = source;
        entry.commands = source->Instructions;
        // Only remove known native triangles. A replacement model with different topology is left intact.
        bool matches = true;
        for (const auto& t : trims)
            if (!std::strcmp(t.path, path))
                matches &= t.index < entry.commands.size() && entry.commands[t.index].words.w0 == t.w0 &&
                           entry.commands[t.index].words.w1 == t.w1;
        if (matches)
            for (const auto& t : trims)
                if (!std::strcmp(t.path, path)) {
                    auto* command = &entry.commands[t.index];
                    gSPNoOp(command);
                }
    }
    return entry.commands.data();
}
} // namespace
static void DrawNativeMask(PlayState* play, int item) {
    const Visual* v = nullptr;
    for (const auto& candidate : visuals)
        if (candidate.item == item) {
            v = &candidate;
            break;
        }
    if (!v)
        return;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    if (auto* list = MaskList(v->first)) {
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)list);
    }
    if (v->second) {
        auto* list = MaskList(v->second);
        if (list) {
            if (v->translucent) {
                Gfx_SetupDL25_Xlu(play->state.gfxCtx);
                MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
                gSPDisplayList(POLY_XLU_DISP++, (Gfx*)list);
            } else {
                gSPDisplayList(POLY_OPA_DISP++, (Gfx*)list);
            }
        }
    }
    CLOSE_DISPS(play->state.gfxCtx);
}
namespace mmvrgame {
void DrawMaskModel(PlayState* play, int item) {
    DrawNativeMask(play, item);
}
} // namespace mmvrgame
#endif
