#pragma once
// Opt-in diagnostic only. No readbacks occur during normal play.
#include "culling_audit.h"
#include "packet_profile.h"
#include "transfer.h"
#include <functional>
#include <vector>
#include <algorithm>
#include <cstring>
namespace mmvr {
inline std::vector<unsigned char> NativeBoundsReadPixelsDX11(ID3D11Device* device, ID3D11DeviceContext* context,
                                                             ID3D11Texture2D* source) {
    D3D11_TEXTURE2D_DESC desc{};
    source->GetDesc(&desc);
    if (!ColorFamily(desc.Format) || !desc.Width || !desc.Height || desc.Width > 8192 || desc.Height > 8192 ||
        size_t(desc.Width) * desc.Height > 16 * 1024 * 1024 || desc.ArraySize != 1 || desc.MipLevels != 1)
        throw std::runtime_error("Invalid DX11 bounds comparison image");
    Microsoft::WRL::ComPtr<ID3D11Texture2D> resolved, staging;
    if (desc.SampleDesc.Count > 1) {
        auto single = desc;
        single.SampleDesc = { 1, 0 };
        single.Usage = D3D11_USAGE_DEFAULT;
        single.CPUAccessFlags = 0;
        single.MiscFlags = 0;
        if (FAILED(device->CreateTexture2D(&single, nullptr, &resolved)))
            throw std::runtime_error("Cannot resolve DX11 bounds image");
        context->ResolveSubresource(resolved.Get(), 0, source, 0, DXGI_FORMAT_R8G8B8A8_UNORM);
        source = resolved.Get();
        desc = single;
    }
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;
    if (FAILED(device->CreateTexture2D(&desc, nullptr, &staging)))
        throw std::runtime_error("Cannot allocate DX11 bounds readback");
    context->CopyResource(staging.Get(), source);
    std::vector<unsigned char> pixels(size_t(desc.Width) * desc.Height * 4);
    D3D11_MAPPED_SUBRESOURCE map{};
    if (FAILED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &map)))
        throw std::runtime_error("Cannot read DX11 bounds image");
    for (unsigned y = 0; y < desc.Height; ++y)
        std::memcpy(pixels.data() + size_t(y) * desc.Width * 4,
                    static_cast<unsigned char*>(map.pData) + size_t(y) * map.RowPitch, size_t(desc.Width) * 4);
    context->Unmap(staging.Get(), 0);
    return pixels;
}
inline void NativeBoundsPixelComparisonDX11(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11Texture2D* source,
                                            const std::function<void(bool)>& draw, const std::function<void()>& prepare,
                                            const std::string& label, int view) {
    prepare();
    cullingReplayReference = true;
    draw(false);
    auto reference = NativeBoundsReadPixelsDX11(device, context, source);
    prepare();
    cullingReplayReference = false;
    const auto packetsBefore = cullingAudit.skippedPackets;
    const auto verticesBefore = cullingAudit.skippedPacketVertices;
    const auto commandsBefore = cullingAudit.skippedPacketCommands;
    const auto before = cullingAudit.skippedLists;
    draw(false);
    const auto skipped = cullingAudit.skippedLists - before;
    auto candidate = NativeBoundsReadPixelsDX11(device, context, source);
    if (reference.size() != candidate.size())
        throw std::runtime_error("DX11 bounds image size changed");
    size_t nonUniform = 0;
    for (size_t i = 4; i < reference.size(); i += 4)
        nonUniform +=
            reference[i] != reference[0] || reference[i + 1] != reference[1] || reference[i + 2] != reference[2];
    size_t rgb = 0, alpha = 0;
    unsigned maxDelta = 0;
    for (size_t i = 0; i < reference.size(); ++i) {
        const unsigned delta = unsigned(std::abs(int(reference[i]) - int(candidate[i])));
        maxDelta = std::max(maxDelta, delta);
        if (delta) {
            if (i % 4 == 3)
                ++alpha;
            else
                ++rgb;
        }
    }
    D3D11_TEXTURE2D_DESC desc{};
    source->GetDesc(&desc);
    static std::ofstream log = []() {
        std::ofstream out("native-bounds-pixels.log");
        const char* token = std::getenv("MMVR_SESSION_TOKEN");
        out << "session " << (token ? token : "") << "\n";
        return out;
    }();
    log << label << " eye=" << view << " width=" << desc.Width << " height=" << desc.Height
        << " bytes=" << reference.size() << " rgbDifferences=" << rgb << " alphaDifferences=" << alpha
        << " nonUniformPixels=" << nonUniform << " maxDelta=" << maxDelta << " skippedLists=" << skipped
        << " skippedPackets=" << (cullingAudit.skippedPackets - packetsBefore)
        << " skippedPacketVertices=" << (cullingAudit.skippedPacketVertices - verticesBefore) << " skippedPacketCommands=" << (cullingAudit.skippedPacketCommands - commandsBefore) << "\n"
        << std::flush;
    static unsigned failures = 0;
    if ((rgb || alpha) && failures++ < 2) {
        auto save = [&](const std::vector<unsigned char>& bytes, const char* suffix) {
            std::ofstream file(std::string("native-bounds-mismatch-") + std::to_string(failures) + suffix + ".ppm",
                               std::ios::binary);
            file << "P6\n" << desc.Width << " " << desc.Height << "\n255\n";
            for (size_t i = 0; i < bytes.size(); i += 4)
                file.write(reinterpret_cast<const char*>(bytes.data() + i), 3);
        };
        save(reference, "-reference");
        save(candidate, "-candidate");
    }
    ProfilePacketCulling(
        label, view, prepare, [&] { draw(false); },
        [&] { return NativeBoundsReadPixelsDX11(device, context, source); });
}
} // namespace mmvr
