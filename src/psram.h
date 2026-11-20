#ifndef PSRAM_H
#define PSRAM_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/sync.h"
#include "hardware/structs/qmi.h"
#include "hardware/structs/xip_ctrl.h"
#include "hardware/regs/qmi.h"
#include "hardware/regs/xip.h"

// Pico Held 2: APS6404L-3SQR (8 MB) on XIP_CS1 = GPIO 0 (see boards/picoheld_2.h)
#define PSRAM_CS_PIN PICOHELD2_PSRAM_CS_PIN
#define PSRAM_BASE 0x11000000

// Max. PSRAM SCK tried by psram_tune(). The APS6404L does 84 MHz for linear
// bursts that cross a page. 252 MHz / 3 = 84 MHz. psram_tune() finds a
// working RX delay, and lowers the clock if there is no safe window.
#ifndef PSRAM_MAX_FREQ_HZ
#define PSRAM_MAX_FREQ_HZ 84000000
#endif

static inline uint32_t psram_initial_clkdiv(void) {
    uint32_t div = (clock_get_hz(clk_sys) + 63000000 - 1) / 63000000;   // known-good 63 MHz
    return div < 2 ? 2 : div;
}

static void __no_inline_not_in_flash_func(psram_set_timing)(uint32_t clkdiv, uint32_t rxdelay) {
    qmi_hw->m[1].timing =
        QMI_M0_TIMING_PAGEBREAK_VALUE_1024 << QMI_M0_TIMING_PAGEBREAK_LSB |
        3 << QMI_M0_TIMING_SELECT_HOLD_LSB |
        1 << QMI_M0_TIMING_COOLDOWN_LSB |
        rxdelay << QMI_M0_TIMING_RXDELAY_LSB |
        29 << QMI_M0_TIMING_MAX_SELECT_LSB |
        12 << QMI_M0_TIMING_MIN_DESELECT_LSB |
        clkdiv << QMI_M0_TIMING_CLKDIV_LSB;
}

