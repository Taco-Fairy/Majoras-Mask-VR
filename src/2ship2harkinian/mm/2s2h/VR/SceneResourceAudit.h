#pragma once
#include "2s2h/resource/type/Scene.h"
#include "2s2h/resource/type/scenecommand/SetActorList.h"
#include "2s2h/resource/type/scenecommand/SetActorCutsceneList.h"
#include "2s2h/resource/type/scenecommand/SetRoomList.h"
#include <nlohmann/json.hpp>

// Loads actual archived headers through the production factories. This validates
// resources and discovers placements; it does not claim to run actor behavior.
static void NativeSceneResourceAudit() {
    std::ifstream input("scene-audit-resources.txt");
    if (!input) throw std::runtime_error("Missing scene resource audit inventory");
    std::ofstream detail("native-scene-resources.jsonl");
    auto manager=Ship::Context::GetRawInstance()->GetResourceManager();
    unsigned resources=0,failures=0;
    std::string path;
    while (std::getline(input,path)) {
        if (!path.empty() && path.back()=='\r') path.pop_back();
        if (path.empty()) continue;
        detail << nlohmann::json{{"begin",path}}.dump() << '\n' << std::flush;
        auto scene=std::dynamic_pointer_cast<SOH::Scene>(manager->LoadResource(path));
        nlohmann::json row={{"resource",path},{"actors",nlohmann::json::array()},
            {"cutscenes",nlohmann::json::array()},{"rooms",nlohmann::json::array()}};
        bool valid=bool(scene);
        if (scene) for (const auto& command:scene->commands) {
            if (!command) {valid=false;continue;}
            if (auto actors=std::dynamic_pointer_cast<SOH::SetActorList>(command)) {
                valid &= actors->numActors==actors->actorList.size();
                for (const auto& actor:actors->actorList)
                    row["actors"].push_back({{"id",uint16_t(actor.id)}, {"params",uint16_t(actor.params)},
                        {"position",{actor.pos.x,actor.pos.y,actor.pos.z}}, {"rotation",{actor.rot.x,actor.rot.y,actor.rot.z}}});
            }
            if (auto cuts=std::dynamic_pointer_cast<SOH::SetActorCutsceneList>(command))
                for (const auto& cut:cuts->entries)
                    row["cutscenes"].push_back({{"script",cut.scriptIndex},{"camera",cut.csCamId},
                        {"additional",cut.additionalCsId},{"length",cut.length}});
            if (auto rooms=std::dynamic_pointer_cast<SOH::SetRoomList>(command)) {
                valid &= rooms->numRooms==rooms->fileNames.size();
                row["rooms"]=rooms->fileNames;
            }
        }
        row["passed"]=valid;detail<<row.dump()<<'\n'<<std::flush;
        ++resources;failures+=!valid;
    }
    std::ofstream("native-scene-resource-summary.json") << nlohmann::json{{"resources",resources},{"failures",failures}}.dump();
    Ship::Context::GetRawInstance()->GetWindow()->Close();
}
