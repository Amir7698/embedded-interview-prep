/*
 * ANSWERS: 04_Bare_Metal_Peripherals/01_gpio_registers.c
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <assert.h>

typedef struct {
    volatile uint32_t MODER, OTYPER, OSPEEDR, PUPDR;
    volatile uint32_t IDR, ODR, BSRR, LCKR, AFRL, AFRH;
} GPIO_TypeDef;

typedef struct {
    volatile uint32_t AHB1ENR, APB1ENR, APB2ENR;
} RCC_TypeDef;

#define GPIO_MODE_INPUT   0b00u
#define GPIO_MODE_OUTPUT  0b01u
#define GPIO_MODE_AF      0b10u
#define GPIO_MODE_ANALOG  0b11u
#define GPIO_SPEED_HIGH   0b10u
#define GPIO_PULL_NONE    0b00u
#define GPIO_PULL_UP      0b01u

static GPIO_TypeDef _GPIOA={0}, _GPIOB={0};
static RCC_TypeDef  _RCC={0};
GPIO_TypeDef *GPIOA = &_GPIOA;
GPIO_TypeDef *GPIOB = &_GPIOB;
RCC_TypeDef  *RCC   = &_RCC;

/* ============================================================ TASK 1 */

void gpio_clock_enable(RCC_TypeDef *rcc, uint8_t port_index)
{
    rcc->AHB1ENR |= (1u << port_index);
}

void gpio_set_mode(GPIO_TypeDef *gpio, uint8_t pin, uint8_t mode)
{
    gpio->MODER = (gpio->MODER & ~(0b11u << (pin*2))) | ((uint32_t)mode << (pin*2));
}

void gpio_set_speed(GPIO_TypeDef *gpio, uint8_t pin, uint8_t speed)
{
    gpio->OSPEEDR = (gpio->OSPEEDR & ~(0b11u << (pin*2))) | ((uint32_t)speed << (pin*2));
}

void gpio_set_pull(GPIO_TypeDef *gpio, uint8_t pin, uint8_t pull)
{
    gpio->PUPDR = (gpio->PUPDR & ~(0b11u << (pin*2))) | ((uint32_t)pull << (pin*2));
}

void gpio_set_output_type(GPIO_TypeDef *gpio, uint8_t pin, uint8_t otype)
{
    if (otype) gpio->OTYPER |=  (1u << pin);
    else       gpio->OTYPER &= ~(1u << pin);
}

/* ============================================================ TASK 2 */

void gpio_write_pin(GPIO_TypeDef *gpio, uint8_t pin, uint8_t value)
{
    /* BSRR atomic set/clear — no read-modify-write needed */
    if (value)
        gpio->BSRR = (1u << pin);          /* set */
    else
        gpio->BSRR = (1u << (pin + 16u));  /* reset */
}

void gpio_toggle_pin(GPIO_TypeDef *gpio, uint8_t pin)
{
    gpio->ODR ^= (1u << pin);
}

uint8_t gpio_read_pin(GPIO_TypeDef *gpio, uint8_t pin)
{
    return (uint8_t)((gpio->IDR >> pin) & 1u);
}

void gpio_write_port(GPIO_TypeDef *gpio, uint16_t value)
{
    gpio->ODR = value;
}

uint16_t gpio_read_port(GPIO_TypeDef *gpio)
{
    return (uint16_t)(gpio->IDR & 0xFFFFu);
}

/* ============================================================ TASK 3 */

void gpio_set_af(GPIO_TypeDef *gpio, uint8_t pin, uint8_t af_num)
{
    if (pin < 8) {
        uint8_t shift = pin * 4u;
        gpio->AFRL = (gpio->AFRL & ~(0xFu << shift)) | ((uint32_t)af_num << shift);
    } else {
        uint8_t shift = (pin - 8u) * 4u;
        gpio->AFRH = (gpio->AFRH & ~(0xFu << shift)) | ((uint32_t)af_num << shift);
    }
}

/* ============================================================ TASK 4 */

typedef struct { uint8_t mode, otype, speed, pull, af; } GPIO_PinConfig;

void gpio_configure_pin(GPIO_TypeDef *gpio, uint8_t pin, const GPIO_PinConfig *cfg)
{
    gpio_set_mode(gpio, pin, GPIO_MODE_INPUT);   /* temporarily INPUT during reconfig */
    gpio_set_output_type(gpio, pin, cfg->otype);
    gpio_set_speed(gpio, pin, cfg->speed);
    gpio_set_pull(gpio, pin, cfg->pull);
    if (cfg->mode == GPIO_MODE_AF) gpio_set_af(gpio, pin, cfg->af);
    gpio_set_mode(gpio, pin, cfg->mode);         /* set final mode last */
}

/* ============================================================ TASK 5 */

void init_usart2_pins(void)
{
    gpio_clock_enable(RCC, 0);   /* GPIOA = port index 0 */

    GPIO_PinConfig tx_cfg = {GPIO_MODE_AF, 0 /*PP*/, GPIO_SPEED_HIGH, GPIO_PULL_NONE, 7 /*AF7=USART2*/};
    GPIO_PinConfig rx_cfg = {GPIO_MODE_AF, 0 /*PP*/, GPIO_SPEED_HIGH, GPIO_PULL_UP,   7 /*AF7=USART2*/};

    gpio_configure_pin(GPIOA, 2, &tx_cfg);   /* PA2 = USART2_TX */
    gpio_configure_pin(GPIOA, 3, &rx_cfg);   /* PA3 = USART2_RX */
}

