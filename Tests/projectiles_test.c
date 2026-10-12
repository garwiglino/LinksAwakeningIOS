#include "DXProjectiles.h"
#include "actor_oracle.h"
#define LINK_FIELD(address, field) {address,offsetof(DXProjectileLink,field),#field}
static const struct { unsigned address; size_t offset; const char *name; } link_fields[]={
 LINK_FIELD(0xffa2,z),LINK_FIELD(0xc11c,motion),LINK_FIELD(0xff9d,animation),LINK_FIELD(0xff9e,direction),LINK_FIELD(0xc15b,using_shield),
 LINK_FIELD(0xc13e,ignore_collisions),LINK_FIELD(0xdbc7,invincibility),LINK_FIELD(0xc1c6,immune),LINK_FIELD(0xc166,ocarina),LINK_FIELD(0xc1a9,got_item),
 LINK_FIELD(0xdc0f,tunic),LINK_FIELD(0xd47c,power_up),LINK_FIELD(0xd47a,power_up_hits),LINK_FIELD(0xd471,acorn_counter),LINK_FIELD(0xdb94,subtract_health),
 LINK_FIELD(0xc14b,boots_charge),LINK_FIELD(0xc14a,running),LINK_FIELD(0xc1be,boss_battle),LINK_FIELD(0xffb0,default_music),
 LINK_FIELD(0xd368,music_to_play),LINK_FIELD(0xffbf,next_music),LINK_FIELD(0xff9a,speed_x),LINK_FIELD(0xff9b,speed_y)
};
static void write_link(GBContext *ctx,const DXProjectileLink *p) {
    for(unsigned i=0;i<sizeof link_fields/sizeof *link_fields;i++) gb_write8(ctx,link_fields[i].address,*((const uint8_t*)p+link_fields[i].offset));
    gb_write8(ctx,0xff98,p->x-160); gb_write8(ctx,0xff99,p->y-1152);
}
static void compare_link(GBContext *ctx,const DXProjectileLink *p) {
    for(unsigned i=0;i<sizeof link_fields/sizeof *link_fields;i++) {
        uint8_t actual=*((const uint8_t*)p+link_fields[i].offset),expected=gb_read8(ctx,link_fields[i].address);
        if(actual!=expected) { fprintf(stderr,"Link %s native %02x original %02x\n",link_fields[i].name,actual,expected); assert(actual==expected); }
    }
}
static void active(GBContext *ctx,const DXActor *a) {
    gb_write8(ctx,0xffeb,a->body.collision.type); gb_write8(ctx,0xffea,5);
    gb_write8(ctx,0xffee,a->body.x.position-160); gb_write8(ctx,0xffef,a->body.y.position-1152);
    gb_write8(ctx,0xffec,a->body.y.position-1152-a->body.collision.z);
    gb_write8(ctx,0xfff9,0); gb_write8(ctx,0xdba5,0); gb_write8(ctx,0xfff7,0);
    gb_write8(ctx,0xfff2,0); gb_write8(ctx,0xfff3,0); gb_write8(ctx,0xc502,0);
    gb_write8(ctx,0xffe7,a->body.collision.frame);
}
int main(void) {
    if(DX_IS_FIXTURE || strcmp(DX_ROM_SHA256,"211a1d1559dc2684d63d87f62a6b1e9109201a6383b4004e9dd5e18ee926ee83")) {
        fprintf(stderr,"SKIP: verified French ROM required\n"); return 0;
    }
    GBConfig config={.model=GB_MODEL_CGB,.enable_audio=false};
    GBContext *ctx=gb_context_create(&config); assert(ctx); rom_init(ctx); gb_write8(ctx,0xff40,0); gb_write8(ctx,0xff70,1); gb_write8(ctx,0xfffe,1);
    unsigned contacts=0,motions=0,integrations=0;
    for(unsigned type=0;type<2;type++) for(unsigned profile=0;profile<256;profile++) for(int x=-8;x<8;x++) for(int y=-8;y<8;y++) {
        snprintf(oracle_case,sizeof oracle_case,"case type %u profile %u",type,profile);
        DXActor a=sample(0); a.body.collision.type=type?0x0c:0x0a; assert(dx_actor_configure(ctx->rom,ctx->rom_size,0x91,&a));
        a.body.collision.status=5; a.direction=profile&3; a.body.collision.z=(profile&128)?8:0;
        if(profile%19==0) a.physics_flags|=0x80;
        DXProjectileLink p={.x=240+x,.y=1224+y,.direction=(profile>>2)&3,.using_shield=(profile>>4)&1,
            .motion=(profile%17==0)?2:0,.z=(profile%23==0)?4:0,.animation=(profile%13==0)?0x4e:0,
            .tunic=(profile>>5)%3,.power_up=(profile>>3)%3,.power_up_hits=(profile>>2)&3,
            .acorn_counter=12,.subtract_health=250,.boots_charge=9,.running=1,.boss_battle=(profile>>6)&1,
            .default_music=(profile&64)?0x22:7,.music_to_play=0xee,.next_music=0xdd,
            .ignore_collisions=(profile%29==0)?8:0,.invincibility=(profile%31==0)?9:0,
            .immune=(profile%37==0)?1:0,.ocarina=(profile%41==0)?3:0,.got_item=(profile%43==0)?1:0};
        write_actor(ctx,7,&a); write_link(ctx,&p); active(ctx,&a);
        original(ctx,3,0x6bdf,7);
        DXProjectileEffects fx; assert(dx_projectile_contact(ctx->rom,ctx->rom_size,&a,&p,&fx));
        compare_actor(ctx,7,&a); compare_link(ctx,&p);
        assert(fx.jingle==gb_read8(ctx,0xfff2) && fx.wave==gb_read8(ctx,0xfff3)); contacts++;
    }
    DXWorldRoom room={.room=0x91}; const DXWorldRoom *rooms[256]={0}; rooms[0x91]=&room;
    for(unsigned type=0;type<2;type++) for(unsigned object=0;object<256;object++) for(unsigned profile=0;profile<128;profile++) {
        memset(room.blocks,object,sizeof room.blocks); for(unsigned i=0;i<256;i++) gb_write8(ctx,0xd700+i,object);
        snprintf(oracle_case,sizeof oracle_case,"case type %u profile %u",type,profile);
        DXActor a=sample(0); a.body.collision.type=type?0x0c:0x0a; assert(dx_actor_configure(ctx->rom,ctx->rom_size,0x91,&a));
        a.body.collision.status=5; a.direction=profile&3;
        a.body.collision.countdown=(uint8_t[]){0,0,1,8,16,23,24,32}[(profile>>2)&7];
        a.body.collision.collision_flags=(profile&64)?0xff:0;
        a.body.collision.thrown_direction=profile&3;
        a.body.collision.ledge_count=(profile&16)?3:0;
        a.body.collision.z=(profile&32)?8:0; a.body.collision.frame=profile&3;
        a.body.x.fraction=123; a.body.y.fraction=231; a.z_fraction=201;
        a.ground.speed_x=(int8_t[]){32,-32,127,-128}[profile&3]; a.ground.speed_y=-7; a.ground.speed_z=(profile&2)?-17:16;
        snprintf(oracle_case,sizeof oracle_case,"motion type %u object %u profile %u",type,object,profile);
        write_actor(ctx,7,&a); active(ctx,&a);
        gb_write8(ctx,0xc11c,(profile&8)?5:0); gb_write8(ctx,0xddd6,(profile&16)?2:0); gb_write8(ctx,0xc124,(profile&32)?1:0);
        original(ctx,3,0x6ade,7); // after rendering/interactivity, before countdown branch
        DXProjectileEffects fx;
        assert(dx_projectile_move(ctx->rom,ctx->rom_size,rooms,&a,(profile&8)?5:0,(profile&16)!=0,(profile&32)!=0,&fx));
        compare_actor(ctx,7,&a);
        assert(fx.jingle==gb_read8(ctx,0xfff2)); assert((fx.alert_sword_moblins?4:0)==gb_read8(ctx,0xc502)); motions++;
    }
    // Entire contact -> physics transaction, including damage and shield bounce.
    for(unsigned type=0;type<2;type++) for(unsigned profile=0;profile<4096;profile++) {
        unsigned object=profile&255;
        memset(room.blocks,object,sizeof room.blocks); for(unsigned i=0;i<256;i++) gb_write8(ctx,0xd700+i,object);
        snprintf(oracle_case,sizeof oracle_case,"case type %u profile %u",type,profile);
        DXActor a=sample(0); a.body.collision.type=type?0x0c:0x0a; assert(dx_actor_configure(ctx->rom,ctx->rom_size,0x91,&a));
        a.body.collision.status=5; a.direction=(profile>>8)&3; a.ground.speed_x=32;
        a.body.collision.countdown=(profile&2048)?8:0;
        DXProjectileLink p={.x=240+((profile&1024)?2:30),.y=1224,.direction=a.direction^1,.using_shield=(profile>>9)&1};
        snprintf(oracle_case,sizeof oracle_case,"combined type %u object %u profile %u",type,object,profile);
        write_actor(ctx,7,&a); active(ctx,&a); write_link(ctx,&p);
        gb_write8(ctx,0xddd6,0); gb_write8(ctx,0xc124,0);
        if(!a.body.collision.countdown) original(ctx,3,0x6bdf,7);
        original(ctx,3,0x6ade,7);
        DXProjectileEffects fx;
        assert(dx_projectile_step(ctx->rom,ctx->rom_size,rooms,&a,&p,true,false,false,&fx));
        compare_actor(ctx,7,&a); compare_link(ctx,&p);
        assert(fx.jingle==gb_read8(ctx,0xfff2) && fx.wave==gb_read8(ctx,0xfff3)); integrations++;
    }
    // Crossing the old room edge samples the neighbor, with no 8-bit aliases.
    DXWorldRoom neighbor={.room=0x92}; rooms[0x92]=&neighbor;
    memset(room.blocks,4,sizeof room.blocks); memset(neighbor.blocks,0,sizeof neighbor.blocks);
    DXActor a={0}; a.body.collision.type=0x0c; a.body.collision.status=5;
    assert(dx_actor_configure(ctx->rom,ctx->rom_size,0x91,&a));
    a.body.x.position=319; a.body.y.position=1216; a.ground.speed_x=32;
    DXProjectileLink p={.x=319+256,.y=1216}; DXProjectileEffects fx;
    assert(dx_projectile_step(ctx->rom,ctx->rom_size,rooms,&a,&p,true,false,false,&fx));
    assert(!p.subtract_health && !p.invincibility); // distant Link is not a hit
    assert(a.body.x.position==319 && a.body.collision.countdown==24 && a.ground.speed_x==-4 && fx.jingle==7);
    memset(neighbor.blocks,4,sizeof neighbor.blocks);
    a.body.collision.countdown=0; a.body.collision.collision_flags=0; a.ground.speed_x=32;
    assert(dx_projectile_step(ctx->rom,ctx->rom_size,rooms,&a,&p,true,false,false,&fx));
    assert(a.body.x.position==321 && !a.body.collision.countdown);
    // Reaching missing geometry after a damaging contact rolls back Link too.
    rooms[0x92]=NULL; p.x=a.body.x.position; p.y=a.body.y.position;
    DXActor saved_actor=a; DXProjectileLink saved_link=p; DXProjectileEffects saved_fx=fx;
    assert(!dx_projectile_step(ctx->rom,ctx->rom_size,rooms,&a,&p,true,false,false,&fx));
    assert(!memcmp(&a,&saved_actor,sizeof a) && !memcmp(&p,&saved_link,sizeof p) && !memcmp(&fx,&saved_fx,sizeof fx));
    fprintf(stderr,"Cross-room projectile motion, distant-player rejection and damage rollback passed\n");
    gb_context_destroy(ctx);
    fprintf(stderr,"PASS: %u projectile/player contacts, %u projectile motions, %u combined damage/shield/terrain steps\n",contacts,motions,integrations);
}
