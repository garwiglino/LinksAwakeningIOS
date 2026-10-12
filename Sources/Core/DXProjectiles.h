#pragma once
#include "DXActors.h"

// Shared player state touched by enemy arrows/rocks in top-down rooms.
typedef struct {
    int32_t x,y;
    uint8_t z,motion,animation,direction,using_shield;
    uint8_t ignore_collisions,invincibility,immune,ocarina,got_item;
    uint8_t tunic,power_up,power_up_hits,acorn_counter,subtract_health;
    uint8_t boots_charge,running,boss_battle,default_music,music_to_play,next_music;
    int8_t speed_x,speed_y;
} DXProjectileLink;
typedef struct { uint8_t jingle,wave; bool alert_sword_moblins; } DXProjectileEffects;

// These phases support Moblin arrows and Octorok rocks, overworld only.
// They do not replace the player-arrow/bomb-arrow/fireball handlers.
bool dx_projectile_contact(const uint8_t *rom,size_t size,DXActor *actor,
                            DXProjectileLink *link,DXProjectileEffects *effects);
bool dx_projectile_move(const uint8_t *rom,size_t size,const DXWorldRoom *const rooms[256],
                         DXActor *actor,uint8_t link_motion,bool palette_fade,bool room_transition,
                         DXProjectileEffects *effects);
// Original contact-before-interactivity ordering; caller already updated timers
// and supplies the original active-status/gate result. Atomic actor+Link+effects.
bool dx_projectile_step(const uint8_t *rom,size_t size,const DXWorldRoom *const rooms[256],
                         DXActor *actor,DXProjectileLink *link,bool interactive,
                         bool palette_fade,bool room_transition,DXProjectileEffects *effects);