/* ============================================================ TASK 6 — Bug hunt FIXED

Bug 1: RCC clock not enabled before accessing GPIO registers.
   On STM32, peripheral clocks are disabled by default to save power.
   Writing to a GPIO register without enabling its clock has no effect
   (or causes a bus fault on some devices).
   FIX: RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN; before any GPIO access.

Bug 2: gpio->MODER |= (GPIO_MODE_AF << (pin*2))
   This ORs the new mode into existing bits WITHOUT clearing first.
   If pin was in AF mode (0b10) and you OR in 0b10 → no change, correct.
   But if pin was in OUTPUT mode (0b01) and you try to set INPUT (0b00):
   0b01 | 0b00 = 0b01 → still output! The clear step is mandatory.
   FIX: gpio->MODER = (gpio->MODER & ~(0b11u<<(pin*2))) | (mode<<(pin*2));

Bug 3: gpio->OTYPER &= ~(1u << pin) sets PUSH-PULL (bit=0).
   I2C SDA must be OPEN-DRAIN (bit=1) — multiple devices share the line.
   In push-pull mode, if one device drives HIGH and another drives LOW
   simultaneously → short circuit → hardware damage.
   FIX: gpio->OTYPER |= (1u << pin);  // set open-drain
*/

/* ============================================================
 * INTERVIEW QUESTION ANSWERS
 * ============================================================

Q1: Why use BSRR instead of ODR? What race condition does BSRR eliminate?

A: ODR requires read-modify-write: read ODR → modify bit → write ODR.
   Between the read and the write, an ISR can fire and also modify ODR
   for a different pin. When main's write completes, it overwrites the ISR's
   change → pin that ISR set gets cleared.
   BSRR: a single 32-bit write atomically sets OR resets one or more pins
   without affecting any other pin. No read needed → no race condition.
   Bits [15:0] = set mask, bits [31:16] = reset mask. One write = atomic.

Q2: PA5 configured as OUTPUT but reading IDR.5 always returns 0.

A: Possible causes:
   (a) External hardware: something is pulling the pin to GND (short circuit).
       If output is driving HIGH but IDR reads 0, the pin is being overdriven.
   (b) MODER configured incorrectly — still in INPUT or ANALOG mode.
       Check MODER[11:10] — should be 0b01 for output.
   (c) Clock not enabled — GPIO peripheral not clocked, register writes ignored.
   (d) ODR bit not set — MODER=output but ODR.5=0 → pin is LOW → IDR.5=0 (correct!).
   Note: in output mode, IDR reflects the actual pin voltage, not ODR.
   An output pin pulled LOW externally will show IDR.5=0 even if ODR.5=1.

Q3: What happens if you forget to enable GPIO peripheral clock?

A: On STM32, all peripheral clocks are gated by default (low power).
   Writing to GPIO registers without enabling the clock:
   - The APB/AHB bus write may complete without error (no bus fault).
   - But the write is discarded — the peripheral is not clocked, registers
     don't latch the values.
   - The GPIO pin stays in default state (input, no pull).
   Silent failure — very common beginner mistake. Always enable clock first.

Q4: Open-drain output on 5V I2C bus from 3.3V MCU.

A: Configure as open-drain output (OTYPER bit = 1).
   In open-drain mode: the MCU can only pull the line LOW (drive 0).
   To go HIGH, the MCU releases the line (drives nothing), and the
   external pull-up resistor pulls to 5V.
   The MCU's input protection diodes handle the 5V HIGH level safely
   (as long as pin is 5V-tolerant — check datasheet!).
   If configured as push-pull and driving HIGH at 3.3V on a 5V bus:
   - Short circuit with other 5V devices that drive LOW.
   - 3.3V HIGH may not meet 5V I2C VOH spec (0.7×VCC = 3.5V).

Q5: How many GPIO pins can you set simultaneously with BSRR? Atomic?

A: All 16 pins of a port simultaneously in a single 32-bit write.
   BSRR[15:0] = set mask (multiple bits = set multiple pins)
   BSRR[31:16] = reset mask
   Example: gpio->BSRR = (1u<<3)|(1u<<7)|(1u<<(5+16)) → set pins 3,7; clear pin 5.
   Yes, this is atomic: a single 32-bit AHB bus write is atomic on Cortex-M.
   The GPIO peripheral latches all bits in the same clock cycle.
*/

int main(void)
{
    GPIOA->MODER = 0;
    GPIOA->ODR   = 0;
    GPIOA->BSRR  = 0;
    GPIOA->PUPDR = 0;
    GPIOA->AFRL  = 0;
    GPIOA->AFRH  = 0;

    gpio_set_mode(GPIOA, 5, GPIO_MODE_OUTPUT);
    assert((GPIOA->MODER & (0b11u << 10)) == (GPIO_MODE_OUTPUT << 10u));

    gpio_write_pin(GPIOA, 5, 1);
    assert(GPIOA->BSRR & (1u << 5));

    gpio_write_pin(GPIOA, 5, 0);
    assert(GPIOA->BSRR & (1u << 21));  /* bit 16+5=21 */

    gpio_set_pull(GPIOA, 3, GPIO_PULL_UP);
    assert((GPIOA->PUPDR & (0b11u << 6)) == (GPIO_PULL_UP << 6u));

    gpio_set_af(GPIOA, 2, 7);
    assert((GPIOA->AFRL & (0xFu << 8)) == (7u << 8));

    gpio_set_af(GPIOA, 9, 7);
    assert((GPIOA->AFRH & (0xFu << 4)) == (7u << 4));

    init_usart2_pins();
    printf("All GPIO answers verified.\n");
    return 0;
}
