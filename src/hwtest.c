/*
 * Pico Held 2 bring-up test for the wili8jam port (target: picoheld2_hwtest).
 *
 * Shows a 128x128 PICO-8 test pattern through the same path as the emulator
 * (gfx.c -> held_lcd_present), so it checks:
 *   - LCD init at 252 MHz, double buffering, CLUT colours (16 PICO-8 colours)
 *   - orientation: the "top-left" corner is marked, the arrow points up
 *   - crop vs fit scaling: the outer 4 px frame is only visible in fit mode
 *   - buttons + analog stick (shown live), battery
 *   - audio: A plays a tone, B a noise burst
 *
 * Controls: SELECT+L toggles crop/fit, SELECT+UP/DOWN volume,
 *           SELECT+LEFT/RIGHT backlight, hold R = power off.
 * The raw ADC values of the D-pad axes, L, and the analog stick are shown
 * on screen and on the serial port (for tuning thresholds).
 * Debug output on USB CDC serial.
 */
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"

#include "held_hw.h"
#include "gfx.h"
#include "input.h"
#include "audio.h"
#include "p8_sfx.h"

// p8_sfx.c reads SFX/music data from PICO-8 memory; the test has no carts
static uint8_t dummy_p8_mem[0x8000];
uint8_t *p8_get_memory(void) { return dummy_p8_mem; }

static void draw_pattern(uint32_t frame) {
    gfx_cls(0);

    // outer 4 px frame: only visible in fit mode (cropped in 2x mode)
    gfx_rect(0, 0, 127, 127, 8);
    gfx_rect(1, 1, 126, 126, 8);
    gfx_rect(2, 2, 125, 125, 9);
    gfx_rect(3, 3, 124, 124, 9);
    // first fully visible pixel ring in crop mode
    gfx_rect(4, 4, 123, 123, 11);

    // top-left marker
    gfx_rectfill(5, 5, 12, 12, 7);
    gfx_print("tl", 14, 6, 7);

    // up arrow
    gfx_line(110, 6, 106, 12, 7);
    gfx_line(110, 6, 114, 12, 7);
    gfx_line(110, 6, 110, 18, 7);

    // 16 colour bars
    for (int i = 0; i < 16; i++) {
        int x = 8 + (i % 8) * 14;
        int y = 22 + (i / 8) * 14;
        gfx_rectfill(x, y, x + 11, y + 11, i);
        gfx_rect(x - 1, y - 1, x + 12, y + 12, 5);
    }

    // pixel-level check: 1 px checkerboard (shows the fit-mode pattern)
    for (int y = 52; y < 64; y++)
        for (int x = 8; x < 120; x++)
            gfx_pset(x, y, ((x + y) & 1) ? 7 : 0);

    // buttons
    static const char *names[] = { "<", ">", "^", "v", "a", "b", "x", "y",
                                   "l", "r", "st", "se", "sw" };
    uint32_t b = input_raw_buttons();
    int x = 8;
    for (int i = 0; i < 13; i++) {
        int c = (b & (1u << i)) ? 11 : 5;
        gfx_print(names[i], x, 70, c);
        x += (int)strlen(names[i]) * 4 + 4;
    }

    char buf[48];
    int raw[HELD_RAW_COUNT];
    held_buttons_raw(raw);
    snprintf(buf, sizeof(buf), "pad lr%4d ud%4d l%4d", raw[HELD_RAW_DPAD_LR],
             raw[HELD_RAW_DPAD_UD], raw[HELD_RAW_L]);
    gfx_print(buf, 8, 78, 6);
    int spx, spy;
    held_stick_get(&spx, &spy);
    snprintf(buf, sizeof(buf), "stick x%4d y%4d %4d%%%4d%%", raw[HELD_RAW_STICK_X],
             raw[HELD_RAW_STICK_Y], spx, spy);
    gfx_print(buf, 8, 85, 6);
    snprintf(buf, sizeof(buf), "bat %d%% %s  bl %d", held_battery_level(),
             held_on_external_power() ? "usb" : "", held_backlight_get());
    gfx_print(buf, 8, 92, 6);
    snprintf(buf, sizeof(buf), "%s  %lu mhz",
             held_lcd_get_scale_mode() == HELD_SCALE_FIT ? "fit" : "crop",
             (unsigned long)(clock_get_hz(clk_sys) / 1000000));
    gfx_print(buf, 8, 99, 6);
    gfx_print("a:tone b:noise sel+l:fit", 8, 107, 13);

    // moving bar: shows tearing / dropped frames
    int mx = 8 + (int)(frame % 104);
    gfx_rectfill(mx, 116, mx + 7, 120, 10);
}

int main(void) {
    held_hw_early_init();

    held_set_sys_clock();

    stdio_init_all();

    gfx_init();
    int err = held_lcd_init();

    p8_sfx_init();
    audio_init();

    uint32_t frame = 0;
    uint32_t t_last = to_ms_since_boot(get_absolute_time());
    uint32_t frames_last = 0;
    while (true) {
        input_update();

        if (input_btnp(4, 0)) audio_tone(0, 440.0f, 300, WAVE_SQUARE);
        if (input_btnp(5, 0)) audio_tone(1, 1000.0f, 200, WAVE_NOISE);

        draw_pattern(frame);
        gfx_flip();
        frame++;

        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (now - t_last >= 2000) {
            int raw[HELD_RAW_COUNT];
            held_buttons_raw(raw);
            int sc[6];
            held_stick_get_calibration(sc);
            printf("stick cal x %d/%d/%d  y %d/%d/%d (min/centre/max)\n",
                   sc[0], sc[1], sc[2], sc[3], sc[4], sc[5]);
            printf("hwtest: lcd=%d  %lu fps  buttons=0x%04lx  bat=%d%%  "
                   "adc lr=%d ud=%d l=%d stick x=%d y=%d\n", err,
                   (unsigned long)((frame - frames_last) * 1000 / (now - t_last)),
                   (unsigned long)input_raw_buttons(), held_battery_level(),
                   raw[HELD_RAW_DPAD_LR], raw[HELD_RAW_DPAD_UD], raw[HELD_RAW_L],
                   raw[HELD_RAW_STICK_X], raw[HELD_RAW_STICK_Y]);
            t_last = now;
            frames_last = frame;
        }
    }
}
