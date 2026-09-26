#pragma once
#include <array>
#include <cmath>
namespace mmvr {
// Count deliberate reversals, not render frames. Positions are raw tracking
// coordinates in metres, so locomotion/camera movement cannot shake for you.
class GrabShake {
    std::array<float,3> previous{}, direction{};
    double time=0, lastStroke=0;
    float distance=0;
    bool ready=false;
public:
    void Reset() { *this = {}; }
    bool Update(const std::array<float,3>& position, double now, bool enabled) {
        for(float v:position) if(!std::isfinite(v)) { Reset(); return false; }
        if(!enabled || !std::isfinite(now)) { Reset(); return false; }
        if(!ready) { ready=true; previous=position; time=lastStroke=now; return false; }
        double dt=now-time;
        if(dt==0) return false;
        if(dt<0 || dt>.1) { Reset(); return false; }
        std::array<float,3> delta{};float length=0,dot=0;
        for(int i=0;i<3;++i) { delta[i]=position[i]-previous[i];length+=delta[i]*delta[i]; }
        previous=position;time=now;length=std::sqrt(length);
        if(length>.25f) { Reset(); return false; } // Tracking jump/recenter.
        if(length/dt<.3f) { if(now-lastStroke>.45) {distance=0;direction={};} return false; }
        for(int i=0;i<3;++i) {delta[i]/=length;dot+=delta[i]*direction[i];}
        bool stroke=false;
        if(dot<-.35f) {
            stroke=distance>=.04f && now-lastStroke>=.08 && now-lastStroke<=.65;
            distance=0;lastStroke=now;
        }
        direction=delta;distance+=length;
        return stroke;
    }
};
}
