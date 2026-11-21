/*
 * Move hot Lua runtime structures into internal SRAM (see main.cpp allocator).
 *
 * PICO-8 code accesses nearly everything through globals, so the global
 * table's hash part is read for almost every function call / variable access.
 * It is a few KB - larger than the small-object limit of the SRAM pool - and
 * would otherwise live in PSRAM behind the XIP cache. After _init() (when the
 * cart has defined its globals) the global table, the string table and the
 * Lua stack are re-allocated once at their current size with the allocator
 * told to prefer SRAM.
 */
#include <stdio.h>
#include "lua.h"
#include "lstate.h"
#include "lobject.h"
#include "ltable.h"
#include "lstring.h"
#include "ldo.h"
#include "lua_pin.h"
#include "held_config.h"

void lua_alloc_prefer_sram(int on);   /* main.cpp */

void lua_pin_hot_to_sram(lua_State *L) {
    lua_alloc_prefer_sram(1);

    /* global table: rebuild array + hash part at the same size */
    lua_pushglobaltable(L);
    Table *g = hvalue(s2v(L->top.p - 1));
    unsigned int na = luaH_realasize(g);
    unsigned int nh = isdummy(g) ? 0 : (unsigned int)sizenode(g);
    luaH_resize(L, g, na, nh);
    lua_pop(L, 1);

    /* interned string table (looked up for every string created) */
    luaS_resize(L, G(L)->strt.size);

    /* Lua stack: pre-size once (the GC no longer shrinks it, see
       LUAI_NO_GC_SHRINKSTACK), so it doesn't oscillate grow/shrink and stays
       in SRAM */
    int ss = stacksize(L);
    if (ss < HELD_LUA_STACK_SLOTS) ss = HELD_LUA_STACK_SLOTS;
    luaD_reallocstack(L, ss, 0);

    lua_alloc_prefer_sram(0);
    printf("lua: globals (%u array, %u hash slots), string table (%d) and stack (%d) "
           "re-allocated, SRAM preferred\n", na, nh, G(L)->strt.size, stacksize(L));
}