// Activate PSRAM and return detected size in bytes (0 if not found).
// Adapted from CircuitPython / rpSRAMExternalTester.cpp
static size_t __no_inline_not_in_flash_func(setup_psram)(void) {
    gpio_set_function(PSRAM_CS_PIN, GPIO_FUNC_XIP_CS1);

    size_t psram_size = 0;
    uint32_t save_irq_status = save_and_disable_interrupts();

    // Try and read the PSRAM ID via direct_csr.
    qmi_hw->direct_csr = 30 << QMI_DIRECT_CSR_CLKDIV_LSB |
        QMI_DIRECT_CSR_EN_BITS;
    while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS) != 0) {}

    // Exit out of QMI in case we've inited already
    qmi_hw->direct_csr |= QMI_DIRECT_CSR_ASSERT_CS1N_BITS;
    qmi_hw->direct_tx = QMI_DIRECT_TX_OE_BITS |
        QMI_DIRECT_TX_IWIDTH_VALUE_Q << QMI_DIRECT_TX_IWIDTH_LSB |
        0xf5;
    while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS) != 0) {}
    (void)qmi_hw->direct_rx;
    qmi_hw->direct_csr &= ~(QMI_DIRECT_CSR_ASSERT_CS1N_BITS);

    // Read the id
    qmi_hw->direct_csr |= QMI_DIRECT_CSR_ASSERT_CS1N_BITS;
    uint8_t kgd = 0;
    uint8_t eid = 0;
    for (size_t i = 0; i < 12; i++) {
        if (i == 0) {
            qmi_hw->direct_tx = 0x9f;
        } else {
            qmi_hw->direct_tx = 0xff;
        }
        while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_TXEMPTY_BITS) == 0) {}
        while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS) != 0) {}
        if (i == 5) {
            kgd = qmi_hw->direct_rx;
        } else if (i == 6) {
            eid = qmi_hw->direct_rx;
        } else {
            (void)qmi_hw->direct_rx;
        }
    }
    qmi_hw->direct_csr &= ~(QMI_DIRECT_CSR_ASSERT_CS1N_BITS | QMI_DIRECT_CSR_EN_BITS);

    if (kgd != 0x5D) {
        restore_interrupts(save_irq_status);
        return 0;
    }

    // Enable quad mode: RESETEN, RESET, quad enable, toggle wrap boundary
    qmi_hw->direct_csr = 30 << QMI_DIRECT_CSR_CLKDIV_LSB |
        QMI_DIRECT_CSR_EN_BITS;
    while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS) != 0) {}

    for (uint8_t cmd_i = 0; cmd_i < 4; cmd_i++) {
        qmi_hw->direct_csr |= QMI_DIRECT_CSR_ASSERT_CS1N_BITS;
        switch (cmd_i) {
            case 0: qmi_hw->direct_tx = 0x66; break; // RESETEN
            case 1: qmi_hw->direct_tx = 0x99; break; // RESET
            case 2: qmi_hw->direct_tx = 0x35; break; // Quad enable
            case 3: qmi_hw->direct_tx = 0xc0; break; // Toggle wrap boundary
        }
        while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS) != 0) {}
        qmi_hw->direct_csr &= ~(QMI_DIRECT_CSR_ASSERT_CS1N_BITS);
        for (size_t j = 0; j < 20; j++) { asm ("nop"); }
        (void)qmi_hw->direct_rx;
    }
    qmi_hw->direct_csr &= ~(QMI_DIRECT_CSR_ASSERT_CS1N_BITS | QMI_DIRECT_CSR_EN_BITS);

    // Configure QMI for PSRAM access. Conservative start values; the clock
    // divider / RX delay are tuned afterwards by psram_tune().
    psram_set_timing(psram_initial_clkdiv(), 2);

    qmi_hw->m[1].rfmt =
        QMI_M0_RFMT_PREFIX_WIDTH_VALUE_Q << QMI_M0_RFMT_PREFIX_WIDTH_LSB |
        QMI_M0_RFMT_ADDR_WIDTH_VALUE_Q << QMI_M0_RFMT_ADDR_WIDTH_LSB |
        QMI_M0_RFMT_SUFFIX_WIDTH_VALUE_Q << QMI_M0_RFMT_SUFFIX_WIDTH_LSB |
        QMI_M0_RFMT_DUMMY_WIDTH_VALUE_Q << QMI_M0_RFMT_DUMMY_WIDTH_LSB |
        QMI_M0_RFMT_DUMMY_LEN_VALUE_24 << QMI_M0_RFMT_DUMMY_LEN_LSB |
        QMI_M0_RFMT_DATA_WIDTH_VALUE_Q << QMI_M0_RFMT_DATA_WIDTH_LSB |
        QMI_M0_RFMT_PREFIX_LEN_VALUE_8 << QMI_M0_RFMT_PREFIX_LEN_LSB |
        QMI_M0_RFMT_SUFFIX_LEN_VALUE_NONE << QMI_M0_RFMT_SUFFIX_LEN_LSB;

    qmi_hw->m[1].rcmd =
        0xeb << QMI_M0_RCMD_PREFIX_LSB |
        0 << QMI_M0_RCMD_SUFFIX_LSB;

    qmi_hw->m[1].wfmt =
        QMI_M0_WFMT_PREFIX_WIDTH_VALUE_Q << QMI_M0_WFMT_PREFIX_WIDTH_LSB |
        QMI_M0_WFMT_ADDR_WIDTH_VALUE_Q << QMI_M0_WFMT_ADDR_WIDTH_LSB |
        QMI_M0_WFMT_SUFFIX_WIDTH_VALUE_Q << QMI_M0_WFMT_SUFFIX_WIDTH_LSB |
        QMI_M0_WFMT_DUMMY_WIDTH_VALUE_Q << QMI_M0_WFMT_DUMMY_WIDTH_LSB |
        QMI_M0_WFMT_DUMMY_LEN_VALUE_NONE << QMI_M0_WFMT_DUMMY_LEN_LSB |
        QMI_M0_WFMT_DATA_WIDTH_VALUE_Q << QMI_M0_WFMT_DATA_WIDTH_LSB |
        QMI_M0_WFMT_PREFIX_LEN_VALUE_8 << QMI_M0_WFMT_PREFIX_LEN_LSB |
        QMI_M0_WFMT_SUFFIX_LEN_VALUE_NONE << QMI_M0_WFMT_SUFFIX_LEN_LSB;

    qmi_hw->m[1].wcmd =
        0x38 << QMI_M0_WCMD_PREFIX_LSB |
        0 << QMI_M0_WCMD_SUFFIX_LSB;

    restore_interrupts(save_irq_status);

    // Determine size from EID
    psram_size = 1024 * 1024; // 1 MiB base
    uint8_t size_id = eid >> 5;
    if (eid == 0x26 || size_id == 2) {
        psram_size *= 8;
    } else if (size_id == 0) {
        psram_size *= 2;
    } else if (size_id == 1) {
        psram_size *= 4;
    }

    // Mark PSRAM as writable
    xip_ctrl_hw->ctrl |= XIP_CTRL_WRITABLE_M1_BITS;

    // Verify with a test write
    volatile uint32_t *psram_nocache = (volatile uint32_t *)0x15000000;
    psram_nocache[0] = 0x12345678;
    volatile uint32_t readback = psram_nocache[0];
    if (readback != 0x12345678) {
        return 0;
    }

    return psram_size;
}

