/*
 * ANSWERS: 04_volatile_const_static.c
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <assert.h>

/* ============================================================
 * TASK 1 — Correct volatile qualifiers
 * ============================================================ */

volatile uint8_t  g_data_ready = 0;   /* volatile: ISR sets, main reads */
volatile uint8_t  g_rx_buf[64];       /* volatile: ISR writes, main reads */
volatile uint8_t  g_rx_head = 0;      /* volatile: written by ISR */
volatile uint8_t  g_rx_tail = 0;      /* volatile: written by main */

void simulated_uart_isr(uint8_t received_byte)
{
    g_rx_buf[g_rx_head % 64] = received_byte;
    g_rx_head++;
    g_data_ready = 1;
}

int main_loop_iteration(void)
{
    if (!g_data_ready) return -1;
    uint8_t byte = g_rx_buf[g_rx_tail % 64];
    g_rx_tail++;
    if (g_rx_tail == g_rx_head) g_data_ready = 0;
    return (int)byte;
}

/* ============================================================
 * TASK 2 — const lookup table in ROM
 * ============================================================ */

/* const → placed in .rodata (flash) — not copied to SRAM */
static const uint8_t pwm_to_dac_table[256] = {
    /* Linear mapping: index i → value i (0..255) */
    0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
    16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
    32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,
    48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,
    64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,
    80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,
    96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,
    112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,
    128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
    144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
    160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
    176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
    192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
    208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
    224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
    240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255
};

uint8_t pwm_to_dac(uint8_t pwm_percent)
{
    if (pwm_percent > 99) pwm_percent = 99;
    /* Linear: 0%→0, 99%→252 (approx) */
    return pwm_to_dac_table[(pwm_percent * 255) / 100];
}

/* ============================================================
 * TASK 3 — static for state retention
 * ============================================================ */

uint8_t debounce_button(uint8_t raw_pin_state)
{
    static uint8_t counter     = 0;
    static uint8_t stable_state = 0;

    if (raw_pin_state == stable_state) {
        counter = 0;
    } else {
        counter++;
        if (counter >= 5) {
            stable_state = raw_pin_state;
            counter = 0;
        }
    }
    return stable_state;
}

static volatile uint32_t g_tick_ms = 0;

void systick_isr(void) { g_tick_ms++; }

uint32_t get_tick_ms(void)
{
    /* On 32-bit ARM: single uint32_t read is atomic — no critical section needed.
     * On 8-bit MCU: would need critical section (4 byte reads not atomic). */
    return g_tick_ms;
}

uint8_t has_elapsed_ms(uint32_t start_tick, uint32_t duration_ms)
{
    /* Unsigned subtraction handles wrap-around correctly.
     * Example: start=0xFFFFFF00, now=0x00000100, duration=512
     * now-start = 0x00000200 = 512 ≥ 512 → elapsed. Correct! */
    return (get_tick_ms() - start_tick) >= duration_ms ? 1u : 0u;
}

/* ============================================================
 * TASK 4 — const volatile pointer to hardware register
 * ============================================================ */

static volatile uint32_t _fake_timer_cnt = 100;

/* const: software must not write it
 * volatile: hardware changes it (compiler must re-read every time) */
const volatile uint32_t *timer_count_reg = &_fake_timer_cnt;

uint32_t read_timer_count(void) { return *timer_count_reg; }

/* This would NOT compile if qualifier is correct:
 * void BAD_write_timer(uint32_t v) { *timer_count_reg = v; } */

/* ============================================================
 * TASK 5 — static file-scope PWM module
 * ============================================================ */

static uint8_t  pwm_duty_pct    = 0;
static uint32_t pwm_period_ticks = 0;
static uint32_t pwm_on_ticks     = 0;

void pwm_set_duty(uint8_t duty_pct, uint32_t period_ticks)
{
    pwm_duty_pct     = duty_pct;
    pwm_period_ticks = period_ticks;
    pwm_on_ticks     = (uint32_t)((duty_pct * (uint64_t)period_ticks) / 100u);
}

uint8_t pwm_get_output(uint32_t current_tick)
{
    if (pwm_period_ticks == 0) return 0;
    return ((current_tick % pwm_period_ticks) < pwm_on_ticks) ? 1u : 0u;
}

