#include "DXPhysics.h"
#include <limits.h>

bool dx_physics_step(const uint8_t *rom, size_t size,
                     const DXWorldRoom *const rooms[256], bool move,
                     DXWorldBody *body, DXGroundState *ground,
                     DXWallContact *contact, DXGroundEffects *effects) {
    if (!body || !ground || !contact || !effects || ground->side_scrolling || body->collision.indoor) return false;
    DXWorldBody next=*body;
    DXGroundState g=*ground;
    DXWallContact hit;
    DXGroundEffects fx;
    if (move && (!dx_entity_move_axis(&next.x,g.speed_x) || !dx_entity_move_axis(&next.y,g.speed_y))) return false;
    DXTerrainPoint point={0};
    if (!(next.collision.z && next.collision.z<128) && !(next.collision.options&0x10)) {
        int64_t x=(int64_t)next.x.position-1,y=(int64_t)next.y.position-7;
        if (x<INT32_MIN || y<INT32_MIN ||
            !dx_collision_world_point(rom,size,rooms,(int32_t)x,(int32_t)y,&point)) return false;
    }
    if (!dx_ground_apply(rom,size,&point,&next,&g,&fx) ||
        !dx_collision_resolve_walls(rom,size,rooms,&next,body->x.position,body->y.position,g.speed_x,g.speed_y,&hit)) return false;
    *body=next; *ground=g; *contact=hit; *effects=fx;
    return true;
}
