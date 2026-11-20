/*
 * Sampling profiler for core 0: a hardware timer alarm interrupts core 0 at
 * a fixed rate and records the interrupted program counter (32-byte buckets).
 * held_prof_report() prints the hottest addresses; resolve them with
 *   arm-none-eabi-addr2line -f -e build/wili8jam.elf <addr>
 * or arm-none-eabi-nm -n build/wili8jam.elf.
 */
#ifndef HELD_PROF_H
#define HELD_PROF_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void held_prof_start(uint32_t hz);   // e.g. 5000
void held_prof_stop(void);
bool held_prof_active(void);
// Print the top_n buckets (and RAM / flash / other split) and reset.
void held_prof_report(int top_n);

#ifdef __cplusplus
}
#endif

#endif
