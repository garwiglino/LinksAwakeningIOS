#include "DXActors.h"
#include <limits.h>
#include <string.h>

bool dx_actor_configure(const uint8_t *rom, size_t size, uint8_t room, DXActor *actor) {
    if (!rom || size!=1024*1024 || !actor || actor->body.collision.type>0xfa) return false;
    DXActor a=*actor;
    unsigned type=a.body.collision.type;
    a.body.x.fraction=a.body.y.fraction=a.z_fraction=0;
    a.ground.speed_x=a.ground.speed_y=a.ground.speed_z=0;
    a.body.collision.z=a.body.collision.state=a.body.collision.collision_flags=0;
    a.body.collision.countdown=a.body.collision.ignore_hits=a.body.collision.ledge_count=0;
    a.body.collision.thrown_direction=0xff;
    memset(a.private_state,0,sizeof a.private_state);
    a.attack_cooldown=a.private_countdown2=a.slow_countdown=0;
    a.direction=a.sprite=a.inertia=a.lifted=a.dropped_item=a.power_recoil=0;
    a.ground.ground_status=a.ground.splash_timer=a.ground.flash_timer=0;
    a.home_room=room; a.load_order=0xff;
    a.physics_flags=rom[0xc000+type];
    a.hitbox_flags=rom[0xc0fb+type];
    a.body.hitbox=a.hitbox_flags&3;
    a.health_group=rom[0xc1f6+type];
    a.health=rom[0xc7bc+a.health_group];
    a.body.collision.options=rom[0xc2f1+type];
    memcpy(a.damage_hitbox,rom+0x3aa5+(a.hitbox_flags&0x7c),4);
    *actor=a;
    return true;
}

static void decrement(uint8_t *value) { if (*value) --*value; }
bool dx_actor_update_timers(DXActor *actor, const DXActorClock *clock, DXActorRenderState *render) {
    if (!actor || !clock || !render) return false;
    *render=(DXActorRenderState){0};
    actor->body.no_bg_collision=false;
    if (clock->gameplay!=1 && (clock->gameplay!=0x0b ||
        (clock->transition_sequence!=4 && actor->body.collision.type!=0x83))) return true;
    if (clock->room_transition || clock->inventory || clock->dialog || clock->ocarina) return true;
    decrement(&actor->body.collision.countdown);
    decrement(&actor->attack_cooldown);
    decrement(&actor->private_countdown2);
    decrement(&actor->ground.splash_timer);
    if (!(clock->frame&3)) decrement(&actor->slow_countdown);
    render->flip=(actor->ground.flash_timer<<2)&0x10;
    decrement(&actor->ground.flash_timer);
    return true;
}

DXSpawnResult dx_actor_spawn_projectile(const uint8_t *rom, size_t size,
                                        DXActorRoster *roster, unsigned parent,
                                        uint8_t current_room, uint8_t projectile_type,
                                        unsigned *spawned_slot) {
    if (!rom || size!=1024*1024 || !roster || parent>=16 || !spawned_slot ||
        (projectile_type!=0x0a && projectile_type!=0x0c)) return DX_SPAWN_INVALID;
    const DXActor *source=&roster->slots[parent];
    if (!source->body.collision.status || source->direction>3) return DX_SPAWN_INVALID;
    int slot=15;
    while(slot>=0 && roster->slots[slot].body.collision.status) slot--;
    if (slot<0) return DX_SPAWN_FULL;
    unsigned direction=source->direction;
    unsigned ox=projectile_type==0x0c ? 0xd932 : 0xd987;
    unsigned oy=projectile_type==0x0c ? 0xd936 : 0xd989;
    unsigned vx=projectile_type==0x0c ? 0xd93a : 0xd98d;
    unsigned vy=projectile_type==0x0c ? 0xd93e : 0xd98f;
    int64_t x=(int64_t)source->body.x.position+(int8_t)rom[ox+direction];
    int64_t y=(int64_t)source->body.y.position+(int8_t)rom[oy+direction];
    if (x<INT32_MIN || x>INT32_MAX || y<INT32_MIN || y>INT32_MAX) return DX_SPAWN_INVALID;
    DXActor next=roster->slots[slot];
    next.body.collision.type=projectile_type;
    next.body.collision.status=5;
    if (!dx_actor_configure(rom,size,current_room,&next)) return DX_SPAWN_INVALID;
    next.body.collision.ignore_hits=1;
    next.body.x.position=(int32_t)x; next.body.y.position=(int32_t)y;
    next.direction=direction;
    next.sprite=projectile_type==0x0c ? direction : 0;
    next.ground.speed_x=(int8_t)rom[vx+direction]; next.ground.speed_y=(int8_t)rom[vy+direction];
    roster->slots[slot]=next; *spawned_slot=(unsigned)slot;
    return DX_SPAWN_OK;
}

