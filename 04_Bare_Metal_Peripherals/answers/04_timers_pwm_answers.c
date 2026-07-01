/*
 * ANSWERS: 04_Bare_Metal_Peripherals/04_timers_pwm.c
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <assert.h>

/* ============================================================
 * INTERVIEW QUESTION ANSWERS
 * ============================================================

Q1: Timer output 1 Hz from 84 MHz, PSC=8399, ARR=9999. Verify.

A: Output frequency = FCLK / ((PSC+1) * (ARR+1))
   = 84,000,000 / (8400 * 10000)
   = 84,000,000 / 84,000,000
   = 1 Hz  ✓
   Period = 1 second. Timer counts 0..9999 (10000 ticks) at 10 kHz internal clock.

Q2: PWM duty cycle stuck at 0%. Debug checklist.

A: 1. Check CCER: CC1E bit (channel 1 output enable) must be set (=1).
      If CC1E=0, the pin is always driven by the GPIO default, not PWM.
   2. Check CCR1 value: if CCR1=0 in PWM Mode 1 (CNT<CCR→HIGH),
      output is HIGH for 0 counts → effectively 0% duty.
   3. Check CCMR1: OC1M[2:0] must be 110 (PWM Mode 1) or 111 (Mode 2).
      If OC1M=000 (frozen), output doesn't change.
   4. Check GPIO alternate function: pin must be in AF mode with correct AF#.
      If pin is in GPIO output mode, PWM signal won't appear on the pin.
   5. Check ARR preload: OC1PE (CCMR1 bit 3) should be set if changing CCR
      during operation — prevents glitches.
   6. Check BDTR (advanced timers TIM1/TIM8): MOE (Main Output Enable) bit
      must be set. Advanced timers require MOE=1 for output to appear on pins.

Q3: Input capture for PWM frequency and duty cycle measurement.

A: Use two channels on the same pin (CC1S/CC2S):
   CC1: trigger on rising edge  (IC1F=00, CC1P=0=rising)
   CC2: trigger on falling edge on the same pin (CC2S=10=IC2 mapped to TI1,
        CC2P=1=falling)
   Period measurement: capture CC1 at rising edge → wait → capture CC1 again.
   frequency = TIMER_CLOCK / (CC1[2] - CC1[1]) -- or use period from CC1
   Duty cycle: capture CC2 (falling) and CC1 (next rising).
   high_time = CC2 - CC1 (previous rising)
   period    = CC1_new - CC1_old
   duty_cycle = (high_time * 100) / period
   Reset counter on CC1 capture (SMS=100 reset mode on TI1FP1) for clean measurement.

Q4: SysTick for FreeRTOS tick vs systick for bare-metal delay. Difference?

A: FreeRTOS tick: SysTick fires every 1 ms (or 1/configTICK_RATE_HZ seconds).
   The SysTick_Handler calls xPortSysTickHandler() which: increments tick count,
   checks if any blocked task should wake up, and triggers PendSV if context
   switch needed. PendSV handler does the actual context switch.
   The tick ISR must NOT disable FreeRTOS interrupts (must be at or below
   configMAX_SYSCALL_INTERRUPT_PRIORITY).

   Bare-metal delay: SysTick counts down from reload value, fires COUNTFLAG.
   Spin in a loop checking COUNTFLAG or decrement a counter in SysTick_Handler.
   No RTOS involvement. Blocking delay.

Q5: Timer resolution at PSC=0, 168 MHz. Minimum measurable pulse?

A: PSC=0: timer increments every 1/168MHz = ~5.95 ns.
   This is the timer resolution.
   Minimum measurable pulse = 1 timer tick = 5.95 ns.
   Practical limit: input capture jitter adds 1-2 ticks of uncertainty.
   So minimum reliably measured pulse ≈ 2-3 ticks ≈ 12-18 ns.
   ARR=0xFFFF (65535): max measurable period = 65535 × 5.95 ns ≈ 390 µs.
   For longer pulses use a higher PSC.

Q6: Timer overflow / rollover in input capture — how to handle?

A: If the measured pulse is longer than one timer period, the captured CC2
   value is less than CC1 (overflow occurred between edges).
   Handle with: if (CC2 >= CC1) width = CC2 - CC1;
                else             width = (ARR + 1 - CC1) + CC2;
   Better: use uint32_t subtraction which wraps correctly if ARR=0xFFFF
   and values are 16-bit: width = (uint16_t)(CC2 - CC1);
   Enable timer overflow interrupt: increment a 32-bit overflow counter.
   True 32-bit timestamp = (overflow_count << 16) | CCR_value.
   Then: width = timestamp2 - timestamp1 (natural uint32_t subtraction handles wrap).
*/

