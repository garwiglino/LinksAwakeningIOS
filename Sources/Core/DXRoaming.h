#pragma once
#include <stdbool.h>
#include <stdint.h>

// Original roaming-enemy decision phase, after timers, recoil and combat.
// This is not the complete enemy handler: returned actions must be executed by
// the world scheduler with its damage, physics, spawning and animation systems.
typedef struct {
    uint8_t type, state, direction, turn_counter, countdown, attack_cooldown;
    uint8_t collisions;
    int8_t speed_x, speed_y;
} DXRoamingState;
typedef struct {
    int32_t x, y, link_x, link_y;
    int8_t z;
    uint8_t active_state; // original hActiveEntityState, distinct from stored state
    bool credits;
} DXRoamingContext;
typedef uint8_t (*DXRandomByte)(void *context);
typedef enum {
    DX_ROAM_MOVE_AND_ANIMATE,
    DX_ROAM_GROUND_ONLY,
    DX_ROAM_FIRE_ARROW_AND_GROUND,
    DX_ROAM_FIRE_ROCK,
    DX_ROAM_WAIT
} DXRoamingAction;
// Tie-breaking matches the original direction helper; extended distances do
// not alias at 128/256 pixels. Zero offset faces down, as in the original.
uint8_t dx_roaming_direction(const DXRoamingContext *context);
bool dx_roaming_decide(DXRoamingState *state, const DXRoamingContext *context,
                       DXRandomByte random_byte, void *random_context,
                       DXRoamingAction *action);
