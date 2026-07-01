/*
 * ============================================================
 * EMBEDDED INTERVIEW PREP
 * Topic : volatile, const, static — The Three Keywords
 *         Every Embedded Interview Asks About
 * File  : 01_C_Fundamentals/04_volatile_const_static.c
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <assert.h>

/* ============================================================
 * THEORY
 * ============================================================
 *
 * volatile:
 *   Tells the compiler: "DO NOT optimise this read/write away.
 *   This variable can change outside the normal program flow
 *   (hardware, ISR, other CPU core)."
 *   WITHOUT volatile, the compiler may cache in a register
 *   and never re-read — your ISR flag never gets seen.
 *
 * const:
 *   "This value will not change." Compiler can place in ROM.
 *   On MCUs: const globals go in .rodata in flash (free SRAM).
 *   const + volatile together: hardware status register that
 *   changes by itself but you never write to it.
 *
 * static:
 *   At function scope: variable survives across calls (stored
 *   in .data or .bss, not on stack).
 *   At file scope: limits visibility to this translation unit
 *   (the embedded equivalent of "private").
 *
 * const volatile uint32_t *:
 *   A read-only hardware register that changes by itself.
 *   Example: RX data register — you read it, hardware writes it.
 * ============================================================ */


/* ============================================================
 * TASK 1 — volatile: ISR to main communication
 *
 * A UART ISR fills a ring buffer and sets a flag.
 * Main loop must see the flag change. What goes wrong without volatile?
 * ============================================================ */

/* The flag — ISR sets it, main reads it */
/* TODO: add the correct qualifier(s) to this declaration */
uint8_t g_data_ready = 0;

/* Ring buffer — written by ISR, read by main */
/* TODO: add the correct qualifier(s) */
uint8_t g_rx_buf[64];
/* TODO: add the correct qualifier(s) */
uint8_t g_rx_head = 0;
uint8_t g_rx_tail = 0;

/* Simulated ISR — would be USART1_IRQHandler on a real MCU */
void simulated_uart_isr(uint8_t received_byte)
{
    g_rx_buf[g_rx_head % 64] = received_byte;
    g_rx_head++;
    g_data_ready = 1;
}

int main_loop_iteration(void)
{
    /* TODO: check g_data_ready
     * if set: read one byte from g_rx_buf[g_rx_tail % 64], increment g_rx_tail
     *         if g_rx_tail == g_rx_head: clear g_data_ready
     *         return the byte read
     * if not set: return -1 */
    return -1;
}

/* ============================================================
 * TASK 2 — const for lookup tables and configuration
 *
 * On a microcontroller, const global arrays go in flash.
 * This frees up precious SRAM.
 *
 * Implement a PWM duty-cycle to DAC output lookup table.
 * The table should live in ROM (flash), never in SRAM.
 * ============================================================ */

/* TODO: declare this table with the correct qualifier so it
 * lives in ROM on an MCU — 256 entries, 0 to 255 */
uint8_t pwm_to_dac_table[256]; /* TODO: fix qualifier, TODO: fill in values 0..255 */

uint8_t pwm_to_dac(uint8_t pwm_percent)
{
    /* TODO: clamp pwm_percent to 0-99, index the table
     * (100% PWM = 255 DAC, linear) */
    (void)pwm_percent;
    return 0;
}

/* ============================================================
 * TASK 3 — static for state retention and encapsulation
 * ============================================================ */

/* A simple debounce filter — must retain state between calls */
uint8_t debounce_button(uint8_t raw_pin_state)
{
    /* TODO: declare a static uint8_t counter = 0
     *       declare a static uint8_t stable_state = 0
     * Logic:
     *   if raw_pin_state == stable_state: reset counter, return stable_state
     *   else: increment counter
     *         if counter >= 5: stable_state = raw_pin_state, counter = 0
     *   return stable_state */
    (void)raw_pin_state;
    return 0;
}

/* Millisecond tick counter — incremented by SysTick ISR */
static volatile uint32_t g_tick_ms = 0;

void systick_isr(void) /* simulated */
{
    g_tick_ms++;
}

uint32_t get_tick_ms(void)
{
    /* TODO: return g_tick_ms
     * Q: should this read be protected? Why / why not on 32-bit ARM? */
    return 0;
}

