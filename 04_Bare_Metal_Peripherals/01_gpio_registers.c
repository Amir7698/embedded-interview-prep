/*
 * ============================================================
 * EMBEDDED INTERVIEW PREP
 * Topic : GPIO — Register-Level Programming (STM32 style)
 * File  : 04_Bare_Metal_Peripherals/01_gpio_registers.c
 * ============================================================
 *
 * "No HAL, no CubeMX. Show me the registers."
 * This is what separates junior from senior embedded engineers.
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <assert.h>

/* ============================================================
 * THEORY — STM32 GPIO Register Map
 * ============================================================
 *
 * Each GPIO port has 10 registers (10×4 bytes = 40 bytes).
 *
 * MODER   (0x00): Mode — 2 bits per pin
 *          00 = Input, 01 = Output, 10 = Alternate Function, 11 = Analog
 *
 * OTYPER  (0x04): Output Type — 1 bit per pin
 *          0 = Push-pull, 1 = Open-drain
 *
 * OSPEEDR (0x08): Output Speed — 2 bits per pin
 *          00=Low, 01=Medium, 10=High, 11=Very High
 *
 * PUPDR   (0x0C): Pull-Up/Pull-Down — 2 bits per pin
 *          00=None, 01=Pull-up, 10=Pull-down
 *
 * IDR     (0x10): Input Data Register (read-only, 1 bit per pin)
 * ODR     (0x14): Output Data Register (1 bit per pin)
 *
 * BSRR    (0x18): Bit Set/Reset Register — ATOMIC operation
 *          Bits [15:0]  = SET   (write 1 to set pin, 0 = no effect)
 *          Bits [31:16] = RESET (write 1 to clear pin, 0 = no effect)
 *
 * AFRL    (0x20): Alternate Function Low  (pins 0-7,  4 bits each)
 * AFRH    (0x24): Alternate Function High (pins 8-15, 4 bits each)
 *
 * Clock enable: RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
 * MUST be done BEFORE accessing any GPIO register.
 * ============================================================ */


/* Simulated GPIO and RCC registers for host-side practice */
typedef struct {
    volatile uint32_t MODER;
    volatile uint32_t OTYPER;
    volatile uint32_t OSPEEDR;
    volatile uint32_t PUPDR;
    volatile uint32_t IDR;
    volatile uint32_t ODR;
    volatile uint32_t BSRR;
    volatile uint32_t LCKR;
    volatile uint32_t AFRL;
    volatile uint32_t AFRH;
} GPIO_TypeDef;

typedef struct {
    volatile uint32_t AHB1ENR;
    volatile uint32_t APB1ENR;
    volatile uint32_t APB2ENR;
} RCC_TypeDef;

/* Bit definitions */
#define RCC_AHB1ENR_GPIOAEN  (1u << 0)
#define RCC_AHB1ENR_GPIOBEN  (1u << 1)
#define RCC_AHB1ENR_GPIOCEN  (1u << 2)

/* GPIO mode constants */
#define GPIO_MODE_INPUT    0b00u
#define GPIO_MODE_OUTPUT   0b01u
#define GPIO_MODE_AF       0b10u
#define GPIO_MODE_ANALOG   0b11u

/* GPIO speed constants */
#define GPIO_SPEED_LOW     0b00u
#define GPIO_SPEED_MEDIUM  0b01u
#define GPIO_SPEED_HIGH    0b10u
#define GPIO_SPEED_VHIGH   0b11u

/* GPIO pull constants */
#define GPIO_PULL_NONE     0b00u
#define GPIO_PULL_UP       0b01u
#define GPIO_PULL_DOWN     0b10u

/* Simulated instances */
static GPIO_TypeDef  _GPIOA = {0};
static GPIO_TypeDef  _GPIOB = {0};
static RCC_TypeDef   _RCC   = {0};
GPIO_TypeDef *GPIOA = &_GPIOA;
GPIO_TypeDef *GPIOB = &_GPIOB;
RCC_TypeDef  *RCC   = &_RCC;


/* ============================================================
 * TASK 1 — GPIO enable and mode configuration
 * ============================================================ */

