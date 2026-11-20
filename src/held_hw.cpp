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

// ---- analog stick ---------------------------------------------------------
// Centre: calibrated from many samples once the supplies have settled
// (held_stick_recalibrate(), called after the LCD is up; the stick must not
// be touched while booting). While running, the centre follows the rest
// position whenever the stick is held still near the centre (mechanical
// hysteresis, drift of ageing sticks).
// Range: the extremes start at centre +/- HELD_STICK_INIT_RANGE and grow
// while running. A new extreme is only taken over if seen in two consecutive
// samples (a single ADC glitch can't widen the range).
static int adc_read_settled(int gpio);

typedef struct {
    int centre, min, max;
    int cand_min, cand_max;   // candidates waiting for confirmation
    int last_v;               // previous sample
    int still_cnt;            // consecutive samples without movement
} stick_axis_t;

static stick_axis_t stick_ax[2];   // 0 = ADC "X" channel, 1 = ADC "Y" channel
static int stick_pct[2];           // last deflection (screen orientation), -100..100

static void stick_axis_init(stick_axis_t *a, int centre) {
    a->centre = centre;
    a->min = centre - HELD_STICK_INIT_RANGE;
    a->max = centre + HELD_STICK_INIT_RANGE;
    if (a->min < 0) a->min = 0;
    if (a->max > 4095) a->max = 4095;
    a->cand_min = a->min;
    a->cand_max = a->max;
    a->last_v = centre;
    a->still_cnt = 0;
}

static int stick_axis_pct(const stick_axis_t *a, int v) {
    int d = v - a->centre;
    int pct;
    if (d >= 0) {
        int span = a->max - a->centre;
        pct = span > 0 ? d * 100 / span : 0;
    } else {
        int span = a->centre - a->min;
        pct = span > 0 ? d * 100 / span : 0;
    }
    if (pct > 100) pct = 100;
    if (pct < -100) pct = -100;
    return pct;
}

// raw ADC value -> -100..100 (relative to centre, scaled per side), dead zone applied
static int stick_axis_update(stick_axis_t *a, int v) {
    // --- range tracking ---
    if (v < a->min) {
        if (a->cand_min < a->min && v <= a->cand_min + HELD_STICK_GLITCH_TOL)
            a->min = (v > a->cand_min) ? v : a->cand_min;   // confirmed
        a->cand_min = v;
    } else {
        a->cand_min = a->min;
    }
    if (v > a->max) {
        if (a->cand_max > a->max && v >= a->cand_max - HELD_STICK_GLITCH_TOL)
            a->max = (v < a->cand_max) ? v : a->cand_max;
        a->cand_max = v;
    } else {
        a->cand_max = a->max;
    }

    // --- centre tracking: stick resting near the centre -> follow it ---
    int dv = v - a->last_v;
    if (dv < 0) dv = -dv;
    a->still_cnt = (dv <= HELD_STICK_STILL_TOL) ? a->still_cnt + 1 : 0;
    a->last_v = v;

    int pct = stick_axis_pct(a, v);
    if (a->still_cnt >= HELD_STICK_RECENTRE_SAMPLES &&
        pct > -HELD_STICK_RECENTRE_PCT && pct < HELD_STICK_RECENTRE_PCT) {
        int diff = v - a->centre;
        // move 1/4 of the way per sample (at least 1 count)
        int step = diff / 4;
        if (step == 0 && diff != 0) step = diff > 0 ? 1 : -1;
        int c = a->centre + step;
        // keep the centre well inside the tracked range
        if (c < a->min + HELD_STICK_GLITCH_TOL) c = a->min + HELD_STICK_GLITCH_TOL;
        if (c > a->max - HELD_STICK_GLITCH_TOL) c = a->max - HELD_STICK_GLITCH_TOL;
        a->centre = c;
        pct = stick_axis_pct(a, v);
    }

    if (pct > -HELD_STICK_DEADZONE_PCT && pct < HELD_STICK_DEADZONE_PCT) pct = 0;
    return pct;
}

void held_stick_get(int *x, int *y) {
    if (x) *x = stick_pct[0];
    if (y) *y = stick_pct[1];
}

// Centre from HELD_STICK_CAL_SAMPLES samples per axis. If the samples spread
// more than HELD_STICK_CAL_SPREAD (stick touched, supply not settled) the
// measurement is repeated, up to 5 times.
void held_stick_recalibrate(void) {
    int cx = 2048, cy = 2048;
    for (int attempt = 0; attempt < 5; attempt++) {
        int sum_x = 0, sum_y = 0;
        int min_x = 4095, max_x = 0, min_y = 4095, max_y = 0;
        for (int i = 0; i < HELD_STICK_CAL_SAMPLES; i++) {
            int x = adc_read_settled(PIN_ANALOG_X);
            int y = adc_read_settled(PIN_ANALOG_Y);
            sum_x += x; sum_y += y;
            if (x < min_x) min_x = x;
            if (x > max_x) max_x = x;
            if (y < min_y) min_y = y;
            if (y > max_y) max_y = y;
            sleep_us(500);
        }
        cx = sum_x / HELD_STICK_CAL_SAMPLES;
        cy = sum_y / HELD_STICK_CAL_SAMPLES;
        bool stable = (max_x - min_x) <= HELD_STICK_CAL_SPREAD &&
                      (max_y - min_y) <= HELD_STICK_CAL_SPREAD;
        printf("stick cal #%d: x %d (spread %d)  y %d (spread %d)%s\n", attempt, cx,
               max_x - min_x, cy, max_y - min_y, stable ? "" : "  -> retry");
        if (stable) break;
        sleep_ms(20);
    }
    stick_axis_init(&stick_ax[0], cx);
    stick_axis_init(&stick_ax[1], cy);
    stick_pct[0] = stick_pct[1] = 0;
}

void held_stick_get_calibration(int out[6]) {
    out[0] = stick_ax[0].min; out[1] = stick_ax[0].centre; out[2] = stick_ax[0].max;
    out[3] = stick_ax[1].min; out[4] = stick_ax[1].centre; out[5] = stick_ax[1].max;
}

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

    // preliminary stick centre; the real calibration happens after the LCD
    // is up (supplies settled), see held_lcd_init()
    stick_axis_init(&stick_ax[0], adc_read_settled(PIN_ANALOG_X));
    stick_axis_init(&stick_ax[1], adc_read_settled(PIN_ANALOG_Y));
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
    int ax = stick_axis_update(&stick_ax[0], sx);
    int ay = stick_axis_update(&stick_ax[1], sy);
    // stick is mounted rotated: map the ADC channels to screen directions
#if HELD_STICK_SWAP_XY
    int px = ay, py = ax;
#else
    int px = ax, py = ay;
#endif
#if HELD_STICK_INVERT_X
    px = -px;
#endif
#if HELD_STICK_INVERT_Y
    py = -py;
#endif
    stick_pct[0] = px;   // + = right
    stick_pct[1] = py;   // + = down
#if HELD_USE_ANALOG_STICK
    if (px <= -HELD_STICK_THRESHOLD_PCT) b |= HELD_BTN_LEFT;
    if (px >=  HELD_STICK_THRESHOLD_PCT) b |= HELD_BTN_RIGHT;
    if (py <= -HELD_STICK_THRESHOLD_PCT) b |= HELD_BTN_UP;
    if (py >=  HELD_STICK_THRESHOLD_PCT) b |= HELD_BTN_DOWN;
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

    // Calibrate the analog stick centre now: a few hundred ms after power-on
    // (panel reset), the supplies have settled.
    held_stick_recalibrate();

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
