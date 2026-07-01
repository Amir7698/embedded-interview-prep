/*
 * ============================================================
 * EMBEDDED INTERVIEW PREP
 * Topic : UART — Bare-Metal Implementation
 * File  : 04_Bare_Metal_Peripherals/02_uart_bare_metal.c
 * ============================================================
 *
 * UART is asked in EVERY embedded interview.
 * Know the registers, the framing, the baud rate formula,
 * and how to write a non-blocking driver.
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* ============================================================
 * THEORY — UART Fundamentals
 * ============================================================
 *
 * UART frame (8N1):
 *   [IDLE][START][D0][D1][D2][D3][D4][D5][D6][D7][STOP][IDLE]
 *   - IDLE = line high (logic 1)
 *   - START bit = logic 0 (1 bit)
 *   - 8 data bits, LSB first
 *   - STOP bit = logic 1 (1 or 2 bits)
 *   - No parity (8N1), or even/odd parity (8E1/8O1)
 *
 * Baud rate formula (STM32 USART):
 *   BRR = FCLK / (16 * BAUD)           — oversampling by 16 (default)
 *   BRR = FCLK / (8  * BAUD)           — oversampling by 8 (OVER8 bit set)
 *
 * Example: FCLK = 84 MHz, BAUD = 115200
 *   BRR = 84_000_000 / (16 * 115200) = 45.57 → 0x002D.09 in STM32 format
 *
 * STM32 USART key registers:
 *   SR   (0x00): Status  — TXE(bit7), TC(bit6), RXNE(bit5), ORE(bit3)
 *   DR   (0x04): Data    — read=receive, write=transmit
 *   BRR  (0x08): Baud Rate Register
 *   CR1  (0x0C): Control1 — UE(bit13), M(bit12), PCE(bit10), PS(bit9),
 *                            TXEIE(bit7), TCIE(bit6), RXNEIE(bit5),
 *                            TE(bit3), RE(bit2)
 *   CR2  (0x10): Control2 — STOP bits [13:12]: 00=1bit, 10=2bits
 *   CR3  (0x14): Control3 — DMAT(bit7), DMAR(bit6), HDSEL(bit3)
 * ============================================================ */

/* Simulated USART registers */
typedef struct {
    volatile uint32_t SR;
    volatile uint32_t DR;
    volatile uint32_t BRR;
    volatile uint32_t CR1;
    volatile uint32_t CR2;
    volatile uint32_t CR3;
} USART_TypeDef;

/* SR bits */
#define USART_SR_TXE    (1u << 7)   /* TX register empty — ready to write */
#define USART_SR_TC     (1u << 6)   /* Transmission complete */
#define USART_SR_RXNE   (1u << 5)   /* RX not empty — data available */
#define USART_SR_ORE    (1u << 3)   /* Overrun error */
#define USART_SR_FE     (1u << 1)   /* Framing error */

/* CR1 bits */
#define USART_CR1_UE      (1u << 13)
#define USART_CR1_M       (1u << 12)
#define USART_CR1_PCE     (1u << 10)
#define USART_CR1_PS      (1u << 9)
#define USART_CR1_TXEIE   (1u << 7)
#define USART_CR1_TCIE    (1u << 6)
#define USART_CR1_RXNEIE  (1u << 5)
#define USART_CR1_TE      (1u << 3)
#define USART_CR1_RE      (1u << 2)

static USART_TypeDef _USART1 = {0};
static USART_TypeDef _USART2 = {0};
USART_TypeDef *USART1 = &_USART1;
USART_TypeDef *USART2 = &_USART2;

/* ============================================================
 * TASK 1 — Baud rate calculation
 * ============================================================ */

uint32_t uart_calc_brr(uint32_t fclk_hz, uint32_t baud, uint8_t over8)
{
    /* TODO: if over8 == 0: BRR = fclk_hz / (16 * baud)
     *       if over8 == 1: BRR = fclk_hz / (8  * baud)
     * Round to nearest integer.
     * Note: STM32 BRR stores mantissa [15:4] and fraction [3:0]
     *       but for this exercise just return the rounded integer. */
    (void)fclk_hz; (void)baud; (void)over8;
    return 0;
}

/* ============================================================
 * TASK 2 — USART peripheral initialization
 * ============================================================ */

typedef enum {
    UART_PARITY_NONE = 0,
    UART_PARITY_EVEN,
    UART_PARITY_ODD
} UartParity;

typedef struct {
    uint32_t    fclk_hz;
    uint32_t    baud;
    uint8_t     word_len_9bit;   /* 0=8bit, 1=9bit */
    uint8_t     stop_bits_2;     /* 0=1 stop bit, 1=2 stop bits */
    UartParity  parity;
    uint8_t     rx_irq_enable;
    uint8_t     tx_irq_enable;
} UartConfig;

void uart_init(USART_TypeDef *usart, const UartConfig *cfg)
{
    /* TODO: Step 1 — disable USART (clear UE) before config */
    /* TODO: Step 2 — set BRR */
    /* TODO: Step 3 — configure CR1:
     *   word length (M bit), parity (PCE, PS), TX/RX enable (TE, RE)
     *   RXNEIE if cfg->rx_irq_enable
     *   TXEIE  if cfg->tx_irq_enable */
    /* TODO: Step 4 — configure CR2 stop bits [13:12] */
    /* TODO: Step 5 — enable USART (set UE) */
    (void)usart; (void)cfg;
}

