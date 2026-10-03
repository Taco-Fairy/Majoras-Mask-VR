#ifdef MMVR_ENABLE
#include "PlayerBody.h"
#include "body_collision.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include "visibility.h"
extern "C" {
#include "global.h"
}
namespace {
// Additional AC registrations caused by stereo-wide actor updates. Keep all
// damage targets; choosing only nearby ACs would discard distant arrow hits.
// Fixed storage avoids allocation in simulation and preserves native ABI.
std::array<Collider*,512> extraAC{};
CollisionCheckContext* extraContext=nullptr;
int extraACCount=0;
int ExtraACSize() { return std::clamp(extraACCount,0,int(extraAC.size())); }
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
// Wide VR visibility can fill native OC before nearby props update. On overflow
// retain the closest collision surfaces, not the first actors in category order.
// Bounds rank only; native OC still owns exact overlap, flags and displacement.
float OCDistanceSquared(const Collider* col, const Vec3f& point, const Actor* player) {
    if (!col) return std::numeric_limits<float>::infinity();
    if (col->actor == player) return -1.f; // Never displace the registered player.
    if (!(col->ocFlags1 & OC1_ON)) return std::numeric_limits<float>::infinity();
    Vec3f low{}, high{};
    bool bounded = false;
    auto include = [&](float x, float y, float z, float radius) {
        radius = std::max(0.f, radius);
        if (!bounded) { low = {x-radius,y-radius,z-radius}; high = {x+radius,y+radius,z+radius}; }
        else {
            low.x=std::min(low.x,x-radius); low.y=std::min(low.y,y-radius); low.z=std::min(low.z,z-radius);
            high.x=std::max(high.x,x+radius); high.y=std::max(high.y,y+radius); high.z=std::max(high.z,z+radius);
        }
        bounded = true;
    };
    auto sphere = [&](const Sphere16& s) { include(s.center.x,s.center.y,s.center.z,s.radius); };
    switch (col->shape) {
        case COLSHAPE_CYLINDER: {
            const auto& d = reinterpret_cast<const ColliderCylinder*>(col)->dim;
            const float y = float(d.pos.y)+d.yShift;
            include(d.pos.x,y,d.pos.z,d.radius);
            low.y=std::min(y,y+d.height); high.y=std::max(y,y+d.height);
            break;
        }
        case COLSHAPE_JNTSPH: {
            const auto& c = *reinterpret_cast<const ColliderJntSph*>(col);
            if (c.elements) for (int i=0;i<c.count;++i) sphere(c.elements[i].dim.worldSphere);
            break;
        }
        case COLSHAPE_SPHERE: sphere(reinterpret_cast<const ColliderSphere*>(col)->dim.worldSphere); break;
        case COLSHAPE_TRIS: {
            const auto& c = *reinterpret_cast<const ColliderTris*>(col);
            if (c.elements) for (int i=0;i<c.count;++i)
                for (const auto& v : c.elements[i].dim.vtx) include(v.x,v.y,v.z,0.f);
            break;
        }
        case COLSHAPE_QUAD:
            for (const auto& v : reinterpret_cast<const ColliderQuad*>(col)->dim.quad) include(v.x,v.y,v.z,0.f);
            break;
        default: break;
    }
    // Unknown/invalid geometry is kept conservatively, not ranked by actor origin.
    if (!bounded) return 0.f;
    const float dx=std::max({low.x-point.x,0.f,point.x-high.x});
    const float dy=std::max({low.y-point.y,0.f,point.y-high.y});
    const float dz=std::max({low.z-point.z,0.f,point.z-high.z});
    const float distance=dx*dx+dy*dy+dz*dz;
    return std::isfinite(distance) ? distance : 0.f;
}
} // namespace
extern "C" int MMVR_ChooseOCOverflowSlot(PlayState* play, CollisionCheckContext* context, Collider* incoming) {
    if (!MMVR_WideVisibility() || !play || !context || !incoming || (context->sacFlags & SAC_ON) ||
        context->colOCCount != ARRAY_COUNT(context->colOC)) return -1;
    auto* player=GET_PLAYER(play);
    if (!player) return -1;
    Vec3f point=player->actor.world.pos;
    point.y += player->cylinder.dim.yShift + player->cylinder.dim.height*.5f;
    const float distance=OCDistanceSquared(incoming,point,&player->actor);
    float farthest=distance;
    int slot=-1;
    for (int i=0;i<context->colOCCount;++i) {
        if (context->colOC[i]==incoming) return i;
        const float candidate=OCDistanceSquared(context->colOC[i],point,&player->actor);
        if (candidate>farthest) { farthest=candidate; slot=i; }
    }
    return slot;
}
extern "C" void MMVR_ClearACOverflow(CollisionCheckContext* context) {
    if (context==extraContext) { extraAC.fill(nullptr); extraACCount=0; extraContext=nullptr; }
}
extern "C" int MMVR_AddACOverflow(CollisionCheckContext* context, Collider* collider) {
    if (!MMVR_WideVisibility() || !context || !collider || (context->sacFlags&SAC_ON)) return -1;
    if (extraContext!=context) {extraAC.fill(nullptr);extraACCount=0;extraContext=context;}
    if(extraACCount<0 || extraACCount>int(extraAC.size())) return -1;
    for(int i=0;i<extraACCount;++i) if(extraAC[i]==collider) return context->colACCount+i;
    if(extraACCount==int(extraAC.size())) return -1;
    extraAC[extraACCount++]=collider;
    return context->colACCount+extraACCount-1;
}
extern "C" int MMVR_CollisionACCount(CollisionCheckContext* context) {
    if(!context) return 0;
    return std::clamp(context->colACCount,0,int(ARRAY_COUNT(context->colAC)))+
        (context==extraContext && !(context->sacFlags&SAC_ON)?ExtraACSize():0);
}
extern "C" Collider* MMVR_CollisionACAt(CollisionCheckContext* context, int index) {
    if(!context || index<0) return nullptr;
    const int nativeCount=std::clamp(context->colACCount,0,int(ARRAY_COUNT(context->colAC)));
    if(index<nativeCount) return context->colAC[index];
    index-=nativeCount;
    return context==extraContext && !(context->sacFlags&SAC_ON) && index<ExtraACSize()?extraAC[index]:nullptr;
}
extern "C" void MMVR_RemoveACOverflow(CollisionCheckContext* context, Collider* collider) {
    if(context!=extraContext) return;
    int count=0;
    for(int i=0;i<ExtraACSize();++i) if(extraAC[i]!=collider) extraAC[count++]=extraAC[i];
    std::fill(extraAC.begin()+count,extraAC.end(),nullptr);extraACCount=count;
}
extern "C" void MMVR_PrioritizeAC(CollisionCheckContext* context, Collider* collider) {
    if(context!=extraContext || !context || context->colACCount<=0 ||
        context->colACCount>int(ARRAY_COUNT(context->colAC))) return;
    for(int i=0;i<ExtraACSize();++i) if(extraAC[i]==collider) {
        // Existing shield code moves its native slot ahead of the body. Retain
        // the displaced target in the overflow slot so nothing is dropped.
        std::swap(extraAC[i],context->colAC[context->colACCount-1]);return;
    }
}
namespace mmvrgame {
bool RoomScalePropBlocked(PlayState* play, Player* p, const std::array<float, 3>& destination) {
    const auto& body = p->cylinder;
    if (!(body.base.ocFlags1 & OC1_ON) || (body.base.ocFlags1 & OC1_NO_PUSH) ||
        !(body.elem.ocElemFlags & OCELEM_ON))
        return false;
    const auto& context = play->colChkCtx;
    for (int i = 0; i < std::clamp(context.colOCCount, 0, int(ARRAY_COUNT(context.colOC))); ++i) {
        auto* collider = context.colOC[i];
        auto* actor = collider ? collider->actor : nullptr;
        if (!actor || !actor->update || actor == &p->actor || actor == p->heldActor || actor == p->rideActor ||
            actor->parent == &p->actor || collider->shape != COLSHAPE_CYLINDER ||
            !(collider->ocFlags1 & OC1_ON) || (collider->ocFlags1 & OC1_NO_PUSH) ||
            CollisionCheck_Incompatible(&p->cylinder.base, collider))
            continue;
        const auto& solid = *reinterpret_cast<ColliderCylinder*>(collider);
        if (!(solid.elem.ocElemFlags & OCELEM_ON)) continue;
        // Room-scale translation happens between simulation ticks. Native OC
        // displacement alone cannot constrain it before the floor/hole probe.
        // Query live registered solids without changing hit flags or ownership.
        if (mmvr::BodyCylinderSweep(p->actor.world.pos.x, p->actor.world.pos.y + body.dim.yShift,
                p->actor.world.pos.z, destination[0], destination[1] + body.dim.yShift, destination[2],
                body.dim.radius, body.dim.height, solid.dim.pos.x, float(solid.dim.pos.y) + solid.dim.yShift,
                solid.dim.pos.z, solid.dim.radius, solid.dim.height))
            return true;
    }
    return false;
}
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

#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
#include "NativeStateFields.h"
extern "C" void MMVR_VisitVrCollisionQueueState(MMVR_StateSink* sink) {
    mmvrgame::NativeStateField(sink,"vr/collision/extraContext",extraContext);
    mmvrgame::NativeStateField(sink,"vr/collision/extraACCount",extraACCount);
    mmvrgame::NativeStateField(sink,"vr/collision/extraAC",extraAC);
    for(auto& collider:extraAC) sink->pointer(sink->context,&collider,0,"vr/collision/extraAC.collider");
}
#endif