uint8_t has_elapsed_ms(uint32_t start_tick, uint32_t duration_ms)
{
    /* TODO: return 1 if (get_tick_ms() - start_tick) >= duration_ms
     * Note: this works correctly even when the counter wraps around!
     * (unsigned subtraction wraps correctly in C)
     * This is the correct way to measure elapsed time in embedded. */
    (void)start_tick; (void)duration_ms;
    return 0;
}

/* ============================================================
 * TASK 4 — const volatile together: read-only hardware register
 *
 * A hardware timer counter register:
 * - Changes continuously (hardware increments it → volatile)
 * - You should never write to it from software (const)
 * ============================================================ */

/* Simulated timer counter register */
static volatile uint32_t _fake_timer_cnt = 0;

/* TODO: declare timer_count_reg as a pointer to
 * a const volatile uint32_t, pointing to _fake_timer_cnt */
/* const volatile uint32_t *timer_count_reg = ... */

uint32_t read_timer_count(void)
{
    /* TODO: dereference timer_count_reg and return the value */
    return 0;
}

/* Try to write — should not compile if declared correctly */
/* void BAD_write_timer(uint32_t v) { *timer_count_reg = v; } */

/* ============================================================
 * TASK 5 — static at file scope: module-private state
 *
 * Implement a software PWM module. The internal state should
 * not be accessible from outside this file.
 * ============================================================ */

/* TODO: declare these as file-private (not accessible from other .c files) */
uint8_t  pwm_duty_pct = 0;
uint32_t pwm_period_ticks = 0;
uint32_t pwm_on_ticks     = 0;

void pwm_set_duty(uint8_t duty_pct, uint32_t period_ticks)
{
    /* TODO: save period_ticks and duty_pct
     * TODO: compute pwm_on_ticks = (duty_pct * period_ticks) / 100 */
    (void)duty_pct; (void)period_ticks;
}

/* Called from a timer ISR every tick */
uint8_t pwm_get_output(uint32_t current_tick)
{
    /* TODO: return 1 if (current_tick % pwm_period_ticks) < pwm_on_ticks
     * return 0 otherwise
     * Handle pwm_period_ticks == 0 (division by zero guard) */
    (void)current_tick;
    return 0;
}

/* ============================================================
 * TASK 6 — BUG HUNT
 *
 * The function below should blink an LED with a non-blocking timer.
 * It has 3 bugs related to volatile/static/const misuse.
 * Find and mark each one.
 * ============================================================ */

uint32_t tick = 0;   /* Bug 1: ??? — this is read in a polling loop */

void led_blink_BUGGY(void)
{
    uint32_t last_toggle = 0;
    uint8_t  led_state   = 0;
    const uint32_t BLINK_INTERVAL = 500;   /* ms */

    /* Bug 2: ??? — 'last_toggle' is a local variable.
     * What happens every time this function is called? */
    while (1) {
        if ((tick - last_toggle) >= BLINK_INTERVAL) {
            led_state   ^= 1;
            last_toggle  = tick;
            printf("LED: %s\n", led_state ? "ON" : "OFF");
        }
        /* Bug 3: ??? — 'tick' is never incremented here. In a real system
         * it would be incremented by SysTick ISR. But what qualifier is
         * missing on the declaration that lets the compiler optimise
         * this loop into an infinite tight loop reading a cached register? */
    }
}

/* ============================================================
 * INTERVIEW QUESTIONS
 * ============================================================
 *
 * Q1: What happens if you remove volatile from an ISR flag variable?
 *     Give a concrete compiler optimisation example.
 *     Answer: TODO
 *
 * Q2: Can a variable be both const and volatile? Give a real example.
 *     Answer: TODO
 *
 * Q3: What is the difference between static global and non-static global?
 *     Why do embedded style guides prefer static file-scope variables?
 *     Answer: TODO
 *
 * Q4: A uint32_t counter is incremented in SysTick ISR and read in main.
 *     On a 32-bit ARM Cortex-M4, is the read atomic? On a Cortex-M0?
 *     Answer: TODO
 *
 * Q5: You have a const uint8_t lookup_table[512] in an STM32 project.
 *     Where does it physically live? How can you verify this?
 *     Answer: TODO
 */
