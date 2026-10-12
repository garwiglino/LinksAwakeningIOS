#include "DXCore.h"
#include "gbrt.h"
#include "ppu.h"
#include "audio.h"
#include "rom.h"
#include "GameIdentity.h"
#include <stdatomic.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <errno.h>
#include <unistd.h>

uint8_t g_joypad_buttons = 0xff, g_joypad_dpad = 0xff;
static GBContext *game;
static char directory[4096], error_text[256];
static pthread_mutex_t frame_lock = PTHREAD_MUTEX_INITIALIZER;
static uint32_t frame[160 * 144];
static bool frame_available;
static bool invalid_battery;
enum { AUDIO_CAPACITY = 8192 };
static int16_t audio_samples[AUDIO_CAPACITY][2];
static _Atomic uint32_t audio_read_index, audio_write_index;

static bool fail(const char *message) {
    snprintf(error_text, sizeof error_text, "%s", message);
    return false;
}
const char *dx_error(void) { return error_text; }
const char *dx_rom_identity(void) { return DX_ROM_SHA256; }

static bool path_for(char *out, size_t capacity, const char *extension) {
    int length = snprintf(out, capacity, "%s/%s.%s", directory, DX_ROM_SHA256, extension);
    return length >= 0 && (size_t)length < capacity;
}

