#ifdef MMVR_ENABLE
#include "PauseWheel.h"
#include "ItemUse.h"
#include "ui.h"
#include "runtime.h"
#include <cstdlib>
#include <cstring>
#include <fstream>
extern "C" {
#include "global.h"
#include "interface/parameter_static/parameter_static.h"
}
extern "C" int MMVR_DrawWheelOutlines(PlayState* play, int page) {
    const char* test = std::getenv("MMVR_NATIVE_TEST");
    const bool privateTest = mmvr::PrivateDebugTools && test && std::strcmp(test, "1") == 0;
    if (!play || (!mmvr::StereoActive() && !privateTest))
        return false;
    Vtx* grid = page == PAUSE_ITEM ? play->pauseCtx.itemVtx : play->pauseCtx.maskVtx;
    if (!grid)
        return true;
    const int firstSlot = page == PAUSE_ITEM ? 0 : 24;
    unsigned drawn = 0;
    OPEN_DISPS(play->state.gfxCtx);
    for (int i = 0; i < mmvr::ActiveItemSlots(mmvr::GetSettings()); ++i) {
        const int slot = mmvr::DisplaySlotAssignment(i);
        if (slot < firstSlot || slot >= firstSlot + 24 || mmvrgame::InventorySlotItem(slot) == ITEM_NONE)
            continue;
        drawn |= 1u << i;
        const auto& origin = grid[(slot - firstSlot) * 4];
        auto* vertices = static_cast<Vtx*>(GRAPH_ALLOC(play->state.gfxCtx, 4 * sizeof(Vtx)));
        // Native selected-item outline geometry, allocated independently for all enabled wheel slots.
        for (int v = 0; v < 4; ++v) {
            vertices[v] = {};
            vertices[v].v.ob[0] = origin.v.ob[0] - 2 + (v & 1 ? 32 : 0);
            vertices[v].v.ob[1] = origin.v.ob[1] + 2 - (v & 2 ? 32 : 0);
            vertices[v].v.tc[0] = (v & 1 ? 32 : 0) << 5;
            vertices[v].v.tc[1] = (v & 2 ? 32 : 0) << 5;
            vertices[v].v.cn[0] = vertices[v].v.cn[1] = vertices[v].v.cn[2] = 255;
            vertices[v].v.cn[3] = play->pauseCtx.alpha;
        }
        gSPVertex(POLY_OPA_DISP++, reinterpret_cast<uintptr_t>(vertices), 4, 0);
        POLY_OPA_DISP = Gfx_DrawTexQuadIA8(POLY_OPA_DISP, (TexturePtr)gEquippedItemOutlineTex, 32, 32, 0);
    }
    CLOSE_DISPS(play->state.gfxCtx);
    if (privateTest) {
        static std::ofstream log("native-wheel-outlines.log");
        static unsigned last[2] = { ~0u, ~0u };
        int index = page == PAUSE_ITEM ? 0 : 1;
        if (last[index] != drawn) {
            last[index] = drawn;
            log << page << " " << drawn << "\n" << std::flush;
        }
    }
    return true;
}
#endif
