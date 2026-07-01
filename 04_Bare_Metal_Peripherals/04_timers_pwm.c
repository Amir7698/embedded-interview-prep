/*
 * ============================================================
 * EMBEDDED INTERVIEW PREP
 * Topic : Timers and PWM — Hardware Timer Configuration
 * File  : 04_Bare_Metal_Peripherals/04_timers_pwm.c
 * ============================================================
 *
 * Timers are the backbone of embedded systems:
 * PWM, input capture, output compare, time-base, watchdog.
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <assert.h>

/* ============================================================
 * THEORY — STM32 General-Purpose Timer (TIM2–TIM5)
 * ============================================================
 *
 * A timer increments its counter CNT every N clock cycles,
 * where N is set by the prescaler (PSC).
 *
 * Timer clock frequency: FCLK (after APB prescaler)
 * Counter clock:         FCLK / (PSC + 1)
 * Update event (UEV):    occurs when CNT reaches ARR (auto-reload)
 *
 * Formula:
 *   Update frequency = FCLK / ((PSC+1) * (ARR+1))
 *   Period (s)       = 1 / Update_frequency
 *
 * Example: 1 ms tick with 84 MHz timer clock
 *   84,000,000 / ((839+1) * (99+1)) = 1000 Hz → 1 ms period
 *   PSC = 839, ARR = 99
 *
 * Key registers:
 *   CR1   : CEN (bit0) — counter enable, DIR (bit4) — up/down
 *   PSC   : Prescaler value (PSC+1 divides input clock)
 *   ARR   : Auto-reload register (counter wraps at ARR)
 *   CNT   : Current counter value
 *   CCR1-4: Capture/Compare registers (for PWM or input capture)
 *   CCMR1 : Channel 1/2 mode (OC1M bits [6:4] for PWM mode)
 *   CCER  : Capture/compare enable
 *   DIER  : Interrupt/DMA enable (UIE bit0 = update interrupt)
 *   SR    : Status (UIF bit0 = update interrupt flag)
 *   EGR   : Event generation (UG bit0 = force update)
 *
 * PWM Mode 1: CNT < CCRx → output HIGH
 *             CNT >= CCRx → output LOW
 *   Duty cycle = CCRx / (ARR+1) × 100%
 * ============================================================ */

typedef struct {
    volatile uint32_t CR1;
    volatile uint32_t CR2;
    volatile uint32_t SMCR;
    volatile uint32_t DIER;
    volatile uint32_t SR;
    volatile uint32_t EGR;
    volatile uint32_t CCMR1;
    volatile uint32_t CCMR2;
    volatile uint32_t CCER;
    volatile uint32_t CNT;
    volatile uint32_t PSC;
    volatile uint32_t ARR;
    volatile uint32_t RCR;
    volatile uint32_t CCR1;
    volatile uint32_t CCR2;
    volatile uint32_t CCR3;
    volatile uint32_t CCR4;
    volatile uint32_t BDTR;
} TIM_TypeDef;

/* CR1 bits */
#define TIM_CR1_CEN   (1u << 0)
#define TIM_CR1_UDIS  (1u << 1)
#define TIM_CR1_URS   (1u << 2)
#define TIM_CR1_ARPE  (1u << 7)   /* Auto-reload preload enable */

/* DIER bits */
#define TIM_DIER_UIE   (1u << 0)
#define TIM_DIER_CC1IE (1u << 1)
#define TIM_DIER_CC2IE (1u << 2)

/* SR bits */
#define TIM_SR_UIF   (1u << 0)
#define TIM_SR_CC1IF (1u << 1)

/* CCER bits */
#define TIM_CCER_CC1E  (1u << 0)
#define TIM_CCER_CC1P  (1u << 1)
#define TIM_CCER_CC2E  (1u << 4)

/* EGR bits */
#define TIM_EGR_UG   (1u << 0)

/* CCMR OC mode bits (in OC mode, bits [6:4] of each nibble) */
#define TIM_CCMR1_OC1M_PWM1  (0b110u << 4)
#define TIM_CCMR1_OC1M_PWM2  (0b111u << 4)
#define TIM_CCMR1_OC1PE      (1u << 3)   /* OC1 preload enable */
#define TIM_CCMR1_CC1S_OUT   (0b00u << 0)