static bool atomic_write(const char *path, const void *data, size_t size) {
    char temporary[4200];
    if (snprintf(temporary, sizeof temporary, "%s.tmp", path) >= (int)sizeof temporary)
        return fail("Chemin de sauvegarde trop long.");
    FILE *f = fopen(temporary, "wb");
    if (!f) return fail("Impossible de créer la sauvegarde.");
    bool ok = fwrite(data, 1, size, f) == size;
    if (fflush(f) != 0) ok = false;
    if (fsync(fileno(f)) != 0) ok = false;
    if (fclose(f) != 0) ok = false;
    if (ok && rename(temporary, path) == 0) return true;
    remove(temporary);
    return fail("Échec d’écriture : la sauvegarde précédente est conservée.");
}
static bool save_ram(GBContext *ctx, const char *name, const void *data, size_t size) {
    (void)ctx; (void)name;
    if (invalid_battery) return fail("Sauvegarde cartouche invalide : fichier conservé.");
    char path[4200];
    return path_for(path, sizeof path, "sav") && atomic_write(path, data, size);
}
static bool load_ram(GBContext *ctx, const char *name, void *data, size_t size) {
    (void)ctx; (void)name;
    char path[4200];
    if (!path_for(path, sizeof path, "sav")) return false;
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    uint8_t *copy = malloc(size);
    if (!copy) { fclose(f); return false; }
    bool ok = fread(copy, 1, size, f) == size && fgetc(f) == EOF;
    fclose(f);
    if (ok) memcpy(data, copy, size);
    else { invalid_battery = true; fail("Sauvegarde cartouche invalide : fichier conservé."); }
    free(copy);
    return ok;
}
static void sample(GBContext *ctx, int16_t left, int16_t right) {
    (void)ctx;
    uint32_t w = atomic_load_explicit(&audio_write_index, memory_order_relaxed);
    uint32_t r = atomic_load_explicit(&audio_read_index, memory_order_acquire);
    if (w - r == AUDIO_CAPACITY) return;
    audio_samples[w % AUDIO_CAPACITY][0] = left;
    audio_samples[w % AUDIO_CAPACITY][1] = right;
    atomic_store_explicit(&audio_write_index, w + 1, memory_order_release);
}
void dx_audio_read(float *left, float *right, size_t frames) {
    uint32_t r = atomic_load_explicit(&audio_read_index, memory_order_relaxed);
    uint32_t w = atomic_load_explicit(&audio_write_index, memory_order_acquire);
    for (size_t i = 0; i < frames; i++) {
        if (r != w) {
            left[i] = audio_samples[r % AUDIO_CAPACITY][0] / 32768.0f;
            right[i] = audio_samples[r % AUDIO_CAPACITY][1] / 32768.0f;
            r++;
        } else left[i] = right[i] = 0;
    }
    atomic_store_explicit(&audio_read_index, r, memory_order_release);
}
void dx_audio_clear(void) {
    atomic_store(&audio_read_index, 0);
    atomic_store(&audio_write_index, 0);
}
static void publish_frame(void) {
    const uint32_t *pixels = gb_get_framebuffer(game);
    if (!pixels) return;
    pthread_mutex_lock(&frame_lock);
    memcpy(frame, pixels, sizeof frame);
    frame_available = true;
    pthread_mutex_unlock(&frame_lock);
}
bool dx_copy_frame(uint32_t *pixels, size_t count) {
    if (!pixels || count < 160 * 144) return false;
    pthread_mutex_lock(&frame_lock);
    bool available = frame_available;
    if (available) memcpy(pixels, frame, sizeof frame);
    pthread_mutex_unlock(&frame_lock);
    return available;
}
bool dx_create(const char *save_directory) {
    if (game) return fail("Le moteur est déjà lancé.");
    error_text[0] = 0;
    invalid_battery = false;
    if (!save_directory || strlen(save_directory) >= sizeof directory)
        return fail("Dossier de sauvegarde invalide.");
    strcpy(directory, save_directory);
    GBConfig config = { .model = GB_MODEL_CGB, .enable_audio = true, .speed_percent = 100 };
    game = gb_context_create(&config);
    if (!game || !game->ppu || !game->apu) {
        dx_destroy();
        return fail("Mémoire insuffisante pour le moteur.");
    }
    GBPlatformCallbacks callbacks = {0};
    callbacks.on_audio_sample = sample;
    callbacks.save_battery_ram = save_ram;
    callbacks.load_battery_ram = load_ram;
    gb_set_platform_callbacks(game, &callbacks);
    rom_init(game);
    if (invalid_battery) {
        dx_destroy();
        return fail("Sauvegarde cartouche invalide. Restaurez une copie valide du fichier .sav.");
    }
    if (!game->rom || game->rom_size != DX_ROM_SIZE) {
        dx_destroy();
        return fail("Le chargement du jeu recompilé a échoué.");
    }
    g_joypad_buttons = g_joypad_dpad = 0xff;
    dx_audio_clear();
    return true;
}
void dx_destroy(void) {
    if (game) gb_context_destroy(game);
    game = NULL;
    pthread_mutex_lock(&frame_lock);
    frame_available = false;
    pthread_mutex_unlock(&frame_lock);
}
bool dx_step(uint8_t buttons, uint8_t directions) {
    if (!game) return false;
    uint8_t old = gb_read8(game, 0xff00);
    g_joypad_buttons = (uint8_t)(0xff ^ (buttons & 15));
    // Opposite directions cancel each other; diagonals remain possible.
    if ((directions & 3) == 3) directions &= ~3;
    if ((directions & 12) == 12) directions &= ~12;
    g_joypad_dpad = (uint8_t)(0xff ^ (directions & 15));
    uint8_t now = gb_read8(game, 0xff00);
    if (old & ~now & 15) game->io[0x0f] |= 0x10;
    game->last_joypad = now;
    gb_run_frame(game);
    publish_frame();
    return true;
}
bool dx_flush_battery(void) {
    if (!game) return false;
    if (!game->eram_size) return true;
    return gb_context_save_ram(game);
}

