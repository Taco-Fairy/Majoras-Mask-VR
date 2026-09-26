#pragma once
#include <array>
struct PlayState;struct Player;
namespace mmvrgame {
void MovePlayerBody(PlayState*,Player*,const std::array<float,3>& destination);
}