typedef struct {
    volatile uint32_t CR1, CR2, SMCR, DIER, SR, EGR;
    volatile uint32_t CCMR1, CCMR2, CCER, CNT, PSC, ARR;
    volatile uint32_t RCR, CCR1, CCR2, CCR3, CCR4;
    volatile uint32_t BDTR, DCR, DMAR;
} TIM_TypeDef;

#define TIM_CR1_CEN       (1u << 0)
#define TIM_CR1_ARPE      (1u << 7)
#define TIM_CCMR1_OC1M_PWM1  (0b110u << 4)
#define TIM_CCMR1_OC1PE   (1u << 3)
#define TIM_CCER_CC1E     (1u << 0)

static TIM_TypeDef _TIM2 = {0}, _TIM3 = {0};
TIM_TypeDef *TIM2 = &_TIM2, *TIM3 = &_TIM3;

/* ============================================================
 * TASK 1 — Timer frequency calculation
 * ============================================================ */

void timer_calc_psc_arr(uint32_t timer_clk_hz, uint32_t target_freq_hz,
                        uint16_t *psc_out, uint32_t *arr_out)
{
    /* Try to find PSC such that ARR < 65536.
       ARR = timer_clk / ((PSC+1) * freq) - 1 */
    for (uint32_t psc = 0; psc < 65536; psc++) {
        uint32_t arr = (timer_clk_hz / ((psc + 1) * target_freq_hz));
        if (arr >= 1 && arr <= 65536) {
            *psc_out = (uint16_t)psc;
            *arr_out = arr - 1;
            return;
        }
    }
    /* Fallback — best effort */
    *psc_out = 0xFFFFu;
    *arr_out = 0xFFFFu;
}

/* ============================================================
 * TASK 2 — Timer timebase init
 * ============================================================ */

void timer_init_timebase(TIM_TypeDef *tim, uint16_t psc, uint32_t arr)
{
    tim->CR1 &= ~TIM_CR1_CEN;     /* disable */
    tim->PSC  = psc;
    tim->ARR  = arr;
    tim->CR1 |= TIM_CR1_ARPE;     /* ARR preload */
    tim->EGR  = 1u;               /* UG: force register update */
    tim->SR   = 0;                /* clear UIF */
    tim->CR1 |= TIM_CR1_CEN;      /* enable */
}

/* ============================================================
 * TASK 3 — PWM output init
 * ============================================================ */

void timer_pwm_init(TIM_TypeDef *tim, uint16_t psc, uint32_t arr)
{
    tim->CR1 &= ~TIM_CR1_CEN;
    tim->PSC  = psc;
    tim->ARR  = arr;
    tim->CR1 |= TIM_CR1_ARPE;

    /* PWM Mode 1 on channel 1, enable preload */
    tim->CCMR1 = (tim->CCMR1 & ~(0b111u << 4)) | TIM_CCMR1_OC1M_PWM1 | TIM_CCMR1_OC1PE;

    /* Enable channel 1 output */
    tim->CCER |= TIM_CCER_CC1E;

    tim->CCR1  = 0;               /* start at 0% duty */
    tim->EGR   = 1u;              /* force update */
    tim->SR    = 0;
    tim->CR1  |= TIM_CR1_CEN;
}

void timer_pwm_set_duty(TIM_TypeDef *tim, uint8_t duty_percent)
{
    if (duty_percent > 100) duty_percent = 100;
    tim->CCR1 = (duty_percent * (tim->ARR + 1)) / 100u;
}

/* ============================================================
 * TASK 4 — Input capture PWM measurement
 * ============================================================ */

