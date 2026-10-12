#include "DXGround.h"
#include <limits.h>

bool dx_ground_apply(const uint8_t *rom, size_t size, const DXTerrainPoint *point,
                     DXWorldBody *body, DXGroundState *ground, DXGroundEffects *effects) {
    if (!rom || size != 1024*1024 || !point || !body || !ground || !effects || point->local_x>15 || point->local_y>15) return false;
    DXWorldBody next=*body;
    DXGroundState g=*ground;
    DXGroundEffects fx={0};
    DXCollisionState *s=&next.collision;
    unsigned previous=g.ground_status, physics=point->physics;
    g.ground_status=0;
    if (s->z && s->z<128) goto done;
    s->ledge_count=0;
    if (s->options & 0x10) goto done;
    if (point->object==0x67) g.ground_status=1;
    else if (!physics) goto done;
    else if (physics==0x0b || physics==7) {
        if (s->type==0xcc || s->type==0xa0 || s->type==0xd5 || s->type==0x6d || s->type==0xc1) g.ground_status=2;
        else { s->status=0; goto splash; }
    } else if (physics==0xb0) g.ground_status=1;
    else if (physics==5) g.ground_status=2;
    else if (physics==6) g.ground_status=3;
    if ((s->options&8) && previous!=g.ground_status && previous!=3 && g.ground_status!=3) {
        if (g.side_scrolling) {
            if (s->type!=0xac && g.speed_y>=0) {
                g.speed_y=0;
                g.speed_x = g.speed_x>=0 ? g.speed_x/2 : -((-(int)g.speed_x+1)/2);
            }
        } else if (g.speed_z>=-25) goto after_splash;
        if (!g.splash_timer) {
 splash:
            fx.splash=true; fx.jingle=0x0e;
            fx.splash_x=next.x.position; fx.splash_y=next.y.position;
        }
    }
 after_splash:
    if (physics>=0xf0 && physics<0xff) {
        if (!(s->frame&3)) {
            unsigned index=physics-0xf0;
            int64_t x=(int64_t)next.x.position+(int8_t)rom[0xf887+index];
            int64_t y=(int64_t)next.y.position+(int8_t)rom[0xf88f+index];
            if (x<INT32_MIN || x>INT32_MAX || y<INT32_MIN || y>INT32_MAX) return false;
            next.x.position=(int32_t)x; next.y.position=(int32_t)y;
        }
        goto done;
    }
    if (point->object!=0x61 && physics!=0x50 && physics!=0x51) goto done;
    if (s->type==0x6d || s->type==0xd5 || s->type==0x36) goto done;
    if (s->type==0xc1 && (g.link_motion!=6 || point->object!=0x61)) goto done;
    if (!s->ignore_hits) goto done;
    s->ignore_hits--;
    g.flash_timer=0;
    s->status=2;
    int64_t x=(int64_t)next.x.position-1-point->local_x+8;
    int64_t y=(int64_t)next.y.position-7-point->local_y+16;
    if (x<INT32_MIN || x>INT32_MAX || y<INT32_MIN || y>INT32_MAX) return false;
    g.falling_x=(int32_t)x; g.falling_y=(int32_t)y;
    s->countdown=0x6f;
    if (s->type==0x14 || s->type==0x0b || s->type==9) goto done;
    s->countdown=0x48;
    if (!s->ignore_hits) { s->countdown=0x2f; fx.jingle=0x18; }
 done:
    *body=next; *ground=g; *effects=fx;
    return true;
}
