#include "DXEntity.h"
#include "gbrt.h"
#include "ppu.h"
#include "rom.h"
#include "GameIdentity.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static void original_axis(GBContext *ctx) {
    ctx->sp = 0xd700; gb_push16(ctx, 0xc000); ctx->pc = 0x7f3c;
    ctx->bc = 7; gb_write8(ctx, 0x2100, 3);
    unsigned instructions = 0;
    while (ctx->pc != 0xc000 && instructions++ < 1000) {
        ctx->stopped = 0; ctx->ime = 0; gb_reset_frame(ctx);
        gb_interpret(ctx, ctx->pc);
    }
    assert(ctx->pc == 0xc000);
}
int main(void) {
    if (DX_IS_FIXTURE || strcmp(DX_ROM_SHA256,"211a1d1559dc2684d63d87f62a6b1e9109201a6383b4004e9dd5e18ee926ee83")) {
        fprintf(stderr,"SKIP: verified French ROM required\n"); return 0;
    }
    GBConfig config = {.model = GB_MODEL_CGB, .enable_audio = false};
    GBContext *ctx = gb_context_create(&config); assert(ctx); rom_init(ctx);
    gb_write8(ctx, 0xff40, 0);
    // Exhaustively compare signed speed and fractional carry to original code.
    for (unsigned speed = 0; speed < 256; speed++) for (unsigned fraction = 0; fraction < 256; fraction++) {
        gb_write8(ctx,0xc207,128); gb_write8(ctx,0xc247,speed); gb_write8(ctx,0xc267,fraction);
        original_axis(ctx);
        DXEntityAxis axis = {100000, fraction};
        assert(dx_entity_move_axis(&axis,(int8_t)speed));
        assert(axis.position == 100000 + (int)gb_read8(ctx,0xc207) - 128);
        assert(axis.fraction == gb_read8(ctx,0xc267));
    }
    DXEntityAxis edge = {INT32_MAX,0};
    assert(!dx_entity_move_axis(&edge,16) && edge.position == INT32_MAX && !edge.fraction);
    edge.position = INT32_MIN;
    assert(!dx_entity_move_axis(&edge,-16) && edge.position == INT32_MIN);

    // Compare each flag combination to the original PPU, including transparent
    // pixels, both banks, all palettes, X/Y flips, 8x16 tile masking, occlusion.
    GBPPU *ppu = ctx->ppu;
    ppu->cgb_mode = true; ppu->scx = ppu->scy = 0;
    for (unsigned i = 0; i < 0x4000; i++) ctx->vram[i] = (i * 37) ^ (i >> 3);
    for (unsigned i = 0; i < 64; i++) ppu->bg_palette_ram[i] = ppu->obj_palette_ram[i] = i * 17;
    uint32_t base[160 * 32], rendered[160 * 32], wide[512 * 32 + 2];
    uint8_t metadata[160 * 32], wide_metadata[512 * 32];
    const int positions[] = {-7,0,79,156,160};
    unsigned cases = 0;
    for (unsigned bgpriority = 0; bgpriority < 2; bgpriority++) {
        memset(ctx->vram + 0x1800, 7, 1024);
        memset(ctx->vram + 0x3800, bgpriority ? 0x80 : 0, 1024);
        ppu->lcdc = 0x91;
        for (unsigned y = 0; y < 32; y++) { ppu->ly = y; ppu_render_scanline(ppu,ctx); }
        memcpy(base,ppu->rgb_framebuffer,sizeof base);
        for (unsigned i = 0; i < 160 * 32; i++) metadata[i] = ppu->framebuffer[i] | (bgpriority ? 0x80 : 0);
        for (unsigned priority = 0; priority < 2; priority++)
        for (unsigned height = 8; height <= 16; height += 8)
        for (unsigned flags = 0; flags < 256; flags++)
        for (unsigned place = 0; place < sizeof positions / sizeof *positions; place++) {
            int x = positions[place], y = place == 0 ? -3 : 11;
            memset(ctx->oam,0,160);
            ctx->oam[0] = y + 16; ctx->oam[1] = x + 8;
            ctx->oam[2] = 123; ctx->oam[3] = flags;
            ppu->lcdc = 0x92 | priority | (height == 16 ? 4 : 0);
            for (unsigned row = 0; row < 32; row++) { ppu->ly = row; ppu_render_scanline(ppu,ctx); }
            memcpy(rendered,base,sizeof base);
            DXEntitySprite sprite = {256 + x, y, 123, flags, height};
            assert(dx_entity_draw_sprite(&sprite,256,0,ctx->vram,ppu->obj_palette_ram,metadata,priority,160,32,rendered,160*32));
            assert(!memcmp(rendered,ppu->rgb_framebuffer,sizeof rendered));
            // Same sprite in the extended field, far beyond original X=160.
            for (unsigned row = 0; row < 32; row++) for (unsigned col = 0; col < 512; col++) {
                wide[1 + row * 512 + col] = base[row * 160 + col % 8];
                wide_metadata[row * 512 + col] = metadata[row * 160 + col % 8];
            }
            wide[0] = wide[512*32+1] = 0xdeadbeef;
            assert(dx_entity_draw_sprite(&sprite,0,0,ctx->vram,ppu->obj_palette_ram,wide_metadata,priority,512,32,wide+1,512*32));
            for (unsigned row = 0; row < 32; row++)
                assert(!memcmp(wide+1+row*512+256,rendered+row*160,160*4));
            assert(wide[0] == 0xdeadbeef && wide[512*32+1] == 0xdeadbeef);
            cases++;
        }
    }
    DXEntitySprite invalid = {0,0,0,0,9};
    assert(!dx_entity_draw_sprite(&invalid,0,0,ctx->vram,ppu->obj_palette_ram,metadata,true,160,32,rendered,160*32));
    invalid.height = 16;
    assert(!dx_entity_draw_sprite(&invalid,0,0,ctx->vram,ppu->obj_palette_ram,metadata,true,160,32,rendered,160*32-1));
    memcpy(rendered,base,sizeof base);
    invalid.x = INT32_MIN; invalid.y = INT32_MAX;
    assert(dx_entity_draw_sprite(&invalid,INT32_MAX,INT32_MIN,ctx->vram,ppu->obj_palette_ram,metadata,true,160,32,rendered,160*32));
    assert(!memcmp(rendered,base,sizeof base));
    gb_context_destroy(ctx);
    fprintf(stderr,"PASS: 65536 original movement comparisons; %u sprite/occlusion/camera comparisons\n",cases);
    return 0;
}