static TIM_TypeDef _TIM2 = {0};
static TIM_TypeDef _TIM3 = {0};
TIM_TypeDef *TIM2 = &_TIM2;
TIM_TypeDef *TIM3 = &_TIM3;

/* ============================================================
 * TASK 1 — PSC and ARR calculation
 * ============================================================ */

void timer_calc_psc_arr(uint32_t fclk_hz, uint32_t desired_freq_hz,
                        uint16_t *psc_out, uint16_t *arr_out)
{
    /* TODO: find PSC and ARR such that:
     *   fclk_hz / ((PSC+1) * (ARR+1)) = desired_freq_hz
     *
     * Approach: try to keep ARR close to 999 (3 decimal digits of resolution)
     * PSC = fclk_hz / (desired_freq_hz * (ARR+1)) - 1
     *
     * Hint: start with ARR=999, compute PSC.
     * If PSC > 65535, increase ARR. */
    (void)fclk_hz; (void)desired_freq_hz; (void)psc_out; (void)arr_out;
}

/* ============================================================
 * TASK 2 — Basic timer (time-base) initialization
 * ============================================================ */

void timer_init_timebase(TIM_TypeDef *tim, uint16_t psc, uint32_t arr,
                         uint8_t irq_enable)
{
    /* TODO: disable timer first */
    /* TODO: set PSC = psc */
    /* TODO: set ARR = arr */
    /* TODO: if irq_enable: DIER |= UIE */
    /* TODO: set ARPE (preload ARR updates) */
    /* TODO: generate update event (EGR |= UG) to load PSC and ARR immediately */
    /* TODO: enable timer (CR1 |= CEN) */
    (void)tim; (void)psc; (void)arr; (void)irq_enable;
}

/* ============================================================
 * TASK 3 — PWM output configuration (channel 1)
 *
 * Configure TIM channel 1 as PWM Mode 1 output.
 * After this, changing CCR1 changes the duty cycle.
 * ============================================================ */

void timer_pwm_init(TIM_TypeDef *tim, uint16_t psc, uint32_t arr,
                    uint32_t ccr1_initial)
{
    /* TODO: set PSC and ARR for desired frequency */
    /* TODO: configure CCMR1:
     *   CC1S = 00 (output compare mode)
     *   OC1M = 110 (PWM Mode 1)
     *   OC1PE = 1 (preload enable — duty cycle takes effect on next update) */
    /* TODO: configure CCER: CC1E = 1 (enable channel 1 output) */
    /* TODO: set CCR1 = ccr1_initial */
    /* TODO: enable ARPE, generate UG, enable CEN */
    (void)tim; (void)psc; (void)arr; (void)ccr1_initial;
}

void timer_pwm_set_duty(TIM_TypeDef *tim, uint8_t duty_percent)
{
    /* TODO: CCR1 = (duty_percent * (tim->ARR + 1)) / 100
     * Note: with OC1PE set, this takes effect at next update event */
    (void)tim; (void)duty_percent;
}

/* ============================================================
 * TASK 4 — Input capture (measure pulse width or frequency)
 *
 * Configure channel 1 to capture on rising edge,
 * channel 2 to capture on falling edge.
 * Pulse width = CCR2 - CCR1 (in timer ticks).
 * ============================================================ */

void timer_input_capture_init(TIM_TypeDef *tim, uint16_t psc)
{
    /* TODO: set PSC for desired timer resolution
     * TODO: ARR = 0xFFFF (max — don't wrap during pulse)
     * TODO: CCMR1:
     *   CC1S = 01 (channel 1 input, mapped to TI1)
     *   CC2S = 10 (channel 2 input, mapped to TI1 inverted)
     * TODO: CCER:
     *   CC1P = 0 (rising edge), CC1E = 1
     *   CC2P = 1 (falling edge), CC2E = 1
     * TODO: SMCR = 0 (no slave mode — free running)
     * TODO: DIER: CC1IE = 1 to fire ISR on rising edge capture
     * TODO: enable timer */
    (void)tim; (void)psc;
}

uint32_t timer_get_pulse_width_us(TIM_TypeDef *tim, uint32_t fclk_hz)
{
    /* TODO: ticks = tim->CCR2 - tim->CCR1 (handles wrap if CCR2 < CCR1)
     * TODO: return ticks * 1_000_000 / (fclk_hz / (tim->PSC + 1)) */
    (void)tim; (void)fclk_hz;
    return 0;
}

