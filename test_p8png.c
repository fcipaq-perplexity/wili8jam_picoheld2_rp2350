/*
 * Host-side .p8.png check: PNG decode -> PXA/:c: code decompression ->
 * PICO-8 preprocessor -> Lua compile, using the same sources as the firmware
 * (src/p8_png.c, src/p8_preprocess.c). Writes the decompressed and the
 * preprocessed code next to the cart (<cart>.raw.lua, <cart>.pp.lua).
 *
 * Usage: test_p8png <cart.p8.png>
 *
 * Build:
 *   gcc -o test_p8png test_p8png.c src/p8_png.c src/p8_preprocess.c tlsf/tlsf.c \
 *       <lua-5.4.7/src/*.c without lua.c and luac.c> \
 *       -Isrc -I. -Ilua-5.4.7/src -DLUA_32BITS=1 -lm
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "tlsf/tlsf.h"
#include "p8_png.h"
#include "p8_preprocess.h"
static uint8_t mem[0x10000];
uint8_t *p8_get_memory(void) { return mem; }
int main(int argc, char **argv) {
    if (argc < 2) { printf("usage: %s <cart.p8.png>\n", argv[0]); return 2; }
    size_t pool_sz = 16 << 20;
    void *pool = malloc(pool_sz);
    tlsf_t t = tlsf_create_with_pool(pool, pool_sz);
    p8_png_init(t);
    p8_preprocess_init(t);
    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 2; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *d = malloc(n); fread(d, 1, n, f); fclose(f);
    size_t len;
    char *code = p8_png_load(d, n, &len);
    if (!code) { printf("png load failed\n"); return 1; }
    char fn[1024]; snprintf(fn, sizeof(fn), "%s.raw.lua", argv[1]);
    FILE *o = fopen(fn, "wb"); fwrite(code, 1, len, o); fclose(o);
    size_t pl;
    p8_preprocess_set_p8scii(1);
    char *pp = p8_preprocess(code, len, &pl);
    snprintf(fn, sizeof(fn), "%s.pp.lua", argv[1]);
    o = fopen(fn, "wb"); fwrite(pp, 1, pl, o); fclose(o);
    lua_State *L = luaL_newstate();
    int st = luaL_loadbuffer(L, pp, pl, "@cart");
    printf("raw %zu bytes, preprocessed %zu bytes: %s\n", len, pl, st ? lua_tostring(L, -1) : "compiles OK");
    return st ? 1 : 0;
}