// States are deliberately bound to this runtime ABI and exact ROM. No pointer,
// callback, file handle or allocator-owned object is ever restored from disk.
// Version changes require migration, not silently interpreting a new layout.
enum { STATE_VERSION = 1, HEADER_SIZE = 112 };
static void put32(uint8_t *p, uint32_t v) {
    for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (i * 8));
}
static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static uint32_t crc32(const uint8_t *data, size_t size) {
    uint32_t crc = ~0u;
    for (size_t i = 0; i < size; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1)));
    }
    return ~crc;
}
static size_t state_size(void) {
    return offsetof(GBContext, rom) + sizeof game->rtc + 1 +
        0x8000 + 0x4000 + 0xa0 + 0x7f + 0x81 + game->eram_size +
        sizeof(GBPPU) + gb_audio_state_size();
}
static void transfer_state(uint8_t *buffer, bool loading) {
    uint8_t *p = buffer;
#define COPY_STATE(location, n) do { size_t z = (n); if (loading) memcpy((location), p, z); \
    else memcpy(p, (location), z); p += z; } while (0)
    COPY_STATE(game, offsetof(GBContext, rom));
    COPY_STATE(&game->rtc, sizeof game->rtc);
    COPY_STATE(&game->last_joypad, 1);
    COPY_STATE(game->wram, 0x8000);
    COPY_STATE(game->vram, 0x4000);
    COPY_STATE(game->oam, 0xa0);
    COPY_STATE(game->hram, 0x7f);
    COPY_STATE(game->io, 0x81);
    if (game->eram_size) COPY_STATE(game->eram, game->eram_size);
    COPY_STATE(game->ppu, sizeof(GBPPU));
    if (loading) gb_audio_restore_state(game->apu, p, gb_audio_state_size());
    else gb_audio_copy_state(game->apu, p, gb_audio_state_size());
#undef COPY_STATE
}
bool dx_save_state(void) {
    if (!game) return false;
    size_t size = state_size();
    uint8_t *data = calloc(1, HEADER_SIZE + size);
    if (!data) return fail("Mémoire insuffisante pour sauvegarder.");
    memcpy(data, "LADXIOS\0", 8);
    put32(data + 8, STATE_VERSION);
    put32(data + 12, (uint32_t)size);
    memcpy(data + 16, DX_ROM_SHA256, 64);
    put32(data + 84, (uint32_t)offsetof(GBContext, rom));
    put32(data + 88, (uint32_t)sizeof(GBPPU));
    put32(data + 92, (uint32_t)gb_audio_state_size());
    put32(data + 96, (uint32_t)game->eram_size);
    transfer_state(data + HEADER_SIZE, false);
    put32(data + 80, crc32(data + HEADER_SIZE, size));
    char path[4200];
    bool ok = path_for(path, sizeof path, "state") && atomic_write(path, data, HEADER_SIZE + size);
    free(data);
    return ok;
}
bool dx_has_state(void) {
    char path[4200];
    return path_for(path, sizeof path, "state") && access(path, R_OK) == 0;
}
bool dx_load_state(void) {
    if (!game) return false;
    char path[4200];
    if (!path_for(path, sizeof path, "state")) return false;
    FILE *f = fopen(path, "rb");
    if (!f) return fail("Aucune reprise enregistrée pour cette ROM.");
    size_t size = state_size();
    uint8_t *data = malloc(HEADER_SIZE + size);
    if (!data) { fclose(f); return fail("Mémoire insuffisante."); }
    bool ok = fread(data, 1, HEADER_SIZE + size, f) == HEADER_SIZE + size && fgetc(f) == EOF;
    fclose(f);
    ok = ok && !memcmp(data, "LADXIOS\0", 8) && get32(data + 8) == STATE_VERSION &&
        get32(data + 12) == size && !memcmp(data + 16, DX_ROM_SHA256, 64) &&
        get32(data + 84) == offsetof(GBContext, rom) && get32(data + 88) == sizeof(GBPPU) &&
        get32(data + 92) == gb_audio_state_size() && get32(data + 96) == game->eram_size &&
        get32(data + 80) == crc32(data + HEADER_SIZE, size);
    if (ok) {
        transfer_state(data + HEADER_SIZE, true);
        publish_frame();
    }
    free(data);
    return ok || fail("Reprise incompatible ou endommagée : partie actuelle conservée.");
}