void timer_input_capture_init(TIM_TypeDef *tim, uint16_t psc)
{
    tim->CR1 &= ~TIM_CR1_CEN;
    tim->PSC  = psc;
    tim->ARR  = 0xFFFFu;

    /* CC1S=01: IC1 mapped to TI1 (rising edge for period)
       CC2S=10: IC2 mapped to TI1 (falling edge for high time) */
    tim->CCMR1 = (0b01u << 0) | (0b10u << 8);  /* CC1S | CC2S */

    /* CC1P=0 (rising), CC2P=1 (falling) */
    tim->CCER  = (1u << 0) | (1u << 4) | (1u << 5);

    tim->CR1  |= TIM_CR1_CEN;
}

/* ============================================================
 * TASK 5 — SysTick
 * ============================================================ */

typedef struct { volatile uint32_t CTRL, LOAD, VAL, CALIB; } SysTick_TypeDef;
static SysTick_TypeDef _SysTick = {0};
SysTick_TypeDef *SysTick = &_SysTick;
#define SYSTICK_CTRL_ENABLE    (1u << 0)
#define SYSTICK_CTRL_TICKINT   (1u << 1)
#define SYSTICK_CTRL_CLKSOURCE (1u << 2)

volatile uint32_t g_tick_ms = 0;

void systick_init(uint32_t cpu_hz, uint32_t tick_hz)
{
    SysTick->CTRL  = 0;
    SysTick->LOAD  = (cpu_hz / tick_hz) - 1u;
    SysTick->VAL   = 0;
    SysTick->CTRL  = SYSTICK_CTRL_ENABLE | SYSTICK_CTRL_TICKINT | SYSTICK_CTRL_CLKSOURCE;
}

void SysTick_Handler(void) { g_tick_ms++; }

uint32_t get_tick_ms(void) { return g_tick_ms; }

void delay_ms(uint32_t ms)
{
    uint32_t start = get_tick_ms();
    while ((get_tick_ms() - start) < ms) {}
}

/* ============================================================
 * TASK 6 — Bug hunt FIXED
 *
 * Bug 1: PSC = 84000000 / 1000 = 84000 — but PSC is 16-bit (max 65535).
 *        Produces wrong prescaler (wraps), timer runs at wrong frequency.
 *        FIX: Use PSC=839, ARR=99999 (if ARR is 32-bit as in TIM2/TIM5)
 *             or PSC=8399, ARR=9999.
 *
 * Bug 2: CCER CC1E bit not set — channel output is disabled.
 *        The PWM signal is generated internally but never reaches the pin.
 *        FIX: tim->CCER |= TIM_CCER_CC1E;
 *
 * Bug 3: CCR1 = arr (100% duty always).
 *        PWM Mode 1: output HIGH while CNT < CCR1.
 *        If CCR1 = ARR, output is HIGH for almost all ticks = ~100% duty.
 *        FIX: CCR1 = arr / 2 for 50% duty. Or use timer_pwm_set_duty().
 * ============================================================ */

int main(void)
{
    uint16_t psc; uint32_t arr;

    /* 84 MHz timer → 1 Hz */
    timer_calc_psc_arr(84000000, 1, &psc, &arr);
    assert((uint32_t)(psc + 1) * (arr + 1) == 84000000u);

    /* 84 MHz timer → 1000 Hz */
    timer_calc_psc_arr(84000000, 1000, &psc, &arr);
    assert((uint32_t)(psc + 1) * (arr + 1) == 84000u);

    /* PWM init and duty */
    timer_pwm_init(TIM2, 839, 99999);
    assert(TIM2->CR1 & TIM_CR1_CEN);
    assert(TIM2->CCER & TIM_CCER_CC1E);

    timer_pwm_set_duty(TIM2, 50);
    assert(TIM2->CCR1 == 50000u);

    timer_pwm_set_duty(TIM2, 0);
    assert(TIM2->CCR1 == 0u);

    timer_pwm_set_duty(TIM2, 100);
    assert(TIM2->CCR1 == 100000u);

    printf("All timer/PWM answers verified.\n");
    return 0;
}
