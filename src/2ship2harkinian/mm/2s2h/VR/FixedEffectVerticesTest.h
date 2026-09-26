#pragma once
#include "2s2h/resource/type/Array.h"
extern "C" {
#include "overlays/actors/ovl_Obj_Smork/z_obj_smork.h"
#include "overlays/ovl_Dm_Char01/ovl_Dm_Char01.h"
#include "overlays/actors/ovl_Obj_Entotu/z_obj_entotu.h"
#include "overlays/actors/ovl_Obj_Toudai/z_obj_toudai.h"
#include "overlays/ovl_Obj_Smork/ovl_Obj_Smork.h"
#include "overlays/ovl_Obj_Entotu/ovl_Obj_Entotu.h"
#include "overlays/ovl_Obj_Toudai/ovl_Obj_Toudai.h"
char* ResourceMgr_LoadVtxArrayByName(const char*);
size_t ResourceMgr_GetVtxArraySizeByName(const char*);
void ObjSmork_Init(Actor*, PlayState*);
void ObjEntotu_Init(Actor*, PlayState*);
void ObjToudai_Init(Actor*, PlayState*);
}
namespace mmvrtest {
template <class T>
unsigned CheckFixedEffect(PlayState* play, const char* path, void (*initialize)(Actor*, PlayState*)) {
    auto manager = Ship::Context::GetRawInstance()->GetResourceManager();
    const std::string key = std::string(path).substr(7);
    auto original = manager->LoadResource(key);
    RequireResource(original != nullptr, "Native effect resource missing");
    struct Restore {
        std::shared_ptr<Ship::ResourceManager> manager;
        std::string path;
        std::shared_ptr<Ship::IResource> resource;
        ~Restore() {
            manager->CacheExternalResource(path, resource);
        }
    } restore{ manager, key, original };
    T native{};
    native.actor.update = native.actor.draw = [](Actor*, PlayState*) {};
    initialize(&native.actor, play);
    RequireResource(native.actor.update != nullptr, "Original effect topology rejected");
    constexpr size_t count = sizeof(native.unk_148) / sizeof(native.unk_148[0]);
    RequireResource(ResourceMgr_GetVtxArraySizeByName(path) == count, "Unexpected original topology");
    RequireResource(std::memcmp(native.unk_148, ResourceMgr_LoadVtxArrayByName(path), sizeof(native.unk_148)) == 0,
                    "Original effect vertices changed");
    unsigned cases = 1;
    for (size_t n : { size_t(0), count - 1, count, count + 1, count + 1024 }) {
        auto replacement = std::make_shared<SOH::Array>();
        replacement->ArrayType = SOH::ArrayResourceType::Vertex;
        replacement->Vertices.resize(n);
        for (size_t i = 0; i < n; ++i)
            std::memset(&replacement->Vertices[i], int(i % 127) + 1, sizeof(replacement->Vertices[i]));
        manager->CacheExternalResource(key, replacement);
        struct Guard {
            T actor{};
            uint64_t canary = 0x3141592653589793ULL;
        } checked;
        checked.actor.actor.update = checked.actor.actor.draw = [](Actor*, PlayState*) {};
        initialize(&checked.actor.actor, play);
        RequireResource(checked.canary == 0x3141592653589793ULL, "Effect initialization escaped actor storage");
        if (n == count) {
            RequireResource(checked.actor.actor.update != nullptr, "Compatible effect rejected");
            RequireResource(
                std::memcmp(checked.actor.unk_148, replacement->Vertices.data(), sizeof(checked.actor.unk_148)) == 0,
                "Compatible effect copy differs");
        } else {
            RequireResource(!checked.actor.actor.update && !checked.actor.actor.draw,
                            "Incompatible effect was not stopped");
            Vtx blank[count]{};
            RequireResource(std::memcmp(checked.actor.unk_148, blank, sizeof(blank)) == 0,
                            "Rejected effect changed vertices");
        }
        ++cases;
    }
    manager->CacheExternalResource(key, std::make_shared<ProbeResource>());
    T wrong{};
    wrong.actor.update = wrong.actor.draw = [](Actor*, PlayState*) {};
    initialize(&wrong.actor, play);
    RequireResource(!wrong.actor.update && !wrong.actor.draw, "Wrong resource type accepted as vertices");
    ++cases;
    // Restore the actor's static source pointer as well as its cached owner.
    manager->CacheExternalResource(key, original);
    initialize(&native.actor, play);
    return cases;
}
} // namespace mmvrtest
static void VerifyFixedEffectVertices(PlayState* play) {
    mmvrtest::RequireResource(ResourceMgr_GetVtxArraySizeByName(gWoodfallSceneryDynamicPoisonWaterVtx) == 205,
                              "Original Woodfall topology changed");
    unsigned cases = 0;
    cases += mmvrtest::CheckFixedEffect<ObjSmork>(play, ovl_Obj_Smork_Vtx_000C10, ObjSmork_Init);
    cases += mmvrtest::CheckFixedEffect<ObjEntotu>(play, ovl_Obj_Entotu_Vtx_000D10, ObjEntotu_Init);
    cases += mmvrtest::CheckFixedEffect<ObjToudai>(play, ovl_Obj_Toudai_Vtx_D_80A34590, ObjToudai_Init);
    std::ofstream("native-fixed-effect-vertices.json") << "{\"passed\":true,\"actualActorCases\":" << cases << "}";
}
