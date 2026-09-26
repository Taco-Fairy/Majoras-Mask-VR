#include "desktop_mirror_dx11.h"
#include <iostream>
#include <vector>
using namespace mmvr;
using Microsoft::WRL::ComPtr;
static void Require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
int main(){try{
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    LensDxCheck(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context));
    auto texture=[&](unsigned rgba){
        std::vector<unsigned> pixels(64,rgba);D3D11_TEXTURE2D_DESC d{};d.Width=d.Height=8;d.ArraySize=d.MipLevels=d.SampleDesc.Count=1;
        d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA data{pixels.data(),32,0};ComPtr<ID3D11Texture2D> out;LensDxCheck(device->CreateTexture2D(&d,&data,&out));return out;
    };
    auto blue=texture(0x00ff0000),red=texture(0xff0000ff),halfRed=texture(0x80000080);
    const auto eyeChain=reinterpret_cast<XrSwapchain>(1),hudChain=reinterpret_cast<XrSwapchain>(2);
    const auto viewSpace=reinterpret_cast<XrSpace>(1),localSpace=reinterpret_cast<XrSpace>(2);
    DesktopMirrorDx11 mirror;mirror.Capture(context.Get(),eyeChain,blue.Get());mirror.Capture(context.Get(),hudChain,red.Get());
    XrView eye{XR_TYPE_VIEW};eye.pose.orientation.w=1;eye.fov={-.78539816f,.78539816f,.78539816f,-.78539816f};
    XrPosef head{{0,0,0,1},{0,0,0}};
    XrCompositionLayerProjectionView pv{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};pv.pose=eye.pose;pv.fov=eye.fov;pv.subImage={eyeChain,{{0,0},{8,8}},0};
    XrCompositionLayerProjection projection{XR_TYPE_COMPOSITION_LAYER_PROJECTION};projection.space=localSpace;projection.viewCount=1;projection.views=&pv;
    XrCompositionLayerQuad hud{XR_TYPE_COMPOSITION_LAYER_QUAD};hud.space=viewSpace;hud.pose={{0,0,0,1},{0,0,-1}};hud.size={1,1};hud.subImage={hudChain,{{0,0},{8,8}},0};
    hud.layerFlags=XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
    const XrCompositionLayerBaseHeader* layers[]{reinterpret_cast<XrCompositionLayerBaseHeader*>(&projection),reinterpret_cast<XrCompositionLayerBaseHeader*>(&hud)};
    auto sample=[&](int x,int y){
        auto* srv=reinterpret_cast<ID3D11ShaderResourceView*>(mirror.View());Require(srv,"mirror missing");
        ComPtr<ID3D11Resource> resource;srv->GetResource(&resource);ComPtr<ID3D11Texture2D> image;LensDxCheck(resource.As(&image));
        D3D11_TEXTURE2D_DESC d{};image->GetDesc(&d);Require(d.Width==1920&&d.Height==1080,"not widescreen");
        d.Usage=D3D11_USAGE_STAGING;d.BindFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        ComPtr<ID3D11Texture2D> staging;LensDxCheck(device->CreateTexture2D(&d,nullptr,&staging));context->CopyResource(staging.Get(),image.Get());
        D3D11_MAPPED_SUBRESOURCE map{};LensDxCheck(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&map));
        unsigned value=*reinterpret_cast<unsigned*>(static_cast<char*>(map.pData)+y*map.RowPitch+x*4);context->Unmap(staging.Get(),0);return value;
    };
    D3D11_VIEWPORT original{3,4,55,66,0,1};context->RSSetViewports(1,&original);context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    mirror.Compose(context.Get(),eye,head,viewSpace,layers,2);
    Require(sample(960,540)==0xff0000ff,"HUD not centered over eye");Require(sample(50,540)==0xffff0000,"eye missing or stretched HUD");
    D3D11_VIEWPORT after{};UINT count=1;context->RSGetViewports(&count,&after);D3D11_PRIMITIVE_TOPOLOGY topology;context->IAGetPrimitiveTopology(&topology);
    Require(after.TopLeftX==3&&after.Width==55&&topology==D3D11_PRIMITIVE_TOPOLOGY_LINELIST,"graphics state leaked");
    mirror.Capture(context.Get(),hudChain,halfRed.Get());mirror.Compose(context.Get(),eye,head,viewSpace,layers,2);
    unsigned blend=sample(960,540);Require((blend&255)==128&&((blend>>16)&255)==127,"premultiplied alpha mismatch");
    head.orientation={0,.70710678f,0,.70710678f};eye.pose=head;
    mirror.Compose(context.Get(),eye,head,viewSpace,layers,2);Require(sample(960,540)==blend,"head-locked HUD drifts on yaw");
    hud.space=localSpace;mirror.Compose(context.Get(),eye,head,viewSpace,layers,2);Require(sample(960,540)==0xffff0000,"world quad failed to follow perspective");
    auto* previousMirror=reinterpret_cast<ID3D11ShaderResourceView*>(mirror.View());
    context->PSSetShaderResources(0,1,&previousMirror);
    mirror.Compose(context.Get(),eye,head,viewSpace,layers,2);
    ComPtr<ID3D11ShaderResourceView> restoredResource;context->PSGetShaderResources(0,1,&restoredResource);
    Require(restoredResource.Get()==previousMirror,"previous mirror GUI texture lost on RTV restoration");
    mirror.BeginFrame();Require(!mirror.View(),"stale mirror exposed");
    std::cout<<"Desktop mirror: widescreen eye, HUD projection, alpha, head/world spaces and render-state preservation passed\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