/* ============================================================
 * TASK 3 — Blocking transmit (polling)
 *
 * Used for debug output. Fine for startup messages.
 * DO NOT use in production ISR-driven code.
 * ============================================================ */

void uart_send_byte_blocking(USART_TypeDef *usart, uint8_t byte)
{
    /* TODO: wait until TXE is set (SR bit 7)
     * write byte to usart->DR */
    (void)usart; (void)byte;
}

void uart_send_string_blocking(USART_TypeDef *usart, const char *str)
{
    /* TODO: send each character until '\0' */
    (void)usart; (void)str;
}

void uart_send_buffer_blocking(USART_TypeDef *usart, const uint8_t *buf, uint16_t len)
{
    /* TODO: send len bytes */
    (void)usart; (void)buf; (void)len;
}

/* ============================================================
 * TASK 4 — Non-blocking transmit using TX-empty interrupt
 *
 * The driver maintains a TX ring buffer.
 * uart_send_async() puts data into the ring buffer and enables TXEIE.
 * USART_TXE_IRQHandler() sends one byte per interrupt; when buffer
 * is empty, it disables TXEIE.
 * ============================================================ */

#define TX_BUF_SIZE  256u
#define TX_BUF_MASK  (TX_BUF_SIZE - 1)

typedef struct {
    uint8_t  buf[TX_BUF_SIZE];
    volatile uint8_t head;   /* written by uart_send_async */
    volatile uint8_t tail;   /* written by ISR */
} TxRingBuf;

static TxRingBuf g_tx_buf = {0};

int uart_send_async(USART_TypeDef *usart, const uint8_t *data, uint16_t len)
{
    /* TODO: push each byte into g_tx_buf ring buffer
     * if buffer full: return -1 (drop data)
     * after pushing: enable TXEIE so ISR fires
     * return 0 on success */
    (void)usart; (void)data; (void)len;
    return -1;
}

void USART_TXE_IRQHandler(USART_TypeDef *usart)
{
    /* Called when USART_SR_TXE is set (DR is empty)
     * TODO: if g_tx_buf has data (head != tail):
     *         write g_tx_buf.buf[tail] to usart->DR
     *         advance tail
     *       else:
     *         disable TXEIE (clear USART_CR1_TXEIE)
     *         no more data to send */
    (void)usart;
}

/* ============================================================
 * TASK 5 — Receive with error handling
 * ============================================================ */

typedef enum {
    UART_RX_OK      = 0,
    UART_RX_OVERRUN = 1,
    UART_RX_FRAMING = 2
} UartRxStatus;

UartRxStatus uart_receive_byte(USART_TypeDef *usart, uint8_t *out)
{
    /* TODO: if ORE set: clear it (read SR then read DR), return UART_RX_OVERRUN
     * if FE  set: clear it (read SR then read DR), return UART_RX_FRAMING
     * if RXNE set: *out = (uint8_t)usart->DR, return UART_RX_OK
     * else: *out = 0, return UART_RX_OK (no data — caller should check RXNE first) */
    (void)usart; (void)out;
    return UART_RX_OK;
}

/* ============================================================
 * TASK 6 — BUG HUNT: UART initialization mistakes
 *
 * The code below initializes USART1 at 9600 baud.
 * It has 4 bugs. Find and mark each one.
 * ============================================================ */

void uart_init_BUGGY(USART_TypeDef *usart, uint32_t fclk_hz, uint32_t baud)
{
    /* Bug 1: USART is not disabled before configuration */
    /* Missing: usart->CR1 &= ~USART_CR1_UE; */

    /* Bug 2: BRR formula is wrong (uses 8x instead of 16x for no reason) */
    usart->BRR = fclk_hz / (8 * baud);   /* should be 16 * baud (default oversampling) */

    /* Bug 3: TE and RE bits not set — transmitter and receiver not enabled */
    usart->CR1 = USART_CR1_UE;   /* missing USART_CR1_TE | USART_CR1_RE */

    /* Bug 4: Order wrong — UE set before BRR is configured.
     * UE should be set LAST, after all register config is done. */
}

/* ============================================================
 * SELF-TEST
 * ============================================================ */

static void test_uart(void)
{
    assert(uart_calc_brr(84000000, 115200, 0) == 45);   /* 84M / (16*115200) ≈ 45 */
    assert(uart_calc_brr(16000000, 9600,   0) == 104);  /* 16M / (16*9600) = 104.17 ≈ 104 */

    printf("All UART tests PASSED.\n");
}

int main(void)
{
    test_uart();
    return 0;
}

/* ============================================================
 * INTERVIEW QUESTIONS
 * ============================================================
 *
 * Q1: UART shows garbled characters. First 3 things to check?
 *     Answer: TODO
 *
 * Q2: What is the difference between TXE and TC flags?
 *     When should you use each one?
 *     Answer: TODO
 *
 * Q3: In an ISR-driven UART driver, why do you enable TXEIE
 *     in the send function and disable it in the ISR when done?
 *     Answer: TODO
 *
 * Q4: How do you implement RS-485 with a UART peripheral?
 *     What GPIO pin is needed and how/when is it toggled?
 *     Answer: TODO
 *
 * Q5: Calculate the bit time and frame duration (8N1) at 115200 baud.
 *     How does this constrain your ISR latency requirements?
 *     Answer: TODO
 */
