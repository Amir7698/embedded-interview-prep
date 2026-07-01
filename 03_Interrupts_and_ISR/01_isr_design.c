/*
 * ============================================================
 * EMBEDDED INTERVIEW PREP
 * Topic : Interrupt Design — Rules, Patterns, Pitfalls
 * File  : 03_Interrupts_and_ISR/01_isr_design.c
 * ============================================================
 *
 * ISRs (Interrupt Service Routines) are THE most interview-tested
 * topic in embedded firmware. Get these rules in your head cold.
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* ============================================================
 * THEORY — The Golden Rules of ISR Design
 * ============================================================
 *
 * RULE 1: ISRs must be SHORT.
 *   Long ISRs block other interrupts. Rule of thumb: < 1µs on a
 *   200MHz MCU. Do the minimum: set a flag, push to a queue, toggle a pin.
 *   Heavy processing belongs in the main loop or an RTOS task.
 *
 * RULE 2: No blocking inside an ISR.
 *   No printf(), no malloc(), no delay_ms(), no mutex lock,
 *   no I2C/SPI transaction that waits for completion.
 *
 * RULE 3: Variables shared with ISRs must be volatile.
 *   Without volatile, the compiler caches in a register and
 *   main() never sees the ISR's write.
 *
 * RULE 4: Protect shared multi-byte data.
 *   On 32-bit ARM, uint32_t reads are atomic. uint64_t are NOT.
 *   Structs are NOT atomic. Use critical sections or double-buffering.
 *
 * RULE 5: Clear the interrupt flag FIRST in the ISR.
 *   (On most MCUs) — before doing any processing. This prevents
 *   missing a second event that arrives while you're processing the first.
 *   Exception: some peripherals require reading data register to clear flag.
 *
 * RULE 6: No floating-point in ISR without saving FPU context.
 *   On ARM Cortex-M4F/M7, the FPU registers are NOT saved by default
 *   on interrupt entry (lazy stacking). If your ISR uses float,
 *   either enable LSPEN or save/restore manually.
 * ============================================================ */

/* ============================================================
 * TASK 1 — What's wrong with this ISR?
 *
 * Review the fake ISR below and identify all violations.
 * Then implement a correct version.
 * ============================================================ */

/* Simulated peripheral registers */
static volatile uint32_t UART_SR   = 0;   /* status: bit0=RXNE */
static volatile uint8_t  UART_DR   = 0;   /* data register */
#define UART_SR_RXNE  (1u << 0)

/* Bad ISR — has multiple problems */
char rx_line[128];
int  rx_pos = 0;

void UART_IRQHandler_BAD(void)
{
    /* Problem 1: global non-volatile — compiler may cache rx_pos */
    if (UART_SR & UART_SR_RXNE) {
        char c = (char)UART_DR;
        /* Problem 2: no bounds check on rx_pos — buffer overflow */
        rx_line[rx_pos++] = c;

        if (c == '\n') {
            /* Problem 3: printf inside ISR — blocks, uses heap internally */
            printf("Received: %s\n", rx_line);
            rx_pos = 0;
        }
        /* Problem 4: interrupt flag not cleared here for this peripheral type */
    }
}

/* Correct version — implement this */
#define RX_BUF_SIZE  128u

volatile uint8_t  g_rx_buf[RX_BUF_SIZE];
volatile uint8_t  g_rx_head = 0;
volatile uint8_t  g_rx_tail = 0;
volatile uint8_t  g_rx_overrun = 0;
volatile uint8_t  g_line_ready  = 0;

void UART_IRQHandler_CORRECT(void)
{
    /* TODO: check UART_SR_RXNE flag */
    /* TODO: read UART_DR — this clears RXNE on most UARTs */
    /* TODO: check for buffer full condition before writing
     *       (head+1) % RX_BUF_SIZE == tail → overrun */
    /* TODO: write byte to g_rx_buf[g_rx_head], advance head */
    /* TODO: if byte == '\n', set g_line_ready = 1 */
    /* NOTE: NO printf, NO malloc, NO blocking */
}

/* Main loop processes the buffer */
int process_received_line(char *out_buf, uint8_t out_max)
{
    /* TODO: check g_line_ready
     * if set: drain bytes from ring buffer (tail to head) into out_buf
     *         until '\n' or out_max reached
     *         null-terminate, clear g_line_ready
     *         return length
     * if not set: return -1 */
    (void)out_buf; (void)out_max;
    return -1;
}

/* ============================================================
 * TASK 2 — Ring buffer for ISR/task communication
 *
 * A lock-free single-producer single-consumer (SPSC) ring buffer.
 * ISR = producer (writes), main loop = consumer (reads).
 * This pattern appears in UART, SPI, CAN receive paths.
 * ============================================================ */

#define RING_BUF_MASK  0x3Fu   /* size must be power of 2, mask = size-1 */
#define RING_BUF_SIZE  (RING_BUF_MASK + 1)  /* 64 */

typedef struct {
    volatile uint8_t  buf[RING_BUF_SIZE];
    volatile uint8_t  head;   /* written by producer (ISR) */
    volatile uint8_t  tail;   /* written by consumer (main) */
} RingBuf;

void ring_init(RingBuf *rb)
{
    /* TODO: zero the struct */
    (void)rb;
}

int ring_push(RingBuf *rb, uint8_t byte)
{
    /* TODO: check if full: ((rb->head + 1) & RING_BUF_MASK) == rb->tail
     * if full: return -1 (drop byte)
     * write byte to buf[rb->head & RING_BUF_MASK]
     * advance head: rb->head = (rb->head + 1) & RING_BUF_MASK
     * return 0
     *
     * NOTE: on 8-bit MCU, the head increment must be atomic.
     * On Cortex-M this single uint8_t write IS atomic. */
    (void)rb; (void)byte;
    return -1;
}

