/* Sampling profiler for core 0, see held_prof.h */
#include "held_prof.h"
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/irq.h"
#include "hardware/sync.h"

#define PROF_SLOTS 1024            // open addressing, power of two
#define PROF_SHIFT 5               // 32-byte buckets

static uint32_t prof_key[PROF_SLOTS];     // bucket address | 1 (0 = empty)
static uint32_t prof_cnt[PROF_SLOTS];
static volatile uint32_t prof_total, prof_lost, prof_ram, prof_flash, prof_other;
static volatile uint32_t prof_period_us;
static volatile bool prof_on = false;
static int prof_alarm = -1;

void __not_in_flash_func(held_prof_sample)(uint32_t *frame) {
    // exception frame: r0 r1 r2 r3 r12 lr pc xpsr
    uint32_t pc = frame[6];
    timer_hw->intr = 1u << prof_alarm;
    timer_hw->alarm[prof_alarm] = timer_hw->timerawl + prof_period_us;

    prof_total++;
    if (pc >= 0x20000000u && pc < 0x20082000u) prof_ram++;
    else if (pc >= 0x10000000u && pc < 0x11000000u) prof_flash++;
    else prof_other++;

    uint32_t key = (pc >> PROF_SHIFT << PROF_SHIFT) | 1u;
    uint32_t h = (key * 2654435761u) >> (32 - 10);
    for (int n = 0; n < 16; n++) {
        uint32_t i = (h + n) & (PROF_SLOTS - 1);
        if (prof_key[i] == key) { prof_cnt[i]++; return; }
        if (prof_key[i] == 0) { prof_key[i] = key; prof_cnt[i] = 1; return; }
    }
    prof_lost++;
}

// Naked entry: the hardware stacked the interrupted context on MSP (no RTOS,
// everything runs on MSP); pass that frame to the C handler with LR untouched
// so the C function's return performs the exception return.
__attribute__((naked, section(".time_critical.held_prof_isr")))
static void held_prof_isr(void) {
    __asm volatile(
        "mrs r0, msp\n"
        "b   held_prof_sample\n");
}

static void prof_clear(void) {
    memset(prof_key, 0, sizeof(prof_key));
    memset(prof_cnt, 0, sizeof(prof_cnt));
    prof_total = prof_lost = prof_ram = prof_flash = prof_other = 0;
}

void held_prof_start(uint32_t hz) {
    if (prof_on) return;
    if (prof_alarm < 0) {
        prof_alarm = hardware_alarm_claim_unused(true);
        uint irq = timer_hardware_alarm_get_irq_num(timer_hw, prof_alarm);
        irq_set_exclusive_handler(irq, held_prof_isr);
        irq_set_priority(irq, 0x60);   // above normal IRQs, below the LCD (0x40)
    }
    prof_clear();
    prof_period_us = hz ? 1000000u / hz : 200;
    if (prof_period_us < 50) prof_period_us = 50;
    uint irq = timer_hardware_alarm_get_irq_num(timer_hw, prof_alarm);
    timer_hw->intr = 1u << prof_alarm;
    hw_set_bits(&timer_hw->inte, 1u << prof_alarm);
    irq_set_enabled(irq, true);
    prof_on = true;
    timer_hw->alarm[prof_alarm] = timer_hw->timerawl + prof_period_us;
}

void held_prof_stop(void) {
    if (!prof_on) return;
    prof_on = false;
    hw_clear_bits(&timer_hw->inte, 1u << prof_alarm);
    irq_set_enabled(timer_hardware_alarm_get_irq_num(timer_hw, prof_alarm), false);
    timer_hw->armed = 1u << prof_alarm;   // disarm
}

bool held_prof_active(void) {
    return prof_on;
}

void held_prof_report(int top_n) {
    if (top_n > 64) top_n = 64;
    // snapshot
    uint32_t irq_state = save_and_disable_interrupts();
    static uint32_t k[PROF_SLOTS], c[PROF_SLOTS];
    memcpy(k, prof_key, sizeof(k));
    memcpy(c, prof_cnt, sizeof(c));
    uint32_t total = prof_total, lost = prof_lost;
    uint32_t ram = prof_ram, flash = prof_flash, other = prof_other;
    prof_clear();
    restore_interrupts(irq_state);

    if (!total) { printf("prof: no samples\n"); return; }
    printf("prof: %lu samples, code in RAM %lu%%, flash %lu%%, other %lu%% (lost %lu)\n",
           (unsigned long)total, (unsigned long)(ram * 100 / total),
           (unsigned long)(flash * 100 / total), (unsigned long)(other * 100 / total),
           (unsigned long)lost);
    for (int r = 0; r < top_n; r++) {
        int best = -1;
        for (int i = 0; i < PROF_SLOTS; i++)
            if (k[i] && (best < 0 || c[i] > c[best])) best = i;
        if (best < 0 || c[best] == 0) break;
        printf("prof %2d: 0x%08lx %5lu %3lu.%lu%%\n", r, (unsigned long)(k[best] & ~1u),
               (unsigned long)c[best], (unsigned long)(c[best] * 100 / total),
               (unsigned long)(c[best] * 1000 / total % 10));
        k[best] = 0;
    }
}
