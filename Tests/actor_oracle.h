#pragma once
#include "DXActors.h"
#include "gbrt.h"
#include "rom.h"
#include "GameIdentity.h"
#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
static char oracle_case[100];
#define FIELD(address, field) {address,offsetof(DXActor,field),#field}
static const struct { unsigned address; size_t offset; const char *name; } fields[]={
 FIELD(0xc240,ground.speed_x),FIELD(0xc250,ground.speed_y),FIELD(0xc260,body.x.fraction),FIELD(0xc270,body.y.fraction),
 FIELD(0xc280,body.collision.status),FIELD(0xc290,body.collision.state),FIELD(0xc2a0,body.collision.collision_flags),
 FIELD(0xc2b0,private_state[0]),FIELD(0xc2c0,private_state[1]),FIELD(0xc2d0,private_state[2]),
 FIELD(0xc2e0,body.collision.countdown),FIELD(0xc2f0,attack_cooldown),FIELD(0xc300,private_countdown2),
 FIELD(0xc310,body.collision.z),FIELD(0xc320,ground.speed_z),FIELD(0xc330,z_fraction),
 FIELD(0xc340,physics_flags),FIELD(0xc350,hitbox_flags),FIELD(0xc360,health),FIELD(0xc380,direction),
 FIELD(0xc390,private_state[4]),FIELD(0xc3a0,body.collision.type),FIELD(0xc3b0,sprite),FIELD(0xc3d0,inertia),
 FIELD(0xc3e0,home_room),FIELD(0xc3f0,recoil_x),FIELD(0xc400,recoil_y),FIELD(0xc410,body.collision.ignore_hits),
 FIELD(0xc420,ground.flash_timer),FIELD(0xc430,body.collision.options),FIELD(0xc440,private_state[3]),
 FIELD(0xc450,slow_countdown),FIELD(0xc460,load_order),FIELD(0xc470,ground.ground_status),FIELD(0xc480,ground.splash_timer),
 FIELD(0xc490,lifted),FIELD(0xc4a0,power_recoil),FIELD(0xc4d0,health_group),FIELD(0xc4e0,dropped_item),
 FIELD(0xc4f0,body.collision.ledge_count),FIELD(0xc5d0,body.collision.thrown_direction)
};
static void write_actor(GBContext *ctx,unsigned slot,const DXActor *a) {
    for(unsigned f=0;f<sizeof fields/sizeof *fields;f++) gb_write8(ctx,fields[f].address+slot,*((const uint8_t*)a+fields[f].offset));
    gb_write8(ctx,0xc200+slot,a->body.x.position-160); gb_write8(ctx,0xc210+slot,a->body.y.position-1152);
    gb_write8(ctx,0xc4b0+slot,a->ground.falling_x-160); gb_write8(ctx,0xc4c0+slot,a->ground.falling_y-1152);
    for(unsigned i=0;i<4;i++) gb_write8(ctx,0xd580+slot*4+i,a->damage_hitbox[i]);
}
static void compare_actor(GBContext *ctx,unsigned slot,const DXActor *a) {
    for(unsigned f=0;f<sizeof fields/sizeof *fields;f++) {
        uint8_t actual=*((const uint8_t*)a+fields[f].offset), expected=gb_read8(ctx,fields[f].address+slot);
        if(actual!=expected) { fprintf(stderr,"%s slot %u field %s: native %02x original %02x\n",oracle_case,slot,fields[f].name,actual,expected); assert(actual==expected); }
    }
    assert(a->body.x.position==160+gb_read8(ctx,0xc200+slot));
    assert(a->body.y.position==1152+gb_read8(ctx,0xc210+slot));
    assert(a->ground.falling_x==160+gb_read8(ctx,0xc4b0+slot));
    assert(a->ground.falling_y==1152+gb_read8(ctx,0xc4c0+slot));
    for(unsigned i=0;i<4;i++) assert(a->damage_hitbox[i]==gb_read8(ctx,0xd580+slot*4+i));
}
static bool original(GBContext *ctx,unsigned bank,unsigned address,unsigned slot) {
    ctx->sp=0xdfe0; gb_push16(ctx,0xc000); ctx->pc=address; ctx->bc=slot;
    gb_write8(ctx,0x2100,bank); bool splash=false;
    for(unsigned i=0;i<4000;i++) {
        if(ctx->pc==0xc000) return splash;
        if(ctx->pc==0x0cb7) splash=true;
        ctx->stopped=ctx->ime=0; gb_reset_frame(ctx); gb_interpret(ctx,ctx->pc);
    }
    fprintf(stderr,"Oracle failed at %02x:%04x\n",ctx->rom_bank,ctx->pc); assert(0); return false;
}
static DXActor sample(unsigned value) {
    DXActor a={0};
    for(unsigned f=0;f<sizeof fields/sizeof *fields;f++) *((uint8_t*)&a+fields[f].offset)=(uint8_t)(value+f*13);
    a.body.x.position=240; a.body.y.position=1224;
    a.ground.falling_x=215; a.ground.falling_y=1218;
    for(unsigned i=0;i<4;i++) a.damage_hitbox[i]=value+i;
    a.body.hitbox=a.hitbox_flags&3;
    return a;
}
