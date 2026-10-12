#include "actor_oracle.h"
#include "DXActorBridge.h"

static uint8_t memory[0x8000];
static void unchanged(GBContext *ctx) { assert(!memcmp(memory,ctx->wram,sizeof memory)); }
static void seed(GBContext *ctx,unsigned value) {
    for (unsigned i=0;i<0x8000;i++) ctx->wram[i]=(uint8_t)(i*17+value);
    for (unsigned i=0;i<0x7f;i++) ctx->hram[i]=(uint8_t)(i*31+value);
    for (unsigned s=0;s<16;s++) {
        DXActor a=sample(value+s*7);
        write_actor(ctx,s,&a);
        gb_write8(ctx,0xc220+s,0); gb_write8(ctx,0xc230+s,0);
    }
}

int main(void) {
    GBConfig config={.model=GB_MODEL_CGB,.enable_audio=false};
    GBContext *ctx=gb_context_create(&config); assert(ctx);
    gb_write8(ctx,0xff70,1);
    DXActorBridgeFrame baseline, after;
    unsigned roundtrips=0;
    // All signed 16-bit positions, every physical slot, both axes. The other
    // entity fields come from the independent original-ROM oracle mapping.
    for (unsigned value=0;value<65536;value++) {
        seed(ctx,value);
        for (unsigned slot=0;slot<16;slot++) {
            unsigned x=(value+slot*17)&65535, y=(65535-value+slot*31)&65535;
            gb_write8(ctx,0xc200+slot,x); gb_write8(ctx,0xc220+slot,x>>8);
            gb_write8(ctx,0xc210+slot,y); gb_write8(ctx,0xc230+slot,y>>8);
        }
        assert(dx_actor_bridge_read(ctx,2400,1920,&baseline));
        for (unsigned slot=0;slot<16;slot++) {
            const DXActor *a=&baseline.roster.slots[slot];
            unsigned x=(value+slot*17)&65535, y=(65535-value+slot*31)&65535;
            assert(a->body.x.position==2400+(int)x-(x>=32768?65536:0));
            assert(a->body.y.position==1920+(int)y-(y>=32768?65536:0));
            for(unsigned f=0;f<sizeof fields/sizeof *fields;f++)
                assert(*((const uint8_t*)a+fields[f].offset)==gb_read8(ctx,fields[f].address+slot));
            for(unsigned i=0;i<4;i++) assert(a->damage_hitbox[i]==gb_read8(ctx,0xd580+slot*4+i));
            assert(a->ground.falling_x==2400+gb_read8(ctx,0xc4b0+slot));
            assert(a->ground.falling_y==1920+gb_read8(ctx,0xc4c0+slot));
            roundtrips++;
        }
        memcpy(memory,ctx->wram,sizeof memory);
        assert(dx_actor_bridge_commit(ctx,&baseline,&baseline.roster)); unchanged(ctx);
    }
    // Adjacent room coordinate frames describe the same actor. In particular,
    // the left neighbor must remain negative locally, not alias to x=240.
    seed(ctx,0);
    gb_write8(ctx,0xc200,144); gb_write8(ctx,0xc220,0);
    assert(dx_actor_bridge_read(ctx,160,1152,&baseline));
    gb_write8(ctx,0xc200,240); gb_write8(ctx,0xc220,255);
    assert(dx_actor_bridge_read(ctx,320,1152,&after));
    assert(baseline.roster.slots[0].body.x.position==after.roster.slots[0].body.x.position);
    // Positive writes cover all fields, signed positions, inactive records and
    // the entire damage-hitbox array. Unrelated bytes must remain unchanged.
    assert(dx_actor_bridge_read(ctx,160,1152,&baseline));
    DXActorRoster changed=baseline.roster;
    uint8_t allowed[0x8000]={0};
    for(unsigned slot=0;slot<16;slot++) {
        DXActor *a=&changed.slots[slot];
        for(unsigned f=0;f<sizeof fields/sizeof *fields;f++) {
            *((uint8_t*)a+fields[f].offset)^=0x5a;
            allowed[fields[f].address-0xc000+slot]=1;
        }
        a->body.hitbox=a->hitbox_flags&3;
        a->body.x.position=160-16; a->body.y.position=1152+300;
        a->ground.falling_x=160+31; a->ground.falling_y=1152+221;
        for(unsigned i=0;i<4;i++) { a->damage_hitbox[i]^=0x5a; allowed[0x1580+slot*4+i]=1; }
        const unsigned positions[]={0x200,0x210,0x220,0x230,0x4b0,0x4c0};
        for(unsigned i=0;i<6;i++) allowed[positions[i]+slot]=1;
    }
    memcpy(memory,ctx->wram,sizeof memory);
    assert(dx_actor_bridge_commit(ctx,&baseline,&changed));
    for(unsigned i=0;i<sizeof memory;i++) if(!allowed[i]) assert(memory[i]==ctx->wram[i]);
    assert(dx_actor_bridge_read(ctx,160,1152,&after));
    for(unsigned slot=0;slot<16;slot++) {
        for(unsigned f=0;f<sizeof fields/sizeof *fields;f++)
            assert(*((const uint8_t*)&after.roster.slots[slot]+fields[f].offset)==
                   *((const uint8_t*)&changed.slots[slot]+fields[f].offset));
        assert(after.roster.slots[slot].body.x.position==144);
        assert(after.roster.slots[slot].body.y.position==1452);
        assert(after.roster.slots[slot].ground.falling_x==191);
        assert(after.roster.slots[slot].ground.falling_y==1373);
        assert(!memcmp(after.roster.slots[slot].damage_hitbox,changed.slots[slot].damage_hitbox,4));
    }
    // An actual native phase: original timers -> world record -> native timers
    // -> original tables. Compare every owned field with interpreter execution.
    unsigned native_steps=0;
    if (!DX_IS_FIXTURE && !strcmp(DX_ROM_SHA256,"211a1d1559dc2684d63d87f62a6b1e9109201a6383b4004e9dd5e18ee926ee83")) {
        rom_init(ctx); gb_write8(ctx,0xff40,0); gb_write8(ctx,0xff70,1);
        for(unsigned value=0;value<1024;value++) {
            seed(ctx,value); gb_write8(ctx,0xdb95,0xb); gb_write8(ctx,0xc16b,4);
            gb_write8(ctx,0xc124,0); gb_write8(ctx,0xc14f,0);
            gb_write8(ctx,0xc19f,0); gb_write8(ctx,0xc166,0);
            assert(dx_actor_bridge_read(ctx,160,1152,&baseline));
            DXActorRoster next=baseline.roster;
            DXActorRenderState render;
            assert(dx_actor_update_timers(&next.slots[7],&baseline.clock,&render));
            assert(dx_actor_bridge_commit(ctx,&baseline,&next));
            uint8_t expected[sizeof fields/sizeof *fields];
            for(unsigned f=0;f<sizeof fields/sizeof *fields;f++) expected[f]=gb_read8(ctx,fields[f].address+7);
            // Restore the before image through the same adapter, then run ROM.
            assert(dx_actor_bridge_read(ctx,160,1152,&after));
            assert(dx_actor_bridge_commit(ctx,&after,&baseline.roster));
            original(ctx,0x14,0x4d73,7);
            for(unsigned f=0;f<sizeof fields/sizeof *fields;f++) assert(expected[f]==gb_read8(ctx,fields[f].address+7));
            native_steps++;
        }
    }
    seed(ctx,0);
    assert(dx_actor_bridge_read(ctx,160,1152,&baseline));
    DXActorRoster next=baseline.roster;
    next.slots[0].health=17;
    next.slots[15].body.x.position=160+32768; // invalid LAST slot: no partial write
    memcpy(memory,ctx->wram,sizeof memory);
    assert(!dx_actor_bridge_commit(ctx,&baseline,&next)); unchanged(ctx);
    next=baseline.roster; next.slots[15].ground.falling_x=159;
    assert(!dx_actor_bridge_commit(ctx,&baseline,&next)); unchanged(ctx);
    next=baseline.roster; next.slots[15].body.collision.link_immune^=1;
    assert(!dx_actor_bridge_commit(ctx,&baseline,&next)); unchanged(ctx);
    next=baseline.roster; next.slots[15].body.hitbox^=1;
    assert(!dx_actor_bridge_commit(ctx,&baseline,&next)); unchanged(ctx);
    next=baseline.roster; next.slots[15].body.no_bg_collision=true;
    assert(!dx_actor_bridge_commit(ctx,&baseline,&next)); unchanged(ctx);
    // A reused slot or a changed room/clock must invalidate the entire write.
    gb_write8(ctx,0xc3af,gb_read8(ctx,0xc3af)^1);
    memcpy(memory,ctx->wram,sizeof memory);
    assert(!dx_actor_bridge_commit(ctx,&baseline,&baseline.roster)); unchanged(ctx);
    assert(dx_actor_bridge_read(ctx,160,1152,&baseline));
    ctx->hram[0x76]^=1;
    assert(!dx_actor_bridge_commit(ctx,&baseline,&baseline.roster)); unchanged(ctx);
    assert(dx_actor_bridge_read(ctx,160,1152,&baseline));
    ctx->hram[0x67]^=1;
    assert(!dx_actor_bridge_commit(ctx,&baseline,&baseline.roster)); unchanged(ctx);
    // Coordinate-origin overflow and unsupported WRAM banking fail unchanged.
    after=baseline;
    assert(!dx_actor_bridge_read(ctx,INT32_MAX,0,&after));
    assert(!memcmp(&after,&baseline,sizeof after));
    gb_write8(ctx,0xff70,2);
    assert(!dx_actor_bridge_read(ctx,0,0,&after));
    assert(!memcmp(&after,&baseline,sizeof after));
    gb_context_destroy(ctx);
    fprintf(stderr,"PASS: %u signed entity round-trips, %u native timer round-trips against ROM; stale/overflow/partial-write rejection\n",roundtrips,native_steps);
    return 0;
}
