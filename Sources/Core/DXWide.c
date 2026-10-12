#include "DXWide.h"
#include "DXWorld.h"
#include "gbrt.h"
#include "ppu.h"
#include <string.h>

bool dx_wide_compose(const GBContext *game, const uint32_t *original, unsigned width, uint32_t *output) {
    if (!game || !original || !output || width <= 160 || width > DX_VIDEO_MAX_WIDTH) return false;
    // This stage deliberately excludes indoor layouts, screen transitions,
    // menus and cutscenes. Their coordinates are not overworld adjacency.
    const uint8_t *ram = game->wram;
    if (ram[0x1b95] != 0x0b || ram[0x1b96] != 7 || ram[0x1ba5] || ram[0x124] || game->hram[0x79]) return false;
    unsigned room_id = game->hram[0x76], column = room_id & 15;
    unsigned tileset = game->hram[0x14];
    DXWorldRoom current;
    if (!dx_world_decode(game->rom, game->rom_size, room_id, ram + 0x1800, &current)) return false;
    // Do not show bright neighboring rooms during a palette fade.
    const uint8_t *rom = game->rom;
    unsigned palette_id = rom[0x842ef + room_id];
    unsigned ptr = rom[0x842b1 + palette_id * 2] | (unsigned)rom[0x842b2 + palette_id * 2] << 8;
    if (ptr < 0x4000 || ptr > 0x7fc0) return false;
    const GBPPU *ppu = game->ppu;
    if (memcmp(ppu->bg_palette_ram, rom + 0x84000 + ptr - 0x4000, 64)) return false;
    int center_x = (int)(width - 160) / 2;
    int first = (-center_x - 159) / 160, last = ((int)width - 1 - center_x) / 160;
    // No invented scenery or horizontal world wrapping at the island's edges.
    if ((int)column + first < 0 || (int)column + last > 15) return false;
    uint32_t pixels[160 * 128];
    for (int relative = first; relative <= last; relative++) {
        if (relative == 0) continue;
        DXWorldRoom neighbor;
        if (!dx_world_decode(rom, game->rom_size, (uint8_t)(room_id + relative), ram + 0x1800, &neighbor) ||
            neighbor.animation != current.animation ||
            !dx_world_draw(rom, game->rom_size, &neighbor, game->vram, tileset, pixels)) return false;
        int origin = center_x + relative * 160;
        for (int y = 0; y < 128; y++) for (int x = 0; x < 160; x++) {
            int destination = origin + x;
            if (destination >= 0 && destination < (int)width) output[y * width + destination] = pixels[y * 160 + x];
        }
    }
    // HUD stays legible and at its original scale, centered on a plain strip.
    // No stretching or repeated edge pixels to impersonate extra game scenery.
    for (unsigned y = 128; y < 144; y++) for (unsigned x = 0; x < width; x++) output[y * width + x] = 0xff101820;
    for (unsigned y = 0; y < 144; y++) memcpy(output + y * width + center_x, original + y * 160, 160 * sizeof *output);
    return true;
}
