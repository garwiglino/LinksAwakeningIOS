#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "DXWorld.h"
#include "DXEntity.h"

// French Rev 1 collision rules, independent of GB memory and screen coordinates.
typedef struct {
    uint8_t type, z, options, ignore_hits, thrown_direction;
    uint8_t ledge_count, countdown, status, state, collision_flags;
    uint8_t frame, indoor, switch_state, link_on_switch, link_immune;
} DXCollisionState;
typedef struct {
    uint8_t object, physics, room;
    uint8_t local_x, local_y;
} DXTerrainPoint;

bool dx_collision_physics(const uint8_t *rom, size_t size, uint8_t object,
                          uint8_t map_group, bool color_dungeon, uint8_t *physics);
// Update the same gameplay fields as ApplyEntityCollisionWithObject. Returns
// success separately from blocked; false never changes state or blocked.
bool dx_collision_apply(const uint8_t *rom, size_t size, const DXTerrainPoint *point,
                        unsigned direction, DXCollisionState *state, bool *blocked);
// One collision point, in world pixels, using the original four hitbox types.
bool dx_collision_point(const uint8_t *rom, size_t size, int32_t x, int32_t y,
                        unsigned hitbox, unsigned direction, uint8_t type, uint8_t z,
                        int32_t *point_x, int32_t *point_y);
// Explicit loaded-room set. Missing rooms and island edges return false; never
// wrap around the island or silently borrow the active room's collision map.
bool dx_collision_world_point(const uint8_t *rom, size_t size,
                              const DXWorldRoom *const rooms[256],
                              int32_t x, int32_t y, DXTerrainPoint *point);

typedef struct {
    DXEntityAxis x, y;
    DXCollisionState collision;
    uint8_t hitbox;
    bool no_bg_collision;
} DXWorldBody;
typedef struct { uint8_t horizontal_object, vertical_object; } DXWallContact;
// Original movement + wall-resolution order, in world coordinates. Ground
// effects (pits, grass, conveyors, water) are handled separately, not inferred.
// Missing collision data aborts transactionally: body/contact remain unchanged.
bool dx_collision_move_walls(const uint8_t *rom, size_t size,
                             const DXWorldRoom *const rooms[256], DXWorldBody *body,
                             int8_t speed_x, int8_t speed_y, DXWallContact *contact);

// Resolve walls after movement and ground effects. Rollback coordinates are
// the original active position copied before those phases.
bool dx_collision_resolve_walls(const uint8_t *rom, size_t size,
                                const DXWorldRoom *const rooms[256], DXWorldBody *body,
                                int32_t rollback_x, int32_t rollback_y,
                                int8_t speed_x, int8_t speed_y, DXWallContact *contact);
