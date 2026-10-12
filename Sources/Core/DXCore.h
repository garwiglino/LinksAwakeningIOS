#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// One engine instance, owned by one serial queue. Only the audio and frame-copy
// functions may run concurrently with that queue.
bool dx_create(const char *save_directory);
void dx_destroy(void);
bool dx_step(uint8_t pressed_buttons, uint8_t pressed_directions);
bool dx_flush_battery(void);
bool dx_save_state(void);
bool dx_load_state(void);
bool dx_has_state(void);
const char *dx_error(void);
const char *dx_rom_identity(void);
bool dx_copy_frame(uint32_t *pixels, size_t pixel_count);
// Optional experimental native scenery extension (French DX Rev 1 only).
// Main thread may request a width; unsupported scenes publish 160 unchanged.
bool dx_wide_supported(void);
void dx_request_video_width(unsigned width);
// Buffer capacity must be at least 512*144; returned frame is tightly packed.
bool dx_copy_video(uint32_t *pixels, size_t pixel_count, unsigned *width);
// Single realtime consumer. Produces silence when the ring is empty.
void dx_audio_read(float *left, float *right, size_t frames);
// Call only with the audio engine stopped and no producer in flight.
void dx_audio_clear(void);
#ifdef __cplusplus
}
#endif
