#include "DXGround.h"
#include "gbrt.h"
#include "rom.h"
#include "GameIdentity.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static bool original_ground(GBContext *ctx) {
    ctx->sp=0xdfe0; gb_push16(ctx,0xc000); ctx->pc=0x7897; ctx->bc=7;
    gb_write8(ctx,0x2100,3); bool splash=false;
    for (unsigned step=0;step<2000;step++) {
        if (ctx->pc==0xc000) return splash;
        if (ctx->pc==0x0cb7) splash=true;
        ctx->stopped=0; ctx->ime=0; gb_reset_frame(ctx); gb_interpret(ctx,ctx->pc);
    }
    assert(!"ground phase failed to reach wall resolution"); return false;
}
int main(void) {
    if (DX_IS_FIXTURE || strcmp(DX_ROM_SHA256,"211a1d1559dc2684d63d87f62a6b1e9109201a6383b4004e9dd5e18ee926ee83")) {
        fprintf(stderr,"SKIP: verified French ROM required\n"); return 0;
    }
    GBConfig config={.model=GB_MODEL_CGB,.enable_audio=false};
    GBContext *ctx=gb_context_create(&config); assert(ctx); rom_init(ctx); gb_write8(ctx,0xff40,0);
    // End the oracle at the ground/wall boundary, including fallthrough paths.
    // Only the following wall-phase entry is replaced by RET in this isolated ROM.
    ctx->rom[0xfa1c]=0xc9;
    const uint8_t types[]={0,2,3,9,0x0b,0x14,0x36,0x59,0x6d,0x99,0xa0,0xa8,0xac,0xc1,0xcc,0xd5};
    unsigned checked=0,failures=0;
    for (unsigned group=0;group<3;group++) for (unsigned object=0;object<256;object++) {
        for (unsigned i=0;i<256;i++) gb_write8(ctx,0xd700+i,object);
        for (unsigned t=0;t<sizeof types;t++) for (unsigned v=0;v<64;v++) {
            DXWorldBody body={.x={240,0},.y={1224,0},.collision={
                .type=types[t],.z=(v&16)?((v&8)?128:8):0,
                .options=((v&4)?8:0)|((v&32)?16:0),.ledge_count=5,
                .ignore_hits=v%3,.status=5,.countdown=77,.frame=v&3,.indoor=group}};
            DXGroundState ground={.ground_status=v&3,.flash_timer=42,.splash_timer=(v&8)?0:5,
                .link_motion=(v&2)?6:0,.speed_x=-7,.speed_y=(v&2)?-8:8,
                .speed_z=(v&1)?-32:-16,.falling_x=160+55,.falling_y=1152+66,.side_scrolling=(v&8)!=0};
            DXTerrainPoint point={.object=object,.local_x=15,.local_y=1};
            assert(dx_collision_physics(ctx->rom,ctx->rom_size,object,group,false,&point.physics));
            gb_write8(ctx,0xffeb,body.collision.type); gb_write8(ctx,0xc317,body.collision.z);
            gb_write8(ctx,0xc437,body.collision.options); gb_write8(ctx,0xc4f7,5);
            gb_write8(ctx,0xc417,body.collision.ignore_hits); gb_write8(ctx,0xc287,5);
            gb_write8(ctx,0xc2e7,77); gb_write8(ctx,0xffe7,body.collision.frame);
            gb_write8(ctx,0xdba5,group); gb_write8(ctx,0xfff7,0);
            gb_write8(ctx,0xc477,ground.ground_status); gb_write8(ctx,0xc427,42);
            gb_write8(ctx,0xc487,ground.splash_timer); gb_write8(ctx,0xc11c,ground.link_motion);
            gb_write8(ctx,0xc247,ground.speed_x); gb_write8(ctx,0xc257,ground.speed_y);
            gb_write8(ctx,0xc327,ground.speed_z); gb_write8(ctx,0xc4b7,55); gb_write8(ctx,0xc4c7,66);
            gb_write8(ctx,0xfff9,ground.side_scrolling); gb_write8(ctx,0xfff2,0);
            gb_write8(ctx,0xc207,80); gb_write8(ctx,0xc217,72);
            bool splash=original_ground(ctx); DXGroundEffects effects;
            assert(dx_ground_apply(ctx->rom,ctx->rom_size,&point,&body,&ground,&effects));
            bool mismatch=body.collision.status!=gb_read8(ctx,0xc287) ||
                body.collision.ledge_count!=gb_read8(ctx,0xc4f7) || body.collision.ignore_hits!=gb_read8(ctx,0xc417) ||
                body.collision.countdown!=gb_read8(ctx,0xc2e7) || ground.ground_status!=gb_read8(ctx,0xc477) ||
                ground.flash_timer!=gb_read8(ctx,0xc427) || ground.falling_x!=160+gb_read8(ctx,0xc4b7) ||
                ground.falling_y!=1152+gb_read8(ctx,0xc4c7) || body.x.position!=160+gb_read8(ctx,0xc207) ||
                body.y.position!=1152+gb_read8(ctx,0xc217) || (uint8_t)ground.speed_x!=gb_read8(ctx,0xc247) ||
                (uint8_t)ground.speed_y!=gb_read8(ctx,0xc257) || effects.jingle!=gb_read8(ctx,0xfff2) || effects.splash!=splash;
            if (mismatch) {
                if (failures<12) fprintf(stderr,"ground group %u object %02x physics %02x type %02x profile %u: status %u/%u ground %u/%u splash %u/%u jingle %u/%u\n",group,object,point.physics,types[t],v,body.collision.status,gb_read8(ctx,0xc287),ground.ground_status,gb_read8(ctx,0xc477),effects.splash,splash,effects.jingle,gb_read8(ctx,0xfff2));
                failures++;
            }
            checked++;
        }
    }
    gb_context_destroy(ctx);
    fprintf(stderr,"Compared %u original ground phases: %u mismatches\n",checked,failures);
    return failures?1:0;
}
