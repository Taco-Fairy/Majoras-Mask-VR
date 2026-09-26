#pragma once

// Native actor/song recipes and base-layer arrival panoramas omitted by the
// generated alternate-scene-layer catalog.
enum class DebugNativeTriggerKind {
    BombersBalloon, InvertedTime, NorthClockTownArrival, WestClockTownArrival, EastClockTownArrival,
    TingleBalloon, WoodfallCrystal
};
struct DebugNativeTrigger {
    const char* name;
    int entrance;
    int scene;
    DebugNativeTriggerKind kind;
};
constexpr DebugNativeTrigger debugNativeTriggers[] = {
    {"Bombers balloon reaction", ENTRANCE(NORTH_CLOCK_TOWN, 0), SCENE_BACKTOWN,
     DebugNativeTriggerKind::BombersBalloon},
    {"Inverted Time song effect", ENTRANCE(SOUTH_CLOCK_TOWN, 0), SCENE_CLOCKTOWER,
     DebugNativeTriggerKind::InvertedTime},
    {"North Clock Town arrival", ENTRANCE(NORTH_CLOCK_TOWN, 0), SCENE_BACKTOWN,
     DebugNativeTriggerKind::NorthClockTownArrival},
    {"West Clock Town arrival", ENTRANCE(WEST_CLOCK_TOWN, 0), SCENE_ICHIBA,
     DebugNativeTriggerKind::WestClockTownArrival},
    {"East Clock Town arrival", ENTRANCE(EAST_CLOCK_TOWN, 0), SCENE_TOWN,
     DebugNativeTriggerKind::EastClockTownArrival},
    {"Tingle balloon pop and fall", ENTRANCE(NORTH_CLOCK_TOWN, 0), SCENE_BACKTOWN,
     DebugNativeTriggerKind::TingleBalloon},
    {"Woodfall crystal camera", ENTRANCE(WOODFALL_TEMPLE, 3), SCENE_MITURIN,
     DebugNativeTriggerKind::WoodfallCrystal},
};
// The Woodfall room-0 recipe is retained for an opt-in native fixture until
// its actor-owned camera completes in that fixture. Do not offer it on a pad.
constexpr int visibleNativeTriggerCount = 6;
