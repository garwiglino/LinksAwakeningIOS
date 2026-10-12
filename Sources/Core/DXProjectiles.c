#include "DXProjectiles.h"
#include <limits.h>

static bool supported(const DXActor *a) {
    return a && (a->body.collision.type==0x0a || a->body.collision.type==0x0c) &&
        !a->body.collision.indoor && !a->ground.side_scrolling;
}
static void recoil_link(const DXActor *a,DXProjectileLink *link) {
    int64_t dx=(int64_t)link->x-a->body.x.position;
    int64_t dy=(int64_t)link->y-a->body.y.position+(int8_t)a->body.collision.z;
    uint64_t ax=dx<0?-dx:dx,ay=dy<0?-dy:dy;
    uint64_t maximum=ax>ay?ax:ay;
    // The original integer divider returns (20,20) for a coincident center.
    int vx=maximum?(int)(ax*20/maximum):20;
    int vy=maximum?(int)(ay*20/maximum):20;
    link->speed_x=(int8_t)(dx<0?-vx:vx); link->speed_y=(int8_t)(dy<0?-vy:vy);
}
bool dx_projectile_contact(const uint8_t *rom,size_t size,DXActor *actor,
                            DXProjectileLink *link,DXProjectileEffects *effects) {
    if (!rom || size!=1024*1024 || !supported(actor) || !link || !effects ||
        actor->direction>3 || link->direction>3) return false;
    DXActor a=*actor; DXProjectileLink p=*link; DXProjectileEffects fx={0};
    int64_t dx=(int64_t)p.x-a.body.x.position;
    int64_t dy=(int64_t)p.y-a.body.y.position+(int8_t)a.body.collision.z;
    if (p.motion>=2 || p.z || dx<-6 || dx>=6 || dy<-6 || dy>=6) goto done;
    if (p.using_shield && p.direction==(a.direction^1)) {
        fx.jingle=0x16; a.body.collision.collision_flags=0xff; goto done;
    }
    if (!(a.physics_flags&0x80) && p.animation!=0x4e && p.animation!=0x4f &&
        !p.ignore_collisions && !p.invincibility && !p.immune && !p.ocarina && !p.got_item) {
        unsigned damage=rom[0xc7f1+a.health_group];
        if (p.tunic==2) damage>>=1;
        else if (p.power_up==2) damage=damage==4?0:damage>>1;
        p.subtract_health=(uint8_t)(p.subtract_health+damage);
        p.invincibility=0x50; p.acorn_counter=0; fx.wave=3;
        if (p.power_up && ++p.power_up_hits>=3) {
            p.power_up=0;
            if (!p.boss_battle) {
                if (p.default_music!=0x22) p.music_to_play=p.default_music;
                p.next_music=p.default_music;
            }
        }
        p.boots_charge=p.running=0;
        p.ignore_collisions=0x10;
        recoil_link(&a,&p);
    }
    if (a.body.collision.type==0x0c) a.body.collision.status=0;
    else a.body.collision.collision_flags=0xff;
done:
    *actor=a; *link=p; *effects=fx; return true;
}

static int8_t bounce(int8_t speed) {
    // Negation wraps as an 8-bit value before the three arithmetic shifts.
    int value=(int8_t)(uint8_t)(-(int)speed);
    return (int8_t)(value>=0?value/8:-((-value+7)/8));
}
bool dx_projectile_move(const uint8_t *rom,size_t size,const DXWorldRoom *const rooms[256],
                         DXActor *actor,uint8_t link_motion,bool palette_fade,bool room_transition,
                         DXProjectileEffects *effects) {
    if (!rom || size!=1024*1024 || !rooms || !supported(actor) || !effects) return false;
    DXActor a=*actor; DXProjectileEffects fx={0};
    if (a.body.collision.countdown==1) { a.body.collision.status=0; goto done; }
    if (a.body.collision.countdown && a.body.collision.type!=0x0a) {
        static const uint8_t spin[]={0,3,1,2};
        a.sprite=spin[(a.body.collision.countdown>>3)&3];
    }
    if (!dx_entity_move_axis(&a.body.x,a.ground.speed_x) || !dx_entity_move_axis(&a.body.y,a.ground.speed_y)) return false;
    if (a.body.collision.countdown) {
        DXEntityAxis z={a.body.collision.z,a.z_fraction};
        if (!dx_entity_move_axis(&z,a.ground.speed_z)) return false;
        a.body.collision.z=(uint8_t)z.position; a.z_fraction=z.fraction;
        a.ground.speed_z=(int8_t)(uint8_t)(a.ground.speed_z-2);
        goto done;
    }
    DXTerrainPoint point;
    int64_t sample_y=(int64_t)a.body.y.position-8;
    if (sample_y<INT32_MIN || !dx_collision_world_point(rom,size,rooms,a.body.x.position,(int32_t)sample_y,&point)) return false;
    unsigned physics=point.physics;
    bool blocked=false;
    if (point.object==0xac || (point.object==0xab && (link_motion==5 || palette_fade || room_transition))) {
        // Enemy projectiles never ignite torches. A lit/fading torch passes.
    } else if (physics==0xff) a.body.collision.status=0;
    else if (physics>=0xd0 && physics<0xd4) {
        if (physics-0xd0==a.body.collision.thrown_direction) {
            if (a.body.collision.z) a.body.collision.ledge_count++;
            else blocked=true;
        } else if (!a.body.collision.ledge_count) blocked=true;
        else if (a.body.collision.frame&1) a.body.collision.ledge_count--;
    } else if (!(physics>=0x7c && physics<0x90) && physics<0xa0 && physics!=0x50 && physics!=0x51) {
        blocked=physics>=0x10 || physics==1;
    }
    if (blocked) {
        a.body.collision.collision_flags=1;
        a.body.x.position=actor->body.x.position; a.body.y.position=actor->body.y.position;
    }
    if (a.body.collision.collision_flags) {
        a.body.collision.countdown=0x18; a.ground.speed_z=0x10;
        if (a.body.collision.collision_flags!=0xff) fx.jingle=7;
        fx.alert_sword_moblins=true;
        a.ground.speed_x=bounce(a.ground.speed_x); a.ground.speed_y=bounce(a.ground.speed_y);
    }
done:
    *actor=a; *effects=fx; return true;
}
bool dx_projectile_step(const uint8_t *rom,size_t size,const DXWorldRoom *const rooms[256],
                         DXActor *actor,DXProjectileLink *link,bool interactive,
                         bool palette_fade,bool room_transition,DXProjectileEffects *effects) {
    if (!rom || size!=1024*1024 || !rooms || !supported(actor) || !link || !effects) return false;
    DXActor a=*actor; DXProjectileLink p=*link; DXProjectileEffects fx={0},motion={0};
    if (!a.body.collision.countdown && !dx_projectile_contact(rom,size,&a,&p,&fx)) return false;
    if (interactive) {
        if (!dx_projectile_move(rom,size,rooms,&a,p.motion,palette_fade,room_transition,&motion)) return false;
        if (motion.jingle) fx.jingle=motion.jingle;
        fx.alert_sword_moblins=motion.alert_sword_moblins;
    }
    *actor=a; *link=p; *effects=fx; return true;
}
