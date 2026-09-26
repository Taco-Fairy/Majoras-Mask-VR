#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <openxr/openxr.h>
#include "theater.h"
#include "transfer.h"
#include "runtime.h"
#include "stereo.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <stdexcept>
using Microsoft::WRL::ComPtr;
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void hr(HRESULT r) { require(SUCCEEDED(r), "D3D11 operation failed"); }
bool close(float a, float b) { return std::abs(a-b)<0.0001f; }
inline DirectX::XMMATRIX AcceptedDesktopProjection(const XrPosef& eye,const XrFovf& fov,const XrPosef& origin, float n=1.f, float f=30000.f) {
    using namespace DirectX;
    auto q=eye.orientation;auto o=origin.orientation;
    auto originTransform=XMMatrixRotationQuaternion(XMVectorSet(o.x,o.y,o.z,o.w))*XMMatrixTranslation(origin.position.x,origin.position.y,origin.position.z);
    auto eyeTransform=XMMatrixRotationQuaternion(XMVectorSet(q.x,q.y,q.z,q.w))*XMMatrixTranslation(eye.position.x,eye.position.y,eye.position.z);
    auto relative=eyeTransform*XMMatrixInverse(nullptr,originTransform);
    relative.r[3]=XMVectorSetW(XMVectorScale(relative.r[3],40.f),1.f);
    auto view=XMMatrixInverse(nullptr,relative);
    const float l=std::tan(fov.angleLeft),r=std::tan(fov.angleRight),b=std::tan(fov.angleDown),t=std::tan(fov.angleUp);
    // Fast3D performs the GL [-1,1] to D3D [0,1] depth conversion itself.
    auto projection=XMMatrixSet(2/(r-l),0,0,0, 0,2/(t-b),0,0,
        (r+l)/(r-l),(t+b)/(t-b),-(f+n)/(f-n),-1, 0,0,-2*f*n/(f-n),0);
    return view*projection;
}
int main(int argc, char** argv) {
 try {
    require(mmvr::Axis(.1f,0)==0&&mmvr::Axis(1,0)==85&&mmvr::Axis(-1,0)==-85,"Stick deadzone/range");
    require(mmvr::Axis(1,1)<85&&mmvr::CButtons(1,-1)==5,"Diagonal normalization/C buttons");
    mmvr::PadLatch latch;latch.Update({0x8000,0,0,true});latch.Update({0,0,0,true});
    require(latch.Consume().buttons==0x8000&&latch.Consume().buttons==0,"Short press lost or repeated");
    latch.Update({0x2000,85,0,true});latch.Update({});require(!latch.Consume().active&&latch.Consume().buttons==0,"Focus loss stuck input");
    using namespace DirectX;
    XrPosef center{{0,0,0,1},{0,0,0}};
    XrFovf fov{-.7f,.9f,.8f,-.6f};
    auto projection=mmvr::EyeProjection(center,fov,center);
    auto left=XMVector4Transform(XMVectorSet(std::tan(fov.angleLeft)*10,0,-10,1),projection);
    require(close(XMVectorGetX(left)/XMVectorGetW(left),-1),"Asymmetric left frustum edge");
    auto nearZ=XMVector4Transform(XMVectorSet(0,0,-1,1),projection);
    auto farZ=XMVector4Transform(XMVectorSet(0,0,-30000,1),projection);
    require(close(XMVectorGetZ(nearZ)/XMVectorGetW(nearZ),-1)&&close(XMVectorGetZ(farZ)/XMVectorGetW(farZ),1),"GL depth projection");
    // Fog must match the native projection even when XR uses a different far plane.
    const float nativeNear=10.f,nativeFar=12800.f,scale=.5f;
    const float a=-scale*(nativeFar+nativeNear)/(nativeFar-nativeNear);
    const float b=-scale*2*nativeFar*nativeNear/(nativeFar-nativeNear);
    for(float distance:{10.f,100.f,1000.f,10000.f}) {
        const float expected=(-distance*a+b)/(distance*scale);
        require(close(mmvr::NativeFogDepth(a,b,scale,distance),expected),"Native fog depth changed by stereo projection");
    }
    auto preservedNear=XMVector4Transform(XMVectorSet(0,0,-nativeNear,1),mmvr::EyeProjection(center,fov,center,nativeNear));
    require(close(XMVectorGetZ(preservedNear)/XMVectorGetW(preservedNear),-1),"Native near plane not retained");
    for(int i=0;i<160;++i) {
        XMFLOAT4 q;XMStoreFloat4(&q,XMQuaternionRotationRollPitchYaw(.013f*i,.031f*i,-.007f*i));
        XrPosef sample{{q.x,q.y,q.z,q.w},{.03f*i,1.7f+.002f*i,-.02f*i}};
        XMStoreFloat4(&q,XMQuaternionRotationRollPitchYaw(0,-.021f*i,0));
        XrPosef recenter{{q.x,q.y,q.z,q.w},{-.01f*i,1.6f,.01f*i}};
        XMFLOAT4X4 accepted,shared;
        XMStoreFloat4x4(&accepted,AcceptedDesktopProjection(sample,fov,recenter,10.f));
        XMStoreFloat4x4(&shared,mmvr::EyeProjection(sample,fov,recenter,10.f));
        for(int row=0;row<4;++row)for(int col=0;col<4;++col)
            // Allow float rounding between a general SIMD inverse and a rigid-pose inverse.
            if(std::abs(accepted.m[row][col]-shared.m[row][col]) > .001f + .000005f*std::abs(accepted.m[row][col])) {
                std::cerr << "sample=" << i << " row=" << row << " col=" << col << " accepted=" << accepted.m[row][col] << " shared=" << shared.m[row][col] << " delta=" << std::abs(accepted.m[row][col]-shared.m[row][col]) << "\n";
                require(false,"Shared projection differs from accepted desktop transform");
            }
    }
    auto leftEye=center,rightEye=center;leftEye.position.x=-.032f;rightEye.position.x=.032f;
    auto lp=XMVector4Transform(XMVectorSet(0,0,-100,1),mmvr::EyeProjection(leftEye,fov,center));
    auto rp=XMVector4Transform(XMVectorSet(0,0,-100,1),mmvr::EyeProjection(rightEye,fov,center));
    require(XMVectorGetX(lp)/XMVectorGetW(lp)>XMVectorGetX(rp)/XMVectorGetW(rp),"Stereo eye disparity reversed");
    XrPosef head{{0,0,0,1},{1,1.7f,2}};
    auto screen=mmvr::TheaterPose(head);
    require(close(screen.position.x,1)&&close(screen.position.y,1.7f)&&close(screen.position.z,-1), "Screen forward/height incorrect");
    head.orientation={0,std::sqrt(.5f),0,std::sqrt(.5f)};
    screen=mmvr::TheaterPose(head);
    require(close(screen.position.x,-2)&&close(screen.position.z,2), "Yaw recenter incorrect");
    head.orientation={std::sin(.3f),0,0,std::cos(.3f)};
    screen=mmvr::TheaterPose(head);
    require(close(screen.orientation.x,0)&&close(screen.orientation.z,0)&&close(screen.position.y,1.7f), "Pitch tilted screen");
    ComPtr<ID3D11Device> device; ComPtr<ID3D11DeviceContext> context;
    hr(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context));
    if (argc > 1 && std::string(argv[1]) == "--missing-runtime") {
        _putenv_s("XR_RUNTIME_JSON", "mmvr-intentionally-missing-runtime.json");
        _putenv_s("MMVR_ENABLE", "1");
        D3D11_TEXTURE2D_DESC probeDesc{};
        probeDesc.Width=probeDesc.Height=16;probeDesc.ArraySize=probeDesc.MipLevels=probeDesc.SampleDesc.Count=1;
        probeDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;probeDesc.Usage=D3D11_USAGE_DEFAULT;
        ComPtr<ID3D11Texture2D> probe;hr(device->CreateTexture2D(&probeDesc,nullptr,&probe));
        std::ifstream before("mmvr.log",std::ios::binary|std::ios::ate);
        std::streamoff offset=before ? static_cast<std::streamoff>(before.tellg()) : 0; before.close();
        mmvr::SubmitTheater(device.Get(),context.Get(),probe.Get());
        mmvr::SubmitTheater(device.Get(),context.Get(),probe.Get());
        mmvr::Recenter();mmvr::Shutdown();
        std::ifstream log("mmvr.log",std::ios::binary);log.seekg(offset);
        std::ostringstream text;text<<log.rdbuf();
        require(text.str().find("XR disabled:")!=std::string::npos,"Missing-runtime failure was not logged");
        auto first=text.str().find("=== MMVR");
        require(first!=std::string::npos && text.str().find("=== MMVR",first+1)==std::string::npos,"Failed XR retried every game frame");
        std::cout<<"PASS: missing runtime returns to caller, logs reason, disables retries and cleans up safely\n";
        return 0;
    }
    std::vector<uint32_t> pixels(64*48);
    for(size_t i=0;i<pixels.size();++i) pixels[i]=0xff000000u | uint32_t(i*73);
    D3D11_TEXTURE2D_DESC desc{}; desc.Width=64;desc.Height=48;desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_RENDER_TARGET;
    D3D11_SUBRESOURCE_DATA data{pixels.data(),64*4,0};
    ComPtr<ID3D11Texture2D> source,destination,staging;
    hr(device->CreateTexture2D(&desc,&data,&source));
    for (auto destinationFormat : {DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_TYPELESS}) {
    destination.Reset(); staging.Reset(); desc.SampleDesc.Count=1;
    desc.Format=destinationFormat; desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_RENDER_TARGET;desc.CPUAccessFlags=0;
    hr(device->CreateTexture2D(&desc,nullptr,&destination));
    mmvr::CopyGameImage(context.Get(),source.Get(),destination.Get());
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    hr(device->CreateTexture2D(&desc,nullptr,&staging));
    context->CopyResource(staging.Get(),destination.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};hr(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped));
    bool equal=true;
    for(unsigned y=0;y<48;++y) for(unsigned x=0;x<64;++x)
      equal &= reinterpret_cast<uint32_t*>(static_cast<char*>(mapped.pData)+y*mapped.RowPitch)[x]==pixels[y*64+x];
    context->Unmap(staging.Get(),0); require(equal,"Game image copy changed pixels/orientation");
    // Exercise the MSAA path used when users change desktop anti-aliasing.
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_RENDER_TARGET;desc.CPUAccessFlags=0;desc.SampleDesc.Count=4;
    ComPtr<ID3D11Texture2D> msaa;hr(device->CreateTexture2D(&desc,nullptr,&msaa));
    ComPtr<ID3D11RenderTargetView> rtv;hr(device->CreateRenderTargetView(msaa.Get(),nullptr,&rtv));
    const float color[]={1,0,0,1};context->ClearRenderTargetView(rtv.Get(),color);
    mmvr::CopyGameImage(context.Get(),msaa.Get(),destination.Get());
    context->CopyResource(staging.Get(),destination.Get());
    hr(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped));
    equal=*static_cast<uint32_t*>(mapped.pData)==0xff0000ff;
    context->Unmap(staging.Get(),0);require(equal,"MSAA resolve failed");
    }
    desc.Width=32;desc.SampleDesc.Count=1;
    ComPtr<ID3D11Texture2D> wrong;hr(device->CreateTexture2D(&desc,nullptr,&wrong));
    bool rejected=false;try{mmvr::CopyGameImage(context.Get(),source.Get(),wrong.Get());}catch(const std::runtime_error&){rejected=true;}
    require(rejected,"Mismatched dimensions not rejected");
    desc.Width=64;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
    wrong.Reset();hr(device->CreateTexture2D(&desc,nullptr,&wrong));
    rejected=false;try{mmvr::CopyGameImage(context.Get(),source.Get(),wrong.Get());}catch(const std::runtime_error&){rejected=true;}
    require(rejected,"Incompatible channel order not rejected");
    D3D11_TEXTURE2D_DESC dd{};dd.Width=dd.Height=16;dd.ArraySize=dd.MipLevels=dd.SampleDesc.Count=1;
    dd.Format=DXGI_FORMAT_R32_TYPELESS;dd.Usage=D3D11_USAGE_DEFAULT;dd.BindFlags=D3D11_BIND_DEPTH_STENCIL;
    ComPtr<ID3D11Texture2D> depth,depthRead;hr(device->CreateTexture2D(&dd,nullptr,&depth));
    D3D11_DEPTH_STENCIL_VIEW_DESC vd{};vd.Format=DXGI_FORMAT_D32_FLOAT;vd.ViewDimension=D3D11_DSV_DIMENSION_TEXTURE2D;
    ComPtr<ID3D11DepthStencilView> dsv;hr(device->CreateDepthStencilView(depth.Get(),&vd,&dsv));
    context->ClearDepthStencilView(dsv.Get(),D3D11_CLEAR_DEPTH,.37f,0);
    mmvr::TextureBackup depthBackup;depthBackup.Save(context.Get(),depth.Get());
    context->ClearDepthStencilView(dsv.Get(),D3D11_CLEAR_DEPTH,1.f,0);depthBackup.Restore(context.Get(),depth.Get());
    dd.Usage=D3D11_USAGE_STAGING;dd.BindFlags=0;dd.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    hr(device->CreateTexture2D(&dd,nullptr,&depthRead));context->CopyResource(depthRead.Get(),depth.Get());
    D3D11_MAPPED_SUBRESOURCE depthMap{};hr(context->Map(depthRead.Get(),0,D3D11_MAP_READ,0,&depthMap));
    bool restored=close(*static_cast<float*>(depthMap.pData),.37f);context->Unmap(depthRead.Get(),0);
    require(restored,"Game depth did not survive extra-pass clear/restore");
    _putenv_s("MMVR_ENABLE", "0");mmvr::SubmitTheater(device.Get(),context.Get(),source.Get());mmvr::Recenter();mmvr::Shutdown();
    std::cout<<"PASS: level/yaw theater placement, byte-exact GPU copy, MSAA resolve, invalid-copy rejection, disabled runtime lifecycle\n";
    return 0;
 } catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
