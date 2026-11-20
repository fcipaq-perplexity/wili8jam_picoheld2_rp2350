/*
 * Pico Held 2 tuning knobs for wili8jam.
 *
 * Everything here can be overridden from CMake, e.g.
 *   target_compile_definitions(wili8jam PRIVATE HELD_DPAD_RIGHT_MIN=2300)
 * The D-pad thresholds can also be changed at runtime from the REPL:
 *   input.dpad()                      -- print current values + raw ADC
 *   input.dpad(1150, 2250, 1600, 2750) -- left max, right min, up max, down min
 */
#ifndef HELD_CONFIG_H
#define HELD_CONFIG_H

/* ---------------------------------------------------------------------------
 * D-pad (one ADC channel per axis, voltage divider, 12 bit: 0..4095)
 *
 * Measured (Pico Held 2 v1.5):
 *   LR axis: left ~300,  idle ~2000, right ~2500
 *   UD axis: up   ~1200, idle ~2000, down  ~3500
 * Defaults are the midpoints between idle and pressed. Moving a threshold
 * towards idle makes the direction more sensitive (and diagonals register
 * more easily); moving it towards the pressed value makes it stricter.
 * ------------------------------------------------------------------------- */
#ifndef HELD_DPAD_LEFT_MAX
#define HELD_DPAD_LEFT_MAX   1150   // LR below this: LEFT
#endif
#ifndef HELD_DPAD_RIGHT_MIN
#define HELD_DPAD_RIGHT_MIN  2250   // LR above this: RIGHT
#endif
#ifndef HELD_DPAD_UP_MAX
#define HELD_DPAD_UP_MAX     1600   // UD below this: UP
#endif
#ifndef HELD_DPAD_DOWN_MIN
#define HELD_DPAD_DOWN_MIN   2750   // UD above this: DOWN
#endif

// L shoulder button (analog pin with pull-up): pressed below this
#ifndef HELD_BTN_L_MAX
#define HELD_BTN_L_MAX        500
#endif

/* ---------------------------------------------------------------------------
 * SELECT glitch filter: SELECT (GPIO 23) picks up glitches from the ADC
 * activity nearby. It only counts as pressed after this many consecutive
 * pressed samples (one sample per input poll, i.e. per frame: 8 = ~130 ms at
 * 60 fps, ~270 ms at 30 fps). SELECT is only used for slow system combos.
 * 1 = no filtering.
 * ------------------------------------------------------------------------- */
#ifndef HELD_SEL_CONSECUTIVE
#define HELD_SEL_CONSECUTIVE    8
#endif

/* ---------------------------------------------------------------------------
 * Analog stick
 * Centre is calibrated at boot (don't touch the stick while switching on);
 * min/max are tracked while running. Deflection is scaled per side to -100..100 %.
 * ------------------------------------------------------------------------- */
// Stick also drives the PICO-8 direction buttons
#ifndef HELD_USE_ANALOG_STICK
#define HELD_USE_ANALOG_STICK     1
#endif
// Deflection (%) at which a direction counts as pressed
#ifndef HELD_STICK_THRESHOLD_PCT
#define HELD_STICK_THRESHOLD_PCT 50
#endif
// Dead zone around the centre (%), reported as 0
#ifndef HELD_STICK_DEADZONE_PCT
#define HELD_STICK_DEADZONE_PCT  10
#endif
// Assumed range (ADC counts, each side of the centre) before the stick has
// been pushed to its limits. Smaller = more sensitive right after boot.
#ifndef HELD_STICK_INIT_RANGE
#define HELD_STICK_INIT_RANGE   600
#endif
// Two consecutive samples beyond the current extreme must agree within this
// many counts to widen the range (glitch rejection)
#ifndef HELD_STICK_GLITCH_TOL
#define HELD_STICK_GLITCH_TOL   100
#endif
// Boot calibration: samples per axis and max. allowed spread (ADC counts);
// a larger spread (stick touched / supply not settled) repeats the measurement
#ifndef HELD_STICK_CAL_SAMPLES
#define HELD_STICK_CAL_SAMPLES   64
#endif
#ifndef HELD_STICK_CAL_SPREAD
#define HELD_STICK_CAL_SPREAD    80
#endif
// Centre tracking (off by default): if the stick moves less than STILL_TOL
// counts per sample for RECENTRE_SAMPLES samples (one sample per frame) while
// within RECENTRE_PCT of the centre, the centre is pulled towards the rest
// position. Can be provoked by holding the stick slightly off-centre.
#ifndef HELD_STICK_RECENTRE
#define HELD_STICK_RECENTRE       0
#endif
#ifndef HELD_STICK_STILL_TOL
#define HELD_STICK_STILL_TOL     40
#endif
#ifndef HELD_STICK_RECENTRE_SAMPLES
#define HELD_STICK_RECENTRE_SAMPLES 20
#endif
#ifndef HELD_STICK_RECENTRE_PCT
#define HELD_STICK_RECENTRE_PCT  45
#endif
// Orientation. The stick is mounted rotated: the ADC "X" channel is the
// vertical axis and "Y" the horizontal one -> swap. Inversion is applied
// after the swap; + must be right (X) / down (Y).
#ifndef HELD_STICK_SWAP_XY
#define HELD_STICK_SWAP_XY        1
#endif
#ifndef HELD_STICK_INVERT_X
#define HELD_STICK_INVERT_X       0
#endif
#ifndef HELD_STICK_INVERT_Y
#define HELD_STICK_INVERT_Y       0
#endif

/* ---------------------------------------------------------------------------
 * Display / system
 * ------------------------------------------------------------------------- */
// LCD framebuffers (240x240x8bit = 57.6 KB each): 2 = double buffered, 1 = single
#ifndef HELD_LCD_NUM_FB
#define HELD_LCD_NUM_FB         2
#endif

// Give the CPU cores priority over DMA on the bus fabric (0/1). The LCD
// scanout DMA is busy all the time; with 1 the CPU wins on conflicts (the
// panel's pixel clock may then stall a bit more). Runtime: sys.busprio(true)
#ifndef HELD_CPU_BUS_PRIORITY
#define HELD_CPU_BUS_PRIORITY   0
#endif

// Backlight at boot, 5..100
#ifndef HELD_BACKLIGHT_DEFAULT
#define HELD_BACKLIGHT_DEFAULT 50
#endif

// System clock in kHz (normally set via the CMake cache variable)
#ifndef HELD_SYS_CLOCK_KHZ
#define HELD_SYS_CLOCK_KHZ 252000
#endif

#endif // HELD_CONFIG_H
