#pragma once
#include "lens_aperture.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <stdexcept>
#include <string>
#include <fstream>
#include <cstdlib>
#include <cstring>
namespace mmvr {
using LensCompiler = HRESULT(WINAPI*)(LPCVOID, SIZE_T, LPCSTR, const D3D_SHADER_MACRO*, ID3DInclude*, LPCSTR, LPCSTR,
                                      UINT, UINT, ID3DBlob**, ID3DBlob**);
inline void LensDxCheck(HRESULT r) {
    if (FAILED(r))
        throw std::runtime_error("Lens DX11 resource/render operation failed");
}
struct LensDxState {
    ID3D11DeviceContext* c;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> ps;
    Microsoft::WRL::ComPtr<ID3D11GeometryShader> gs;
    ID3D11ClassInstance *vc[256]{}, *pc[256]{}, *gc[256]{};
    UINT vn = 256, pn = 256, gn = 256;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> layout;
    Microsoft::WRL::ComPtr<ID3D11Buffer> vcb, pcb;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> raster;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depth;
    Microsoft::WRL::ComPtr<ID3D11BlendState> blend;
    ID3D11RenderTargetView* targets[8]{};
    ID3D11DepthStencilView* depthTarget = nullptr;
    D3D11_PRIMITIVE_TOPOLOGY topology;
    D3D11_VIEWPORT viewports[16]{};
    UINT count = 16;
    FLOAT factors[4]{};
    UINT sampleMask = 0, reference = 0;
    explicit LensDxState(ID3D11DeviceContext* context) : c(context) {
        c->VSGetShader(&vs, vc, &vn);
        c->PSGetShader(&ps, pc, &pn);
        c->GSGetShader(&gs, gc, &gn);
        c->IAGetInputLayout(&layout);
        c->IAGetPrimitiveTopology(&topology);
        c->VSGetConstantBuffers(0, 1, &vcb);
        c->PSGetConstantBuffers(0, 1, &pcb);
        c->RSGetState(&raster);
        c->RSGetViewports(&count, viewports);
        c->OMGetDepthStencilState(&depth, &reference);
        c->OMGetBlendState(&blend, factors, &sampleMask);
        c->OMGetRenderTargets(8, targets, &depthTarget);
    }
    ~LensDxState() {
        c->OMSetRenderTargets(8, targets, depthTarget);
        c->VSSetShader(vs.Get(), vc, vn);
        c->PSSetShader(ps.Get(), pc, pn);
        c->GSSetShader(gs.Get(), gc, gn);
        c->IASetInputLayout(layout.Get());
        c->IASetPrimitiveTopology(topology);
        c->VSSetConstantBuffers(0, 1, vcb.GetAddressOf());
        c->PSSetConstantBuffers(0, 1, pcb.GetAddressOf());
        c->RSSetState(raster.Get());
        c->RSSetViewports(count, viewports);
        c->OMSetDepthStencilState(depth.Get(), reference);
        c->OMSetBlendState(blend.Get(), factors, sampleMask);
        for (UINT i = 0; i < vn; ++i)
            if (vc[i])
                vc[i]->Release();
        for (UINT i = 0; i < pn; ++i)
            if (pc[i])
                pc[i]->Release();
        for (UINT i = 0; i < gn; ++i)
            if (gc[i])
                gc[i]->Release();
        for (auto* t : targets)
            if (t)
                t->Release();
        if (depthTarget)
            depthTarget->Release();
    }
};
inline void LensDepthDescription(D3D11_DEPTH_STENCIL_DESC& d, int mode) {
    d.StencilEnable = mode != 0;
    d.StencilReadMask = 1;
    d.StencilWriteMask = 0;
    d.FrontFace.StencilFailOp = d.FrontFace.StencilDepthFailOp = d.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
    d.FrontFace.StencilFunc = mode == 1 ? D3D11_COMPARISON_EQUAL : D3D11_COMPARISON_NOT_EQUAL;
    d.BackFace = d.FrontFace;
}
class LensStencilDx {
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> ps;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constants;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> raster;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> writeStencil;
    Microsoft::WRL::ComPtr<ID3D11BlendState> noColor, allColor;
    struct Values {
        float aperture[4];
        float color[4];
        float depth;
        int shape;
        float pad[2];
        float points[LensSegments][4];
    };

