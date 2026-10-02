#pragma once
#include <array>
struct PlayState;struct Player;
namespace mmvrgame {
bool RoomScalePropBlocked(PlayState*, Player*, const std::array<float, 3>& destination);
void MovePlayerBody(PlayState*,Player*,const std::array<float,3>& destination);
}
