#include "DXWorld.h"
#include "gbrt.h"
#include "ppu.h"
#include "DXWide.h"
#include "rom.h"
#include "GameIdentity.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
// Differential oracle: run the ORIGINAL room loader on an isolated context.
// The native decoder is compared with its output; it never calls this oracle.
static void call_original(GBContext *ctx, unsigned address) {
    ctx->sp = 0xd700; gb_push16(ctx, 0xc000); ctx->pc = address;
    unsigned steps = 0;
    while (ctx->pc != 0xc000 && steps++ < 100000) {
        ctx->stopped = 0; ctx->ime = 0; gb_reset_frame(ctx); gb_interpret(ctx, ctx->pc);
    }
    assert(ctx->pc == 0xc000);
}
int main(void) {
#if DX_IS_FIXTURE
    puts("SKIP: room oracle requires the verified French Rev 1 ROM"); return 0;
#endif
    if (strcmp(DX_ROM_SHA256, "211a1d1559dc2684d63d87f62a6b1e9109201a6383b4004e9dd5e18ee926ee83")) {
        puts("SKIP: room oracle profile not available for this revision"); return 0;
    }
    unsigned failures = 0, checked = 0, compositions = 0;
    // Every room, with independent persistent-state combinations (bits 2,4,5).
    for (unsigned flags = 0; flags < 8; flags++) for (unsigned room = 0; room < 256; room++) {
        GBConfig config = {.model = GB_MODEL_CGB, .enable_audio = false};
        GBContext *ctx = gb_context_create(&config); assert(ctx);
        rom_init(ctx);
        uint8_t status[256];
        memset(status, ((flags & 1) ? 4 : 0) | ((flags & 2) ? 16 : 0) | ((flags & 4) ? 32 : 0), 256);
        gb_write8(ctx, 0xff40, 0); gb_write8(ctx, 0xfffe, 1);
        gb_write8(ctx, 0xfff6, room); gb_write8(ctx, 0xfff7, 0);
        gb_write8(ctx, 0xc125, 0xff); // stable palette, no entry-direction transition
        gb_write8(ctx, 0xdb96, 7); gb_write8(ctx, 0xdba5, 0); gb_write8(ctx, 0xff80, 1);
        for (unsigned i = 0; i < 256; i++) gb_write8(ctx, 0xd800 + i, status[i]);
        call_original(ctx, 0x30ef);
        DXWorldRoom native; assert(dx_world_decode(ctx->rom,ctx->rom_size,room,status,&native));
        for (unsigned y = 0; y < 8; y++) for (unsigned x = 0; x < 10; x++) {
            unsigned p = 0x11 + y * 16 + x;
            uint8_t actual = gb_read8(ctx,0xd700+p);
            if (native.blocks[p] != actual) {
                if (failures < 25) fprintf(stderr,"room %02x flags %02x cell %u,%u: native %02x expected %02x\n",room,status[room],x,y,native.blocks[p],actual);
                failures++;
            }
            gb_write8(ctx,0xff70,2);
            uint8_t attr = gb_read8(ctx,0xd700+p);
            gb_write8(ctx,0xff70,1);
            if (native.attributes[y*10+x] != attr) {
                if (failures < 25) fprintf(stderr,"attr room %02x flags %02x cell %u,%u: native %02x expected %02x\n",room,status[room],x,y,native.attributes[y*10+x],attr);
                failures++;
            }
        }
        if (flags == 0) {
            // A deliberately varied pattern catches tile-bank, X/Y flip, and
            // palette errors independently of the game's art assets.
            for (unsigned i = 0; i < 0x4000; i++) ctx->vram[i] = (i * 13) ^ (i >> 8);
            unsigned region_id = room == 7 ? 8 : room;
            unsigned region = ((region_id & 0xe0) >> 2) | ((region_id & 0x0e) >> 1);
            unsigned tileset = ctx->rom[0x82e84 + region];
            if (tileset == 15 || (tileset == 26 && room != 0x37)) tileset = 0x28;
            gb_write8(ctx,0xff94,tileset); ctx->de = 0x9000;
            call_original(ctx,0x2fa8); // original room-specific pattern loader
            call_original(ctx,0x3096); // original object-to-background conversion
            GBPPU *ppu = ctx->ppu;
            ppu->lcdc = 0x81; ppu->cgb_mode = true; ppu->scx = ppu->scy = 0;
            memcpy(ppu->bg_palette_ram,ctx->wram + 0x1c10,64);
            for (unsigned y = 0; y < 128; y++) { ppu->ly = y; ppu_render_scanline(ppu,ctx); }
            uint32_t rendered[160 * 128];
            assert(dx_world_draw(ctx->rom,ctx->rom_size,&native,ctx->vram,tileset,rendered));
            unsigned different = 0;
            for (unsigned i = 0; i < 160 * 128; i++) if (rendered[i] != ppu->rgb_framebuffer[i]) different++;
            if (different) {
                if (failures < 25) fprintf(stderr,"render room %02x: %u different pixels\n",room,different);
                failures++;
            }
        }
        if (flags == 0) {
            gb_write8(ctx, 0xdb95, 0x0b); gb_write8(ctx, 0xdb96, 7);
            unsigned palette_id = ctx->rom[0x842ef + room];
            unsigned ptr = ctx->rom[0x842b1 + palette_id * 2] | (unsigned)ctx->rom[0x842b2 + palette_id * 2] << 8;
            memcpy(((GBPPU *)ctx->ppu)->bg_palette_ram, ctx->rom + 0x80000 + ptr, 64);
            uint32_t output[DX_VIDEO_MAX_WIDTH * 144 + 2];
            const unsigned widths[] = {161, 319, 320, 512};
            for (unsigned test = 0; test < 4; test++) {
                unsigned width = widths[test];
                output[0] = output[width * 144 + 1] = 0xdeadbeef;
                const uint32_t *original = ((GBPPU *)ctx->ppu)->rgb_framebuffer;
                if (dx_wide_compose(ctx, original, width, output + 1)) {
                    compositions++;
                    unsigned start = (width - 160) / 2;
                    for (unsigned y = 0; y < 144; y++)
                        assert(!memcmp(output + 1 + y * width + start, original + y * 160, 160 * 4));
                }
                assert(output[0] == 0xdeadbeef && output[width * 144 + 1] == 0xdeadbeef);
            }
            gb_write8(ctx, 0xc124, 1);
            assert(!dx_wide_compose(ctx, ((GBPPU *)ctx->ppu)->rgb_framebuffer, 320, output));
            gb_write8(ctx, 0xc124, 0);
        }
        // Compare spawn placement with the original loader, retaining cleared
        // masks instead of invoking its unrelated recent-room housekeeping.
        static const uint8_t masks[] = {0,1,2,4,8,16,0x55,0xff};
        uint8_t mask = masks[flags];
        gb_write8(ctx, 0xcf00 + room, mask);
        gb_write8(ctx, 0xc124, 0); gb_write8(ctx, 0xc125, 4);
        call_original(ctx, 0x3801);
        DXWorldSpawns spawns;
        assert(dx_world_spawns(ctx->rom, ctx->rom_size, room, mask, &spawns));
        unsigned count = 0;
        for (unsigned slot = 0; slot < 16; slot++) if (gb_read8(ctx, 0xc280 + slot)) {
            if (count >= spawns.count) { failures++; continue; }
            DXWorldSpawn *spawn = &spawns.entries[count++];
            if (spawn->x != (room & 15) * 160 + gb_read8(ctx, 0xc200 + slot) ||
                spawn->y != (room >> 4) * 128 + gb_read8(ctx, 0xc210 + slot) ||
                spawn->type != gb_read8(ctx, 0xc3a0 + slot) ||
                spawn->order != gb_read8(ctx, 0xc460 + slot)) {
                if (failures < 25) fprintf(stderr,"spawn room %02x mask %02x slot %u differs\n",room,mask,slot);
                failures++;
            }
        }
        if (count != spawns.count) failures++;
        checked++; gb_context_destroy(ctx);
    }
    fprintf(stderr,"Compared %u room/state combinations: %u mismatches\n",checked,failures);
    assert(compositions > 0);
    fprintf(stderr,"Checked %u successful wide compositions; original center preserved\n",compositions);
    return failures ? 1 : 0;
}
