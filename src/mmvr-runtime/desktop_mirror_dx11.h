#pragma once
// Recording-only composition of submitted XR images. No native scene replay,
// readback, eye blending, or modification of gameplay color/depth attachments.
#include "transfer.h"
#include "projection.h"
#include "stereo_tracking.h"
#include "lens_stencil_dx11.h"
#include <unordered_map>

namespace mmvr {
class DesktopMirrorDx11 {
    template<class T> using Ptr = Microsoft::WRL::ComPtr<T>;
    struct Image { Ptr<ID3D11Texture2D> texture; Ptr<ID3D11ShaderResourceView> view; unsigned width=0,height=0; };
    std::unordered_map<XrSwapchain,Image> images;
    Image output;
    Ptr<ID3D11RenderTargetView> target;
    Ptr<ID3D11VertexShader> vertex;
    Ptr<ID3D11PixelShader> pixel;
    Ptr<ID3D11Buffer> constants;
    Ptr<ID3D11RasterizerState> raster;
    Ptr<ID3D11DepthStencilState> depth;
    Ptr<ID3D11BlendState> opaque,alpha,straightAlpha;
    Ptr<ID3D11SamplerState> sampler;
    bool visible=false;
    struct Values { float positions[4][4]; float uv[4][4]; };
    void Allocate(ID3D11Device* device,Image& image,unsigned w,unsigned h,bool renderTarget=false) {
        if(image.texture && image.width==w && image.height==h) return;
        image={};image.width=w;image.height=h;
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width=w;desc.Height=h;desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;
        desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.BindFlags=D3D11_BIND_SHADER_RESOURCE|(renderTarget?D3D11_BIND_RENDER_TARGET:0);
        LensDxCheck(device->CreateTexture2D(&desc,nullptr,&image.texture));
        LensDxCheck(device->CreateShaderResourceView(image.texture.Get(),nullptr,&image.view));
    }
    void Init(ID3D11Device* device) {
        if(vertex) return;
        // Resolve the same system compiler the native renderer already requires.
        static const auto compiler=[] {
            HMODULE dll=LoadLibraryW(L"d3dcompiler_47.dll");
            return dll?reinterpret_cast<LensCompiler>(GetProcAddress(dll,"D3DCompile")):nullptr;
        }();
        if(!compiler) throw std::runtime_error("Desktop mirror shader compiler unavailable");
        const char* shader=R"(
cbuffer Mirror:register(b0){float4 positions[4];float4 uv[4];};
struct V{float4 position:SV_POSITION;float2 tex:TEXCOORD0;};
V VSMain(uint i:SV_VertexID){V v;v.position=positions[i];v.tex=uv[i].xy;return v;}
Texture2D image:register(t0);SamplerState linearClamp:register(s0);
float4 PSMain(V v):SV_TARGET{return image.Sample(linearClamp,v.tex);})";
        Ptr<ID3DBlob> vs,ps,error;
        LensDxCheck(compiler(shader,strlen(shader),nullptr,nullptr,nullptr,"VSMain","vs_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&vs,&error));
        LensDxCheck(compiler(shader,strlen(shader),nullptr,nullptr,nullptr,"PSMain","ps_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&ps,&error));
        // Set vertex last so initialization can be retried safely after allocation failure.
        LensDxCheck(device->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,&pixel));
        D3D11_BUFFER_DESC cb{};cb.ByteWidth=sizeof(Values);cb.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
        LensDxCheck(device->CreateBuffer(&cb,nullptr,&constants));
        D3D11_RASTERIZER_DESC rs{};rs.FillMode=D3D11_FILL_SOLID;rs.CullMode=D3D11_CULL_NONE;rs.DepthClipEnable=TRUE;
        LensDxCheck(device->CreateRasterizerState(&rs,&raster));
        D3D11_DEPTH_STENCIL_DESC ds{};ds.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ZERO;ds.DepthFunc=D3D11_COMPARISON_ALWAYS;
        LensDxCheck(device->CreateDepthStencilState(&ds,&depth));
        D3D11_BLEND_DESC bs{};auto& b=bs.RenderTarget[0];b.RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
        // XR opaque layers ignore source alpha. Keep the desktop output opaque
        // as well, including games whose world clear leaves alpha at zero.
        b.BlendEnable=TRUE;b.SrcBlend=D3D11_BLEND_ONE;b.DestBlend=D3D11_BLEND_ZERO;b.BlendOp=D3D11_BLEND_OP_ADD;
        b.SrcBlendAlpha=D3D11_BLEND_ZERO;b.DestBlendAlpha=D3D11_BLEND_ONE;b.BlendOpAlpha=D3D11_BLEND_OP_ADD;
        LensDxCheck(device->CreateBlendState(&bs,&opaque));
        b.DestBlend=D3D11_BLEND_INV_SRC_ALPHA;
        LensDxCheck(device->CreateBlendState(&bs,&alpha));
        b.SrcBlend=D3D11_BLEND_SRC_ALPHA;
        LensDxCheck(device->CreateBlendState(&bs,&straightAlpha));
        D3D11_SAMPLER_DESC sd{};sd.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sd.AddressU=sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sd.MaxLOD=D3D11_FLOAT32_MAX;
        LensDxCheck(device->CreateSamplerState(&sd,&sampler));
        Allocate(device,output,1920,1080,true);
        LensDxCheck(device->CreateRenderTargetView(output.texture.Get(),nullptr,&target));
        LensDxCheck(device->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,&vertex));
    }
    void Draw(ID3D11DeviceContext* c,const XrSwapchainSubImage& sub,const Values& v,XrCompositionLayerFlags flags) {
        auto found=images.find(sub.swapchain);if(found==images.end()) return;
        c->UpdateSubresource(constants.Get(),0,nullptr,&v,0,0);
        auto* blend=(flags&XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT)?
            ((flags&XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT)?straightAlpha.Get():alpha.Get()):opaque.Get();
        c->OMSetBlendState(blend,nullptr,0xffffffff);
        auto* view=found->second.view.Get();c->PSSetShaderResources(0,1,&view);c->Draw(4,0);
    }
    Values Fullscreen(const XrSwapchainSubImage& sub,float cropX,float cropY) const {
        Values v{};auto found=images.find(sub.swapchain);if(found==images.end())return v;
        const auto& im=found->second;
        for(int i=0;i<4;++i){
            float x=(i&1)?1.f:-1.f,y=(i&2)?-1.f:1.f;
            v.positions[i][0]=x;v.positions[i][1]=y;v.positions[i][3]=1;
            v.uv[i][0]=(sub.imageRect.offset.x+sub.imageRect.extent.width*(.5f+x*.5f*cropX))/im.width;
            v.uv[i][1]=(sub.imageRect.offset.y+sub.imageRect.extent.height*(.5f-y*.5f*cropY))/im.height;
        }
        return v;
    }
  public:
    void BeginFrame(){visible=false;}
    uintptr_t View() const {return visible?reinterpret_cast<uintptr_t>(output.view.Get()):0;}
    void Forget(XrSwapchain chain){images.erase(chain);}
    void Capture(ID3D11DeviceContext* c,XrSwapchain chain,ID3D11Texture2D* source) {
        Ptr<ID3D11Device> device;c->GetDevice(&device);
        D3D11_TEXTURE2D_DESC desc{};source->GetDesc(&desc);
        auto& image=images[chain];Allocate(device.Get(),image,desc.Width,desc.Height);
        CopyGameImage(c,source,image.texture.Get());
    }
    void Compose(ID3D11DeviceContext* c,const XrView& eye,const XrPosef& head,XrSpace viewSpace,
                 const XrCompositionLayerBaseHeader* const* layers,unsigned count) {
        Ptr<ID3D11Device> device;c->GetDevice(&device);Init(device.Get());
        Ptr<ID3D11ShaderResourceView> oldResource;Ptr<ID3D11SamplerState> oldSampler;
        c->PSGetShaderResources(0,1,&oldResource);c->PSGetSamplers(0,1,&oldSampler);
        struct RestoreSamples {ID3D11DeviceContext* c;ID3D11ShaderResourceView* resource;ID3D11SamplerState* sampler;
            ~RestoreSamples(){c->PSSetShaderResources(0,1,&resource);c->PSSetSamplers(0,1,&sampler);}} restore{c,oldResource.Get(),oldSampler.Get()};
        // Restore render targets before sampled textures: the previous GUI SRV
        // can be our output itself, which cannot remain bound as an RTV.
        LensDxState saved(c);
        auto* rt=target.Get();c->OMSetRenderTargets(1,&rt,nullptr);
        const float black[4]{0,0,0,1};c->ClearRenderTargetView(rt,black);
        D3D11_VIEWPORT vp{0,0,1920,1080,0,1};c->RSSetViewports(1,&vp);c->RSSetState(raster.Get());
        c->OMSetDepthStencilState(depth.Get(),0);c->VSSetShader(vertex.Get(),nullptr,0);c->PSSetShader(pixel.Get(),nullptr,0);
        c->GSSetShader(nullptr,nullptr,0);c->IASetInputLayout(nullptr);c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        c->VSSetConstantBuffers(0,1,constants.GetAddressOf());c->PSSetSamplers(0,1,sampler.GetAddressOf());
        // Center-crop a real eye to a normal widescreen recording. Apply the same
        // crop to compositor geometry, so its HUD/text stays in headset positions.
        const float l=tanf(eye.fov.angleLeft),r=tanf(eye.fov.angleRight),u=tanf(eye.fov.angleUp),d=tanf(eye.fov.angleDown);
        const float aspect=(r-l)/(u-d),targetAspect=1920.f/1080.f;
        const float cropX=std::min(1.f,targetAspect/aspect),cropY=std::min(1.f,aspect/targetAspect);
        for(unsigned n=0;n<count;++n){
            const auto* layer=layers[n];
            if(layer->type==XR_TYPE_COMPOSITION_LAYER_PROJECTION){
                const auto& projection=*reinterpret_cast<const XrCompositionLayerProjection*>(layer);
                if(projection.viewCount)Draw(c,projection.views[0].subImage,Fullscreen(projection.views[0].subImage,cropX,cropY),layer->layerFlags);
            }else if(layer->type==XR_TYPE_COMPOSITION_LAYER_QUAD){
                const auto& quad=*reinterpret_cast<const XrCompositionLayerQuad*>(layer);
                if(quad.eyeVisibility==XR_EYE_VISIBILITY_RIGHT)continue;
                auto v=Fullscreen(quad.subImage,1,1);
                auto pose=PoseMatrix(quad.pose);
                if(quad.space==viewSpace)pose=Multiply(pose,PoseMatrix(head));
                auto relative=Multiply(pose,InversePose(PoseMatrix(eye.pose)));
                for(int i=0;i<4;++i){
                    const float p[4]{(i&1)?.5f*quad.size.width:-.5f*quad.size.width,(i&2)?-.5f*quad.size.height:.5f*quad.size.height,0,1};
                    float q[4]{};for(int j=0;j<4;++j)for(int k=0;k<4;++k)q[j]+=p[k]*relative.m[k][j];
                    const float w=-q[2];
                    v.positions[i][0]=(2*q[0]-(r+l)*w)/((r-l)*cropX);
                    v.positions[i][1]=(2*q[1]-(u+d)*w)/((u-d)*cropY);
                    v.positions[i][2]=0;v.positions[i][3]=w;
                }
                Draw(c,quad.subImage,v,layer->layerFlags);
            }
        }
        ID3D11ShaderResourceView* none=nullptr;c->PSSetShaderResources(0,1,&none);
        visible=true;
    }
};
} // namespace mmvr