// ---------------------------------------------------------------------------
// PSRAM timing tuning: for each clock divider (starting at PSRAM_MAX_FREQ_HZ)
// try all RX delays 0..7 with a pattern test and pick the middle of the
// widest passing window. A window of >= 3 delays is required as margin
// (temperature / voltage), otherwise the next slower divider is tried.
// Must run before anything else lives in PSRAM (it overwrites test areas).
// ---------------------------------------------------------------------------
typedef struct {
    uint32_t clkdiv;
    uint32_t rxdelay;
    uint32_t freq_hz;
    uint8_t  pass_mask[8];   // per tried divider (index = clkdiv - first divider)
    uint32_t first_div;
    uint32_t tried;
    bool     ok;
} psram_tune_result_t;

static inline uint32_t psram_xorshift(uint32_t x) {
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return x;
}

// Pattern test through the uncached window: 16 KB at both ends plus one word
// every 4 KB over the whole chip, written and read back twice (two seeds).
static bool __no_inline_not_in_flash_func(psram_pattern_test)(size_t size) {
    volatile uint32_t *p = (volatile uint32_t *)0x15000000;
    const uint32_t words = size / 4;
    const uint32_t blk = 16 * 1024 / 4;
    for (uint32_t seed = 0x1234567u; seed != 0; seed = (seed == 0x1234567u) ? 0x89abcdefu : 0) {
        uint32_t x = seed;
        for (uint32_t i = 0; i < blk; i++)            { x = psram_xorshift(x); p[i] = x; }
        for (uint32_t i = words - blk; i < words; i++) { x = psram_xorshift(x); p[i] = x; }
        for (uint32_t i = blk; i < words - blk; i += 1024) { x = psram_xorshift(x); p[i] = x; }
        x = seed;
        for (uint32_t i = 0; i < blk; i++)            { x = psram_xorshift(x); if (p[i] != x) return false; }
        for (uint32_t i = words - blk; i < words; i++) { x = psram_xorshift(x); if (p[i] != x) return false; }
        for (uint32_t i = blk; i < words - blk; i += 1024) { x = psram_xorshift(x); if (p[i] != x) return false; }
    }
    return true;
}

static psram_tune_result_t psram_tune(size_t size) {
    psram_tune_result_t r;
    memset(&r, 0, sizeof(r));
    uint32_t sys = clock_get_hz(clk_sys);
    uint32_t div = (sys + PSRAM_MAX_FREQ_HZ - 1) / PSRAM_MAX_FREQ_HZ;
    if (div < 2) div = 2;
    r.first_div = div;

    int best_len = 0, best_start = 0;
    uint32_t best_div = 0;
    for (; div <= 8 && r.tried < 8; div++, r.tried++) {
        uint8_t mask = 0;
        for (uint32_t rx = 0; rx < 8; rx++) {
            psram_set_timing(div, rx);
            if (psram_pattern_test(size)) mask |= 1u << rx;
        }
        r.pass_mask[r.tried] = mask;
        // widest run of passing RX delays
        int len = 0, start = 0;
        for (int rx = 0; rx < 8; rx++) {
            if (mask & (1u << rx)) {
                int s = rx, l = 0;
                while (rx < 8 && (mask & (1u << rx))) { l++; rx++; }
                if (l > len) { len = l; start = s; }
            }
        }
        if (len > best_len) { best_len = len; best_start = start; best_div = div; }
        if (len >= 3) break;   // enough margin
    }

    if (best_len > 0) {
        r.clkdiv = best_div;
        r.rxdelay = best_start + best_len / 2;
        r.ok = true;
    } else {
        r.clkdiv = psram_initial_clkdiv();   // nothing passed: fall back, report failure
        r.rxdelay = 2;
        r.ok = false;
    }
    r.freq_hz = sys / r.clkdiv;
    psram_set_timing(r.clkdiv, r.rxdelay);
    return r;
}

#endif // PSRAM_H
