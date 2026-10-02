#pragma once
#include "first_person.h"
#include <algorithm>

namespace mmvr::body {
struct Vec {
    float x=0, y=0, z=0;
    Vec operator+(Vec b) const { return {x+b.x,y+b.y,z+b.z}; }
    Vec operator-(Vec b) const { return {x-b.x,y-b.y,z-b.z}; }
    Vec operator*(float s) const { return {x*s,y*s,z*s}; }
};
inline float Dot(Vec a, Vec b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline Vec Cross(Vec a, Vec b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
inline float Length(Vec v) { return std::sqrt(Dot(v,v)); }
inline Vec Unit(Vec v, Vec fallback={1,0,0}) { float n=Length(v); return n>1e-5f?v*(1/n):fallback; }
inline Vec Position(const Matrix& m) { return {m.m[3][0],m.m[3][1],m.m[3][2]}; }
inline Vec Transform(Vec p, const Matrix& m) {
    return {p.x*m.m[0][0]+p.y*m.m[1][0]+p.z*m.m[2][0]+m.m[3][0],
            p.x*m.m[0][1]+p.y*m.m[1][1]+p.z*m.m[2][1]+m.m[3][1],
            p.x*m.m[0][2]+p.y*m.m[1][2]+p.z*m.m[2][2]+m.m[3][2]};
}
inline bool Finite(const Matrix& m) {
    for(const auto& row:m.m) for(float v:row) if(!std::isfinite(v)) return false;
    return m.m[3][3]>.5f;
}
inline Vec Perpendicular(Vec direction, Vec preferred) {
    Vec p=preferred-direction*Dot(direction,preferred);
    if(Length(p)<1e-4f) {
        Vec alternate=std::abs(direction.y)<.8f?Vec{0,1,0}:Vec{0,0,1};
        p=alternate-direction*Dot(direction,alternate);
    }
    return Unit(p);
}
// Render-only attachment from the authored neck/torso to a level HMD neck.
// Shoulder origins and waist identify torso lean without using either hand.
// This preserves limb size and leg animation, but removes root bob/aim twist.
inline bool AnchorTorso(const Matrix (&bones)[BodyBoneCount], float nativeYaw,
                        const Matrix& targetNeck, Matrix& correction) {
    for (int i : {0, 3, 6, 7}) if (!Finite(bones[i])) return false;
    if (!Finite(targetNeck) || !std::isfinite(nativeYaw)) return false;
    const Vec neck = Position(bones[6]);
    Vec up = neck - Position(bones[7]);
    Vec across = Position(bones[3]) - Position(bones[0]);
    if (Length(up) < .01f || Length(across) < .01f) return false;
    up = Unit(up);
    across = across - up * Dot(across, up);
    if (Length(across) < .01f) return false;
    Vec right = Unit(across), forward = Cross(right, up);
    const Vec nativeForward{std::sin(nativeYaw), 0, std::cos(nativeYaw)};
    // Native left/right coordinates can be mirrored by a form/asset. Keep the
    // torso basis in the player's forward hemisphere, not a controller's yaw.
    if (Dot(forward, nativeForward) < 0) { right = right * -1; forward = forward * -1; }
    Matrix source = YawPose(0, neck.x, neck.y, neck.z);
    const Vec axes[]{right, up, forward};
    for (int i=0;i<3;++i) {
        source.m[i][0]=axes[i].x; source.m[i][1]=axes[i].y; source.m[i][2]=axes[i].z;
    }
    correction = Multiply(InversePose(source), targetNeck);
    return Finite(correction);
}
// Fit the authored joint-to-child axis to the solved segment. Stretch only
// along that axis: sleeve thickness stays unchanged while the native mesh spans the arm.
inline Matrix Segment(const Matrix& native, Vec child, Vec start, Vec end, Vec pole) {
    Matrix inverse;
    if(!InverseAffine(native,inverse)) return {};
    Vec localEnd=Transform(child,inverse);
    const float modelLength=Length(localEnd), worldLength=Length(end-start);
    if(modelLength<1e-4f || worldLength<1e-4f) return {};
    Vec a=Unit(localEnd), b=Perpendicular(a,{0,1,0}), c=Cross(a,b);
    Vec x=Unit(end-start), y=Perpendicular(x,pole), z=Cross(x,y);
    const float width=std::sqrt(native.m[0][0]*native.m[0][0]+native.m[0][1]*native.m[0][1]+native.m[0][2]*native.m[0][2]);
    const float along=worldLength/modelLength;
    Matrix out=YawPose(0);
    const float local[3][3]={{a.x,a.y,a.z},{b.x,b.y,b.z},{c.x,c.y,c.z}};
    const float world[3][3]={{x.x,x.y,x.z},{y.x,y.y,y.z},{z.x,z.y,z.z}};
    for(int r=0;r<3;++r) for(int col=0;col<3;++col) {
        out.m[r][col]=0;
        for(int k=0;k<3;++k) out.m[r][col]+=local[k][r]*world[k][col]*(k==0?along:width);
    }
    out.m[3][0]=start.x;out.m[3][1]=start.y;out.m[3][2]=start.z;
    return out;
}
struct Arm { Matrix upper{}, lower{}, wrist{}; bool valid=false; };
// Stateless two-bone IK, evaluated once per predicted display frame and shared
// by both eyes. The controller is authoritative; this never moves the hand.
inline Arm Solve(const Matrix& shoulder, const Matrix& elbow, const Matrix& wrist,
                 const Matrix& trackedWrist, Vec pole) {
    Arm out;
    if(!Finite(shoulder)||!Finite(elbow)||!Finite(wrist)||!Finite(trackedWrist)) return out;
    Vec s=Position(shoulder), e=Position(elbow), w=Position(wrist), target=Position(trackedWrist);
    float upper=Length(e-s), lower=Length(w-e), distance=Length(target-s);
    if(upper<.01f||lower<.01f||distance<.001f||distance>4*(upper+lower)) return out;
    // Accommodate real arm reach without detaching the wrist. Implausible
    // tracking displacements fail closed above instead of making giant spikes.
    float stretch=std::max(1.f,distance/(upper+lower)*1.001f);
    upper*=stretch;lower*=stretch;
    if(distance<=std::abs(upper-lower)+.001f) upper=lower=(upper+lower)*.5f;
    Vec direction=Unit(target-s), bend=Perpendicular(direction,pole);
    float along=std::clamp((upper*upper-lower*lower+distance*distance)/(2*distance),-upper,upper);
    Vec joint=s+direction*along+bend*std::sqrt(std::max(0.f,upper*upper-along*along));
    out.upper=Segment(shoulder,e,s,joint,pole);
    out.lower=Segment(elbow,w,joint,target,pole);
    out.wrist=trackedWrist;
    out.valid=Finite(out.upper)&&Finite(out.lower);
    return out;
}
} // namespace mmvr::body
