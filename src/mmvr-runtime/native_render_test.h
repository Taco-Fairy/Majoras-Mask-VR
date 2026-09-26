#pragma once
#include "settings.h"
#include <fstream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <d3d11.h>
#include <wrl/client.h>
// Opt-in local integration harness; normal launchers never set this environment variable.
inline bool NativeRenderTest() {
    const char* e = std::getenv("MMVR_NATIVE_TEST");
    return mmvr::PrivateDebugTools && e && std::strcmp(e, "1") == 0;
}
inline void CaptureNativeLayer(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11Texture2D* source,
                               const std::string& prefix = "native-pause") {
    D3D11_TEXTURE2D_DESC desc;
    source->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;
    if (FAILED(device->CreateTexture2D(&desc, nullptr, &staging)))
        return;
    context->CopyResource(staging.Get(), source);
    D3D11_MAPPED_SUBRESOURCE map{};
    if (FAILED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &map)))
        return;
    std::ofstream rgb(prefix + ".ppm", std::ios::binary), alpha(prefix + "-alpha.pgm", std::ios::binary);
    rgb << "P6\n" << desc.Width << " " << desc.Height << "\n255\n";
    alpha << "P5\n" << desc.Width << " " << desc.Height << "\n255\n";
    size_t covered = 0;
    for (unsigned y = 0; y < desc.Height; ++y)
        for (unsigned x = 0; x < desc.Width; ++x) {
            auto* p = static_cast<unsigned char*>(map.pData) + y * map.RowPitch + x * 4;
            rgb.write((char*)p, 3);
            alpha.put(p[3]);
            covered += p[3] > 0;
        }
    context->Unmap(staging.Get(), 0);
    std::ofstream(prefix + "-coverage.txt") << covered << " / " << desc.Width * desc.Height;
}
