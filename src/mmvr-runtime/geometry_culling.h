#pragma once
#include <array>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <openxr/openxr.h>
namespace mmvr {
// Geometry only: a packet never includes a matrix/material/texture/state command.
// Loaded positions are recomputed by the normal interpreter for every eye/frame.
struct TrianglePacket {
    uint16_t commands = 0;
    uint8_t count = 0;
    std::array<uint8_t, 68> vertices{};
};
template <class Command> TrianglePacket CompileTrianglePacket(const Command* commands) {
    TrianglePacket out;
    std::array<bool, 68> used{};
    for (unsigned n = 0; n < 64; ++n) {
        const uint32_t a = uint32_t(commands[n].words.w0), b = uint32_t(commands[n].words.w1);
        const unsigned op = a >> 24;
        if (op != 5 && op != 6)
            break; // F3DEX2 TRI1/TRI2 only; callers verify the microcode.
        const unsigned words[2] = { a, b };
        for (unsigned w = 0; w < (op == 6 ? 2u : 1u); ++w) {
            for (unsigned shift : { 16u, 8u, 0u }) {
                const unsigned encoded = (words[w] >> shift) & 255;
                const unsigned index = encoded / 2;
                if ((encoded & 1) || index >= used.size())
                    return {};
                if (!used[index]) {
                    used[index] = true;
                    out.vertices[out.count++] = uint8_t(index);
                }
            }
        }
        ++out.commands;
    }
    // Tiny packets are cheaper on the existing triangle path.
    if (out.commands < 3)
        return {};
    return out;
}
struct CullingGuard {
    float horizontal = 0, vertical = 0;
    bool active = false;
};
inline CullingGuard MakeCullingGuard(XrFovf fov, float degrees) {
    constexpr float rad = .017453292519943295f, limit = 1.553343034f;
    if (!std::isfinite(degrees) || degrees < 0 || degrees > 60 || !std::isfinite(fov.angleLeft) ||
        !std::isfinite(fov.angleRight) || !std::isfinite(fov.angleUp) || !std::isfinite(fov.angleDown) ||
        fov.angleLeft >= 0 || fov.angleRight <= 0 || fov.angleDown >= 0 || fov.angleUp <= 0 ||
        fov.angleLeft <= -limit || fov.angleRight >= limit || fov.angleDown <= -limit || fov.angleUp >= limit)
        return {};
    // FOVs too wide to add the complete safety margin fall back to normal drawing.
    const float margin = degrees * rad;
    if (fov.angleLeft - margin <= -limit || fov.angleRight + margin >= limit || fov.angleDown - margin <= -limit ||
        fov.angleUp + margin >= limit)
        return {};
    auto expansion = [margin](float low, float high) {
        const float l = std::tan(low), h = std::tan(high);
        return 1.f + 2.f * std::max(l - std::tan(low - margin), std::tan(high + margin) - h) / (h - l);
    };
    return { expansion(fov.angleLeft, fov.angleRight), expansion(fov.angleDown, fov.angleUp), true };
}
// Immutable archive geometry only. Preserve the final contents of every vertex
// slot written by a rejected run, so callers and later lists see identical state.
struct GeometryLoad {
    const void* vertices = nullptr;
    uint16_t count = 0, destination = 0, words = 0;
};
struct GeometryRun {
    uint16_t commands = 0, triangles = 0;
    uint32_t vertices = 0, retainedVertices = 0;
    std::array<float, 3> low{}, high{};
    std::vector<GeometryLoad> retained;
};
template<class Command, class Resolve>
GeometryRun CompileGeometryRun(const Command* commands, Resolve resolve) {
    GeometryRun out;
    std::vector<GeometryLoad> loads;
    std::array<bool, 68> written{};
    bool bounded = false;
    unsigned cursor = 0;
    // All unfamiliar commands terminate the run, including matrices, material,
    // segment changes, calls, microcode switches, vertex edits and end commands.
    while (cursor < 512) {
        const auto* cmd = commands + cursor;
        const unsigned op = uint32_t(cmd->words.w0) >> 24;
        if (op == 5 || op == 6) {
            const uint32_t words[] = {uint32_t(cmd->words.w0), uint32_t(cmd->words.w1)};
            for (unsigned w=0; w<(op==6 ? 2u:1u); ++w)
                for (unsigned shift : {16u,8u,0u}) {
                    const unsigned encoded=(words[w]>>shift)&255, slot=encoded/2;
                    if ((encoded&1) || slot>=written.size() || !written[slot]) return {};
                }
            ++out.triangles; ++cursor;
        } else {
            GeometryLoad load;
            if (!resolve(cmd, load)) break;
            if (!load.vertices || !load.words || load.destination>=written.size() ||
                !load.count || load.count>written.size()-load.destination || loads.size()>=128) return {};
            loads.push_back(load);
            // Resolver exposes exact signed object positions without assuming a
            // packed vertex layout in the shared runtime.
            for (unsigned v=0; v<load.count; ++v) {
                const auto point=resolve.Position(load, v);
                for (unsigned axis=0; axis<3; ++axis) {
                    if (!std::isfinite(point[axis])) return {};
                    if (!bounded) out.low[axis]=out.high[axis]=point[axis];
                    else {out.low[axis]=std::min(out.low[axis],point[axis]); out.high[axis]=std::max(out.high[axis],point[axis]);}
                }
                bounded=true;
                written[load.destination+v]=true;
            }
            out.vertices+=load.count; cursor+=load.words;
        }
    }
    if (loads.size()<3 || out.triangles<6) return {};
    std::array<bool,68> covered{};
    for (auto it=loads.rbegin(); it!=loads.rend(); ++it) {
        bool required=false;
        for (unsigned v=0; v<it->count; ++v) required|=!covered[it->destination+v];
        if (required) {
            out.retained.push_back(*it); out.retainedVertices+=it->count;
            for (unsigned v=0; v<it->count; ++v) covered[it->destination+v]=true;
        }
    }
    if (out.retainedVertices>=out.vertices) return {};
    std::reverse(out.retained.begin(),out.retained.end());
    out.commands=uint16_t(cursor);
    return out;
}
inline bool GeometryOutsideGuard(const GeometryRun& run, const float matrix[4][4], CullingGuard guard) {
    if (!guard.active || !run.commands) return false;
    float center[3], extent[3];
    for (unsigned a=0;a<3;++a) {center[a]=(run.low[a]+run.high[a])*.5f;extent[a]=(run.high[a]-run.low[a])*.5f;}
    // Center/radius support test of the entire local AABB against homogeneous
    // planes. No perspective divide, GPU query, delayed result or distance cull.
    for (unsigned plane=0;plane<4;++plane) {
        const unsigned axis=plane/2; const float sign=(plane&1)?1.f:-1.f;
        const float expansion=axis?guard.vertical:guard.horizontal;
        auto outside=[&](float amount) {
            float maximum=amount*matrix[3][3]-sign*matrix[3][axis];
            float scale=1.f+std::abs(maximum);
            for (unsigned a=0;a<3;++a) {
                float coefficient=amount*matrix[a][3]-sign*matrix[a][axis];
                maximum+=coefficient*center[a]+std::abs(coefficient)*extent[a];
                scale+=std::abs(coefficient)*(std::abs(center[a])+extent[a]);
            }
            return std::isfinite(maximum) && std::isfinite(scale) && maximum < -1e-4f*scale;
        };
        // Original rejection must also hold for geometry crossing/behind the eye.
        if (outside(expansion) && outside(1.f)) return true;
    }
    return false;
}
inline bool GeometryOutsideBothGuards(const GeometryRun& run,
                                     const float leftMatrix[4][4], CullingGuard left,
                                     const float rightMatrix[4][4], CullingGuard right) {
    return left.active && right.active &&
           GeometryOutsideGuard(run, leftMatrix, left) && GeometryOutsideGuard(run, rightMatrix, right);
}
struct TurnCullingGuard {
    XrQuaternionf previous{};
    double time = 0;
    bool valid = false;
    float Update(XrQuaternionf q, double now) {
        const float norm = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
        if (!std::isfinite(norm) || std::abs(norm - 1.f) > .02f || !std::isfinite(now)) {
            valid = false;
            return 0;
        }
        float margin = 0;
        const double dt = now - time;
        if (valid && dt > .0001 && dt < .25) {
            float dot = std::abs(q.x * previous.x + q.y * previous.y + q.z * previous.z + q.w * previous.w);
            margin = std::clamp(float(2 * std::acos(std::clamp(dot, 0.f, 1.f)) * 57.295779513 * .02 / dt), 0.f, 15.f);
        }
        previous = q;
        time = now;
        valid = true;
        return margin;
    }
};
template <class Vertex>
bool PacketOutsideGuard(const TrianglePacket& packet, const Vertex* vertices, size_t capacity, CullingGuard guard) {
    if (!guard.active || !packet.commands || !packet.count || packet.count > packet.vertices.size() || !vertices)
        return false;
    unsigned common = 31;
    for (unsigned i = 0; i < packet.count; ++i) {
        const unsigned index = packet.vertices[i];
        if (index >= capacity)
            return false;
        const auto& v = vertices[index];
        if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.w))
            return false;
        const float x = guard.horizontal * v.w, y = guard.vertical * v.w;
        const float epsilon = 1e-4f * (1.f + std::abs(v.w) + std::abs(v.x) + std::abs(v.y));
        unsigned planes = (v.x < -x - epsilon ? 1u : 0u) | (v.x > x + epsilon ? 2u : 0u) |
                          (v.y < -y - epsilon ? 4u : 0u) | (v.y > y + epsilon ? 8u : 0u) | (v.w < -epsilon ? 16u : 0u);
        common &= planes;
        if (!common)
            return false;
    }
    return common != 0;
}
} // namespace mmvr
