#include "DXEntity.h"
#include <limits.h>

bool dx_entity_move_axis(DXEntityAxis *axis, int8_t speed) {
    if (!axis) return false;
    unsigned fraction = axis->fraction + ((unsigned)(uint8_t)speed & 15) * 16;
    int integral = speed >= 0 ? speed / 16 : -((-(int)speed + 15) / 16);
    int64_t position = (int64_t)axis->position + integral + (fraction >> 8);
    if (position < INT32_MIN || position > INT32_MAX) return false;
    axis->fraction = (uint8_t)fraction;
    axis->position = (int32_t)position;
    return true;
}

bool dx_entity_draw_sprite(const DXEntitySprite *sprite, int32_t camera_x,
                           int32_t camera_y, const uint8_t vram[0x4000],
                           const uint8_t palette[64], const uint8_t *background,
                           bool background_priority, unsigned width, unsigned height,
                           uint32_t *pixels, size_t capacity) {
    if (!sprite || !vram || !palette || !background || !pixels || !width || !height ||
        (sprite->height != 8 && sprite->height != 16) ||
        (size_t)width > SIZE_MAX / height || capacity < (size_t)width * height) return false;
    int64_t left = (int64_t)sprite->x - camera_x, top = (int64_t)sprite->y - camera_y;
    if (left >= width || left + 8 <= 0 || top >= height || top + sprite->height <= 0) return true;
    unsigned flags = sprite->flags;
    unsigned tile = sprite->height == 16 ? sprite->tile & 0xfe : sprite->tile;
    unsigned bank = (flags & 8) ? 0x2000 : 0;
    for (unsigned y = 0; y < sprite->height; y++) {
        int64_t dy = top + y;
        if (dy < 0 || dy >= height) continue;
        unsigned row = (flags & 0x40) ? sprite->height - 1 - y : y;
        unsigned address = bank + tile * 16 + row * 2;
        for (unsigned x = 0; x < 8; x++) {
            int64_t dx = left + x;
            if (dx < 0 || dx >= width) continue;
            unsigned bit = (flags & 0x20) ? x : 7 - x;
            unsigned color = ((vram[address] >> bit) & 1) | (((vram[address + 1] >> bit) & 1) << 1);
            if (!color) continue;
            size_t destination = (size_t)dy * width + (size_t)dx;
            uint8_t bg = background[destination];
            if (background_priority && ((flags | bg) & 0x80) && (bg & 3)) continue;
            unsigned entry = ((flags & 7) * 4 + color) * 2;
            unsigned c = palette[entry] | (unsigned)palette[entry + 1] << 8;
            unsigned r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31;
            pixels[destination] = 0xff000000 | ((r << 3 | r >> 2) << 16) |
                                  ((g << 3 | g >> 2) << 8) | (b << 3 | b >> 2);
        }
    }
    return true;
}
