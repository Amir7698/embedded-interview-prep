/*
 * ANSWERS: 04_Bare_Metal_Peripherals/02_uart_bare_metal.c
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

typedef struct {
    volatile uint32_t SR, DR, BRR, CR1, CR2, CR3;
} USART_TypeDef;

#define USART_SR_TXE    (1u << 7)
#define USART_SR_TC     (1u << 6)
#define USART_SR_RXNE   (1u << 5)
#define USART_SR_ORE    (1u << 3)
#define USART_SR_FE     (1u << 1)
#define USART_CR1_UE    (1u << 13)
#define USART_CR1_M     (1u << 12)
#define USART_CR1_PCE   (1u << 10)
#define USART_CR1_PS    (1u << 9)
#define USART_CR1_TXEIE (1u << 7)
#define USART_CR1_RXNEIE (1u << 5)
#define USART_CR1_TE    (1u << 3)
#define USART_CR1_RE    (1u << 2)

static USART_TypeDef _USART1={0}, _USART2={0};
USART_TypeDef *USART1=&_USART1, *USART2=&_USART2;

/* ============================================================ TASK 1 */

uint32_t uart_calc_brr(uint32_t fclk_hz, uint32_t baud, uint8_t over8)
{
    uint32_t div = over8 ? (8u * baud) : (16u * baud);
    return (fclk_hz + div/2) / div;   /* round to nearest */
}

/* ============================================================ TASK 2 */

typedef enum { UART_PARITY_NONE=0, UART_PARITY_EVEN, UART_PARITY_ODD } UartParity;
typedef struct {
    uint32_t fclk_hz, baud;
    uint8_t word_len_9bit, stop_bits_2;
    UartParity parity;
    uint8_t rx_irq_enable, tx_irq_enable;
} UartConfig;

void uart_init(USART_TypeDef *usart, const UartConfig *cfg)
{
    usart->CR1 &= ~USART_CR1_UE;           /* Step 1: disable */

    usart->BRR = uart_calc_brr(cfg->fclk_hz, cfg->baud, 0);  /* Step 2: BRR */

    uint32_t cr1 = USART_CR1_TE | USART_CR1_RE;
    if (cfg->word_len_9bit)     cr1 |= USART_CR1_M;
    if (cfg->parity != UART_PARITY_NONE) {
        cr1 |= USART_CR1_PCE;
        if (cfg->parity == UART_PARITY_ODD) cr1 |= USART_CR1_PS;
    }
    if (cfg->rx_irq_enable)     cr1 |= USART_CR1_RXNEIE;
    if (cfg->tx_irq_enable)     cr1 |= USART_CR1_TXEIE;
    usart->CR1 = cr1;                        /* Step 3: CR1 */

    /* Step 4: CR2 stop bits [13:12] */
    if (cfg->stop_bits_2)
        usart->CR2 = (usart->CR2 & ~(0b11u << 12)) | (0b10u << 12);

    usart->CR1 |= USART_CR1_UE;             /* Step 5: enable LAST */
}

/* ============================================================ TASK 3 */

void uart_send_byte_blocking(USART_TypeDef *usart, uint8_t byte)
{
    while (!(usart->SR & USART_SR_TXE)) {}  /* wait TX register empty */
    usart->DR = byte;
}

void uart_send_string_blocking(USART_TypeDef *usart, const char *str)
{
    while (*str) uart_send_byte_blocking(usart, (uint8_t)*str++);
}

void uart_send_buffer_blocking(USART_TypeDef *usart, const uint8_t *buf, uint16_t len)
{
    while (len--) uart_send_byte_blocking(usart, *buf++);
}

/* ============================================================ TASK 4 */

#define TX_BUF_SIZE  256u
#define TX_BUF_MASK  (TX_BUF_SIZE - 1)

typedef struct {
    uint8_t  buf[TX_BUF_SIZE];
    volatile uint8_t head, tail;
} TxRingBuf;
static TxRingBuf g_tx_buf = {0};

int uart_send_async(USART_TypeDef *usart, const uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++) {
        uint8_t next = (g_tx_buf.head + 1u) & TX_BUF_MASK;
        if (next == g_tx_buf.tail) return -1;   /* buffer full */
        g_tx_buf.buf[g_tx_buf.head] = data[i];
        g_tx_buf.head = next;
    }
    usart->CR1 |= USART_CR1_TXEIE;   /* enable TXE interrupt */
    return 0;
}

void USART_TXE_IRQHandler(USART_TypeDef *usart)
{
    if (g_tx_buf.head != g_tx_buf.tail) {
        usart->DR = g_tx_buf.buf[g_tx_buf.tail];
        g_tx_buf.tail = (g_tx_buf.tail + 1u) & TX_BUF_MASK;
    } else {
        usart->CR1 &= ~USART_CR1_TXEIE;   /* no more data — disable interrupt */
    }
}

