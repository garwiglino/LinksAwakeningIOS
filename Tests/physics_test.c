#include "DXPhysics.h"
#include "gbrt.h"
#include "rom.h"
#include "GameIdentity.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static bool original_ground(GBContext *ctx, unsigned address) {
    ctx->sp=0xdfe0; gb_push16(ctx,0xc000); ctx->pc=address; ctx->bc=7;
    gb_write8(ctx,0x2100,3); bool splash=false;
    for (unsigned step=0;step<2000;step++) {
        if (ctx->pc==0xc000) return splash;
        if (ctx->pc==0x0cb7) splash=true;
        ctx->stopped=0; ctx->ime=0; gb_reset_frame(ctx); gb_interpret(ctx,ctx->pc);
    }
    assert(!"original physics routine failed to return"); return false;
}
int main(void) {
    if (DX_IS_FIXTURE || strcmp(DX_ROM_SHA256,"211a1d1559dc2684d63d87f62a6b1e9109201a6383b4004e9dd5e18ee926ee83")) {
        fprintf(stderr,"SKIP: verified French ROM required\n"); return 0;
    }
    GBConfig config={.model=GB_MODEL_CGB,.enable_audio=false};
    GBContext *ctx=gb_context_create(&config); assert(ctx); rom_init(ctx); gb_write8(ctx,0xff40,0);
    DXWorldRoom scene={.room=0x91};
    const DXWorldRoom *rooms[256]={0}; rooms[0x91]=&scene;
    const uint8_t types[]={0,2,3,9,0x0b,0x14,0x36,0x59,0x6d,0x99,0xa0,0xa8,0xac,0xc1,0xcc,0xd5};
    unsigned checked=0,failures=0;
    for (unsigned pattern=0;pattern<2;pattern++) for (unsigned group=0;group<1;group++) for (unsigned object=0;object<256;object++) {
        for (unsigned i=0;i<256;i++) {
            scene.blocks[i]=pattern ? (uint8_t)(object+i*17) : object;
            gb_write8(ctx,0xd700+i,scene.blocks[i]);
        }
        for (unsigned t=0;t<sizeof types;t++) for (unsigned v=0;v<64;v++) {
            DXWorldBody body={.x={240,123},.y={1224,231},.hitbox=v&3,.no_bg_collision=(v&8)!=0,.collision={
                .type=types[t],.z=(v&16)?((v&8)?128:8):0,
                .options=((v&4)?8:0)|((v&32)?16:0),.ledge_count=5,
                .ignore_hits=v%3,.status=5,.countdown=77,.frame=v&3,.indoor=group}};
            DXGroundState ground={.ground_status=v&3,.flash_timer=42,.splash_timer=(v&8)?0:5,
                .link_motion=(v&2)?6:0,.speed_x=-7,.speed_y=(v&2)?-8:8,
                .speed_z=(v&1)?-32:-16,.falling_x=160+55,.falling_y=1152+66,.side_scrolling=false};
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
            gb_write8(ctx,0xc267,123); gb_write8(ctx,0xc277,231);
            gb_write8(ctx,0xc357,body.hitbox); gb_write8(ctx,0xffbe,body.no_bg_collision);
            gb_write8(ctx,0xffee,80); gb_write8(ctx,0xffef,72);
            gb_write8(ctx,0xc2a7,0); gb_write8(ctx,0xc297,0);
            gb_write8(ctx,0xc5d7,0); gb_write8(ctx,0xd6fb,0); gb_write8(ctx,0xd6f9,0); gb_write8(ctx,0xc1c6,0);
            gb_write8(ctx,0xc503,0); gb_write8(ctx,0xc50d,0);
            bool move=(v&1)!=0;
            if(move) original_ground(ctx,0x7f2f);
            bool splash=original_ground(ctx,0x7897); DXGroundEffects effects; DXWallContact contact;
            assert(dx_physics_step(ctx->rom,ctx->rom_size,rooms,move,&body,&ground,&contact,&effects));
            bool mismatch=body.x.fraction!=gb_read8(ctx,0xc267) || body.y.fraction!=gb_read8(ctx,0xc277) ||
                body.collision.collision_flags!=gb_read8(ctx,0xc2a7) || body.collision.state!=gb_read8(ctx,0xc297) ||
                contact.horizontal_object!=gb_read8(ctx,0xc503) || contact.vertical_object!=gb_read8(ctx,0xc50d) ||
                body.collision.status!=gb_read8(ctx,0xc287) ||
                body.collision.ledge_count!=gb_read8(ctx,0xc4f7) || body.collision.ignore_hits!=gb_read8(ctx,0xc417) ||
                body.collision.countdown!=gb_read8(ctx,0xc2e7) || ground.ground_status!=gb_read8(ctx,0xc477) ||
                ground.flash_timer!=gb_read8(ctx,0xc427) || ground.falling_x!=160+gb_read8(ctx,0xc4b7) ||
                ground.falling_y!=1152+gb_read8(ctx,0xc4c7) || body.x.position!=160+gb_read8(ctx,0xc207) ||
                body.y.position!=1152+gb_read8(ctx,0xc217) || (uint8_t)ground.speed_x!=gb_read8(ctx,0xc247) ||
                (uint8_t)ground.speed_y!=gb_read8(ctx,0xc257) || effects.jingle!=gb_read8(ctx,0xfff2) || effects.splash!=splash;
            if (mismatch) {
                if (failures<12) fprintf(stderr,"physics group %u object %02x physics %02x type %02x profile %u: status %u/%u ground %u/%u splash %u/%u jingle %u/%u\n",group,object,point.physics,types[t],v,body.collision.status,gb_read8(ctx,0xc287),ground.ground_status,gb_read8(ctx,0xc477),effects.splash,splash,effects.jingle,gb_read8(ctx,0xfff2));
                failures++;
            }
            checked++;
        }
    }
    // Ground sampling must follow the moved position into a different room.
    DXWorldRoom neighbor={.room=0x92}; rooms[0x92]=&neighbor;
    memset(scene.blocks,4,sizeof scene.blocks); // empty terrain
    uint8_t water=0,pit=0;
    for(unsigned i=0;i<256;i++) {
        uint8_t physics; assert(dx_collision_physics(ctx->rom,ctx->rom_size,i,0,false,&physics));
        if(physics==7) water=i;
        if(physics==0x50) pit=i;
    }
    assert(water && pit);
    memset(neighbor.blocks,water,sizeof neighbor.blocks);
    DXWorldBody body={.x={320,0},.y={1216,0},.collision={.type=9,.status=5}};
    DXGroundState ground={.speed_x=16}; DXWallContact contact; DXGroundEffects effects;
    assert(dx_physics_step(ctx->rom,ctx->rom_size,rooms,true,&body,&ground,&contact,&effects));
    assert(body.x.position==321 && !body.collision.status && effects.splash);
    assert(effects.splash_x==321 && effects.splash_y==1216);
    memset(neighbor.blocks,pit,sizeof neighbor.blocks);
    body=(DXWorldBody){.x={320,0},.y={1216,0},.collision={.type=9,.status=5,.ignore_hits=1}};
    ground=(DXGroundState){.speed_x=16};
    assert(dx_physics_step(ctx->rom,ctx->rom_size,rooms,true,&body,&ground,&contact,&effects));
    assert(body.collision.status==2 && body.collision.countdown==0x6f);
    assert(ground.falling_x==328 && ground.falling_y==1216);
    // Missing geometry after ground effects must roll back the entire step,
    // including its deferred splash/audio requests and fractional position.
    memset(scene.blocks,water,sizeof scene.blocks); rooms[0x92]=NULL;
    body=(DXWorldBody){.x={320,77},.y={1216,88},.collision={.type=9,.status=5}};
    ground=(DXGroundState){.speed_x=16};
    contact=(DXWallContact){0xaa,0xbb}; effects=(DXGroundEffects){.jingle=0xcc,.splash=true,.splash_x=5,.splash_y=6};
    DXWorldBody saved_body=body; DXGroundState saved_ground=ground;
    DXWallContact saved_contact=contact; DXGroundEffects saved_effects=effects;
    assert(!dx_physics_step(ctx->rom,ctx->rom_size,rooms,false,&body,&ground,&contact,&effects));
    assert(!memcmp(&body,&saved_body,sizeof body) && !memcmp(&ground,&saved_ground,sizeof ground));
    assert(!memcmp(&contact,&saved_contact,sizeof contact) && !memcmp(&effects,&saved_effects,sizeof effects));
    fprintf(stderr,"Cross-room water, pits and deferred-effect rollback passed\n");
    gb_context_destroy(ctx);
    fprintf(stderr,"Compared %u complete original physics steps: %u mismatches\n",checked,failures);
    return failures?1:0;
}
