#pragma once
extern "C" {
#include "global.h"
#include "z64pictograph.h"
}
namespace mmvrgame {
// Native photographs and actor validation must use the same viewfinder frame.
// Preserve its controls and complete image until the native confirmation closes.
inline bool NativeViewfinderActive(PlayState* play) {
    return play && ((play->actorCtx.flags & ACTORCTX_FLAG_PICTO_BOX_ON) || sPictoState != PICTO_BOX_STATE_OFF);
}
} // namespace mmvrgame
