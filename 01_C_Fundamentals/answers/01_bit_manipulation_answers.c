/*
 * ANSWERS — 01_bit_manipulation.c
 * Do NOT open until you have attempted every TODO.
 */

#include <stdio.h>
#include <stdint.h>
#include <assert.h>

/* ── Task 1 ─────────────────────────────────────────────────── */

void gpio_set_pin(uint16_t *reg, uint8_t pin)   { *reg |=  (uint16_t)(1u << pin); }
void gpio_clear_pin(uint16_t *reg, uint8_t pin) { *reg &= ~(uint16_t)(1u << pin); }
void gpio_toggle_pin(uint16_t *reg, uint8_t pin){ *reg ^=  (uint16_t)(1u << pin); }
uint8_t gpio_read_pin(uint16_t reg, uint8_t pin){ return (reg >> pin) & 1u; }

/* ── Task 2 ─────────────────────────────────────────────────── */

uint8_t can_get_tx_count(uint32_t r)    { return (uint8_t)((r >>  0) & 0xFF); }
uint8_t can_get_rx_count(uint32_t r)    { return (uint8_t)((r >>  8) & 0xFF); }
uint8_t can_get_error_counter(uint32_t r){ return (uint8_t)((r >> 16) & 0xFF); }

uint32_t can_set_tx_count(uint32_t reg, uint8_t count)
{
    return (reg & ~0x000000FFu) | ((uint32_t)count & 0xFF);
}

/* ── Task 3 ─────────────────────────────────────────────────── */

int is_little_endian(void)
{
    uint16_t x = 0x0001;
    return *(uint8_t*)&x == 0x01;   /* 1 if little-endian */
}

uint16_t swap16(uint16_t x)
{
    return (uint16_t)((x >> 8) | (x << 8));
}

uint32_t swap32(uint32_t x)
{
    return ((x & 0x000000FFu) << 24) |
           ((x & 0x0000FF00u) <<  8) |
           ((x & 0x00FF0000u) >>  8) |
           ((x & 0xFF000000u) >> 24);
}

/* ── Task 4 ─────────────────────────────────────────────────── */

uint8_t count_set_bits(uint32_t x)
{
    uint8_t count = 0;
    while (x) { x &= x - 1; count++; }   /* Brian Kernighan */
    return count;
}

int8_t find_highest_set_bit(uint32_t x)
{
    if (x == 0) return -1;
    int8_t pos = 0;
    while (x >>= 1) pos++;
    return pos;
}

uint32_t reverse_bits(uint32_t x)
{
    uint32_t result = 0;
    for (int i = 0; i < 32; i++) {
        result = (result << 1) | (x & 1u);
        x >>= 1;
    }
    return result;
}

/* ── Task 5 ─────────────────────────────────────────────────── */

#define USART_CR1_UE    (1u << 13)
#define USART_CR1_M     (1u << 12)
#define USART_CR1_PCE   (1u << 10)
#define USART_CR1_PS    (1u <<  9)

typedef struct {
    volatile uint32_t CR1, CR2, CR3, BRR, SR, DR;
} USART_TypeDef;

void usart_enable(USART_TypeDef *u)         { u->CR1 |= USART_CR1_UE; }
void usart_configure_8n1(USART_TypeDef *u)  { u->CR1 &= ~(USART_CR1_M | USART_CR1_PCE | USART_CR1_PS); }
void usart_configure_8e1(USART_TypeDef *u)  { u->CR1 = (u->CR1 & ~(USART_CR1_M | USART_CR1_PS)) | USART_CR1_PCE; }
void usart_configure_9o1(USART_TypeDef *u)  { u->CR1 |= USART_CR1_M | USART_CR1_PCE | USART_CR1_PS; }

/* ── Task 6 — Bug Hunt Answers ──────────────────────────────── */
/*
 * Bug 1: reg = reg & mask;
 *        This KEEPS only the field bits — should CLEAR them.
 *        FIX: reg = reg & ~mask;   (complement of mask)
 *
 * Bug 2: uint16_t field = val << shift;
 *        val is uint8_t (8-bit). Shifting left by 12 is UB for 8-bit type.
 *        FIX: uint16_t field = (uint16_t)((uint16_t)val << shift);
 *             (cast val to uint16_t first, THEN shift)
 *
 * Bug 3: return reg | field;
 *        By this point reg still has old field bits because Bug 1 was wrong.
 *        After fixing Bug 1, reg is properly masked and this OR is correct.
 *        BUT: the mask complement logic must precede the OR.
 *
 * Corrected function:
 *   uint16_t set_field(uint16_t reg, uint8_t val) {
 *       uint16_t mask  = 0xF000;
 *       uint8_t  shift = 12;
 *       reg &= ~mask;                              // clear field
 *       return reg | ((uint16_t)((uint16_t)val << shift) & mask);
 *   }
 */

/* ── Interview Question Answers ─────────────────────────────── */
/*
 * Q1: (1<<31) is signed int shift — UB on most platforms.
 *     (1u<<31) is unsigned — defined behaviour, gives 0x80000000.
 *
 * Q2: On Cortex-M, use LDREX/STREX (exclusive access) or disable interrupts
 *     with __disable_irq()/__enable_irq() around the read-modify-write.
 *     Many STM32 peripherals also have BSRR register: atomic set/clear
 *     in one write — no read-modify-write needed.
 *
 * Q3: reg &= ~0x0Fu;   (clears bits 3:0, preserves all others)
 *
 * Q4: Bit-band maps each bit to a word-aligned address, making 32-bit
 *     write = single-bit toggle. Eliminates read-modify-write, so it's
 *     inherently atomic from ISR perspective. Deprecated on Cortex-M7+.
 *
 * Q5: Precompute 256 entries (one per byte value) at compile time with a
 *     constexpr/const table, then XOR table[byte] for each input byte.
 *     Total ROM: 256 × 2 bytes = 512 bytes for CRC-16.
 */
