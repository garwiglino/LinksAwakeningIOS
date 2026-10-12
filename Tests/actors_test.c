#include "actor_oracle.h"
int main(void) {
    if(DX_IS_FIXTURE || strcmp(DX_ROM_SHA256,"211a1d1559dc2684d63d87f62a6b1e9109201a6383b4004e9dd5e18ee926ee83")) {
        fprintf(stderr,"SKIP: verified French ROM required\n"); return 0;
    }
    GBConfig config={.model=GB_MODEL_CGB,.enable_audio=false};
    GBContext *ctx=gb_context_create(&config); assert(ctx); rom_init(ctx); gb_write8(ctx,0xff40,0); gb_write8(ctx,0xff70,1);
    unsigned configured=0,timers=0,spawns=0,continuations=0;
    for(unsigned type=0;type<=0xfa;type++) for(unsigned v=0;v<16;v++) {
        DXActor a=sample(v*17); a.body.collision.type=type;
        write_actor(ctx,7,&a); gb_write8(ctx,0xfff6,v*17);
        original(ctx,3,0x485b,7);
        assert(dx_actor_configure(ctx->rom,ctx->rom_size,v*17,&a)); compare_actor(ctx,7,&a);
        assert(a.body.hitbox==(gb_read8(ctx,0xc357)&3)); configured++;
    }
    for(unsigned v=0;v<32768;v++) {
        DXActor a=sample(v); a.body.collision.type=(v&1)?0x83:9;
        DXActorClock clock={.gameplay=(uint8_t[]){0,1,7,0xb}[(v>>1)&3],.transition_sequence=(v&8)?4:0,
            .room_transition=(v>>4)&1,.inventory=(v>>5)&1,.dialog=(v>>6)&1,.ocarina=(v>>7)&1,.frame=(v>>8)&3};
        a.body.collision.countdown=v&0xff; a.ground.flash_timer=(v>>7)&0xff;
        write_actor(ctx,7,&a);
        gb_write8(ctx,0xdb95,clock.gameplay); gb_write8(ctx,0xc16b,clock.transition_sequence);
        gb_write8(ctx,0xc124,clock.room_transition); gb_write8(ctx,0xc14f,clock.inventory);
        gb_write8(ctx,0xc19f,clock.dialog); gb_write8(ctx,0xc166,clock.ocarina); gb_write8(ctx,0xffe7,clock.frame);
        gb_write8(ctx,0xffed,0xff); gb_write8(ctx,0xffbe,0xff); gb_write8(ctx,0xfff5,0xff);
        original(ctx,0x14,0x4d73,7);
        DXActorRenderState render={0}; assert(dx_actor_update_timers(&a,&clock,&render)); compare_actor(ctx,7,&a);
        assert(render.flip==gb_read8(ctx,0xffed) && render.tiles_offset==gb_read8(ctx,0xfff5));
        assert(render.no_bg_collision==gb_read8(ctx,0xffbe) && !a.body.no_bg_collision); timers++;
    }
    for(unsigned projectile=0;projectile<2;projectile++) for(unsigned direction=0;direction<4;direction++) for(unsigned mask=0;mask<256;mask++) {
        DXActorRoster roster;
        for(unsigned slot=0;slot<16;slot++) {
            roster.slots[slot]=sample(slot*7+mask);
            roster.slots[slot].body.collision.status=(mask&(1u<<(slot&7)))?5:0;
        }
        roster.slots[7].body.collision.status=5; roster.slots[7].direction=direction;
        for(unsigned slot=0;slot<16;slot++) write_actor(ctx,slot,&roster.slots[slot]);
        gb_write8(ctx,0xfff6,0x91);
        original(ctx,3,projectile?0x5942:0x5993,7);
        unsigned spawned=99;
        DXSpawnResult result=dx_actor_spawn_projectile(ctx->rom,ctx->rom_size,&roster,7,0x91,projectile?0x0c:0x0a,&spawned);
        assert(result==(ctx->f_c?DX_SPAWN_FULL:DX_SPAWN_OK));
        if(result==DX_SPAWN_FULL) assert(spawned==99); else assert(spawned==ctx->e);
        for(unsigned slot=0;slot<16;slot++) compare_actor(ctx,slot,&roster.slots[slot]); spawns++;
    }
    DXWorldRoom scene={.room=0x91}; const DXWorldRoom *rooms[256]={0}; rooms[0x91]=&scene;
    for(unsigned type=0;type<3;type++) for(unsigned v=0;v<8192;v++) {
        unsigned object=v&255;
        for(unsigned i=0;i<256;i++) { scene.blocks[i]=object; gb_write8(ctx,0xd700+i,object); }
        DXActorRoster roster={0};
        for(unsigned i=0;i<16;i++) {
            roster.slots[i]=sample(i); roster.slots[i].body.collision.status=(v&256)?5:0;
        }
        DXActor *a=&roster.slots[7]; a->body.collision.type=(uint8_t[]){9,0x0b,0x24}[type];
        assert(dx_actor_configure(ctx->rom,ctx->rom_size,0x91,a));
        a->body.collision.status=5; a->body.collision.state=(v>>8)&3;
        a->body.collision.countdown=(uint8_t[]){0,1,10,32}[(v>>10)&3];
        a->direction=(v>>7)&3; a->ground.speed_x=8; a->ground.speed_y=-8;
        a->body.x.fraction=123; a->body.y.fraction=231;
        a->body.collision.frame=17;
        DXRoamingContext context={.active_state=(v>>9)&1,.link_x=240,.link_y=1224,.credits=(v&4096)!=0};
        // Ensure firing can occur in every facing direction without far aliases.
        context.link_x+=(int[]){32,-32,0,0}[a->direction]; context.link_y+=(int[]){0,0,-32,32}[a->direction];
        for(unsigned slot=0;slot<16;slot++) write_actor(ctx,slot,&roster.slots[slot]);
        gb_write8(ctx,0xffeb,a->body.collision.type); gb_write8(ctx,0xfff0,context.active_state);
        gb_write8(ctx,0xffee,80); gb_write8(ctx,0xffef,72); gb_write8(ctx,0xff98,context.link_x-160); gb_write8(ctx,0xff99,context.link_y-1152);
        gb_write8(ctx,0xdb95,context.credits?1:0xb); gb_write8(ctx,0xffe7,17); gb_write8(ctx,0xc13d,v);
        gb_write8(ctx,0xdba5,0); gb_write8(ctx,0xfff6,0x91); gb_write8(ctx,0xfff7,0); gb_write8(ctx,0xfff9,0);
        gb_write8(ctx,0xffbe,0); gb_write8(ctx,0xfff2,0); gb_write8(ctx,0xc503,0); gb_write8(ctx,0xc50d,0);
        gb_write8(ctx,0xd6fb,0); gb_write8(ctx,0xd6f9,0); gb_write8(ctx,0xc1c6,0);
        bool splash=original(ctx,3,0x5859,7);
        DXActorRandom rng={.seed=v,.frame=17}; DXRoamingEffects effects;
        assert(dx_actor_roaming_continue(ctx->rom,ctx->rom_size,rooms,&roster,7,0x91,&context,&rng,&effects));
        for(unsigned slot=0;slot<16;slot++) compare_actor(ctx,slot,&roster.slots[slot]);
        assert(rng.seed==gb_read8(ctx,0xc13d));
        assert(effects.ground.splash==splash && effects.ground.jingle==gb_read8(ctx,0xfff2));
        assert(effects.contact.horizontal_object==gb_read8(ctx,0xc503) && effects.contact.vertical_object==gb_read8(ctx,0xc50d));
        continuations++;
    }
    // Failure after RNG consumption or allocation commits no partial turn.
    DXActorRoster roster={0}; DXActor *a=&roster.slots[7];
    a->body.x.position=240; a->body.y.position=1224; a->body.collision.type=0x0b;
    assert(dx_actor_configure(ctx->rom,ctx->rom_size,0x91,a)); a->body.collision.status=5;
    DXRoamingContext target={.link_x=280,.link_y=1224};
    DXActorRandom rng={.seed=123,.frame=17}; DXRoamingEffects fx={.spawned_slot=44};
    rooms[0x91]=NULL;
    for(unsigned branch=0;branch<2;branch++) {
        a->body.collision.state=branch; target.active_state=branch;
        a->body.collision.countdown=branch?10:0;
        DXActorRoster saved=roster; DXActorRandom saved_rng=rng; DXRoamingEffects saved_fx=fx;
        assert(!dx_actor_roaming_continue(ctx->rom,ctx->rom_size,rooms,&roster,7,0x91,&target,&rng,&fx));
        assert(!memcmp(&roster,&saved,sizeof roster) && !memcmp(&rng,&saved_rng,sizeof rng));
        assert(!memcmp(&fx,&saved_fx,sizeof fx));
    }
    a->body.x.position=INT32_MAX;
    DXActorRoster saved=roster; unsigned invalid_slot=99;
    assert(dx_actor_spawn_projectile(ctx->rom,ctx->rom_size,&roster,7,0x91,0x0c,&invalid_slot)==DX_SPAWN_INVALID);
    assert(!memcmp(&roster,&saved,sizeof roster) && invalid_slot==99);
    fprintf(stderr,"RNG/allocation rollback and coordinate overflow passed\n");
    gb_context_destroy(ctx);
    fprintf(stderr,"PASS: %u configurations, %u timer states, %u projectile allocations, %u integrated roaming continuations\n",configured,timers,spawns,continuations);
}
