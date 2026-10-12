#include "DXWorld.h"
#include <string.h>

static size_t offset(unsigned bank, unsigned address) {
    return bank * 0x4000u + (address & 0x3fffu);
}
static unsigned word(const uint8_t *p) { return p[0] | (unsigned)p[1] << 8; }
static bool address_ok(unsigned address) { return address >= 0x4000 && address < 0x8000; }

static uint8_t changed_object(uint8_t object, uint8_t room, uint8_t state) {
    if (state & 0x20) {
        if (object == 0x5e) return 0xc6;
        if (object == 0x91) return 0x5e;
        if (object == 0xdc) return 0x91;
    }
    if ((state & 0x10) && object >= 0xd8 && object <= 0xda) return 0xdb;
    if ((state & 0x10) && object == 0xc2) return 0xe3;
    if ((state & 4) && object == 0xba) return 0xe1;
    if ((state & 0x10) && object == 0xd3 &&
        (room == 0x75 || room == 7 || room == 0xaa || room == 0x4a)) return 0xc6;
    if ((state & 0x10) && object == 0xa0) return 0xa1;
    return object;
}
static void tree(uint8_t *grid, uint8_t p) {
    uint8_t q = p + 1, r = p + 16, s = p + 17;
    grid[p] = grid[p] < 0x10 ? 0x25 : 0x29;
    grid[q] = grid[q] < 0x10 ? 0x26 : 0x2a;
    grid[r] = grid[r] >= 0x8a ? 0x82 : (grid[r] < 0x10 ? 0x27 : 0x2a);
    grid[s] = grid[s] >= 0x8a ? 0x83 : (grid[s] < 0x10 ? 0x28 : 0x29);
}
static void backup_attribute(DXWorldRoom *scene, uint8_t p) {
    uint8_t object = scene->blocks[p];
    if (object == 4 || (object == 9 && scene->room != 0x97) ||
        (object == 0xe1 && (scene->room == 0x0e || scene->room == 0x0c || scene->room == 0x1b))) return;
    static const uint8_t list[] = {3,4,9,0x5e,0x91,0xa1,0xaa,0xc4,0xc6,0xcc,0xdb,0xe1,0xe3,0xe8};
    unsigned x = p & 15, y = p >> 4;
    if (!x || x > 10 || !y || y > 8) return;
    for (unsigned i = 0; i < sizeof list; i++) if (object == list[i]) {
        scene->attributes[(y - 1) * 10 + x - 1] = object;
        break;
    }
}
static void macro(const uint8_t *rom, DXWorldRoom *scene, unsigned object,
                  unsigned position, unsigned run, uint8_t state) {
    uint8_t *grid = scene->blocks;
    uint8_t p = (uint8_t)(0x11 + position);
    if (object == 0xf5) {
        if (!(p & 15)) p -= 16; // original left-border tree convention
        unsigned count = run ? run & 15 : 1;
        for (unsigned i = 0; i < count; i++) {
            tree(grid, p);
            p += (run & 0x40) ? 32 : 2;
        }
        return;
    }
    // Offsets/IDs are read from the game's own macro data, not duplicated assets.
    static const uint16_t positions[] = {0x762e,0x765a,0x760b,0x767a,0x7694,0x76b7,0x76cd,0x76f7};
    static const uint16_t objects[] = {0x763e,0x7664,0x7612,0x7681,0x7699,0x76bc,0x76dc,0x76fe};
    unsigned i = object - 0xf6;
    const uint8_t *pos = rom + offset(0x24, positions[i] - 0x14);
    unsigned ids = objects[i] - 0x14;
    if (object == 0xfa && (state & 4)) ids = 0x769d - 0x14;
    const uint8_t *values = rom + offset(0x24, ids);
    for (unsigned n = 0; n < 16 && pos[n] != 0xff; n++) {
        uint8_t cell = (uint8_t)(p + pos[n]);
        grid[cell] = values[n];
        backup_attribute(scene, cell);
    }
}
bool dx_world_decode(const uint8_t *rom, size_t size, uint8_t room,
                     const uint8_t status[256], DXWorldRoom *out) {
    if (!rom || size != 1024 * 1024 || !status || !out) return false;
    unsigned bank = room < 0x80 ? 9 : 0x1a;
    unsigned address = word(rom + offset(9, 0x4000) + 2 * room);
    size_t overlay = offset(room < 0xcc ? 0x26 : 0x27, 0x4000) + (room < 0xcc ? room : room - 0xcc) * 80;
    static const struct { uint8_t room, state_room; uint16_t address, overlay; } alternatives[] = {
        {0x0e,0x0e,0x47ec,0x5090}, {0x8c,0x8c,0x434e,0x51d0},
        {0x79,0x79,0x6513,0x5180}, {0x06,0x06,0x4496,0x5040},
        {0x1b,0x2b,0x4c0f,0x50e0}, {0x2b,0x2b,0x509a,0x5130}
    };
    for (unsigned i = 0; i < sizeof alternatives / sizeof *alternatives; i++) {
        if (room == alternatives[i].room && (status[alternatives[i].state_room] & 0x10)) {
            address = alternatives[i].address;
            overlay = offset(0x27, alternatives[i].overlay);
        }
    }
    if (!address_ok(address)) return false;
    size_t cursor = offset(bank, address), end = (bank + 1) * 0x4000;
    if (cursor + 2 >= end || rom[cursor] == 0xfe) return false;
    DXWorldRoom result;
    result.room = room;
    result.animation = rom[cursor++];
    memset(result.blocks, rom[cursor++], sizeof result.blocks);
    memcpy(result.attributes, rom + overlay, 80);
    bool terminated = false;
    while (cursor < end) {
        unsigned first = rom[cursor++], run = 0;
        if (first == 0xfe) { terminated = true; break; }
        if ((first & 0xfc) == 0xe0) { // warp records do not draw objects
            if (end - cursor < 4) return false;
            cursor += 4;
            continue;
        }
        if ((first & 0x90) == 0x80) {
            run = first;
            if (!(run & 15) || cursor >= end) return false;
            first = rom[cursor++];
        }
        if (cursor >= end) return false;
        unsigned object = rom[cursor++];
        if (object >= 0xf5 && object <= 0xfd) {
            macro(rom, &result, object, first, run, status[room]);
        } else {
            uint8_t p = (uint8_t)(0x11 + first);
            object = changed_object(object, room, status[room]);
            unsigned count = run ? run & 15 : 1;
            for (unsigned i = 0; i < count; i++) {
                result.blocks[p] = (uint8_t)object;
                if (!run) backup_attribute(&result, p);
                p += (run & 0x40) ? 16 : 1;
            }
        }
    }
    if (!terminated) return false;
    // The original room loader pads outside the 10x8 collision rectangle.
    for (unsigned y = 0; y < 16; y++) for (unsigned x = 0; x < 16; x++)
        if (!y || y > 8 || !x || x > 10) result.blocks[y * 16 + x] = 0xff;
    *out = result;
    return true;
}

