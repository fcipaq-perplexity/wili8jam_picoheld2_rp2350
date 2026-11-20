/*
 * Input for wili8jam on the Pico Held 2: built-in buttons only.
 *
 * PICO-8 mapping (player 0):
 *   D-pad / analog stick -> btn 0..3 (left, right, up, down)
 *   A -> btn 4 (O)      B -> btn 5 (X)      START -> btn 6 (pause/menu)
 *
 * System combos (handled here, work everywhere; while SELECT is held the
 * game sees no buttons at all):
 *   SELECT + START  -> exit cart / back to the cart picker
 *   SELECT + L      -> toggle screen scaling (crop 2x <-> fit 1.875x)
 *   SELECT + UP/DN  -> volume up/down
 *   hold R (2 s)    -> power off
 */
#include "input.h"
#include <stdio.h>
#include <string.h>
#include "lua.h"
#include "lauxlib.h"
#include "pico/stdlib.h"
#include "held_hw.h"
#include "audio.h"

#define POWER_OFF_HOLD_MS 2000

static uint32_t raw_now;    // physical buttons, this frame
static uint32_t raw_prev;   // physical buttons, previous frame
static uint8_t  p8_state;   // PICO-8 buttons 0..6 (bit i = btn i), player 0

// btnp auto-repeat counters: [player][button]
// 0 = not held, 1..15 = initial delay, 16+ = repeating every 4 frames
static uint8_t btnp_hold[2][7]; // 7 buttons: 0-5 + menu(6)

static volatile bool exit_req = false;
static int volume_level = 6;          // matches audio.c default
static uint32_t r_down_since = 0;

static inline bool pressed_edge(uint32_t bit) {
    return (raw_now & bit) && !(raw_prev & bit);
}

static void handle_system_combos(void) {
    if (raw_now & HELD_BTN_SELECT) {
        if (pressed_edge(HELD_BTN_START)) exit_req = true;
        if (pressed_edge(HELD_BTN_L)) {
            int m = held_lcd_get_scale_mode() == HELD_SCALE_CROP ? HELD_SCALE_FIT : HELD_SCALE_CROP;
            held_lcd_set_scale_mode(m);
            printf("screen: %s\n", m == HELD_SCALE_FIT ? "fit" : "crop");
        }
        if (pressed_edge(HELD_BTN_UP) && volume_level < 7) {
            audio_volume(++volume_level);
            printf("volume: %d\n", volume_level);
        }
        if (pressed_edge(HELD_BTN_DOWN) && volume_level > 0) {
            audio_volume(--volume_level);
            printf("volume: %d\n", volume_level);
        }
    }

    // long press R -> power off
    uint32_t now_ms = to_ms_since_boot(get_absolute_time());
    if (raw_now & HELD_BTN_R) {
        if (!(raw_prev & HELD_BTN_R)) r_down_since = now_ms;
        else if (now_ms - r_down_since >= POWER_OFF_HOLD_MS) held_power_off();
    }
}

void input_update(void) {
    raw_prev = raw_now;
    raw_now = held_buttons_poll();

    handle_system_combos();

    uint8_t s = 0;
    if (!(raw_now & HELD_BTN_SELECT)) {
        if (raw_now & HELD_BTN_LEFT)  s |= 1 << 0;
        if (raw_now & HELD_BTN_RIGHT) s |= 1 << 1;
        if (raw_now & HELD_BTN_UP)    s |= 1 << 2;
        if (raw_now & HELD_BTN_DOWN)  s |= 1 << 3;
        if (raw_now & HELD_BTN_A)     s |= 1 << 4;
        if (raw_now & HELD_BTN_B)     s |= 1 << 5;
        if (raw_now & HELD_BTN_START) s |= 1 << 6;
    }
    p8_state = s;

    // Update btnp hold counters for each player/button
    for (int p = 0; p < 2; p++) {
        for (int i = 0; i < 7; i++) {
            if (input_btn(i, p)) {
                if (btnp_hold[p][i] < 255)
                    btnp_hold[p][i]++;
            } else {
                btnp_hold[p][i] = 0;
            }
        }
    }
}

bool input_exit_requested(void) {
    if (exit_req) {
        exit_req = false;
        return true;
    }
    return false;
}

uint32_t input_raw_buttons(void) {
    return raw_now;
}

bool input_btn(int i, int player) {
    if (i < 0 || i > 6 || player != 0) return false;
    return (p8_state >> i) & 1;
}

bool input_btnp(int i, int player) {
    if (i < 0 || i > 6 || player < 0 || player > 1) return false;

    if (!input_btn(i, player)) return false;

    uint8_t hold = btnp_hold[player][i];
    // Frame 1: just pressed (input_update already incremented from 0 to 1)
    if (hold == 1) return true;
    // Initial delay: frames 2-15 (14 frames)
    if (hold < 16) return false;
    // Auto-repeat every 4 frames after initial delay
    if ((hold - 16) % 4 == 0) return true;
    return false;
}

void input_flush(void) {
    // Swallow currently held buttons: they must be released before btnp fires again
    for (int p = 0; p < 2; p++)
        for (int i = 0; i < 7; i++)
            if (btnp_hold[p][i]) btnp_hold[p][i] = 255;
    exit_req = false;
}

// --- No keyboard / mouse on this device ---
bool input_key(uint8_t keycode) { (void)keycode; return false; }
int input_getchar(void) { return -1; }
int input_mouse_x(void) { return 64; }
int input_mouse_y(void) { return 64; }
uint8_t input_mouse_buttons(void) { return 0; }
int input_mouse_wheel(void) { return 0; }
void input_mouse_reset(void) {}

// --- Lua bindings ---

static int l_btn(lua_State *L) {
    int i = (int)luaL_checkinteger(L, 1);
    int p = (int)luaL_optinteger(L, 2, 0);
    lua_pushboolean(L, input_btn(i, p));
    return 1;
}

static int l_btnp(lua_State *L) {
    int i = (int)luaL_checkinteger(L, 1);
    int p = (int)luaL_optinteger(L, 2, 0);
    lua_pushboolean(L, input_btnp(i, p));
    return 1;
}

static int l_key(lua_State *L) {
    (void)luaL_checkinteger(L, 1);
    lua_pushboolean(L, 0);
    return 1;
}

static int l_update(lua_State *L) {
    (void)L;
    input_update();
    return 0;
}

static int l_debug(lua_State *L) {
    (void)L;
    static const char *names[] = { "left", "right", "up", "down", "a", "b", "x", "y",
                                   "l", "r", "start", "select", "stick" };
    uint32_t b = held_buttons_poll();
    printf("buttons: 0x%04lx", (unsigned long)b);
    for (int i = 0; i < 13; i++)
        if (b & (1u << i)) printf(" %s", names[i]);
    printf("\nbattery: %d%% (%s)\n", held_battery_level(),
           held_on_external_power() ? "usb" : "battery");
    return 0;
}

static const luaL_Reg inputlib[] = {
    {"btn",    l_btn},
    {"btnp",   l_btnp},
    {"key",    l_key},
    {"update", l_update},
    {"debug",  l_debug},
    {NULL, NULL}
};

int luaopen_input(lua_State *L) {
    luaL_newlib(L, inputlib);
    return 1;
}
