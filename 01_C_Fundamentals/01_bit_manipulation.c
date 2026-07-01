/*
 * ============================================================
 * EMBEDDED INTERVIEW PREP â€” github.com/Amir7698/embedded-interview-prep
 * Topic : Bit Manipulation
 * File  : 01_C_Fundamentals/01_bit_manipulation.c
 * Level : Beginner â†’ Advanced
 * ============================================================
 *
 * Every embedded interview touches bit manipulation.
 * Master these and you will never blank on a whiteboard.
 *
 * HOW TO USE:
 *   1. Read the theory block for each task.
 *   2. Implement every TODO without looking at answers/.
 *   3. Compile:  gcc -Wall -Wextra -o bit_manip 01_bit_manipulation.c
 *   4. Compare with answers/01_bit_manipulation_answers.c
 * ============================================================
 */

#include <stdio.h>
#include <stdint.h>
#include <assert.h>

/* ============================================================
 * THEORY â€” The 6 operations you must know cold
 * ============================================================
 *
 *  SET bit N   :  reg |=  (1u << N)
 *  CLEAR bit N :  reg &= ~(1u << N)
 *  TOGGLE bit N:  reg ^=  (1u << N)
 *  READ bit N  :  (reg >> N) & 1u
 *  MASK        :  reg & 0xFF          (keep lower 8 bits)
 *  FIELD write :  reg = (reg & ~MASK) | ((val << SHIFT) & MASK)
 *
 * Why 1u not 1?
 *   1 is a signed int. On a 32-bit machine (1 << 31) is UB.
 *   1u is unsigned â€” safe for all shifts within bit-width.
 *   Use UINT32_C(1) or (uint32_t)1 for register-level code.
 * ============================================================ */


/* ============================================================
 * TASK 1 â€” Basic register operations
 * A GPIO output data register (ODR) controls 16 LEDs.
 * Implement the four primitives below.
 * ============================================================ */

void gpio_set_pin(uint16_t *reg, uint8_t pin)
{
    /* TODO: set bit 'pin' in *reg */
    (void)reg; (void)pin;
}

void gpio_clear_pin(uint16_t *reg, uint8_t pin)
{
    /* TODO: clear bit 'pin' in *reg */
    (void)reg; (void)pin;
}

void gpio_toggle_pin(uint16_t *reg, uint8_t pin)
{
    /* TODO: toggle bit 'pin' in *reg */
    (void)reg; (void)pin;
}

uint8_t gpio_read_pin(uint16_t reg, uint8_t pin)
{
    /* TODO: return 1 if bit 'pin' is set, 0 otherwise */
    (void)reg; (void)pin;
    return 0;
}

/* ============================================================
 * TASK 2 â€” Bit field extraction and insertion
 *
 * A CAN status register layout (32-bit):
 *   [31:24] = reserved
 *   [23:16] = error counter (8-bit)
 *   [15: 8] = rx message count (8-bit)
 *   [ 7: 0] = tx message count (8-bit)
 * ============================================================ */

#define CAN_TX_SHIFT   0
#define CAN_TX_MASK    0x000000FFu
#define CAN_RX_SHIFT   8
#define CAN_RX_MASK    0x0000FF00u
#define CAN_ERR_SHIFT  16
#define CAN_ERR_MASK   0x00FF0000u

uint8_t can_get_tx_count(uint32_t status_reg)
{
    /* TODO: extract bits [7:0] */
    (void)status_reg;
    return 0;
}

uint8_t can_get_rx_count(uint32_t status_reg)
{
    /* TODO: extract bits [15:8] */
    (void)status_reg;
    return 0;
}

uint8_t can_get_error_counter(uint32_t status_reg)
{
    /* TODO: extract bits [23:16] */
    (void)status_reg;
    return 0;
}

uint32_t can_set_tx_count(uint32_t status_reg, uint8_t count)
{
    /* TODO: write 'count' into bits [7:0], preserve all other bits */
    (void)count;
    return status_reg;
}

/* ============================================================
 * TASK 3 â€” Endianness detection and byte swap
 *
 * Endianness:
 *   Little-endian: LSB at lowest address. (x86, ARM default)
 *   Big-endian   : MSB at lowest address. (network byte order)
 *
 * CAN, Ethernet, SOME/IP headers are big-endian.
 * Most MCUs are little-endian.
 * You WILL need to swap bytes when parsing protocol frames.
 * ============================================================ */

int is_little_endian(void)
{
    /* TODO: return 1 if this machine is little-endian, 0 if big-endian
     * Hint: cast a uint16_t 0x0001 address to uint8_t* and check first byte */
    return -1;
}

uint16_t swap16(uint16_t x)
{
    /* TODO: swap byte order of a 16-bit value
     * 0xAABB â†’ 0xBBAA
     * Hint: use shifts and OR, no stdlib */
    (void)x;
    return 0;
}

uint32_t swap32(uint32_t x)
{
    /* TODO: swap byte order of a 32-bit value
     * 0xAABBCCDD â†’ 0xDDCCBBAA */
    (void)x;
    return 0;
}

/* ============================================================
 * TASK 4 â€” Counting and finding bits
 * These appear in CRC, checksum, and protocol code.
 * ============================================================ */

uint8_t count_set_bits(uint32_t x)
{
    /* TODO: return the number of 1-bits in x (popcount)
     * Implement WITHOUT using __builtin_popcount
     * Classic: Brian Kernighan's algorithm: while(x) { x &= x-1; count++; } */
    (void)x;
    return 0;
}

