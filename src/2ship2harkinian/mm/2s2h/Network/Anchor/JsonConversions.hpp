#ifndef NETWORK_ANCHOR_JSON_CONVERSIONS_H
#define NETWORK_ANCHOR_JSON_CONVERSIONS_H
#ifdef __cplusplus

#include <nlohmann/json.hpp>
#include <libultraship/libultraship.h>
#include "Anchor.h"
#include "BenJsonConversions.hpp"

using json = nlohmann::json;

inline void to_json(json& j, const Color_RGB8& color) {
    j = json{{"r", color.r}, {"g", color.g}, {"b", color.b}};
}

inline void from_json(const json& j, Color_RGB8& color) {
    color.r = j.value("r", (u8)255);
    color.g = j.value("g", (u8)255);
    color.b = j.value("b", (u8)255);
}

inline void to_json(json& j, const PosRot& posRot) {
    j = json{{"pos", posRot.pos}, {"rot", posRot.rot}};
}

inline void from_json(const json& j, PosRot& posRot) {
    j.at("pos").get_to(posRot.pos);
    j.at("rot").get_to(posRot.rot);
}

inline void from_json(const json& j, AnchorClient& client) {
    client.clientId = j.value("clientId", (u32)0);
    client.name = j.value("name", "???");
    client.color = j.value("color", Color_RGB8{ 255, 255, 255 });
    client.clientVersion = j.value("clientVersion", "???");
    client.teamId = j.value("teamId", "default");
    client.online = j.value("online", false);
    client.seed = j.value("seed", (u32)0);
    client.isSaveLoaded = j.value("isSaveLoaded", false);
    client.isGameComplete = j.value("isGameComplete", false);
    client.sceneId = j.value("sceneId", (s16)SCENE_MAX);
    client.entrance = j.value("entrance", (s32)0);
    client.self = j.value("self", false);
}

#endif // __cplusplus
#endif // NETWORK_ANCHOR_JSON_CONVERSIONS_H