/* ============================================================
 * TASK 6 — Bug hunt FIXED
 *
 * Bug 1: `uint32_t tick` should be `volatile uint32_t tick`
 *        Without volatile, the compiler sees tick never changes in the
 *        while(1) loop (from its perspective, ISR is invisible) and
 *        optimizes the whole condition away → infinite tight loop.
 *
 * Bug 2: `uint32_t last_toggle = 0` is a LOCAL variable inside led_blink_BUGGY().
 *        Every time the function is called it resets to 0.
 *        It must be `static uint32_t last_toggle = 0` to persist across calls.
 *        (Or the function should be called only once and loop internally.)
 *
 * Bug 3: `tick` is never incremented in the loop.
 *        On a real MCU this is done by a SysTick ISR. The missing `volatile`
 *        on Bug 1 prevents the compiler from even checking for ISR changes.
 *        Fix: volatile + ISR that increments tick.
 * ============================================================ */

/* ============================================================
 * INTERVIEW QUESTION ANSWERS
 * ============================================================

Q1: What happens if you remove volatile from an ISR flag variable?

A: Without volatile, the compiler analyzes the code as if the variable
   never changes outside normal program flow. In main():
     while (!g_flag) {}  // compiler: g_flag is always 0 → while(1)
   The compiler caches g_flag in a register at the top of the loop and
   never re-reads memory. The ISR writes to RAM, but main() reads its
   cached register forever. System hangs.
   Example with GCC -O2: the loop becomes a single CMP + branch back to itself.

Q2: Can a variable be both const and volatile? Real example.

A: Yes. const volatile uint32_t *STATUS_REG = (uint32_t*)0x40013800;
   `const`:    software must not write to it (read-only register).
   `volatile`: the value changes autonomously (hardware writes it), so
               compiler must re-read on every access, never cache.
   Without volatile: compiler reads once and optimizes away future reads.
   Without const:    a coding mistake could write to the HW register.

Q3: static global vs non-static global. Why prefer static?

A: Non-static global: visible to ALL translation units via `extern`. Any .c
   file can accidentally read or modify it.
   Static global (file-scope): visible only in the declaring .c file.
   Benefits for embedded:
   - Encapsulation: prevents other modules from bypassing your API.
   - Linker optimization: linker can eliminate unused static globals.
   - Name collision prevention: two files can have `static int counter` without conflict.
   Embedded style guides (MISRA C:2012 Rule 8.7) require static for variables
   not used by other translation units.

Q4: uint32_t counter in SysTick ISR, read in main. Atomic on M4? M0?

A: Cortex-M4 (32-bit): yes, a single 32-bit aligned read (LDR instruction) is
   atomic — the bus completes the entire 32-bit transaction indivisibly.
   Cortex-M0/M0+ (32-bit bus but narrower instruction set): also atomic for
   aligned 32-bit access.
   However: if the counter is non-volatile, the compiler may not re-read it
   each time. Always use `volatile` regardless of atomicity.
   Non-atomic cases: uint64_t (two 32-bit reads), structs, unaligned access.

Q5: const uint8_t lookup_table[512] in STM32. Where does it live?

A: In the .rodata section, which the linker places in FLASH (read-only memory).
   It does NOT consume SRAM. This is the key advantage: a 512-byte table in SRAM
   would eat 512/8192 = 6.25% of an 8KB SRAM budget for nothing.
   Verify with: arm-none-eabi-nm firmware.elf | grep lookup_table
   You'll see the address is in the flash range (e.g., 0x08002xxx).
   Also: arm-none-eabi-size firmware.elf → check .text+.rodata vs .data+.bss.
*/

int main(void)
{
    /* ISR→main communication test */
    simulated_uart_isr(0x42);
    int b = main_loop_iteration();
    assert(b == 0x42);
    assert(main_loop_iteration() == -1);  /* no more data */

    /* Debounce: need 5 consecutive different reads to change state */
    assert(debounce_button(1) == 0);
    assert(debounce_button(1) == 0);
    assert(debounce_button(1) == 0);
    assert(debounce_button(1) == 0);
    assert(debounce_button(1) == 1);  /* 5th call → stable */

    /* Tick elapsed */
    g_tick_ms = 1000;
    assert(has_elapsed_ms(900, 100) == 1);
    assert(has_elapsed_ms(900, 200) == 0);

    /* Timer count register */
    assert(read_timer_count() == 100);

    /* PWM */
    pwm_set_duty(50, 1000);
    assert(pwm_on_ticks == 500);
    assert(pwm_get_output(0)   == 1);
    assert(pwm_get_output(499) == 1);
    assert(pwm_get_output(500) == 0);
    assert(pwm_get_output(999) == 0);

    printf("All volatile/const/static answers verified.\n");
    return 0;
}
