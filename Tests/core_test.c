#include "DXCore.h"
#include "GameIdentity.h"
#include "gbrt.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

static uint32_t pixels[160 * 144];
static unsigned samples;
static void count_sample(GBContext *ctx, int16_t l, int16_t r) {
    (void)ctx; (void)l; (void)r; samples++;
}
static void timing_check(void) {
    for (int speed = 1; speed <= 2; speed++) {
        GBConfig config = {.model = GB_MODEL_CGB, .enable_audio = true};
        GBContext *ctx = gb_context_create(&config); assert(ctx);
        GBPlatformCallbacks callbacks = {.on_audio_sample = count_sample};
        gb_set_platform_callbacks(ctx, &callbacks);
        gb_write8(ctx, 0xff40, 0); // LCD off still needs a bounded host slice
        ctx->io[0x4d] = speed == 2 ? 0x80 : 0;
        samples = 0;
        gb_reset_frame(ctx);
        for (unsigned t = 0; t < 4194304; t += 4) gb_tick(ctx, 4 * speed);
        assert(samples == 44100);
        assert(ctx->frame_done);
        gb_context_destroy(ctx);
    }
}
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
    timing_check();
    assert(dx_create(argv[1]));
    assert(!dx_create(argv[1])); // never create two owners of global runtime
    assert(!dx_copy_frame(pixels, 12));
    step(600, 0);
    step(1, 8); // Start
    step(120, 0);
    step(180, 0);
    step(3, 8); // Start from the title screen into the save-selection menu
    step(60, 0);
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
    if (!DX_IS_FIXTURE) {
        char battery[4096]; snprintf(battery, sizeof battery, "%s/%s.sav", argv[1], dx_rom_identity());
        size_t size; unsigned char *valid = read_file(battery, &size);
        write_file(battery, valid, 1);
        assert(!dx_create(argv[1]));
        size_t unchanged; unsigned char *bad = read_file(battery, &unchanged);
        assert(unchanged == 1 && bad[0] == valid[0]);
        write_file(battery, valid, size); free(valid); free(bad);
    }
    puts("PASS: audio cadence at both CPU speeds, boot/input, deterministic snapshot, corruption rejection, restart");
    return 0;
}
