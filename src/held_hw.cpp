/*
 * Pico Held 2 hardware glue for wili8jam (see held_hw.h).
 */
#include "held_hw.h"

#include <cstdio>
#include <cstring>

#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include "hardware/clocks.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "hardware/vreg.h"

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

#include "held_config.h"

#define LCD_W 240
#define LCD_H 240
#define P8_W  128
#define P8_H  128

// ---------------------------------------------------------------------------
// Power / buttons
// ---------------------------------------------------------------------------

static int stick_x0 = 2048, stick_y0 = 2048;

// D-pad thresholds (runtime adjustable, defaults in held_config.h)
static int dpad_left_max  = HELD_DPAD_LEFT_MAX;
static int dpad_right_min = HELD_DPAD_RIGHT_MIN;
static int dpad_up_max    = HELD_DPAD_UP_MAX;
static int dpad_down_min  = HELD_DPAD_DOWN_MIN;

static int sel_consec_cnt = 0;

void held_dpad_set_thresholds(int left_max, int right_min, int up_max, int down_min) {
    dpad_left_max = left_max;
    dpad_right_min = right_min;
    dpad_up_max = up_max;
    dpad_down_min = down_min;
}

void held_dpad_get_thresholds(int out[4]) {
    out[0] = dpad_left_max;
    out[1] = dpad_right_min;
    out[2] = dpad_up_max;
    out[3] = dpad_down_min;
}
static int raw_adc[HELD_RAW_COUNT];

// The RP2350 ADC has one sample capacitor for all channels: right after
// switching channels it still holds (part of) the previous channel's voltage,
// which matters for the high-impedance D-pad dividers. Throw away two
// conversions after the switch, then average two.
static int adc_read_settled(int gpio) {
    adc_select_input(gpio - ADC_PIN_OFFSET);
    (void)adc_read();
    (void)adc_read();
    return (adc_read() + adc_read()) / 2;
}

// START is active high with a pull-down -> affected by RP2350-E9 (the pad can
// latch at ~2 V after being driven high). Keep the input buffer off between
// reads, enable it only for a short sample, give the pin time to settle.
static bool read_start_button(void) {
    gpio_set_input_enabled(PIN_BTN_ST, true);
    busy_wait_us(5);
    bool v = gpio_get(PIN_BTN_ST);
    gpio_set_input_enabled(PIN_BTN_ST, false);
    return v;
}

void held_hw_early_init(void) {
    ctrl_init();   // GPIOs (pulls) + ADC for buttons, also needed by pwr_init
    pwr_init();    // latch PWR_EN so the device stays on

    // START: plain input with pull-down, input buffer only on while sampling
    gpio_init(PIN_BTN_ST);
    gpio_set_dir(PIN_BTN_ST, GPIO_IN);
    gpio_set_pulls(PIN_BTN_ST, false, true);
    gpio_set_input_enabled(PIN_BTN_ST, false);

    // stick switch: digital, active low (hwcfg.h lists it as analog)
    gpio_init(PIN_BTN_SW);
    gpio_set_dir(PIN_BTN_SW, GPIO_IN);
    gpio_set_pulls(PIN_BTN_SW, true, false);

    // analog stick centre
    stick_x0 = adc_read_settled(PIN_ANALOG_X);
    stick_y0 = adc_read_settled(PIN_ANALOG_Y);
}

void held_set_sys_clock(void) {
    vreg_set_voltage(VREG_VOLTAGE_1_30);
    sleep_ms(10);  // let the voltage settle
#if HELD_SYS_CLOCK_KHZ == 252000
    set_sys_clock_pll(1260000000, 5, 1);  // VCO 1260 MHz / 5 / 1 = 252 MHz exactly
#else
    set_sys_clock_khz(HELD_SYS_CLOCK_KHZ, true);
#endif
}

void held_buttons_raw(int out[HELD_RAW_COUNT]) {
    for (int i = 0; i < HELD_RAW_COUNT; i++) out[i] = raw_adc[i];
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
    uint32_t b = 0;

    // digital, active low with pull-ups
    if (!gpio_get(PIN_BTN_A))   b |= HELD_BTN_A;
    if (!gpio_get(PIN_BTN_B))   b |= HELD_BTN_B;
    if (!gpio_get(PIN_BTN_X))   b |= HELD_BTN_X;
    if (!gpio_get(PIN_BTN_Y))   b |= HELD_BTN_Y;
    if (!gpio_get(PIN_BTN_R))   b |= HELD_BTN_R;
    if (!gpio_get(PIN_BTN_SW))  b |= HELD_BTN_STICK;
    if (read_start_button())    b |= HELD_BTN_START;

    // SELECT glitches: only accept it after HELD_SEL_CONSECUTIVE pressed samples
    if (!gpio_get(PIN_BTN_SEL)) {
        if (sel_consec_cnt < HELD_SEL_CONSECUTIVE) sel_consec_cnt++;
    } else {
        sel_consec_cnt = 0;
    }
    if (sel_consec_cnt >= HELD_SEL_CONSECUTIVE) b |= HELD_BTN_SELECT;

    // analog: D-pad axes (voltage dividers) and L
    int lr = adc_read_settled(PIN_DPAD_LR);
    int ud = adc_read_settled(PIN_DPAD_UD);
    int l  = adc_read_settled(PIN_BTN_L);
    raw_adc[HELD_RAW_DPAD_LR] = lr;
    raw_adc[HELD_RAW_DPAD_UD] = ud;
    raw_adc[HELD_RAW_L] = l;

    if (lr < dpad_left_max)  b |= HELD_BTN_LEFT;
    if (lr > dpad_right_min) b |= HELD_BTN_RIGHT;
    if (ud < dpad_up_max)    b |= HELD_BTN_UP;
    if (ud > dpad_down_min)  b |= HELD_BTN_DOWN;
    if (l  < HELD_BTN_L_MAX) b |= HELD_BTN_L;

    int sx = adc_read_settled(PIN_ANALOG_X);
    int sy = adc_read_settled(PIN_ANALOG_Y);
    raw_adc[HELD_RAW_STICK_X] = sx;
    raw_adc[HELD_RAW_STICK_Y] = sy;
#if HELD_USE_ANALOG_STICK
    int dx = sx - stick_x0;
    int dy = sy - stick_y0;
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

static int backlight_level = HELD_BACKLIGHT_DEFAULT;

void held_backlight_set(int level) {
    if (level < 5) level = 5;       // never fully dark while running
    if (level > 100) level = 100;
    backlight_level = level;
    if (lcd) lcd_backlight_set(level);
}

int held_backlight_get(void) {
    return backlight_level;
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
    lcd_backlight_set(backlight_level);

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

static bool frame_drop = false;
static uint32_t frames_dropped = 0;

uint32_t held_lcd_frames_dropped(void) {
    return frames_dropped;
}

void held_lcd_set_frame_drop(bool enable) {
    frame_drop = enable;
}

void held_lcd_present(const uint8_t *fb4, const uint8_t *disp_pal, const uint16_t *pal565) {
    if (!lcd) return;

#if HELD_LCD_NUM_FB > 1
    // Game loop: never wait for the panel. If the previous frame hasn't been
    // picked up by a vblank yet, skip this one (keeps carts at their speed
    // when the panel refreshes slower than _update60).
    if (frame_drop && !lcd->is_backbuffer_free()) {
        frames_dropped++;
        return;
    }
#endif

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
