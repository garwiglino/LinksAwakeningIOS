#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Native primitives; not yet connected to neighboring-room AI.
// Fraction uses the original 8-bit accumulator (1/256 pixel); speed is 1/16 px.
typedef struct { int32_t position; uint8_t fraction; } DXEntityAxis;
bool dx_entity_move_axis(DXEntityAxis *axis, int8_t speed);

// One already resolved 8x8 or 8x16 CGB sprite, in world coordinates.
// Caller resolves animation/tilesheet and entity order; no OAM coordinate wrap.
typedef struct {
    int32_t x, y;
    uint8_t tile, flags, height;
} DXEntitySprite;
// Background metadata: original color index in bits 0..1, priority in bit 7.
// Required for faithful occlusion, since an RGB color cannot recover this data.
bool dx_entity_draw_sprite(const DXEntitySprite *sprite, int32_t camera_x,
                           int32_t camera_y, const uint8_t vram[0x4000],
                           const uint8_t palette[64], const uint8_t *background,
                           bool background_priority, unsigned width, unsigned height,
                           uint32_t *pixels, size_t capacity);
