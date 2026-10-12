#pragma once
#include "DXGround.h"
// Integrated original phase order for an overworld entity. The caller controls
// whether this handler moves or only checks background. Not a combat/AI loop.
bool dx_physics_step(const uint8_t *rom, size_t size,
                     const DXWorldRoom *const rooms[256], bool move,
                     DXWorldBody *body, DXGroundState *ground,
                     DXWallContact *contact, DXGroundEffects *effects);
