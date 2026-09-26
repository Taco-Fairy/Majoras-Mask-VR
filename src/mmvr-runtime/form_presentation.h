#pragma once
#include "first_person.h"
namespace mmvr {
// Both resources are native left/right fins; use rotations, never reflections.
// The fin's length is model X. Align it with the hand's finger axis (+Y),
// flat against the lower pinky side (opposite the thumb, +/-Z).
inline Matrix AttachedFin(const Matrix& hand, int side, const Settings& s) {
    if (!hand.m[3][3])
        return {};
    Matrix local{
        { { 0, side ? 1.f : -1.f, 0, 0 }, { -1, 0, 0, 0 }, { 0, 0, side ? 1.f : -1.f, 0 }, { -100, 100, 0, 1 } }
    };
    float scale = s.Get(Setting::ZoraFinSize);
    for (int a = 0; a < 3; ++a)
        for (int b = 0; b < 3; ++b)
            local.m[a][b] *= scale;
    // The central grip edge is (0,0,-100); end tips at Z=527 are not attachment points.
    local.m[3][2] =
        (side ? -1.f : 1.f) * (-100.f * scale + s.Get(Setting::ZoraFinOffset) * 4000.f / s.Get(Setting::HandScale));
    return Multiply(local, hand);
}
// Native expanded shield vertices are authored in a tilted forearm basis.
// Rebase their fitted plane and center into the thrown fin's X/Z model space.
inline Matrix ZoraShieldToFin() {
    return Matrix{ { { 0.87752632f, 0.20986917f, 0.43116410f, 0.00000000f },
                     { -0.30732218f, 0.93635042f, 0.16970850f, 0.00000000f },
                     { -0.36810411f, -0.28142997f, 0.88616959f, 0.00000000f },
                     { 559.55500253f, -221.73248702f, -656.31799650f, 1.00000000f } } };
}
inline Matrix AttachedZoraShield(const Matrix& hand, int side, const Settings& settings) {
    if (!hand.m[3][3]) return {};
    auto rebase = ZoraShieldToFin();
    rebase.m[3][2] += 286.5f;
    const float scale = settings.Get(Setting::ZoraFinSize);
    const float sign = side ? -1.f : 1.f;
    // Native hand vertices: fingers +Y, thumb +X, curled fingers toward -Z
    // on the left mesh (+Z on the mirrored right). The dorsal plane is +/-Z,
    // NOT -X (the pinky edge used by the unexpanded fin attachment).
    // Rebased shield plane is X/Z with normal +Y: map length to fingers and
    // width across the knuckles. Both bases have positive determinant.
    Matrix local{{{0, scale, 0, 0}, {0, 0, sign * scale, 0},
                  {sign * scale, 0, 0, 0}, {0, 335, 0, 1}}};
    const float clearance = settings.Get(Setting::ZoraFinOffset) * 4000.f / settings.Get(Setting::HandScale);
    // Mount at the dorsal surface. The fin's curved outer rim must not
    // push the entire mount away by its maximum thickness. Keep orientation.
    local.m[3][2] = sign * (126.f + clearance * .25f);
    return Multiply(Multiply(rebase, local), hand);
}
inline Matrix FormEffectAnchor(const Matrix& head) {
    if (!head.m[3][3])
        return {};
    // World upright; camera roll/pitch cannot flip the aura or turn the horizon.
    return YawPose(PoseYaw(head), head.m[3][0], head.m[3][1], head.m[3][2]);
}
} // namespace mmvr
