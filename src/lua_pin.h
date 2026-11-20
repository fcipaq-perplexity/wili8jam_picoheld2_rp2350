#ifndef LUA_PIN_H
#define LUA_PIN_H
#include "lua.h"
#ifdef __cplusplus
extern "C" {
#endif
// Re-allocate the global table, string table and Lua stack, preferring the
// internal SRAM pool (call at a safe point, e.g. after _init()).
void lua_pin_hot_to_sram(lua_State *L);
#ifdef __cplusplus
}
#endif
#endif
