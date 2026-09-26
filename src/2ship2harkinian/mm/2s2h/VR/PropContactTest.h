#pragma once
#include "Carry.h"
// Exercise the same loaded resources and world-space query used by palm contact.
static void NativePropContactTest(std::ostream& log) {
    struct Case {
        int id, params;
        float scale, offset;
    };
    const Case cases[] = { { ACTOR_EN_ISHI, 1, .4f, 80 },       { ACTOR_EN_ISHI, 0, .1f, 0 },
                           { ACTOR_EN_ISHI, 8, .1f, 0 },        { ACTOR_OBJ_TSUBO, 0, .197f, 0 },
                           { ACTOR_OBJ_TSUBO, 128, .2955f, 0 }, { ACTOR_OBJ_TSUBO, 256, .197f, 0 },
                           { ACTOR_EN_MM, 0, .02f, 0 } };
    int loaded = 0, stable = 0, bounded = 0;
    float boulderSurface = 0;
    for (const auto& item : cases) {
        Actor actor{};
        actor.id = item.id;
        actor.params = item.params;
        actor.scale = { item.scale, item.scale, item.scale };
        actor.shape.yOffset = item.offset;
        const Vec3f query{ 100, 20, 0 };
        Vec3f surface{};
        bool inside = false;
        if (!mmvrgame::CarryPropSurface(&actor, query, surface, inside))
            continue;
        ++loaded;
        if (item.id == ACTOR_EN_ISHI && item.params == 1)
            boulderSurface = surface.x;
        const float distance = std::sqrt(SQ(query.x - surface.x) + SQ(query.y - surface.y) + SQ(query.z - surface.z));
        actor.world.pos = { 71, -45, 20 };
        actor.shape.rot = { 0x1800, 0x4000, -0x1000 };
        MtxF matrix;
        Matrix_Push();
        Matrix_SetTranslateRotateYXZ(71, -45, 20, &actor.shape.rot);
        Matrix_Get(&matrix);
        Matrix_Pop();
        auto transform = [&](const Vec3f& v) {
            return Vec3f{ matrix.xx * v.x + matrix.xy * v.y + matrix.xz * v.z + matrix.xw,
                          matrix.yx * v.x + matrix.yy * v.y + matrix.yz * v.z + matrix.yw,
                          matrix.zx * v.x + matrix.zy * v.y + matrix.zz * v.z + matrix.zw };
        };
        const auto movedQuery = transform(query), expected = transform(surface);
        Vec3f movedSurface;
        bool movedInside = false;
        bool found = mmvrgame::CarryPropSurface(&actor, movedQuery, movedSurface, movedInside);
        if (found && inside == movedInside && std::abs(movedSurface.x - expected.x) < .02f &&
            std::abs(movedSurface.y - expected.y) < .02f && std::abs(movedSurface.z - expected.z) < .02f)
            ++stable;
        Vec3f boundedSurface;
        bool boundedInside = false;
        if (mmvrgame::CarryPropSurface(&actor, movedQuery, boundedSurface, boundedInside, 6.f)) {
            const float d = std::sqrt(SQ(movedQuery.x - boundedSurface.x) + SQ(movedQuery.y - boundedSurface.y) +
                                      SQ(movedQuery.z - boundedSurface.z));
            if ((inside || distance < 6.f) == (boundedInside || d < 6.f))
                ++bounded;
        }
    }
    log << ",\"propSurfaces\":{\"loaded\":" << loaded << ",\"rotatedContacts\":" << stable
        << ",\"boundedEquivalent\":" << bounded << ",\"expected\":7,\"boulderSurfaceX\":" << boulderSurface << "}";
}
