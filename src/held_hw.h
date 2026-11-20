/*
 * Pico Held 2 hardware glue for wili8jam.
 *
 * Thin C API on top of the Pico Engine (C++) drivers in picoengine/:
 * power latch, buttons, battery, backlight and the 720x720 DPI LCD.
 */
#ifndef HELD_HW_H
#define HELD_HW_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// --- Power / buttons ---------------------------------------------------

// Latch the power switch, set up buttons + ADC. Call as early as possible.
void held_hw_early_init(void);

// Core voltage + system clock (HELD_SYS_CLOCK_KHZ, default 252000).
// Call before the LCD, audio and PSRAM are set up: their clock dividers
// are derived from clk_sys.
void held_set_sys_clock(void);

// Cut power (does not return when on battery).
void held_power_off(void);

// Battery level 0..100 (%), coarse.
int  held_battery_level(void);
// true when running on USB power / charging.
bool held_on_external_power(void);

// Physical button bits returned by held_buttons_poll()
#define HELD_BTN_LEFT   (1u << 0)
#define HELD_BTN_RIGHT  (1u << 1)
#define HELD_BTN_UP     (1u << 2)
#define HELD_BTN_DOWN   (1u << 3)
#define HELD_BTN_A      (1u << 4)
#define HELD_BTN_B      (1u << 5)
#define HELD_BTN_X      (1u << 6)
#define HELD_BTN_Y      (1u << 7)
#define HELD_BTN_L      (1u << 8)
#define HELD_BTN_R      (1u << 9)
#define HELD_BTN_START  (1u << 10)
#define HELD_BTN_SELECT (1u << 11)
#define HELD_BTN_STICK  (1u << 12)

// Sample all buttons (D-pad + analog stick both report as directions).
uint32_t held_buttons_poll(void);

// Raw ADC values (0..4095) of the last held_buttons_poll(), for debugging
#define HELD_RAW_DPAD_LR  0
#define HELD_RAW_DPAD_UD  1
#define HELD_RAW_L        2
#define HELD_RAW_STICK_X  3
#define HELD_RAW_STICK_Y  4
#define HELD_RAW_COUNT    5
void held_buttons_raw(int out[HELD_RAW_COUNT]);

// D-pad ADC thresholds (defaults: held_config.h). Order: left max, right min,
// up max, down min.
void held_dpad_set_thresholds(int left_max, int right_min, int up_max, int down_min);
void held_dpad_get_thresholds(int out[4]);

// --- LCD -----------------------------------------------------------------

// Scaling of the 128x128 PICO-8 screen onto the 240x240 LCD framebuffer
#define HELD_SCALE_CROP 0   // 2x, outer 4 PICO-8 pixels on each side cut off
#define HELD_SCALE_FIT  1   // 1.875x nearest neighbour, whole screen visible

// Start the LCD (2 framebuffers, fixed refresh). Must run BEFORE sd_init():
// the panel's config SPI shares GPIO 34 with the SD card.
// Returns 0 on success, negative lcd_error_t otherwise.
int  held_lcd_init(void);

// Present a 128x128 4bpp PICO-8 frame (low nibble = left pixel).
// disp_pal: optional 16-entry screen palette (may be NULL).
// pal565:   16 RGB565 colours for the PICO-8 colour indices.
// May block until the next vblank if no back buffer is free.
void held_lcd_present(const uint8_t *fb4, const uint8_t *disp_pal, const uint16_t *pal565);

// Frame drop mode (used by the cart game loop): if no back buffer is free,
// held_lcd_present() skips the frame instead of waiting for the next vblank.
void held_lcd_set_frame_drop(bool enable);
uint32_t held_lcd_frames_dropped(void);

void held_lcd_set_scale_mode(int mode);
int  held_lcd_get_scale_mode(void);

// Backlight 5..100 (default HELD_BACKLIGHT_DEFAULT, see held_config.h)
void held_backlight_set(int level);
int  held_backlight_get(void);

#ifdef __cplusplus
}
#endif

#endif // HELD_HW_H
