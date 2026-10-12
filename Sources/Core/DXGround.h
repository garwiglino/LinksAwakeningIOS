#pragma once
#include "DXCollision.h"
// Additional state touched by the original ground-effects phase.
typedef struct {
    uint8_t ground_status, flash_timer, splash_timer, link_motion;
    int8_t speed_x, speed_y, speed_z;
    int32_t falling_x, falling_y;
    bool side_scrolling;
} DXGroundState;
typedef struct {
    uint8_t jingle;
    bool splash;
    int32_t splash_x, splash_y;
} DXGroundEffects;
// Runs after movement, before wall resolution. Emits effects for the scheduler;
// does not allocate VFX slots or play audio. A false result changes no outputs.
bool dx_ground_apply(const uint8_t *rom, size_t size, const DXTerrainPoint *point,
                     DXWorldBody *body, DXGroundState *ground, DXGroundEffects *effects);
