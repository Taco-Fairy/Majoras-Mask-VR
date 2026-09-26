#pragma once
#include <algorithm>
#include <array>
#include <cmath>
namespace mmvr {
using HandPoint = std::array<float, 3>;
inline HandPoint HandAdd(HandPoint a, HandPoint b) { for(int i=0;i<3;++i)a[i]+=b[i];return a; }
inline HandPoint HandSub(HandPoint a, HandPoint b) { for(int i=0;i<3;++i)a[i]-=b[i];return a; }
inline HandPoint HandScale(HandPoint a, float s) { for(auto& v:a)v*=s;return a; }
inline float HandDot(HandPoint a, HandPoint b) { return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]; }
inline float HandLength(HandPoint a) { return std::sqrt(HandDot(a,a)); }
struct HandContact { HandPoint normal{}; float depth=0; };
// Float precision is deliberate: the native Sphere16 query quantizes contact
// positions by a world unit, which is noticeable on a tracked hand.
inline HandPoint HandCross(HandPoint a, HandPoint b) {
    return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};
}
inline HandPoint HandClosestSegment(HandPoint p, HandPoint a, HandPoint b) {
    auto d=HandSub(b,a); float length2=HandDot(d,d);
    return HandAdd(a,HandScale(d,length2>1e-8f?std::clamp(HandDot(HandSub(p,a),d)/length2,0.f,1.f):0.f));
}
inline bool HandTriangleContact(HandPoint p, float radius, HandPoint a, HandPoint b, HandPoint c,
                                HandContact& hit) {
    auto ab=HandSub(b,a), ac=HandSub(c,a), normal=HandCross(ab,ac);
    float normal2=HandDot(normal,normal);
    if(normal2<1e-8f)return false; // Degenerate polygons have no collision surface.
    normal=HandScale(normal,1.f/std::sqrt(normal2));
    float planeDistance=HandDot(HandSub(p,a),normal);
    if(std::abs(planeDistance)>=radius)return false;
    auto closest=HandSub(p,HandScale(normal,planeDistance));
    // The projection lies inside only when it is on the inner side of all edges.
    bool inside=HandDot(HandCross(ab,HandSub(closest,a)),normal)>=0 &&
        HandDot(HandCross(HandSub(c,b),HandSub(closest,b)),normal)>=0 &&
        HandDot(HandCross(HandSub(a,c),HandSub(closest,c)),normal)>=0;
    if(!inside) {
        auto q0=HandClosestSegment(p,a,b),q1=HandClosestSegment(p,b,c),q2=HandClosestSegment(p,c,a);
        closest=q0;
        for(auto q:{q1,q2})if(HandLength(HandSub(p,q))<HandLength(HandSub(p,closest)))closest=q;
    }
    auto separation=HandSub(p,closest); float distance=HandLength(separation);
    if(distance>=radius)return false;
    hit.normal=distance>1e-5f?HandScale(separation,1.f/distance):HandScale(normal,planeDistance<0?-1.f:1.f);
    hit.depth=radius-distance;
    return true;
}
// Solid primitives used by native actor collision (pots, stones and pillars).
inline bool HandSphereContact(HandPoint p,float radius,HandPoint center,float solidRadius,HandContact& hit) {
    if(solidRadius<=0)return false;
    auto delta=HandSub(p,center);float distance=HandLength(delta);
    if(distance>=radius+solidRadius)return false;
    hit.normal=distance>1e-5f?HandScale(delta,1.f/distance):HandPoint{0,1,0};
    hit.depth=radius+solidRadius-distance;return true;
}
inline bool HandCylinderContact(HandPoint p,float radius,HandPoint base,float solidRadius,float height,HandContact& hit) {
    if(solidRadius<=0||height<=0)return false;
    auto d=HandSub(p,base);float radial=std::hypot(d[0],d[2]);
    float y=std::clamp(d[1],0.f,height);
    float scale=radial>solidRadius?solidRadius/radial:1.f;
    auto delta=HandSub(d,{d[0]*scale,y,d[2]*scale});float distance=HandLength(delta);
    if(distance>=radius)return false;
    if(distance>1e-5f){hit.normal=HandScale(delta,1.f/distance);hit.depth=radius-distance;return true;}
    // Inside the solid: choose its nearest exit surface, including end caps.
    float side=solidRadius-radial,bottom=d[1],top=height-d[1];
    if(bottom<=side&&bottom<=top){hit.normal={0,-1,0};hit.depth=radius+bottom;}
    else if(top<=side){hit.normal={0,1,0};hit.depth=radius+top;}
    else {hit.normal=radial>1e-5f?HandPoint{d[0]/radial,0,d[2]/radial}:HandPoint{1,0,0};hit.depth=radius+side;}
    return true;
}
// World units (40 per metre). Query returns a two-sided sphere contact. The
// inflated substeps conservatively cover the whole segment, including thin walls.
class HandCollision {
    HandPoint position{};
    bool valid=false;
public:
    static constexpr float RecoveryDistance = .3048f * 40.f;
    bool recovered=false, blocked=false;
    void Reset() { valid=false; recovered=blocked=false; }
    template<class Query> HandPoint Update(HandPoint target, HandPoint controller, float radius, Query&& query) {
        recovered=blocked=false;
        for(float v:target) if(!std::isfinite(v)) {Reset();return target;}
        if(!valid) { position=target;valid=true; }
        auto depenetrate=[&](HandPoint& point) {
            for(int i=0;i<8;++i) {
                HandContact hit;
                if(!query(point,radius,hit))return;
                point=HandAdd(point,HandScale(hit.normal,std::max(.01f,hit.depth+.01f)));
                blocked=true;
            }
        };
        // Moving platforms can approach an otherwise stationary hand.
        depenetrate(position);
        HandPoint delta=HandSub(target,position);
        int steps=std::clamp(int(std::ceil(HandLength(delta)/std::max(.25f,radius*.5f))),1,64);
        HandPoint step=HandScale(delta,1.f/steps);
        for(int i=0;i<steps;++i) {
            auto next=HandAdd(position,step);
            auto motion=step;
            for(int plane=0;plane<3;++plane) {
                HandContact hit;
                const float margin=HandLength(motion)*.5f;
                auto middle=HandAdd(position,HandScale(motion,.5f));
                if(!query(middle,radius+margin,hit))break;
                blocked=true;
                // Find the last free center before contact. Keeping contact skin
                // outside geometry avoids alternating penetration on stationary poses.
                float low=0,high=1;
                for(int search=0;search<9;++search) {
                    float t=(low+high)*.5f;HandContact probe;
                    auto q=HandAdd(position,HandScale(motion,t));
                    if(query(q,radius+.01f,probe))high=t;else low=t;
                }
                position=HandAdd(position,HandScale(motion,low));
                auto remaining=HandSub(next,position);
                float inward=HandDot(remaining,hit.normal);
                if(inward<0)remaining=HandSub(remaining,HandScale(hit.normal,inward));
                motion=remaining;next=HandAdd(position,motion);
            }
            HandContact hit;
            if(!query(next,radius,hit))position=next;
        }
        if(HandLength(HandSub(position,controller))>RecoveryDistance) {
            // Explicit user-requested stuck-hand escape. A controller inside a
            // wall is moved to its nearest free surface instead of displaying
            // an embedded hand. Combat history is invalidated by the caller.
            position=controller;recovered=true;depenetrate(position);
        }
        return position;
    }
};
} // namespace mmvr
