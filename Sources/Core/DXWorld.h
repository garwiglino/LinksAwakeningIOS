#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
// Pure native scene decoding: no GBContext, CPU execution, or mutation of saves.
// This profile is verified against the unmodified French DX Rev 1 ROM.
typedef struct {
    uint8_t blocks[256]; // original padded 16-column room layout
    uint8_t attributes[80];
    uint8_t animation;
    uint8_t room;
} DXWorldRoom;
bool dx_world_decode(const uint8_t *rom, size_t size, uint8_t room,
                     const uint8_t status[256], DXWorldRoom *out);
// Draw a 160x128 room using ROM palettes and the current shared tile patterns.
// Shared VRAM supplies animation frames; room-specific tiles come from the ROM.
bool dx_world_draw(const uint8_t *rom, size_t size, const DXWorldRoom *room,
                   const uint8_t *vram, uint8_t current_tileset, uint32_t *pixels);

// Spawn definitions only: animation, AI, collision and lifetime are not ported.
// Coordinates are global overworld coordinates, without the GB's 8-bit wrap.
typedef struct {
    uint16_t x, y;
    uint8_t type, order;
} DXWorldSpawn;
typedef struct {
    DXWorldSpawn entries[16];
    unsigned count;
} DXWorldSpawns;
bool dx_world_spawns(const uint8_t *rom, size_t size, uint8_t room,
                     uint8_t cleared, DXWorldSpawns *out);

// As above, also exports raw background color and priority for native sprites.
bool dx_world_draw_layers(const uint8_t *rom, size_t size, const DXWorldRoom *room,
                          const uint8_t *vram, uint8_t current_tileset,
                          uint32_t *pixels, uint8_t *priority);
