#include "DXCore.h"
#include "GameIdentity.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

static uint32_t pixels[160 * 144];
static unsigned char *read_file(const char *path, size_t *size) {
    FILE *f = fopen(path, "rb"); assert(f);
    assert(fseek(f, 0, SEEK_END) == 0);
    *size = (size_t)ftell(f); rewind(f);
    unsigned char *data = malloc(*size); assert(data);
    assert(fread(data, 1, *size, f) == *size); fclose(f);
    return data;
}
static void write_file(const char *path, const void *data, size_t size) {
    FILE *f = fopen(path, "wb"); assert(f);
    assert(fwrite(data, 1, size, f) == size); assert(fclose(f) == 0);
}
static void step(int frames, uint8_t buttons) {
    float left[740], right[740];
    for (int i = 0; i < frames; i++) {
        assert(dx_step(buttons, 0));
        dx_audio_read(left, right, 738);
        for (int j = 0; j < 738; j++) assert(isfinite(left[j]) && isfinite(right[j]));
    }
}
int main(int argc, char **argv) {
    assert(argc == 2);
    assert(dx_create(argv[1]));
    assert(!dx_create(argv[1])); // never create two owners of global runtime
    assert(!dx_copy_frame(pixels, 12));
    step(600, 0);
    step(1, 8); // Start
    step(120, 0);
    assert(dx_copy_frame(pixels, 160 * 144));
    char image[4096]; snprintf(image, sizeof image, "%s/frame.ppm", argv[1]);
    FILE *f = fopen(image, "wb"); assert(f);
    fprintf(f, "P6\n160 144\n255\n");
    for (size_t i = 0; i < 160*144; i++) {
        unsigned char rgb[3] = {pixels[i] >> 16, pixels[i] >> 8, pixels[i]};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    assert(dx_save_state()); assert(dx_has_state()); assert(dx_flush_battery());
    char state[4096]; snprintf(state, sizeof state, "%s/%s.state", argv[1], dx_rom_identity());
    size_t saved_size; unsigned char *saved = read_file(state, &saved_size);
    step(60, 0);
    assert(dx_save_state());
    size_t expected_size; unsigned char *expected = read_file(state, &expected_size);
    write_file(state, saved, saved_size);
    assert(dx_load_state()); dx_audio_clear();
    step(60, 0); assert(dx_save_state());
    size_t actual_size; unsigned char *actual = read_file(state, &actual_size);
    assert(expected_size == actual_size && !memcmp(expected, actual, actual_size));
    // Truncation and checksum failures must not mutate the current machine.
    write_file(state, saved, saved_size / 2); assert(!dx_load_state());
    saved[saved_size - 1] ^= 1;
    write_file(state, saved, saved_size); assert(!dx_load_state());
    assert(dx_save_state());
    size_t intact_size; unsigned char *intact = read_file(state, &intact_size);
    assert(actual_size == intact_size && !memcmp(actual, intact, intact_size));
    free(saved); free(expected); free(actual); free(intact);
    dx_destroy();
    assert(dx_create(argv[1])); assert(dx_load_state()); step(10, 0); dx_destroy();
    puts("PASS: boot, frames/audio, deterministic snapshot, corrupt/truncated rejection, restart");
    return 0;
}