void gpio_clock_enable(RCC_TypeDef *rcc, uint8_t port_index)
{
    /* TODO: set the bit for port_index in rcc->AHB1ENR
     * port_index: 0=GPIOA, 1=GPIOB, 2=GPIOC, etc.
     * Bit position = port_index */
    (void)rcc; (void)port_index;
}

void gpio_set_mode(GPIO_TypeDef *gpio, uint8_t pin, uint8_t mode)
{
    /* TODO: MODER has 2 bits per pin at bit position (pin * 2)
     * 1. Clear: gpio->MODER &= ~(0b11u << (pin * 2))
     * 2. Set:   gpio->MODER |=  (mode  << (pin * 2)) */
    (void)gpio; (void)pin; (void)mode;
}

void gpio_set_speed(GPIO_TypeDef *gpio, uint8_t pin, uint8_t speed)
{
    /* TODO: OSPEEDR has 2 bits per pin — same pattern as MODER */
    (void)gpio; (void)pin; (void)speed;
}

void gpio_set_pull(GPIO_TypeDef *gpio, uint8_t pin, uint8_t pull)
{
    /* TODO: PUPDR has 2 bits per pin — same pattern */
    (void)gpio; (void)pin; (void)pull;
}

void gpio_set_output_type(GPIO_TypeDef *gpio, uint8_t pin, uint8_t otype)
{
    /* TODO: OTYPER has 1 bit per pin at position 'pin'
     * otype: 0=push-pull, 1=open-drain */
    (void)gpio; (void)pin; (void)otype;
}

/* ============================================================
 * TASK 2 — GPIO read and write
 * ============================================================ */

void gpio_write_pin(GPIO_TypeDef *gpio, uint8_t pin, uint8_t value)
{
    /* TODO: use BSRR for atomic set/clear — NO read-modify-write
     * if value == 1: gpio->BSRR = (1u << pin)
     * if value == 0: gpio->BSRR = (1u << (pin + 16)) */
    (void)gpio; (void)pin; (void)value;
}

void gpio_toggle_pin(GPIO_TypeDef *gpio, uint8_t pin)
{
    /* TODO: toggle ODR bit — read-modify-write is acceptable here
     * because in this simulated env there's no ISR.
     * On real HW: use BSRR with read of ODR for atomic toggle:
     *   if (ODR & (1<<pin)) BSRR = 1<<(pin+16); else BSRR = 1<<pin; */
    (void)gpio; (void)pin;
}

uint8_t gpio_read_pin(GPIO_TypeDef *gpio, uint8_t pin)
{
    /* TODO: return bit at position 'pin' of IDR */
    (void)gpio; (void)pin;
    return 0;
}

void gpio_write_port(GPIO_TypeDef *gpio, uint16_t value)
{
    /* TODO: write all 16 pins at once via ODR */
    (void)gpio; (void)value;
}

uint16_t gpio_read_port(GPIO_TypeDef *gpio)
{
    /* TODO: read all 16 pins from IDR */
    (void)gpio;
    return 0;
}

/* ============================================================
 * TASK 3 — Alternate Function configuration
 *
 * Each pin can be routed to a peripheral (UART, SPI, I2C, etc.)
 * by selecting an Alternate Function number (AF0–AF15).
 *
 * AFRL: pins 0-7,  4 bits each, starting at bit (pin * 4)
 * AFRH: pins 8-15, 4 bits each, starting at bit ((pin-8) * 4)
 * ============================================================ */

void gpio_set_af(GPIO_TypeDef *gpio, uint8_t pin, uint8_t af_num)
{
    /* TODO: if pin < 8: modify AFRL
     *       else:       modify AFRH (use pin - 8 as bit offset)
     * Each AF field is 4 bits wide */
    (void)gpio; (void)pin; (void)af_num;
}

/* ============================================================
 * TASK 4 — Full pin configuration helper
 * Configure a pin in one call — common pattern in real code.
 * ============================================================ */

