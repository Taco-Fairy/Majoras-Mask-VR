#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <stdexcept>
#include <string>
namespace mmvr {
class TextureBackup {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> image;

  public:
    void Save(ID3D11DeviceContext* context, ID3D11Texture2D* source) {
        D3D11_TEXTURE2D_DESC src{}, old{};
        source->GetDesc(&src);
        if (image)
            image->GetDesc(&old);
        if (!image || src.Width != old.Width || src.Height != old.Height || src.Format != old.Format ||
            src.MipLevels != old.MipLevels || src.ArraySize != old.ArraySize ||
            src.SampleDesc.Count != old.SampleDesc.Count || src.SampleDesc.Quality != old.SampleDesc.Quality) {
            Microsoft::WRL::ComPtr<ID3D11Device> device;
            source->GetDevice(&device);
            image.Reset();
            src.Usage = D3D11_USAGE_DEFAULT;
            src.CPUAccessFlags = 0;
            src.MiscFlags = 0;
            if (FAILED(device->CreateTexture2D(&src, nullptr, &image)))
                throw std::runtime_error("Cannot preserve game framebuffer");
        }
        context->CopyResource(image.Get(), source);
    }
    void Restore(ID3D11DeviceContext* context, ID3D11Texture2D* target) {
        if (image)
            context->CopyResource(target, image.Get());
    }
};

inline bool ColorFamily(DXGI_FORMAT f) {
    return f == DXGI_FORMAT_R8G8B8A8_TYPELESS || f == DXGI_FORMAT_R8G8B8A8_UNORM ||
           f == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
}
inline std::string DescribeImage(const D3D11_TEXTURE2D_DESC& d) {
    return std::to_string(d.Width) + "x" + std::to_string(d.Height) + " format=" + std::to_string(d.Format) +
           " array=" + std::to_string(d.ArraySize) + " mips=" + std::to_string(d.MipLevels) +
           " samples=" + std::to_string(d.SampleDesc.Count);
}
inline void CopyGameImage(ID3D11DeviceContext* context, ID3D11Texture2D* source, ID3D11Texture2D* destination) {
    D3D11_TEXTURE2D_DESC src{}, dst{};
    source->GetDesc(&src);
    destination->GetDesc(&dst);
    if (!ColorFamily(src.Format) || !ColorFamily(dst.Format) || src.Width != dst.Width || src.Height != dst.Height ||
        src.ArraySize != 1 || dst.ArraySize != 1 || src.MipLevels != 1 || dst.MipLevels != 1 ||
        dst.SampleDesc.Count != 1)
        throw std::runtime_error("Incompatible game/theater texture; source " + DescribeImage(src) + "; destination " +
                                 DescribeImage(dst));
    if (src.SampleDesc.Count > 1)
        context->ResolveSubresource(destination, 0, source, 0, DXGI_FORMAT_R8G8B8A8_UNORM);
    else
        context->CopyResource(destination, source);
}
} // namespace mmvr
