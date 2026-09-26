#pragma once
#include <d3d11.h>
namespace mmvr {
inline void NativeLayerAlpha(D3D11_RENDER_TARGET_BLEND_DESC& d) {
    d.SrcBlendAlpha = D3D11_BLEND_ONE;
    d.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    d.BlendOpAlpha = D3D11_BLEND_OP_ADD;
}
} // namespace mmvr
