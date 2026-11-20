/*
 * Pico Held 2 hardware glue for wili8jam (see held_hw.h).
 */
#include "held_hw.h"

#include <cstdio>
#include <cstring>

#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include "hardware/clocks.h"

// Pico Engine (picoengine/src) - individual headers on purpose: picoengine.h
// defines pe_init() in the header, which breaks when included more than once.
#include <hwcfg.h>
#include <setup.h>
#include "hardware/lcd/lcd_dpi/lcd_dpi.h"
#include "hardware/lcd/backlight/backlight.h"
#include "hardware/controls/controls.h"
#include "hardware/power/power.h"

#if LCD_COLORDEPTH != 8
#error "wili8jam needs the Pico Engine LCD in 8 bit (CLUT) mode, see picoengine/src/setup.h"
#endif

// Number of LCD framebuffers (240x240x8bit = 57.6 KB each, internal SRAM).
// 2 = tear-free double buffering, 1 = single buffer (saves RAM, may tear).
#ifndef HELD_LCD_NUM_FB
#define HELD_LCD_NUM_FB 2
#endif

// Analog stick as additional D-pad
#ifndef HELD_USE_ANALOG_STICK
#define HELD_USE_ANALOG_STICK 1
#endif
#ifndef HELD_STICK_THRESHOLD
#define HELD_STICK_THRESHOLD 700   // of 4096 ADC counts
#endif
// Direction convention taken from the Pico Engine's original stick code:
// X above centre = left, Y below centre = up. Flip here if it's mirrored.
#ifndef HELD_STICK_INVERT_X
#define HELD_STICK_INVERT_X 1
#endif
#ifndef HELD_STICK_INVERT_Y
#define HELD_STICK_INVERT_Y 0
#endif

#define LCD_W 240
#define LCD_H 240
#define P8_W  128
#define P8_H  128

// ---------------------------------------------------------------------------
// Power / buttons
// ---------------------------------------------------------------------------

static int stick_x0, stick_y0;

void held_hw_early_init(void) {
    ctrl_init();   // GPIOs + ADC for buttons (also needed by pwr_init)
    pwr_init();    // latch PWR_EN so the device stays on

    // calibrate analog stick centre
    int sx = 0, sy = 0;
    for (int i = 0; i < 8; i++) {
        sx += analog_read(PIN_ANALOG_X);
        sy += analog_read(PIN_ANALOG_Y);
    }
    stick_x0 = sx / 8;
    stick_y0 = sy / 8;
}

void held_power_off(void) {
    lcd_backlight_set(0);
    sleep_ms(50);
    pwr_shutdown();

    // Still alive -> running on USB power. Stay dark until a button is pressed,
    // then reboot (acts like switching on again).
    sleep_ms(500);
    while (held_buttons_poll() != 0) sleep_ms(10);   // wait for release
    while (held_buttons_poll() == 0) sleep_ms(20);
    watchdog_reboot(0, 0, 0);
    while (true) tight_loop_contents();
}

int held_battery_level(void) {
    return pwr_get_bat_level(11);  // 10 % steps
}

bool held_on_external_power(void) {
    return pwr_get_source() == PWR_ON_GRD;
}

uint32_t held_buttons_poll(void) {
    ctrl_button_state_update();

    uint32_t b = 0;
    if (ctrl_is_pressed(BUTTON_LEFT))  b |= HELD_BTN_LEFT;
    if (ctrl_is_pressed(BUTTON_RIGHT)) b |= HELD_BTN_RIGHT;
    if (ctrl_is_pressed(BUTTON_UP))    b |= HELD_BTN_UP;
    if (ctrl_is_pressed(BUTTON_DOWN))  b |= HELD_BTN_DOWN;
    if (ctrl_is_pressed(BUTTON_A))     b |= HELD_BTN_A;
    if (ctrl_is_pressed(BUTTON_B))     b |= HELD_BTN_B;
    if (ctrl_is_pressed(BUTTON_X))     b |= HELD_BTN_X;
    if (ctrl_is_pressed(BUTTON_Y))     b |= HELD_BTN_Y;
    if (ctrl_is_pressed(BUTTON_L))     b |= HELD_BTN_L;
    if (ctrl_is_pressed(BUTTON_R))     b |= HELD_BTN_R;
    if (ctrl_is_pressed(BUTTON_ST))    b |= HELD_BTN_START;
    if (ctrl_is_pressed(BUTTON_SEL))   b |= HELD_BTN_SELECT;
    if (ctrl_is_pressed(BUTTON_SW))    b |= HELD_BTN_STICK;

#if HELD_USE_ANALOG_STICK
    int dx = analog_read(PIN_ANALOG_X) - stick_x0;
    int dy = analog_read(PIN_ANALOG_Y) - stick_y0;
#if HELD_STICK_INVERT_X
    dx = -dx;
#endif
#if HELD_STICK_INVERT_Y
    dy = -dy;
#endif
    if (dx < -HELD_STICK_THRESHOLD) b |= HELD_BTN_LEFT;
    if (dx >  HELD_STICK_THRESHOLD) b |= HELD_BTN_RIGHT;
    if (dy < -HELD_STICK_THRESHOLD) b |= HELD_BTN_UP;
    if (dy >  HELD_STICK_THRESHOLD) b |= HELD_BTN_DOWN;
#endif
    return b;
}

