#pragma once
#include <array>
#include <algorithm>
namespace mmvr {
// Actor-root interpolation contains room-scale translation already applied at
// XR cadence. Remove only its delayed component; native walking remains smooth.
struct RoomScaleInterpolation {
    std::array<float,3> accumulated{}, previous{}, current{};
    bool recorded=false;
    void Reset(){*this={};}
    void Moved(float x,float y,float z){accumulated[0]+=x;accumulated[1]+=y;accumulated[2]+=z;}
    void Record(){previous=recorded?current:accumulated;current=accumulated;recorded=true;}
    std::array<float,3> Correction(float alpha) const {
        std::array<float,3> result{};
        for(int i=0;i<3;++i)result[i]=(current[i]-previous[i])*(1.f-std::clamp(alpha,0.f,1.f));
        return result;
    }
};
}