/* ============================================================
 * TASK 5 — SysTick timer (Cortex-M core timer)
 *
 * SysTick is always present on Cortex-M.
 * 24-bit down-counter, reloads from LOAD register.
 * Used for OS tick (FreeRTOS) or simple delay.
 * ============================================================ */

typedef struct {
    volatile uint32_t CTRL;   /* bit0=ENABLE, bit1=TICKINT, bit2=CLKSOURCE, bit16=COUNTFLAG */
    volatile uint32_t LOAD;
    volatile uint32_t VAL;    /* current count (write any value to clear) */
    volatile uint32_t CALIB;
} SysTick_TypeDef;

#define SYSTICK_CTRL_ENABLE    (1u << 0)
#define SYSTICK_CTRL_TICKINT   (1u << 1)
#define SYSTICK_CTRL_CLKSOURCE (1u << 2)
#define SYSTICK_CTRL_COUNTFLAG (1u << 16)

static SysTick_TypeDef _SysTick = {0};
SysTick_TypeDef *SysTick = &_SysTick;

void systick_init_ms(uint32_t fclk_hz, uint8_t irq_enable)
{
    /* TODO: LOAD = (fclk_hz / 1000) - 1    → fires every 1 ms */
    /* TODO: VAL  = 0                         → clear current count */
    /* TODO: CTRL: CLKSOURCE=1 (core clock), TICKINT=irq_enable, ENABLE=1 */
    (void)fclk_hz; (void)irq_enable;
}

/* ============================================================
 * TASK 6 — BUG HUNT: PWM initialization bugs
 *
 * The code below configures a 1 kHz PWM at 50% duty.
 * Timer clock = 84 MHz. It has 3 bugs.
 * ============================================================ */

void pwm_init_BUGGY(TIM_TypeDef *tim)
{
    uint32_t fclk = 84000000;
    uint32_t pwm_freq = 1000;
    uint32_t arr = 999;

    /* Bug 1: PSC calculation is wrong */
    /* Correct: PSC = fclk / (pwm_freq * (arr+1)) - 1 = 84000000 / 1000000 - 1 = 83 */
    tim->PSC = fclk / pwm_freq;   /* gives 84000 — far too large */

    tim->ARR = arr;

    /* Bug 2: CC1S not set to output (00) before enabling OC mode */
    tim->CCMR1 = TIM_CCMR1_OC1M_PWM1 | TIM_CCMR1_OC1PE;
    /* Missing: | TIM_CCMR1_CC1S_OUT — but 00 is already 0, so this is actually fine.
     * Real bug 2: OC1PE missing in this version — duty cycle won't update cleanly */

    /* Actual Bug 2: channel output not enabled */
    /* Missing: tim->CCER |= TIM_CCER_CC1E; */

    /* Bug 3: 50% duty should be CCR1 = (ARR+1)/2 = 500 */
    tim->CCR1 = arr;   /* this is 100% duty, not 50% */

    tim->EGR |= TIM_EGR_UG;
    tim->CR1 |= TIM_CR1_CEN;
}

/* ============================================================
 * SELF-TEST
 * ============================================================ */

int main(void)
{
    uint16_t psc = 0, arr = 0;
    timer_calc_psc_arr(84000000, 1000, &psc, &arr);
    /* With ARR=999: PSC = 84000000/(1000*1000)-1 = 83 */
    assert(psc == 83 || psc == 83);
    assert(arr == 999);

    printf("All Timer tests PASSED.\n");
    return 0;
}

/* ============================================================
 * INTERVIEW QUESTIONS
 * ============================================================
 *
 * Q1: Timer clock is 84 MHz. You want 50 Hz PWM with 1000 steps
 *     of resolution. Calculate PSC and ARR.
 *     Answer: TODO
 *
 * Q2: What is the difference between PWM Mode 1 and PWM Mode 2?
 *     Answer: TODO
 *
 * Q3: Why should you set OC1PE (preload enable) when changing
 *     duty cycle dynamically?
 *     Answer: TODO
 *
 * Q4: You need a 1 µs timer tick for a sonar range finder.
 *     Timer clock = 72 MHz. What PSC value do you use?
 *     Answer: TODO
 *
 * Q5: What is the difference between TIM2 and TIM6 on STM32F4?
 *     When would you choose each?
 *     Answer: TODO
 */