  public:
    void Init(ID3D11Device* d, LensCompiler compiler) {
        if (vs)
            return;
        struct Rollback {
            LensStencilDx& self;
            bool committed = false;
            ~Rollback() {
                if (!committed) {
                    self.vs.Reset(); self.ps.Reset(); self.constants.Reset(); self.raster.Reset();
                    self.writeStencil.Reset(); self.noColor.Reset(); self.allColor.Reset();
                }
            }
        } rollback{*this};
        const std::string shader = "#define MMVR_LENS_SEGMENTS " + std::to_string(LensSegments) + "\n" + R"(
cbuffer LensCB:register(b0){float4 aperture;float4 color;float depth;int shape;float2 pad;float4 points[MMVR_LENS_SEGMENTS];};
float4 VSMain(uint id:SV_VertexID):SV_POSITION {float2 p;if(shape==0){p=id==0?float2(-1,-1):id==1?float2(3,-1):float2(-1,3);}else{uint corner=id%3;uint index=(id/3+(corner==2?1:0))%MMVR_LENS_SEGMENTS;p=corner==0?aperture.xy:points[index].xy;p=float2(p.x*2-1,1-p.y*2);}return float4(p,depth,1);}
float4 PSMain():SV_TARGET{return color;})";
        Microsoft::WRL::ComPtr<ID3DBlob> vertex, pixel, error;
        LensDxCheck(compiler(shader.data(), shader.size(), nullptr, nullptr, nullptr, "VSMain", "vs_4_0",
                             D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &vertex, &error));
        LensDxCheck(compiler(shader.data(), shader.size(), nullptr, nullptr, nullptr, "PSMain", "ps_4_0",
                             D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &pixel, &error));
        LensDxCheck(d->CreateVertexShader(vertex->GetBufferPointer(), vertex->GetBufferSize(), nullptr, &vs));
        LensDxCheck(d->CreatePixelShader(pixel->GetBufferPointer(), pixel->GetBufferSize(), nullptr, &ps));
        D3D11_BUFFER_DESC cb{};
        cb.ByteWidth = sizeof(Values);
        cb.Usage = D3D11_USAGE_DEFAULT;
        cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        LensDxCheck(d->CreateBuffer(&cb, nullptr, &constants));
        D3D11_RASTERIZER_DESC rs{};
        rs.FillMode = D3D11_FILL_SOLID;
        rs.CullMode = D3D11_CULL_NONE;
        rs.DepthClipEnable = TRUE;
        rs.MultisampleEnable = TRUE;
        LensDxCheck(d->CreateRasterizerState(&rs, &raster));
        D3D11_BLEND_DESC bs{};
        LensDxCheck(d->CreateBlendState(&bs, &noColor));
        bs.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        LensDxCheck(d->CreateBlendState(&bs, &allColor));
        D3D11_DEPTH_STENCIL_DESC ds{};
        ds.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        ds.DepthFunc = D3D11_COMPARISON_ALWAYS;
        ds.StencilEnable = TRUE;
        ds.StencilReadMask = ds.StencilWriteMask = 1;
        ds.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
        ds.FrontFace.StencilFailOp = ds.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
        ds.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
        ds.BackFace = ds.FrontFace;
        LensDxCheck(d->CreateDepthStencilState(&ds, &writeStencil));
        rollback.committed = true;
    }
    void Draw(ID3D11DeviceContext* c, bool circle, float depth, const std::array<float, 4>& rgba) {
        auto a = GetLensAperture();
        Values v{ { a.cx, a.cy, a.rx, a.ry }, { rgba[0], rgba[1], rgba[2], rgba[3] }, depth, circle ? 1 : 0, { 0, 0 } };
        auto center = LensCenter();
        v.aperture[0] = center[0];
        v.aperture[1] = center[1];
        for (int i = 0; i < LensSegments; ++i) {
            auto p = LensBoundary(i);
            v.points[i][0] = p[0];
            v.points[i][1] = p[1];
        }
        c->UpdateSubresource(constants.Get(), 0, nullptr, &v, 0, 0);
        c->VSSetShader(vs.Get(), nullptr, 0);
        c->PSSetShader(ps.Get(), nullptr, 0);
        c->GSSetShader(nullptr, nullptr, 0);
        c->VSSetConstantBuffers(0, 1, constants.GetAddressOf());
        c->PSSetConstantBuffers(0, 1, constants.GetAddressOf());
        c->IASetInputLayout(nullptr);
        c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        c->Draw(circle ? LensSegments * 3 : 3, 0);
    }
    void Prepare(ID3D11Device* d, ID3D11DeviceContext* c, LensCompiler compiler, ID3D11DepthStencilView* target,
                 unsigned width, unsigned height) {
        LensDxState saved(c);
        Init(d, compiler);
        c->ClearDepthStencilView(target, D3D11_CLEAR_STENCIL, 1, 0);
        D3D11_VIEWPORT vp{ 0, 0, float(width), float(height), 0, 1 };
        c->RSSetViewports(1, &vp);
        c->RSSetState(raster.Get());
        c->OMSetBlendState(noColor.Get(), nullptr, 0xffffffff);
        c->OMSetDepthStencilState(writeStencil.Get(), 1);
        Draw(c, true, 0, { 0, 0, 0, 0 });
    }
    void Verify(ID3D11Device* d, ID3D11DeviceContext* c, LensCompiler compiler) {
        LensDxState saved(c);
        Init(d, compiler);
        bool colors = true, depth = true, state = true;
        unsigned cases = 0;
        const auto savedLens = binocularLens;
        const bool savedLensActive = binocularLensActive;
        struct RestoreLens {
            LensPolygon lens;
            bool active;
            ~RestoreLens() {
                binocularLens = lens;
                binocularLensActive = active;
            }
        } restoreLens{ savedLens, savedLensActive };
        for (int optical = 0; optical < 3; ++optical) {
            ClearBinocularLens();
            if (optical) {
                XrPosef head{ { 0, 0, 0, 1 }, { 0, 0, 0 } }, eye = head;
                eye.position.x = optical == 1 ? -.032f : .032f;
                eye.position.y = .012f;
                eye.orientation = { 0, std::sin(optical == 1 ? .06f : -.06f), 0, std::cos(.06f) };
                SetBinocularLens({ -.95f, .65f, .60f, -.9f }, head, eye);
            }
            for (auto size : { std::array<unsigned, 2>{ 64, 48 }, std::array<unsigned, 2>{ 97, 73 } })
                for (unsigned samples : { 1u, 2u }) {
                    Microsoft::WRL::ComPtr<ID3D11Texture2D> target, z, read, resolved;
                    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
                    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> dsv;
                    D3D11_TEXTURE2D_DESC td{};
                    td.Width = size[0];
                    td.Height = size[1];
                    td.MipLevels = td.ArraySize = 1;
                    td.SampleDesc.Count = samples;
                    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                    td.BindFlags = D3D11_BIND_RENDER_TARGET;
                    LensDxCheck(d->CreateTexture2D(&td, nullptr, &target));
                    LensDxCheck(d->CreateRenderTargetView(target.Get(), nullptr, &rtv));
                    td.SampleDesc.Count = 1;
                    td.BindFlags = 0;
                    LensDxCheck(d->CreateTexture2D(&td, nullptr, &resolved));
                    td.Usage = D3D11_USAGE_STAGING;
                    td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
                    LensDxCheck(d->CreateTexture2D(&td, nullptr, &read));
                    td.SampleDesc.Count = samples;
                    td.Usage = D3D11_USAGE_DEFAULT;
                    td.CPUAccessFlags = 0;
                    td.BindFlags = D3D11_BIND_DEPTH_STENCIL;
                    td.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
                    LensDxCheck(d->CreateTexture2D(&td, nullptr, &z));
                    LensDxCheck(d->CreateDepthStencilView(z.Get(), nullptr, &dsv));
                    auto readPixels = [&]() {
                        if (samples > 1) {
                            c->ResolveSubresource(resolved.Get(), 0, target.Get(), 0, DXGI_FORMAT_R8G8B8A8_UNORM);
                            c->CopyResource(read.Get(), resolved.Get());
                        } else
                            c->CopyResource(read.Get(), target.Get());
                    };
                    auto apply = [&](int mode) {
                        D3D11_DEPTH_STENCIL_DESC ds{};
                        ds.DepthEnable = TRUE;
                        ds.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
                        ds.DepthFunc = D3D11_COMPARISON_LESS;
                        LensDepthDescription(ds, mode);
                        Microsoft::WRL::ComPtr<ID3D11DepthStencilState> test;
                        LensDxCheck(d->CreateDepthStencilState(&ds, &test));
                        c->OMSetDepthStencilState(test.Get(), 1);
                    };
                    for (int mode : { 1, 2 })
                        for (int offset : { 0, 1 }) {
                            ++cases;
                            c->OMSetRenderTargets(1, rtv.GetAddressOf(), dsv.Get());
                            const float blue[4] = { 0, 0, 1, 1 };
                            c->ClearRenderTargetView(rtv.Get(), blue);
                            c->ClearDepthStencilView(dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);
                            c->RSSetState(raster.Get());
                            c->OMSetBlendState(allColor.Get(), nullptr, 0xffffffff);
                            float x = offset ? 5.f : 0, y = offset ? 3.f : 0;
                            D3D11_VIEWPORT vp{ x, y, size[0] - 2 * x, size[1] - 2 * y, 0, 1 };
                            c->RSSetViewports(1, &vp);
                            apply(0);
                            Prepare(d, c, compiler, dsv.Get(), size[0], size[1]);
                            D3D11_VIEWPORT actual{};
                            UINT n = 1;
                            c->RSGetViewports(&n, &actual);
                            state &= n == 1 && std::memcmp(&actual, &vp, sizeof(vp)) == 0;
                            apply(mode);
                            Draw(c, false, .25f, { 1, 0, 0, 1 });
                            auto examine = [&](bool after) {
                                readPixels();
                                D3D11_MAPPED_SUBRESOURCE mapped{};
                                LensDxCheck(c->Map(read.Get(), 0, D3D11_MAP_READ, 0, &mapped));
                                for (int gy = 1; gy < 10; ++gy)
                                    for (int gx = 1; gx < 10; ++gx) {
                                        unsigned px = unsigned(gx * .1f * size[0]), py = unsigned(gy * .1f * size[1]);
                                        const float ux = (px + .5f) / size[0], uy = (py + .5f) / size[1];
                                        // Ignore MSAA/polygon boundary samples, not interior mismatches.
                                        bool boundary = false;
                                        for (float dx : { -2.f, 2.f })
                                            for (float dy : { -2.f, 2.f })
                                                boundary |= LensInside(ux + dx / size[0], uy + dy / size[1]) !=
                                                            LensInside(ux, uy);
                                        if (boundary)
                                            continue;
                                        bool inside = LensInside((px + .5f) / size[0], (py + .5f) / size[1]),
                                             visible = mode == 1 ? inside : !inside;
                                        auto* v =
                                            static_cast<unsigned char*>(mapped.pData) + py * mapped.RowPitch + px * 4;
                                        bool ok = visible ? (v[0] == 255 && v[1] == 0 && v[2] == 0)
                                                          : (after ? (v[0] == 0 && v[1] == 255 && v[2] == 0)
                                                                   : (v[0] == 0 && v[1] == 0 && v[2] == 255));
                                        (after ? depth : colors) &= ok;
                                    }
                                c->Unmap(read.Get(), 0);
                            };
                            examine(false);
                            apply(0);
                            Draw(c, false, .5f, { 0, 1, 0, 1 });
                            examine(true);
                            Draw(c, false, .1f, { 1, 1, 1, 1 });
                            Prepare(d, c, compiler, dsv.Get(), size[0], size[1]);
                            apply(mode);
                            Draw(c, false, .2f, { 1, 0, 0, 1 });
                            readPixels();
                            D3D11_MAPPED_SUBRESOURCE mapped{};
                            LensDxCheck(c->Map(read.Get(), 0, D3D11_MAP_READ, 0, &mapped));
                            auto* v = static_cast<unsigned char*>(mapped.pData) + (size[1] / 2) * mapped.RowPitch +
                                      (size[0] / 2) * 4;
                            depth &= v[0] == 255 && v[1] == 255 && v[2] == 255;
                            c->Unmap(read.Get(), 0);
                            apply(0);
                        }
                }
        } // optical configurations
        HRESULT error = d->GetDeviceRemovedReason();
        std::ofstream out("native-lens-aperture.json");
        out << "{\"backend\":\"DX11\",\"msaa\":true,\"cases\":" << cases << ",\"color\":" << colors
            << ",\"depth\":" << depth << ",\"state\":" << state << ",\"error\":" << error
            << ",\"priorError\":0,\"session\":\""
            << (std::getenv("MMVR_SESSION_TOKEN") ? std::getenv("MMVR_SESSION_TOKEN") : "") << "\"}";
        out.close();
        if (!colors || !depth || !state || FAILED(error))
            throw std::runtime_error("Lens aperture render verification failed");
    }
};
} // namespace mmvr