typedef struct {
    uint8_t mode;
    uint8_t otype;
    uint8_t speed;
    uint8_t pull;
    uint8_t af;
} GPIO_PinConfig;

void gpio_configure_pin(GPIO_TypeDef *gpio, uint8_t pin, const GPIO_PinConfig *cfg)
{
    /* TODO: call all setters in this order:
     * 1. gpio_set_mode  (do first — set to INPUT before changing others)
     * 2. gpio_set_output_type
     * 3. gpio_set_speed
     * 4. gpio_set_pull
     * 5. if mode == GPIO_MODE_AF: gpio_set_af */
    (void)gpio; (void)pin; (void)cfg;
}

/* ============================================================
 * TASK 5 — Real-world: configure USART2 pins on STM32F4
 *
 * USART2:
 *   PA2 = TX (AF7)
 *   PA3 = RX (AF7)
 *   Both: AF mode, high speed, push-pull, no pull
 * ============================================================ */

void init_usart2_pins(void)
{
    /* TODO: enable GPIOA clock */
    /* TODO: configure PA2 as AF7, output, high speed, push-pull, no pull */
    /* TODO: configure PA3 as AF7, output, high speed, push-pull, pull-up (for RX) */
}

/* ============================================================
 * TASK 6 — BUG HUNT: GPIO configuration mistakes
 *
 * The function below tries to configure an I2C SDA pin (open-drain).
 * It has 3 bugs. Find and mark each one.
 * ============================================================ */

void configure_i2c_sda_BUGGY(GPIO_TypeDef *gpio, uint8_t pin)
{
    /* Bug 1: ??? */
    /* RCC clock not enabled before accessing GPIO registers */

    /* Bug 2: ??? */
    gpio->MODER |= (GPIO_MODE_AF << (pin * 2));   /* should CLEAR first, then set */

    /* Bug 3: ??? */
    gpio->OTYPER &= ~(1u << pin);   /* sets push-pull — I2C SDA MUST be open-drain */
                                     /* should be: gpio->OTYPER |= (1u << pin) */

    gpio_set_af(gpio, pin, 4);   /* AF4 = I2C on STM32F4 — this is correct */
    gpio_set_pull(gpio, pin, GPIO_PULL_UP);   /* correct — I2C needs pull-up */
}

/* ============================================================
 * SELF-TEST
 * ============================================================ */

static void test_gpio(void)
{
    /* Reset */
    GPIOA->MODER = 0;
    GPIOA->ODR   = 0;
    GPIOA->BSRR  = 0;

    /* Test mode configuration */
    gpio_set_mode(GPIOA, 5, GPIO_MODE_OUTPUT);
    assert((GPIOA->MODER & (0b11u << 10)) == (GPIO_MODE_OUTPUT << 10));

    /* Test atomic set via BSRR */
    gpio_write_pin(GPIOA, 5, 1);
    /* BSRR bit 5 should be set */
    assert(GPIOA->BSRR & (1u << 5));

    /* Test pull configuration */
    gpio_set_pull(GPIOA, 3, GPIO_PULL_UP);
    assert((GPIOA->PUPDR & (0b11u << 6)) == (GPIO_PULL_UP << 6));

    printf("All GPIO tests PASSED.\n");
}

int main(void)
{
    test_gpio();
    return 0;
}

/* ============================================================
 * INTERVIEW QUESTIONS
 * ============================================================
 *
 * Q1: Why use BSRR instead of ODR for GPIO output? What race condition
 *     does BSRR eliminate?
 *     Answer: TODO
 *
 * Q2: You configure PA5 as OUTPUT but reading IDR.5 always returns 0.
 *     What might be wrong?
 *     Answer: TODO
 *
 * Q3: What happens if you forget to enable the GPIO peripheral clock
 *     before writing to its registers?
 *     Answer: TODO
 *
 * Q4: You need an open-drain output on a 5V I2C bus from a 3.3V MCU.
 *     How do you configure it and why?
 *     Answer: TODO
 *
 * Q5: How many GPIO pins can you set simultaneously on STM32
 *     using BSRR? Is this operation truly atomic?
 *     Answer: TODO
 */
