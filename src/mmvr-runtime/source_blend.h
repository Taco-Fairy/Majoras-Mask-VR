#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <stdexcept>
namespace mmvr {
struct SourceBlendBinding {
    ID3D11DeviceContext* context;
    ID3D11BlendState* state;
    static void Apply(void* pointer) {
        auto& binding = *static_cast<SourceBlendBinding*>(pointer);
        binding.context->OMSetBlendState(binding.state, nullptr, 0xffffffff);
    }
};
class SourceBlendCache {
    Microsoft::WRL::ComPtr<ID3D11BlendState> states[2];

  public:
    ID3D11BlendState* Get(ID3D11DeviceContext* context, bool theater) {
        auto& state = states[theater ? 1 : 0];
        if (state)
            return state.Get();
        Microsoft::WRL::ComPtr<ID3D11Device> device;
        context->GetDevice(&device);
        D3D11_BLEND_DESC desc{};
        auto& target = desc.RenderTarget[0];
        target.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        if (theater) {
            target.BlendEnable = TRUE;
            target.SrcBlend = D3D11_BLEND_ONE;
            target.DestBlend = D3D11_BLEND_ZERO;
            target.BlendOp = D3D11_BLEND_OP_ADD;
            // Keep the opaque target alpha; native game alpha does not describe theater opacity.
            target.SrcBlendAlpha = D3D11_BLEND_ZERO;
            target.DestBlendAlpha = D3D11_BLEND_ONE;
            target.BlendOpAlpha = D3D11_BLEND_OP_ADD;
        }
        if (FAILED(device->CreateBlendState(&desc, &state)))
            throw std::runtime_error("VR source blend creation failed");
        return state.Get();
    }
};
} // namespace mmvr
