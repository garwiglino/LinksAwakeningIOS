#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct GBContext GBContext;
enum { DX_VIDEO_MAX_WIDTH = 512, DX_VIDEO_HEIGHT = 144 };
// Experimental overworld compositor. False means use the original frame.
// It never changes the game context, input, timing, or save state.
bool dx_wide_compose(const GBContext *game, const uint32_t *original, unsigned width, uint32_t *output);