/* ============================================================ TASK 5 */

typedef enum { UART_RX_OK=0, UART_RX_OVERRUN=1, UART_RX_FRAMING=2 } UartRxStatus;

UartRxStatus uart_receive_byte(USART_TypeDef *usart, uint8_t *out)
{
    if (usart->SR & USART_SR_ORE) {
        (void)usart->SR; (void)usart->DR;   /* clear ORE: read SR then DR */
        return UART_RX_OVERRUN;
    }
    if (usart->SR & USART_SR_FE) {
        (void)usart->SR; (void)usart->DR;
        return UART_RX_FRAMING;
    }
    if (usart->SR & USART_SR_RXNE) {
        *out = (uint8_t)usart->DR;
        return UART_RX_OK;
    }
    *out = 0;
    return UART_RX_OK;
}

/* ============================================================
 * INTERVIEW QUESTION ANSWERS
 * ============================================================

Q1: UART shows garbled characters — first 3 things to check.

A: 1. Baud rate mismatch: even 1% deviation causes framing errors at high baud rates.
      Verify BRR = FCLK / (16 × BAUD). Check oscilloscope: measure actual bit time.
   2. Voltage level mismatch: 3.3V MCU talking to 5V device (or vice versa). Without
      level shifting, logic HIGH threshold may not be met.
   3. UART configuration mismatch: 8N1 vs 8E1, 1 stop bit vs 2. One side using
      even parity while other expects none → every byte looks corrupted.

Q2: TXE vs TC — when to use each.

A: TXE (Transmit Data Register Empty, SR bit 7): fires when the shift register
   has loaded DR and DR is empty again — ready to accept next byte. Use TXE to
   fill DR back-to-back for maximum throughput.
   TC (Transmission Complete, SR bit 6): fires when the LAST bit has been shifted
   out on the TX line AND TXE is set. The wire is idle.
   Use TC for RS-485 DE (direction enable) pin: you must NOT deassert DE until
   TC fires — otherwise you cut off the last bits mid-transmission.

Q3: Why enable TXEIE in send function and disable in ISR when done?

A: The TXEIE (TX Empty Interrupt Enable) fires continuously as long as TXE=1
   (DR is empty). If we enabled it permanently, the ISR would fire thousands of
   times per second even when there's nothing to send — wasting CPU.
   Pattern:
   - uart_send(): push to ring buffer, then |= TXEIE. ISR fires immediately.
   - ISR: if ring buffer has data → write DR. If empty → clear TXEIE. Done.
   This is the "interrupt-driven TX with auto-disable" pattern. Efficient.

Q4: RS-485 with UART. GPIO pin and timing.

A: RS-485 needs a DE (Driver Enable) pin to switch between transmit and receive
   (half-duplex bus). Connect an MCU GPIO to the DE/RE# pins of the RS-485 transceiver.
   Before transmitting: GPIO HIGH (enable transmitter, disable receiver).
   After transmitting: wait for TC (Transmission Complete) flag, then GPIO LOW
   (disable transmitter, enable receiver).
   MUST use TC flag, not TXE — TXE fires when DR is empty but the last byte is
   still in the shift register. Switching DE too early cuts off the last bits.

Q5: Bit time and frame duration at 115200 baud. ISR latency constraint.

A: Bit time = 1 / 115200 = 8.68 µs.
   8N1 frame = 1 start + 8 data + 1 stop = 10 bits = 86.8 µs per byte.
   At 115200 baud, a new byte arrives every 86.8 µs.
   ISR latency constraint: the ISR must read the DR register (clearing RXNE) before
   the NEXT byte starts arriving and overflows the 1-byte hardware buffer.
   Maximum ISR latency: 86.8 µs (one frame time).
   At 168 MHz: 86.8 µs = ~14,600 CPU cycles — very generous. Even a slow ISR is fine.
   At 921600 baud: frame = 10.8 µs → ~1,815 cycles. ISR must be fast.
*/

int main(void)
{
    assert(uart_calc_brr(84000000, 115200, 0) == 45);
    assert(uart_calc_brr(16000000, 9600,   0) == 104);
    assert(uart_calc_brr(72000000, 115200, 0) == 39);

    /* Simulate TXE-driven send */
    USART2->SR = USART_SR_TXE;  /* TX empty */
    uint8_t msg[] = "Hi";
    uart_send_async(USART2, msg, 2);
    USART_TXE_IRQHandler(USART2);
    assert(USART2->DR == 'H');
    USART_TXE_IRQHandler(USART2);
    assert(USART2->DR == 'i');
    USART_TXE_IRQHandler(USART2);
    assert(!(USART2->CR1 & USART_CR1_TXEIE));  /* disabled after buffer empty */

    printf("All UART answers verified.\n");
    return 0;
}
