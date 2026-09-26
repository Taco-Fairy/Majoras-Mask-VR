#pragma once
#include "first_person.h"
struct PlayState;struct Player;
namespace mmvrgame {
bool BombchuReticleVisible(Player*);
void UpdateBombchuReticle(mmvr::CameraFrame&);
bool HeldBombchu(Player*);
bool PlaceBombchu(PlayState*,Player*);
bool BombchuPlacement(PlayState*,Player*,const mmvr::Matrix& head,mmvr::Matrix& target);
}