int8_t find_highest_set_bit(uint32_t x)
{
    /* TODO: return the position (0-indexed) of the highest set bit
     * Return -1 if x == 0
     * Example: x=0b1010 â†’ returns 3
     * Implement WITHOUT __builtin_clz */
    (void)x;
    return -1;
}

uint32_t reverse_bits(uint32_t x)
{
    /* TODO: reverse all 32 bits of x
     * 0b10110000_00000000_00000000_00000000 â†’
     * 0b00000000_00000000_00000000_00001101
     * Used in CRC reflected algorithms */
    (void)x;
    return 0;
}

/* ============================================================
 * TASK 5 â€” Packed register field (realistic MCU scenario)
 *
 * USART Control Register (32-bit), STM32-style:
 *   Bit  13 : UE   â€” USART enable
 *   Bit  12 : M    â€” word length (0=8bit, 1=9bit)
 *   Bit  10 : PCE  â€” parity control enable
 *   Bit   9 : PS   â€” parity selection (0=even, 1=odd)
 *   Bits 2:0 : reserved (must be 0)
 * ============================================================ */

#define USART_CR1_UE    (1u << 13)
#define USART_CR1_M     (1u << 12)
#define USART_CR1_PCE   (1u << 10)
#define USART_CR1_PS    (1u <<  9)

typedef struct {
    volatile uint32_t CR1;
    volatile uint32_t CR2;
    volatile uint32_t CR3;
    volatile uint32_t BRR;
    volatile uint32_t SR;
    volatile uint32_t DR;
} USART_TypeDef;

void usart_enable(USART_TypeDef *usart)
{
    /* TODO: set UE bit in CR1 */
    (void)usart;
}

void usart_configure_8n1(USART_TypeDef *usart)
{
    /* TODO: 8-bit word (M=0), no parity (PCE=0)
     * Clear M, PCE, PS bits; UE must NOT be changed here */
    (void)usart;
}

void usart_configure_8e1(USART_TypeDef *usart)
{
    /* TODO: 8-bit word, even parity (PCE=1, PS=0) */
    (void)usart;
}

void usart_configure_9o1(USART_TypeDef *usart)
{
    /* TODO: 9-bit word, odd parity (M=1, PCE=1, PS=1) */
    (void)usart;
}

/* ============================================================
 * TASK 6 â€” BUG HUNT
 *
 * The function below should set bits 15:12 of a 16-bit register
 * to the value 'val' (0â€“15), preserving all other bits.
 * It has 3 bugs. Find and mark each one.
 * ============================================================ */

uint16_t set_field_BUGGY(uint16_t reg, uint8_t val)
{
    uint16_t mask  = 0xF000;
    uint8_t  shift = 12;

    /* Bug 1: ??? */
    reg = reg & mask;               /* should clear the field, not keep it */

    /* Bug 2: ??? */
    uint16_t field = val << shift;  /* val is uint8_t â€” shifting 12 causes UB
                                     * because 8-bit type can't hold bit 12 */

    /* Bug 3: ??? */
    return reg | field;             /* mask should be ~mask to clear field first */
}

/* ============================================================
 * SELF-TEST â€” run to check your implementations
 * ============================================================ */

static void test_bit_manipulation(void)
{
    /* Task 1 */
    uint16_t reg = 0x0000;
    gpio_set_pin(&reg, 3);
    assert(reg == 0x0008);
    gpio_set_pin(&reg, 7);
    assert(reg == 0x0088);
    gpio_clear_pin(&reg, 3);
    assert(reg == 0x0080);
    gpio_toggle_pin(&reg, 0);
    assert(reg == 0x0081);
    assert(gpio_read_pin(reg, 0) == 1);
    assert(gpio_read_pin(reg, 1) == 0);

    /* Task 2 */
    uint32_t can_status = 0x00AB1234u;
    assert(can_get_tx_count(can_status)    == 0x34);
    assert(can_get_rx_count(can_status)    == 0x12);
    assert(can_get_error_counter(can_status) == 0xAB);
    uint32_t updated = can_set_tx_count(can_status, 0xFF);
    assert((updated & 0xFF) == 0xFF);
    assert((updated >> 8) == (can_status >> 8));

    /* Task 3 */
    assert(swap16(0xAABB) == 0xBBAA);
    assert(swap32(0xAABBCCDD) == 0xDDCCBBAA);

    /* Task 4 */
    assert(count_set_bits(0b10110101) == 5);
    assert(count_set_bits(0) == 0);
    assert(count_set_bits(0xFFFFFFFF) == 32);
    assert(find_highest_set_bit(0b1010) == 3);
    assert(find_highest_set_bit(0)      == -1);
    assert(find_highest_set_bit(1)      ==  0);

    printf("All bit manipulation tests PASSED.\n");
}

int main(void)
{
    test_bit_manipulation();
    return 0;
}

/* ============================================================
 * INTERVIEW QUESTIONS â€” answer these out loud, then check answers/
 * ============================================================
 *
 * Q1: What is the difference between (1 << 31) and (1u << 31)?
 *     Answer: TODO
 *
 * Q2: How do you atomically set a bit on a bare-metal MCU without
 *     corrupting other bits if an ISR might preempt?
 *     Answer: TODO
 *
 * Q3: A register requires you to write 0 to bits [3:0] and keep all
 *     other bits unchanged. Write the one-liner.
 *     Answer: TODO
 *
 * Q4: What is a bit-band region on ARM Cortex-M and why was it useful?
 *     Answer: TODO
 *
 * Q5: How would you implement a 16-bit CRC table in 256 bytes of ROM?
 *     Answer: TODO
 */
