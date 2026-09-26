#ifdef MMVR_ENABLE
#include <fast/lus_gbi.h>
#include "CrossPosts.h"
#include <fast/resource/factory/DisplayListFactory.h>
#include <fast/resource/type/DisplayList.h>
#include <fast/resource/type/Vertex.h>
#include "2s2h/resource/type/Array.h"
#include <libultraship/libultraship.h>
#include <array>
#include <algorithm>
#include <fstream>
#include <cstring>
namespace mmvrgame {
namespace {
struct Post {
    const char* path;
    uint64_t signature, vertex;
    int halfWidth, height;
};
constexpr Post posts[] = {
#include "CrossPosts.inc"
};
struct CrossPost {
    std::shared_ptr<Fast::DisplayList> list;
    std::array<Vtx, 8> vertices;
};
uint64_t Signature(const std::vector<Gfx>& commands) {
    uint64_t hash = 14695981039346656037ULL;
    for (const auto& cmd : commands)
        for (uintptr_t word : { cmd.words.w0, cmd.words.w1 }) {
            if (word > UINT32_MAX)
                return 0;
            for (int shift = 0; shift < 32; shift += 8)
                hash = (hash ^ ((word >> shift) & 255)) * 1099511628211ULL;
        }
    return hash;
}
const Post* Match(const Fast::DisplayList& list, const Ship::ResourceInitData& init) {
    if (init.IsCustom)
        return nullptr;
    for (const auto& post : posts)
        if (init.Path == post.path && Signature(list.Instructions) == post.signature)
            return &post;
    return nullptr;
}
std::shared_ptr<CrossPost> Convert(std::shared_ptr<Fast::DisplayList> list, const Post& post) {
    auto manager = Ship::Context::GetRawInstance()->GetResourceManager();
    const auto* vertexPath = manager->GetArchiveManager()->HashToString(post.vertex);
    if (!vertexPath)
        return {};
    // Do not enqueue a nested job while the display-list factory occupies a resource worker.
    auto source = std::dynamic_pointer_cast<SOH::Array>(manager->LoadResourceProcess(*vertexPath));
    if (!source || source->ArrayType != SOH::ArrayResourceType::Vertex || source->Vertices.size() < 4)
        return {};
    auto result = std::make_shared<CrossPost>();
    result->list = list;
    for (int i = 0; i < 4; ++i) {
        const auto& native = source->Vertices[i];
        Vtx v{};
        for (int axis = 0; axis < 3; ++axis)
            v.v.ob[axis] = native.v.ob[axis];
        v.v.flag = native.v.flag;
        for (int axis = 0; axis < 2; ++axis)
            v.v.tc[axis] = native.v.tc[axis];
        for (int channel = 0; channel < 4; ++channel)
            v.v.cn[channel] = native.v.cn[channel];
        // Replacement models keep their own topology; only the measured native card is converted.
        if (std::abs(int(v.v.ob[0])) != post.halfWidth || v.v.ob[2] != 0 ||
            (v.v.ob[1] != 0 && v.v.ob[1] != post.height))
            return {};
        result->vertices[i] = v;
        v.v.ob[2] = -v.v.ob[0];
        v.v.ob[0] = 0;
        auto nx = static_cast<int8_t>(v.n.n[0]);
        auto nz = static_cast<int8_t>(v.n.n[2]);
        v.n.n[0] = nz;
        v.n.n[2] = -nx;
        result->vertices[i + 4] = v;
    }
    auto& commands = list->Instructions;
    gSPNoOp(&commands[4]); // Old planar bounds cannot cull the perpendicular plane.
    gSPNoOp(&commands[7]); // Remove only this post's yaw-billboard multiply, preserving its world placement.
    const Gfx geometry = commands[22];
    commands[22].words.w1 &= ~uintptr_t(G_CULL_BOTH); // Both sides of each card remain visible.
    gSPVertex(&commands[23], reinterpret_cast<uintptr_t>(result->vertices.data()), 8, 0);
    gSPNoOp(&commands[24]); // Was the second half of the resource-hash vertex load.
    Gfx perpendicular{};
    gSP2Triangles(&perpendicular, 4, 5, 6, 0, 4, 6, 7, 0);
    commands.insert(commands.begin() + 26, perpendicular);
    commands.insert(commands.begin() + 27, geometry); // Do not leak two-sided state to the surrounding scene.
    return result;
}
} // namespace
std::shared_ptr<Ship::IResource>
CrossPostDisplayListFactory::ReadResource(std::shared_ptr<Ship::File> file,
                                          std::shared_ptr<Ship::ResourceInitData> initData) {
    Fast::ResourceFactoryBinaryDisplayListV0 native;
    auto resource = native.ReadResource(file, initData);
    auto list = std::dynamic_pointer_cast<Fast::DisplayList>(resource);
    if (list && initData)
        if (const auto* post = Match(*list, *initData))
            if (auto storage = Convert(list, *post)) {
                // Aliasing ownership keeps the new vertex buffer alive exactly as long as this resource.
                return std::shared_ptr<Ship::IResource>(storage, list.get());
            }
    return resource;
}
void VerifyCrossPosts() {
    std::ofstream log("native-cross-posts.json");
    log << "[";
    bool first = true;
    for (const auto& post : posts) {
        auto list = std::dynamic_pointer_cast<Fast::DisplayList>(
            Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource(post.path));
        if (list)
            std::ofstream("native-cross-post-details.log", std::ios::app)
                << post.path << " loaded=" << list->GetInitData()->Path << " custom=" << list->GetInitData()->IsCustom
                << " count=" << list->Instructions.size() << " signature=" << std::hex << Signature(list->Instructions)
                << " expected=" << post.signature << "\n";
        bool shape = false, stationary = false, twosided = false, uv = false, guard = false;
        if (list && list->Instructions.size() == 30) {
            const auto& c = list->Instructions;
            auto* vertices = reinterpret_cast<Vtx*>(c[23].words.w1);
            stationary = (c[7].words.w0 >> 24) == G_SPNOOP;
            twosided = !(c[22].words.w1 & G_CULL_BOTH);
            shape = vertices && (c[23].words.w0 >> 24) == G_VTX;
            uv = shape;
            if (shape)
                for (int i = 0; i < 4; ++i) {
                    const auto& a = vertices[i];
                    const auto& b = vertices[i + 4];
                    shape &= a.v.ob[0] == -b.v.ob[2] && a.v.ob[2] == 0 && b.v.ob[0] == 0 && a.v.ob[1] == b.v.ob[1];
                    uv &= a.v.tc[0] == b.v.tc[0] && a.v.tc[1] == b.v.tc[1] && a.v.cn[3] == b.v.cn[3];
                }
            Ship::ResourceInitData replacement = *list->GetInitData();
            replacement.IsCustom = true;
            guard = Match(*list, replacement) == nullptr && Match(*list, *list->GetInitData()) == nullptr;
        }
        if (!first)
            log << ",";
        first = false;
        log << "{\"path\":\"" << post.path << "\",\"cross\":" << shape << ",\"stationary\":" << stationary
            << ",\"twoSided\":" << twosided << ",\"textureCoordinates\":" << uv << ",\"replacementGuard\":" << guard
            << "}";
    }
    log << "]";
}
} // namespace mmvrgame
#endif