// ---------------------------------------------------------------------------
// LCD
// ---------------------------------------------------------------------------

static LCD_DPI *lcd = nullptr;
static color_palette_t hw_pal = nullptr;   // 256 x RGB565, read live by the CLUT PIO/DMA
static int scale_mode = HELD_SCALE_CROP;
static uint8_t lut_x[LCD_W];               // LCD column -> PICO-8 column
static uint8_t lut_y[LCD_H];               // LCD row    -> PICO-8 row

// Each framebuffer gets its own 16-entry palette bank (FB0: 0..15, FB1: 16..31).
// Only the bank of the (invisible) back buffer is rewritten, so screen palette
// changes (pal(c0,c1,1)) never show up half-way through a scanout.
static color8_t *bank_fb[2] = { nullptr, nullptr };

static void build_luts(void) {
    for (int i = 0; i < LCD_W; i++) {
        int s;
        if (scale_mode == HELD_SCALE_FIT)
            s = i * P8_W / LCD_W;          // 0..127, every 16th source pixel shown once
        else
            s = (i + (2 * P8_W - LCD_W) / 2) / 2;   // 2x, centred: source 4..123
        if (s > P8_W - 1) s = P8_W - 1;
        lut_x[i] = (uint8_t)s;
        lut_y[i] = (uint8_t)s;  // square screen: same mapping for rows
    }
}

void held_lcd_set_scale_mode(int mode) {
    scale_mode = (mode == HELD_SCALE_FIT) ? HELD_SCALE_FIT : HELD_SCALE_CROP;
    build_luts();
}

int held_lcd_get_scale_mode(void) {
    return scale_mode;
}

void held_backlight_set(int level) {
    lcd_backlight_set(level);
}

int held_lcd_init(void) {
    build_luts();

    lcd = new LCD_DPI();
    lcd_error_t r = lcd->init(HELD_LCD_NUM_FB, LCD_FRR_REFRESH, 3 /* 720/3 = 240 */);
    if (r != LCD_SUCCESS) {
        printf("LCD init failed: %d\n", (int)r);
        return (int)r;
    }

    if (lcd->get_screen_width() != LCD_W || lcd->get_screen_height() != LCD_H) {
        printf("LCD: unexpected size %dx%d\n", lcd->get_screen_width(), lcd->get_screen_height());
        return LCD_BAD_PARAM;
    }

    lcd->get_palette_ptr(&hw_pal);
    if (((uintptr_t)hw_pal & 0x1FF) != 0) {
        // the CLUT PIO program needs a 512 byte aligned palette
        printf("LCD: palette not 512 byte aligned (%p)\n", (void *)hw_pal);
        return LCD_MEM_ERR;
    }
    for (int i = 0; i < 256; i++)
        hw_pal[i] = 0;   // framebuffers are zeroed -> black screen

    lcd_backlight_init();
    lcd_backlight_set(100);

    printf("LCD: %dx%d, %d FB, sys %lu Hz\n", LCD_W, LCD_H, HELD_LCD_NUM_FB,
           (unsigned long)clock_get_hz(clk_sys));
    return 0;
}

static inline int bank_for(color8_t *fb) {
    for (int i = 0; i < 2; i++) {
        if (bank_fb[i] == fb) return i * 16;
        if (bank_fb[i] == nullptr) { bank_fb[i] = fb; return i * 16; }
    }
    return 0;  // should not happen with <= 2 FBs
}

void held_lcd_present(const uint8_t *fb4, const uint8_t *disp_pal, const uint16_t *pal565) {
    if (!lcd) return;

    gbuffer_t fb;
#if HELD_LCD_NUM_FB > 1
    if (lcd->get_backbuffer(&fb) != LCD_SUCCESS) return;   // waits for a free buffer
#else
    if (lcd->get_fb(&fb) != LCD_SUCCESS) return;
#endif
    const int bank = bank_for(fb.data);
    const int stride = fb.width;

    // palette for this buffer's bank
    for (int i = 0; i < 16; i++) {
        int c = disp_pal ? (disp_pal[i] & 0x0F) : i;
        hw_pal[bank + i] = pal565[c];
    }

    // scale 128x128 4bpp -> 240x240 8bpp
    uint8_t line[P8_W];
    int prev_sy = -1;
    color8_t *prev_row = nullptr;
    for (int y = 0; y < LCD_H; y++) {
        color8_t *row = fb.data + y * stride;
        int sy = lut_y[y];
        if (sy == prev_sy) {
            memcpy(row, prev_row, LCD_W);
        } else {
            const uint8_t *src = fb4 + sy * (P8_W / 2);
            for (int x = 0; x < P8_W / 2; x++) {
                uint8_t pair = src[x];
                line[2 * x]     = (uint8_t)(bank + (pair & 0x0F));
                line[2 * x + 1] = (uint8_t)(bank + (pair >> 4));
            }
            for (int x = 0; x < LCD_W; x++)
                row[x] = line[lut_x[x]];
            prev_sy = sy;
            prev_row = row;
        }
    }

#if HELD_LCD_NUM_FB > 1
    lcd->enqueue_backbuffer();
#endif
}
