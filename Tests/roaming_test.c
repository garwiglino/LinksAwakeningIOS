#include "DXRoaming.h"
#include "gbrt.h"
#include "rom.h"
#include "GameIdentity.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
typedef struct { uint8_t seed, frame; } Random;
static uint8_t next_random(void *opaque) {
    Random *r=opaque;
    uint8_t sum=r->seed+r->frame; // LCD disabled in this oracle: LY=0.
    r->seed=(sum>>1)|(sum<<7);
    return r->seed;
}
static DXRoamingAction original_decision(GBContext *ctx) {
    ctx->sp=0xdfe0; gb_push16(ctx,0xc000); ctx->pc=0x5859; ctx->bc=7;
    gb_write8(ctx,0x2100,3);
    for (unsigned step=0;step<2000;step++) {
        // Stop where the decision delegates movement, physics or spawning.
        // These effects are deliberately not replaced by fake implementations.
        switch(ctx->pc) {
            case 0x7f2f: return DX_ROAM_MOVE_AND_ANIMATE;
            case 0x7897: return DX_ROAM_GROUND_ONLY;
            case 0x5942: return DX_ROAM_FIRE_ARROW_AND_GROUND;
            case 0x5993: return DX_ROAM_FIRE_ROCK;
            case 0xc000: return DX_ROAM_WAIT;
        }
        ctx->stopped=0; ctx->ime=0; gb_reset_frame(ctx); gb_interpret(ctx,ctx->pc);
    }
    assert(!"original roaming decision did not reach a boundary"); return DX_ROAM_WAIT;
}
int main(void) {
    if (DX_IS_FIXTURE || strcmp(DX_ROM_SHA256,"211a1d1559dc2684d63d87f62a6b1e9109201a6383b4004e9dd5e18ee926ee83")) {
        fprintf(stderr,"SKIP: verified French ROM required\n"); return 0;
    }
    GBConfig config={.model=GB_MODEL_CGB,.enable_audio=false};
    GBContext *ctx=gb_context_create(&config); assert(ctx); rom_init(ctx); gb_write8(ctx,0xff40,0);
    const uint8_t types[]={9,0x0b,0x24}, timers[]={0,1,10,32};
    unsigned count=0,actions[5]={0};
    uint32_t random=0x31415926;
    for (unsigned type=0;type<3;type++) for (unsigned profile=0;profile<8192;profile++) {
        random=random*1664525u+1013904223u;
        DXRoamingState state={.type=types[type],.state=(profile>>10)&3,.direction=(profile>>1)&3,
            .turn_counter=(profile>>3)&3,.countdown=timers[(profile>>5)&3],
            .attack_cooldown=(profile&128)?9:0,.collisions=(profile&256)?3:0,
            .speed_x=8,.speed_y=-8};
        DXRoamingContext world={.x=64,.y=64,.link_x=32+(random&63),.link_y=32+((random>>8)&63),
                                .z=3,.active_state=profile&1,.credits=(profile&512)!=0};
        Random rng={.seed=(uint8_t)(random>>16),.frame=17};
        gb_write8(ctx,0xffeb,state.type); gb_write8(ctx,0xfff0,world.active_state);
        gb_write8(ctx,0xc297,state.state); gb_write8(ctx,0xc387,state.direction);
        gb_write8(ctx,0xc2b7,state.turn_counter); gb_write8(ctx,0xc2e7,state.countdown);
        gb_write8(ctx,0xc2f7,state.attack_cooldown); gb_write8(ctx,0xc2a7,state.collisions);
        gb_write8(ctx,0xc247,state.speed_x); gb_write8(ctx,0xc257,state.speed_y);
        gb_write8(ctx,0xc207,world.x); gb_write8(ctx,0xc217,world.y); gb_write8(ctx,0xc317,world.z);
        gb_write8(ctx,0xff98,world.link_x); gb_write8(ctx,0xff99,world.link_y);
        gb_write8(ctx,0xffe7,rng.frame); gb_write8(ctx,0xc13d,rng.seed);
        gb_write8(ctx,0xdb95,world.credits?1:0x0b);
        DXRoamingAction expected=original_decision(ctx),action;
        assert(dx_roaming_decide(&state,&world,next_random,&rng,&action));
        assert(action==expected);
        assert(state.state==gb_read8(ctx,0xc297) && state.direction==gb_read8(ctx,0xc387));
        assert(state.turn_counter==gb_read8(ctx,0xc2b7) && state.countdown==gb_read8(ctx,0xc2e7));
        assert(state.collisions==gb_read8(ctx,0xc2a7));
        assert((uint8_t)state.speed_x==gb_read8(ctx,0xc247) && (uint8_t)state.speed_y==gb_read8(ctx,0xc257));
        assert(rng.seed==gb_read8(ctx,0xc13d));
        actions[action]++; count++;
    }
    for (unsigned i=0;i<5;i++) assert(actions[i]>0);
    DXRoamingContext far={.x=0,.y=0,.link_x=400,.link_y=0};
    assert(dx_roaming_direction(&far)==0); // no old 8-bit sign alias
    far.link_x=-400; assert(dx_roaming_direction(&far)==1);
    far.x=INT32_MIN; far.link_x=INT32_MAX; assert(dx_roaming_direction(&far)==0);
    far.x=far.link_x=0; far.y=INT32_MAX; far.link_y=INT32_MIN; assert(dx_roaming_direction(&far)==2);
    gb_context_destroy(ctx);
    fprintf(stderr,"PASS: %u original roaming decisions; all five action paths covered; wide targeting checked\n",count);
    return 0;
}
