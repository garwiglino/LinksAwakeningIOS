#pragma once
#include "DXPhysics.h"
#include "DXRoaming.h"

// Native entity state. World coordinates remain signed and never wrap at a
// room edge. A roster retains the original sixteen-slot allocation order.
typedef struct {
    DXWorldBody body;
    DXGroundState ground;
    uint8_t direction, sprite, inertia, z_fraction;
    uint8_t private_state[5], attack_cooldown, private_countdown2, slow_countdown;
    uint8_t physics_flags, hitbox_flags, health, health_group;
    uint8_t home_room, load_order, lifted, dropped_item, power_recoil;
    int8_t recoil_x, recoil_y;
    uint8_t damage_hitbox[4];
} DXActor;
typedef struct { DXActor slots[16]; } DXActorRoster;
typedef struct {
    uint8_t gameplay, transition_sequence, room_transition, inventory, dialog, ocarina, frame;
} DXActorClock;
typedef struct { uint8_t flip, tiles_offset; bool no_bg_collision; } DXActorRenderState;

// Generic configuration only. Type-specific room initialization is separate.
// Preserves position, status, type and fields the original reset does not touch.
bool dx_actor_configure(const uint8_t *rom, size_t size, uint8_t room, DXActor *actor);
bool dx_actor_update_timers(DXActor *actor, const DXActorClock *clock, DXActorRenderState *render);

typedef enum { DX_SPAWN_INVALID, DX_SPAWN_FULL, DX_SPAWN_OK } DXSpawnResult;
// Creates the actual arrow/rock record with ROM-defined flags, health, hitbox,
// offsets and velocity. FULL leaves the roster and output slot unchanged.
DXSpawnResult dx_actor_spawn_projectile(const uint8_t *rom, size_t size,
                                        DXActorRoster *roster, unsigned parent,
                                        uint8_t current_room, uint8_t projectile_type,
                                        unsigned *spawned_slot);

// RNG state is copied with the transaction, so missing geometry cannot consume
// random bytes or leave a projectile allocated without advancing its parent.
typedef struct { uint8_t seed, frame, scanline; } DXActorRandom;
typedef struct {
    DXRoamingAction action;
    DXWallContact contact;
    DXGroundEffects ground;
    int spawned_slot; // -1 if no projectile could/should be allocated
} DXRoamingEffects;
// Complete post-combat continuation: decisions, projectile allocation, physics,
// animation. Timers/recoil/damage and projectile execution are separate phases.
bool dx_actor_roaming_continue(const uint8_t *rom, size_t size,
                               const DXWorldRoom *const rooms[256], DXActorRoster *roster,
                               unsigned slot, uint8_t current_room,
                               const DXRoamingContext *context, DXActorRandom *random,
                               DXRoamingEffects *effects);
