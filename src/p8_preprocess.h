#ifndef P8_PREPROCESS_H
#define P8_PREPROCESS_H

#include <stddef.h>
#include <stdbool.h>

/*
 * PICO-8 syntax preprocessor.
 *
 * Transforms PICO-8 Lua dialect into standard Lua 5.4:
 *   != -> ~=
 *   // comments -> -- comments
 *   += -= *= /= %= ..= -> expanded assignments
 *   ?expr -> print(expr)
 *   if (cond) stmt (no then) -> if cond then stmt end
 *   while (cond) stmt (no do) -> while cond do stmt end
 *   \ line continuation
 *
 * Returns a newly-allocated string (via tlsf_malloc on PSRAM).
 * Caller must free with tlsf_free(psram_tlsf, result).
 * Returns NULL on allocation failure.
 */

/* Must be called once to give the preprocessor access to the PSRAM allocator */
void p8_preprocess_init(void *tlsf_handle);

/* Preprocess a PICO-8 source string. Returns new allocated string or NULL. */
char *p8_preprocess(const char *src, size_t src_len, size_t *out_len);

// Source encoding of the following p8_preprocess() calls: false = UTF-8
// (.p8 text carts, REPL; default), true = raw P8SCII bytes (code from .p8.png
// carts). In P8SCII mode glyph bytes outside strings become the same _PG_XXXX
// identifiers as their UTF-8 forms; string contents are left untouched.
void p8_preprocess_set_p8scii(bool enable);

#endif /* P8_PREPROCESS_H */
