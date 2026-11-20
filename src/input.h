#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "lua.h"

// Call once per frame: samples the Pico Held 2 buttons, updates btnp edge
// detection / auto-repeat and handles the system combos (see input.c).
void input_update(void);

// PICO-8 btn(i, player): true if button i is currently held
// i: 0=left, 1=right, 2=up, 3=down, 4=O, 5=X, 6=menu (START)
bool input_btn(int i, int player);

// PICO-8 btnp(i, player): true on initial press, then auto-repeat (15 frame delay, 4 frame repeat)
bool input_btnp(int i, int player);

// SELECT+START was pressed since the last call (exit cart / leave menu)
bool input_exit_requested(void);

// Physical buttons sampled by the last input_update() (HELD_BTN_* bits)
uint32_t input_raw_buttons(void);

// Make currently held buttons inert until released; clears a pending exit request
void input_flush(void);

// Compatibility stubs (no keyboard / mouse on the Pico Held 2)
bool input_key(uint8_t keycode);
int input_getchar(void);
int input_mouse_x(void);
int input_mouse_y(void);
uint8_t input_mouse_buttons(void);
int input_mouse_wheel(void);
void input_mouse_reset(void);

// Lua library opener
int luaopen_input(lua_State *L);

#ifdef __cplusplus
}
#endif

#endif // INPUT_H
