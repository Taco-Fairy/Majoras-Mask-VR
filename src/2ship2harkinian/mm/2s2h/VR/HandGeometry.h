#pragma once
#include "first_person.h"
struct PlayState; struct Player;
namespace mmvrgame {
void ResetHandGeometry();
mmvr::TrackingFrame ResolveHandGeometry(PlayState*, Player*, const mmvr::TrackingFrame& filtered,
    const mmvr::TrackingFrame& raw, const mmvr::Matrix& view, const mmvr::Matrix& relativeHead);
}
