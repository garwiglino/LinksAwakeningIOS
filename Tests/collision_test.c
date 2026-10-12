#include "DXCollision.h"
#include "gbrt.h"
#include "rom.h"
#include "GameIdentity.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static void original(GBContext *ctx, unsigned address) {
    // Keep stack clear of D6F9/D6FB switch state and D700 room data.
    ctx->sp = 0xdfe0; gb_push16(ctx,0xc000); ctx->pc = address;
    unsigned steps = 0;
    while (ctx->pc != 0xc000 && steps++ < 1000) {
        ctx->stopped = 0; ctx->ime = 0; gb_reset_frame(ctx); gb_interpret(ctx,ctx->pc);
    }
    assert(ctx->pc == 0xc000);
}
static void write_state(GBContext *ctx, const DXCollisionState *s) {
    gb_write8(ctx,0xffeb,s->type); gb_write8(ctx,0xc317,s->z);
    gb_write8(ctx,0xc437,s->options); gb_write8(ctx,0xc417,s->ignore_hits);
    gb_write8(ctx,0xc5d7,s->thrown_direction); gb_write8(ctx,0xc4f7,s->ledge_count);
    gb_write8(ctx,0xc2e7,s->countdown); gb_write8(ctx,0xc287,s->status);
    gb_write8(ctx,0xc297,s->state); gb_write8(ctx,0xc2a7,s->collision_flags);
    gb_write8(ctx,0xffe7,s->frame); gb_write8(ctx,0xdba5,s->indoor);
    gb_write8(ctx,0xd6fb,s->switch_state); gb_write8(ctx,0xd6f9,s->link_on_switch);
    gb_write8(ctx,0xc1c6,s->link_immune);
}
int main(void) {
    if (DX_IS_FIXTURE || strcmp(DX_ROM_SHA256,"211a1d1559dc2684d63d87f62a6b1e9109201a6383b4004e9dd5e18ee926ee83")) {
        fprintf(stderr,"SKIP: verified French ROM required\n"); return 0;
    }
    GBConfig config = {.model = GB_MODEL_CGB, .enable_audio = false};
    GBContext *ctx = gb_context_create(&config); assert(ctx); rom_init(ctx);
    gb_write8(ctx,0xff40,0); gb_write8(ctx,0xff70,1);
    unsigned checked = 0, failures = 0;
    static const uint8_t types[] = {0,2,3,5,9,0x16,0x17,0x59,0x99,0xa8,0xcc,0xd5};
    for (unsigned group = 0; group < 3; group++) for (unsigned object = 0; object < 256; object++) {
        for (unsigned i = 0; i < 256; i++) gb_write8(ctx,0xd700+i,object);
        for (unsigned type = 0; type < sizeof types; type++) for (unsigned variant = 0; variant < 64; variant++) {
            unsigned direction = variant & 3, hitbox = (variant >> 2) & 3;
            DXCollisionState state = {
                .type=types[type], .z=(variant & 16) ? ((variant & 8) ? 128 : 24) : 0,
                .options=((variant & 8) ? 1 : 0) | ((variant & 32) ? 0x80 : 0),
                .ignore_hits=(variant & 4) ? 5 : 0, .thrown_direction=(variant >> 3) & 3,
                .ledge_count=(variant & 2) ? 3 : 0, .countdown=(variant & 1) ? 0x26 : 0x25,
                .status=5,.state=7,.collision_flags=0x10,.frame=(variant >> 2) & 3,
                .indoor=group,.switch_state=(variant & 16) ? 2 : 0,
                .link_on_switch=(variant >> 5) & 1,.link_immune=1
            };
            int32_t px,py;
            int x = 72 + ((variant >> 4) & 1)*8, y = 72 + ((variant >> 5) & 1)*8;
            assert(dx_collision_point(ctx->rom,ctx->rom_size,x,y,hitbox,direction,state.type,state.z,&px,&py));
            DXTerrainPoint point = {.object=object,.local_x=px & 15,.local_y=py & 15};
            assert(dx_collision_physics(ctx->rom,ctx->rom_size,object,group,false,&point.physics));
            write_state(ctx,&state);
            gb_write8(ctx,0xc207,x); gb_write8(ctx,0xc217,y); gb_write8(ctx,0xffd7,hitbox*4);
            gb_write8(ctx,0xfff7,0); gb_write8(ctx,0x2100,3); ctx->bc=7; ctx->de=direction;
            original(ctx,0x7ad1);
            bool blocked;
            assert(dx_collision_apply(ctx->rom,ctx->rom_size,&point,direction,&state,&blocked));
            bool mismatch = blocked != !ctx->f_c ||
                state.collision_flags != gb_read8(ctx,0xc2a7) || state.ledge_count != gb_read8(ctx,0xc4f7) ||
                state.status != gb_read8(ctx,0xc287) || state.state != gb_read8(ctx,0xc297) ||
                state.link_immune != gb_read8(ctx,0xc1c6) || (uint8_t)px != gb_read8(ctx,0xffdb) ||
                (uint8_t)py != gb_read8(ctx,0xffdc) || object != gb_read8(ctx,0xffaf);
            if (mismatch) {
                if (failures < 12) fprintf(stderr,"Collision mismatch group %u object %02x physics %02x type %02x variant %u: blocked %d/%d state %u/%u immune %u/%u\n",group,object,point.physics,state.type,variant,blocked,!ctx->f_c,state.state,gb_read8(ctx,0xc297),state.link_immune,gb_read8(ctx,0xc1c6));
                failures++;
            }
            checked++;
        }
    }
    // Integrate native fractional motion with wall resolution and compare the
    // original routines in their real X-before-Y order. Ground effects excluded.
    unsigned motions = 0;
    DXWorldRoom scene = {.room=0x91};
    const DXWorldRoom *motion_rooms[256] = {0}; motion_rooms[0x91] = &scene;
    for (unsigned object = 0; object < 256; object++) for (unsigned profile = 0; profile < 16; profile++) {
        memset(scene.blocks,object,sizeof scene.blocks);
        for (unsigned i=0;i<256;i++) gb_write8(ctx,0xd700+i,object);
        int8_t sx = (profile & 1) ? -23 : 19, sy = (profile & 2) ? -11 : 13;
        DXWorldBody body = {.x={160+80,123},.y={9*128+72,231},.hitbox=profile&3,
            .no_bg_collision=(profile&8)!=0,
            .collision={.type=types[profile % sizeof types],.z=8,.status=5,.countdown=0x26,
                        .options=(profile&4)?1:0,.collision_flags=0x10,.link_immune=1}};
        write_state(ctx,&body.collision);
        gb_write8(ctx,0xc207,80); gb_write8(ctx,0xc217,72);
        gb_write8(ctx,0xc247,sx); gb_write8(ctx,0xc257,sy);
        gb_write8(ctx,0xc267,123); gb_write8(ctx,0xc277,231);
        gb_write8(ctx,0xc357,body.hitbox); gb_write8(ctx,0xffbe,body.no_bg_collision);
        gb_write8(ctx,0xffee,80); gb_write8(ctx,0xffef,72);
        gb_write8(ctx,0xfff7,0); gb_write8(ctx,0xc503,0); gb_write8(ctx,0xc50d,0);
        gb_write8(ctx,0x2100,3); ctx->bc=7;
        original(ctx,0x7f2f); // native position integrator oracle
        original(ctx,0x7a1c); // wall phase, after original ground-effects phase
        DXWallContact contact;
        assert(dx_collision_move_walls(ctx->rom,ctx->rom_size,motion_rooms,&body,sx,sy,&contact));
        assert(body.x.position == 160+gb_read8(ctx,0xc207));
        assert(body.y.position == 9*128+gb_read8(ctx,0xc217));
        assert(body.x.fraction == gb_read8(ctx,0xc267) && body.y.fraction == gb_read8(ctx,0xc277));
        assert(body.collision.collision_flags == gb_read8(ctx,0xc2a7));
        assert(body.collision.state == gb_read8(ctx,0xc297) && body.collision.status == gb_read8(ctx,0xc287));
        assert(body.collision.link_immune == gb_read8(ctx,0xc1c6));
        assert(contact.horizontal_object == gb_read8(ctx,0xc503));
        assert(contact.vertical_object == gb_read8(ctx,0xc50d));
        motions++;
    }
    // World boundary lookup must resolve the neighboring room's own geometry.
    uint8_t status[256] = {0}; DXWorldRoom left,right;
    assert(dx_world_decode(ctx->rom,ctx->rom_size,0x91,status,&left));
    assert(dx_world_decode(ctx->rom,ctx->rom_size,0x92,status,&right));
    const DXWorldRoom *rooms[256] = {0}; rooms[0x91]=&left; rooms[0x92]=&right;
    for (int y=0;y<128;y++) for (int x=0;x<320;x++) {
        DXTerrainPoint point;
        assert(dx_collision_world_point(ctx->rom,ctx->rom_size,rooms,160+x,9*128+y,&point));
        const DXWorldRoom *room = x < 160 ? &left : &right;
        assert(point.room == room->room);
        assert(point.object == room->blocks[0x11+(y/16)*16+(x%160)/16]);
    }
    // A wall immediately across the old screen edge blocks movement; empty
    // neighbor space allows it. Unloaded neighbor data must not advance time.
    memset(left.blocks,0x04,sizeof left.blocks); // physics 0: passable
    memset(right.blocks,0x00,sizeof right.blocks); // physics 1: solid
    DXWorldBody crossing = {.x={319,0},.y={9*128+64,0},.collision={.type=9}};
    DXWallContact contact;
    assert(dx_collision_move_walls(ctx->rom,ctx->rom_size,rooms,&crossing,16,0,&contact));
    assert(crossing.x.position==319 && (crossing.collision.collision_flags&1));
    memset(right.blocks,0x04,sizeof right.blocks);
    assert(dx_collision_move_walls(ctx->rom,ctx->rom_size,rooms,&crossing,16,0,&contact));
    assert(crossing.x.position==320 && !crossing.collision.collision_flags);
    rooms[0x92]=NULL;
    DXWorldBody unchanged=crossing;
    contact.horizontal_object=0xaa; contact.vertical_object=0xbb;
    assert(!dx_collision_move_walls(ctx->rom,ctx->rom_size,rooms,&crossing,16,0,&contact));
    assert(!memcmp(&unchanged,&crossing,sizeof crossing));
    assert(contact.horizontal_object==0xaa && contact.vertical_object==0xbb);
    DXTerrainPoint missing;
    assert(!dx_collision_world_point(ctx->rom,ctx->rom_size,rooms,-1,0,&missing));
    assert(!dx_collision_world_point(ctx->rom,ctx->rom_size,rooms,2560,0,&missing));
    assert(!dx_collision_world_point(ctx->rom,ctx->rom_size,rooms,0,0,&missing));
    int32_t px=1,py=2;
    assert(!dx_collision_point(ctx->rom,ctx->rom_size,INT32_MIN,0,0,1,0,0,&px,&py));
    assert(px==1 && py==2);
    fprintf(stderr,"Compared %u original collision calls: %u mismatches; 40960 cross-room lookups checked\n",checked,failures);
    fprintf(stderr,"Compared %u integrated movement/wall cases; boundary crossing and rollback passed\n",motions);
    gb_context_destroy(ctx);
    return failures ? 1 : 0;
}