static uint8_t random_byte(void *opaque) {
    DXActorRandom *r=opaque;
    uint8_t value=r->seed+r->frame+r->scanline;
    r->seed=(value>>1)|(value<<7);
    return r->seed;
}
bool dx_actor_roaming_continue(const uint8_t *rom, size_t size,
                               const DXWorldRoom *const rooms[256], DXActorRoster *roster,
                               unsigned slot, uint8_t current_room,
                               const DXRoamingContext *context, DXActorRandom *random,
                               DXRoamingEffects *effects) {
    if (!rom || size!=1024*1024 || !rooms || !roster || slot>=16 || !context || !random || !effects) return false;
    DXActorRoster next=*roster;
    DXActorRandom rng=*random;
    DXActor *a=&next.slots[slot];
    if (a->body.collision.indoor || a->ground.side_scrolling) return false;
    DXRoamingEffects fx={.spawned_slot=-1};
    DXRoamingState state={.type=a->body.collision.type,.state=a->body.collision.state,
        .direction=a->direction,.turn_counter=a->private_state[0],.countdown=a->body.collision.countdown,
        .attack_cooldown=a->attack_cooldown,.collisions=a->body.collision.collision_flags,
        .speed_x=a->ground.speed_x,.speed_y=a->ground.speed_y};
    // A caller must supply the active pre-combat state copy explicitly. The
    // spatial part always comes from this actor, avoiding stale room offsets.
    DXRoamingContext target=*context;
    target.x=a->body.x.position; target.y=a->body.y.position; target.z=(int8_t)a->body.collision.z;
    if (!dx_roaming_decide(&state,&target,random_byte,&rng,&fx.action)) return false;
    a->body.collision.state=state.state; a->direction=state.direction;
    a->private_state[0]=state.turn_counter; a->body.collision.countdown=state.countdown;
    a->body.collision.collision_flags=state.collisions;
    a->ground.speed_x=state.speed_x; a->ground.speed_y=state.speed_y;
    if (fx.action==DX_ROAM_FIRE_ARROW_AND_GROUND || fx.action==DX_ROAM_FIRE_ROCK) {
        unsigned spawned;
        DXSpawnResult result=dx_actor_spawn_projectile(rom,size,&next,slot,current_room,
            fx.action==DX_ROAM_FIRE_ROCK ? 0x0a : 0x0c,&spawned);
        if (result==DX_SPAWN_INVALID) return false;
        if (result==DX_SPAWN_OK) fx.spawned_slot=(int)spawned;
    }
    if (fx.action==DX_ROAM_MOVE_AND_ANIMATE || fx.action==DX_ROAM_GROUND_ONLY || fx.action==DX_ROAM_FIRE_ARROW_AND_GROUND) {
        if (!dx_physics_step(rom,size,rooms,fx.action==DX_ROAM_MOVE_AND_ANIMATE,
                             &a->body,&a->ground,&fx.contact,&fx.ground)) return false;
    }
    if (fx.action==DX_ROAM_MOVE_AND_ANIMATE) {
        static const uint8_t variants[]={6,4,2,0};
        a->inertia++;
        a->sprite=variants[a->direction]|((a->inertia>>3)&1);
    }
    *roster=next; *random=rng; *effects=fx;
    return true;
}
