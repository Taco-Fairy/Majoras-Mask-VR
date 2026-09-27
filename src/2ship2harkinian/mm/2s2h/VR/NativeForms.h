#pragma once
#include "forms.h"
struct Player;
namespace mmvrgame {
bool GiantTransformationActive(Player*);
bool FirstPersonFormAllowed(Player*);
float FormEyeHeight(Player*);
float StandingFormEyeHeight(Player*);
void RecordFormEyeHeight(Player*);
bool NativeAbilityOwnsFacing(Player*);
const void* FormHandMesh(Player*, int hand);
} // namespace mmvrgame
