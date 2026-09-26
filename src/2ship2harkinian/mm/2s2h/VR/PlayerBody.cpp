#ifdef MMVR_ENABLE
#include "PlayerBody.h"
extern "C" {
#include "global.h"
}
namespace {
void Shift(Vec3f& p, const Vec3f& d) {
    p.x += d.x;
    p.y += d.y;
    p.z += d.z;
}
void ShiftQuad(ColliderQuad& q, const Vec3f& d) {
    for (auto& p : q.dim.quad)
        Shift(p, d);
    Collider_SetQuadMidpoints(&q.dim);
}
} // namespace
namespace mmvrgame {
void MovePlayerBody(PlayState*, Player* p, const std::array<float, 3>& destination) {
    Vec3f d{ destination[0] - p->actor.world.pos.x, destination[1] - p->actor.world.pos.y,
             destination[2] - p->actor.world.pos.z };
    p->actor.world.pos = { destination[0], destination[1], destination[2] };
    Shift(p->actor.prevPos, d);
    Shift(p->actor.focus.pos, d);
    for (auto& point : p->bodyPartsPos)
        Shift(point, d);
    Shift(p->leftHandWorld.pos, d);
    Shift(p->rightHandWorld.pos, d);
    Collider_UpdateCylinder(&p->actor, &p->cylinder);
    Collider_UpdateCylinder(&p->actor, &p->shieldCylinder);
    for (auto& q : p->meleeWeaponQuads)
        ShiftQuad(q, d);
    ShiftQuad(p->shieldQuad, d);
    // Native contacts/damage are still resolved by the native simulation tick.
}
} // namespace mmvrgame
#endif
