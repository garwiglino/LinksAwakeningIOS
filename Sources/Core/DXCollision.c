#include "DXCollision.h"
#include <limits.h>

bool dx_collision_physics(const uint8_t *rom, size_t size, uint8_t object,
                          uint8_t map_group, bool color_dungeon, uint8_t *physics) {
    if (!rom || size != 1024 * 1024 || !physics || map_group > 2) return false;
    *physics = rom[0x20ad4 + object + 256 * (map_group + (color_dungeon ? 1 : 0))];
    return true;
}
bool dx_collision_point(const uint8_t *rom, size_t size, int32_t x, int32_t y,
                        unsigned hitbox, unsigned direction, uint8_t type, uint8_t z,
                        int32_t *point_x, int32_t *point_y) {
    if (!rom || size != 1024 * 1024 || hitbox > 3 || direction > 3 || !point_x || !point_y) return false;
    unsigned index = hitbox * 4 + direction;
    int64_t px = (int64_t)x - 8 + (int8_t)rom[0xf863 + index];
    int64_t py = (int64_t)y - 16 + (int8_t)rom[0xf873 + index];
    if (direction == 3 && (type == 0xa8 || type == 5) && z < 128) py -= z / 2;
    if (px < INT32_MIN || px > INT32_MAX || py < INT32_MIN || py > INT32_MAX) return false;
    *point_x = (int32_t)px; *point_y = (int32_t)py;
    return true;
}
bool dx_collision_world_point(const uint8_t *rom, size_t size,
                              const DXWorldRoom *const rooms[256],
                              int32_t x, int32_t y, DXTerrainPoint *point) {
    if (!rooms || !point || x < 0 || y < 0 || x >= 16 * 160 || y >= 16 * 128) return false;
    unsigned room = (y / 128) * 16 + x / 160;
    if (!rooms[room] || rooms[room]->room != room) return false;
    unsigned lx = x % 160, ly = y % 128;
    DXTerrainPoint result = {.room = room, .local_x = lx & 15, .local_y = ly & 15};
    result.object = rooms[room]->blocks[0x11 + (ly / 16) * 16 + lx / 16];
    if (!dx_collision_physics(rom,size,result.object,0,false,&result.physics)) return false;
    *point = result;
    return true;
}
bool dx_collision_apply(const uint8_t *rom, size_t size, const DXTerrainPoint *point,
                        unsigned direction, DXCollisionState *state, bool *blocked) {
    if (!rom || size != 1024 * 1024 || !point || !state || !blocked || direction > 3) return false;
    DXCollisionState result = *state;
    unsigned physics = point->physics;
    if (point->object == 0x20) goto hookshottable;
    if (result.type == 0xcc || result.type == 0x99) {
        if (physics == 5 || physics == 7) goto pass;
        goto collide;
    }
    if (!physics) goto pass;
    if (physics == 0x0b || physics == 0x50 || physics == 0x51) {
        if (result.z) goto pass;
        if (!result.ignore_hits || result.type == 0x59) goto collide;
        goto pass;
    }
    if (physics >= 0x7c && physics < 0x90) {
        if (physics >= 0x80) {
            if (result.type == 0xa8 || result.type == 2) goto pass;
        } else if (result.type == 0x16 || result.type == 0x17 || (result.options & 0x80)) {
            goto set_collision;
        }
        unsigned quadrant = ((point->local_x >> 3) & 1) | ((point->local_y >> 2) & 2);
        if (!rom[0xfa89 + (physics - 0x7c) * 4 + quadrant]) goto pass;
    }
    if (physics >= 0xd0 && physics < 0xd4) {
        if (physics - 0xd0 == result.thrown_direction) {
            if (!result.z) goto collide;
            result.ledge_count++;
        } else {
            if (result.type == 0xa8 || !result.ledge_count) goto collide;
            if ((result.frame & 3) && (result.indoor || (result.frame & 1))) result.ledge_count--;
        }
        goto pass;
    }
    if (physics == 0xff) goto set_collision;
    if (physics >= 0xa0) goto pass;
    if (physics >= 0x10) goto collide;
    if (physics == 1 || physics == 3) goto wall_options;
    if (physics != 4) goto pass;
    if (result.type == 0xa8 || result.type == 2) goto pass;
    if (result.type == 3 && result.link_on_switch) goto pass;
    if (point->object < 0xdb || point->object > 0xdc) goto set_collision;
    if (!(result.switch_state ^ rom[0xfcb3 + point->object - 0xdb])) goto pass;
collide:
    if (physics != 0x60) goto wall_options;
hookshottable:
    if (result.type == 3) {
        if (result.countdown >= 0x26) {
            // French Rev 1 includes the hookshot collision-immunity fix.
            result.status = 0;
            result.link_immune = 0;
        } else result.state = 1;
    }
wall_options:
    if (result.options & 1) goto pass;
set_collision:
    result.collision_flags |= (uint8_t)(1u << direction);
    *state = result; *blocked = true; return true;
pass:
    *state = result; *blocked = false; return true;
}

bool dx_collision_move_walls(const uint8_t *rom, size_t size,
                             const DXWorldRoom *const rooms[256], DXWorldBody *body,
                             int8_t speed_x, int8_t speed_y, DXWallContact *contact) {
    if (!rom || size != 1024 * 1024 || !rooms || !body || !contact || body->hitbox > 3) return false;
    DXWorldBody next = *body;
    DXWallContact hit = {0};
    if (!dx_entity_move_axis(&next.x,speed_x) || !dx_entity_move_axis(&next.y,speed_y)) return false;
    if (next.collision.type != 0x6d) { // BowWow bypasses the wall phase.
        next.collision.collision_flags = 0;
        for (unsigned axis = 0; axis < 2; axis++) {
            int speed = axis ? speed_y : speed_x;
            if (!speed) continue;
            unsigned direction = axis ? (speed < 0 ? 2 : 3) : (speed < 0 ? 1 : 0);
            int32_t px,py;
            DXTerrainPoint point;
            bool blocked;
            if (!dx_collision_point(rom,size,next.x.position,next.y.position,next.hitbox,direction,
                                    next.collision.type,next.collision.z,&px,&py) ||
                !dx_collision_world_point(rom,size,rooms,px,py,&point) ||
                !dx_collision_apply(rom,size,&point,direction,&next.collision,&blocked)) return false;
            if (blocked) {
                if (axis) hit.vertical_object = point.object;
                else hit.horizontal_object = point.object;
                if (!next.no_bg_collision) {
                    if (axis) next.y.position = body->y.position;
                    else next.x.position = body->x.position;
                }
            }
        }
    }
    *body = next; *contact = hit;
    return true;
}