int ring_pop(RingBuf *rb, uint8_t *out)
{
    /* TODO: check if empty: rb->head == rb->tail → return -1
     * read buf[rb->tail & RING_BUF_MASK] into *out
     * advance tail
     * return 0 */
    (void)rb; (void)out;
    return -1;
}

uint8_t ring_available(const RingBuf *rb)
{
    /* TODO: return number of bytes available to read
     * = (head - tail) & RING_BUF_MASK */
    (void)rb;
    return 0;
}

/* ============================================================
 * TASK 3 — Critical section patterns
 *
 * When you MUST access a multi-byte shared structure from both
 * ISR and main, you need a critical section.
 * ============================================================ */

/* Simulated disable/enable interrupt primitives */
static uint8_t g_irq_enabled = 1;
static void __disable_irq(void) { g_irq_enabled = 0; }
static void __enable_irq(void)  { g_irq_enabled = 1; }

typedef struct {
    uint32_t timestamp;
    float    temperature;
    float    pressure;
    uint8_t  valid;
} SensorSnapshot;

volatile SensorSnapshot g_sensor;  /* written by ISR, read by main */

/* ISR writes a new snapshot */
void sensor_isr(uint32_t ts, float temp, float pressure)
{
    /* On Cortex-M, a struct write is NOT atomic — main might read
     * a half-written struct. Use the double-buffer trick or critical section. */

    /* TODO: disable IRQ before writing to prevent preemption of this
     *       ISR by a higher-priority ISR that also reads g_sensor */
    /* TODO: write timestamp, temperature, pressure, valid=1 to g_sensor */
    /* TODO: re-enable IRQ */
    (void)ts; (void)temp; (void)pressure;
}

/* Main reads a consistent snapshot */
SensorSnapshot sensor_get_snapshot(void)
{
    SensorSnapshot local;
    /* TODO: disable IRQ */
    /* TODO: local = g_sensor (full struct copy) */
    /* TODO: enable IRQ */
    /* return local */
    return local;
}

/* ============================================================
 * TASK 4 — DMA completion callback pattern
 *
 * DMA transfer complete → ISR fires → set flag.
 * Main loop starts next transfer.
 * Double-buffer: while DMA fills buffer B, main processes buffer A.
 * ============================================================ */

#define DMA_BUF_SIZE  256u

uint8_t g_dma_buf_a[DMA_BUF_SIZE];
uint8_t g_dma_buf_b[DMA_BUF_SIZE];

volatile uint8_t g_dma_buf_ready = 0;  /* 0=none, 1=buf_a, 2=buf_b */
volatile uint8_t g_dma_active_buf = 0; /* which buffer DMA is currently filling */

void DMA_IRQHandler(void)
{
    /* TODO: set g_dma_buf_ready to the buffer that JUST finished
     * TODO: switch g_dma_active_buf to the other buffer
     * TODO: restart DMA on the new active buffer
     *       (simulated: just toggle g_dma_active_buf between 0 and 1) */
}

uint8_t *dma_get_ready_buffer(uint16_t *len)
{
    /* TODO: if g_dma_buf_ready == 0: *len=0, return NULL
     * if g_dma_buf_ready == 1: *len=DMA_BUF_SIZE, clear flag, return g_dma_buf_a
     * if g_dma_buf_ready == 2: *len=DMA_BUF_SIZE, clear flag, return g_dma_buf_b */
    (void)len;
    return NULL;
}

/* ============================================================
 * TASK 5 — BUG HUNT: ISR timing bug
 *
 * The code below measures pulse width using two timer capture ISRs.
 * It has 3 bugs. Find and mark each one.
 * ============================================================ */

volatile uint32_t g_rise_tick = 0;
volatile uint32_t g_fall_tick = 0;
volatile uint8_t  g_pulse_ready = 0;
uint32_t          g_pulse_width_ticks;   /* Bug 1: ??? */

void rising_edge_isr(uint32_t current_tick)
{
    g_rise_tick = current_tick;
    g_pulse_ready = 0;
}

void falling_edge_isr(uint32_t current_tick)
{
    g_fall_tick = current_tick;

    /* Bug 2: ??? */
    g_pulse_width_ticks = g_fall_tick - g_rise_tick;  /* not volatile, might be stale */

    g_pulse_ready = 1;
}

uint32_t get_pulse_width(void)
{
    /* Bug 3: ??? */
    while (!g_pulse_ready) {}   /* busy-wait — blocks main, burns power.
                                 * Should be event-driven or use RTOS block. */
    g_pulse_ready = 0;
    return g_pulse_width_ticks;
}

/* ============================================================
 * INTERVIEW QUESTIONS
 * ============================================================
 *
 * Q1: List 5 things you must NEVER do inside an ISR and why.
 *     Answer: TODO
 *
 * Q2: On ARM Cortex-M4, which registers are automatically saved
 *     on interrupt entry? Which are NOT?
 *     Answer: TODO
 *
 * Q3: What is a spurious interrupt? How do you handle one defensively?
 *     Answer: TODO
 *
 * Q4: You need to share a uint64_t between an ISR and main on a 32-bit MCU.
 *     Is reading it atomic? What do you do to protect it?
 *     Answer: TODO
 *
 * Q5: What is the difference between maskable and non-maskable interrupts?
 *     Give an example of each on ARM Cortex-M.
 *     Answer: TODO
 *
 * Q6: Explain tail-chaining on ARM Cortex-M. Why does it reduce
 *     interrupt latency when multiple IRQs are pending?
 *     Answer: TODO
 */