bool dx_world_draw(const uint8_t *rom, size_t size, const DXWorldRoom *room,
                   const uint8_t *vram, uint8_t current_tileset, uint32_t *pixels) {
    return dx_world_draw_layers(rom,size,room,vram,current_tileset,pixels,NULL);
}

bool dx_world_draw_layers(const uint8_t *rom, size_t size, const DXWorldRoom *room,
                          const uint8_t *vram, uint8_t current_tileset,
                          uint32_t *pixels, uint8_t *priority) {
    if (!rom || size != 1024 * 1024 || !room || !vram || !pixels) return false;
    unsigned id = room->room;
    unsigned attribute_bank = rom[offset(0x1a, 0x6476) + id];
    unsigned attribute_address = word(rom + offset(0x1a, 0x5e76) + id * 2);
    unsigned palette_id = rom[offset(0x21, 0x42ef) + id];
    unsigned palette_address = word(rom + offset(0x21, 0x42b1) + palette_id * 2);
    if (attribute_bank >= 64 || !address_ok(attribute_address) || attribute_address > 0x7c00 ||
        !address_ok(palette_address) || palette_address > 0x7fc0) return false;
    const uint8_t *attrs = rom + offset(attribute_bank, attribute_address);
    const uint8_t *colors = rom + offset(0x21, palette_address);
    uint32_t palette[32];
    for (unsigned i = 0; i < 32; i++) {
        unsigned c = word(colors + i * 2);
        unsigned r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31;
        palette[i] = 0xff000000 | ((r << 3 | r >> 2) << 16) | ((g << 3 | g >> 2) << 8) | (b << 3 | b >> 2);
    }
    unsigned region_id = id == 7 ? 8 : id;
    unsigned region = (region_id & 0xe0) >> 2 | (region_id & 0x0e) >> 1;
    unsigned tileset = rom[offset(0x20, 0x6e84) + region]; // French Rev 1 code bank 20
    if (tileset == 0x0f || (tileset == 0x1a && id != 0x37)) tileset = current_tileset;
    if (tileset > 0x3e) return false;
    for (unsigned by = 0; by < 8; by++) for (unsigned bx = 0; bx < 10; bx++) {
        unsigned attribute = room->attributes[by * 10 + bx];
        for (unsigned q = 0; q < 4; q++) {
            unsigned tile = rom[offset(0x1a, 0x6b1d) + attribute * 4 + q];
            unsigned flags = attrs[attribute * 4 + q];
            unsigned tile_addr = 0x1000 + (int8_t)tile * 16;
            const uint8_t *pattern = vram + ((flags & 8) ? 0x2000 : 0) + tile_addr;
            if (!(flags & 8) && tile < 0x20)
                pattern = rom + offset(0x2f, 0x4000 + tileset * 0x100) + tile * 16;
            for (unsigned y = 0; y < 8; y++) for (unsigned x = 0; x < 8; x++) {
                unsigned py = (flags & 0x40) ? 7 - y : y;
                unsigned bit = (flags & 0x20) ? x : 7 - x;
                unsigned color = ((pattern[py * 2] >> bit) & 1) | (((pattern[py * 2 + 1] >> bit) & 1) << 1);
                unsigned dx = bx * 16 + (q & 1) * 8 + x, dy = by * 16 + (q >> 1) * 8 + y;
                pixels[dy * 160 + dx] = palette[(flags & 7) * 4 + color];
                if (priority) priority[dy * 160 + dx] = (uint8_t)(color | (flags & 0x80));
            }
        }
    }
    return true;
}

bool dx_world_spawns(const uint8_t *rom, size_t size, uint8_t room,
                     uint8_t cleared, DXWorldSpawns *out) {
    if (!rom || size != 1024 * 1024 || !out) return false;
    unsigned address = word(rom + offset(0x16, 0x4000) + room * 2);
    if (!address_ok(address)) return false;
    size_t cursor = offset(0x16, address), end = offset(0x17, 0x4000);
    DXWorldSpawns result = {0};
    for (unsigned order = 0; order < 256 && cursor < end; order++) {
        unsigned position = rom[cursor++];
        if (position == 0xff) { *out = result; return true; }
        if (cursor == end) return false;
        unsigned type = rom[cursor++];
        if ((order < 8 && (cleared & (1u << order))) || result.count == 16) continue;
        DXWorldSpawn *spawn = &result.entries[result.count++];
        spawn->x = (room & 15) * 160 + ((position & 15) * 16 + 8);
        spawn->y = (room >> 4) * 128 + (uint8_t)((position & 0xf0) + 16);
        spawn->type = type;
        spawn->order = order;
    }
    return false;
}
