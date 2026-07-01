/*
 * ANSWERS: 03_Interrupts_and_ISR/01_isr_design.c
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* ============================================================
 * TASK 1 — Correct ISR with ring buffer
 * ============================================================ */

static volatile uint32_t UART_SR   = 0x20;  /* RXNE set */
static volatile uint8_t  UART_DR   = 0;
#define UART_SR_RXNE  (1u << 0)

#define RX_BUF_SIZE  128u

volatile uint8_t  g_rx_buf[RX_BUF_SIZE];
volatile uint8_t  g_rx_head    = 0;
volatile uint8_t  g_rx_tail    = 0;
volatile uint8_t  g_rx_overrun = 0;
volatile uint8_t  g_line_ready  = 0;

void UART_IRQHandler_CORRECT(void)
{
    if (!(UART_SR & UART_SR_RXNE)) return;   /* Rule 5: check flag */
    uint8_t c = UART_DR;                      /* reading DR clears RXNE on real HW */

    uint8_t next_head = (uint8_t)((g_rx_head + 1) % RX_BUF_SIZE);
    if (next_head == g_rx_tail) {
        g_rx_overrun = 1;                     /* buffer full — drop byte */
        return;
    }
    g_rx_buf[g_rx_head] = c;
    g_rx_head = next_head;

    if (c == '\n') g_line_ready = 1;
}

int process_received_line(char *out_buf, uint8_t out_max)
{
    if (!g_line_ready) return -1;

    uint8_t len = 0;
    while (g_rx_tail != g_rx_head && len < out_max - 1u) {
        char c = (char)g_rx_buf[g_rx_tail];
        g_rx_tail = (uint8_t)((g_rx_tail + 1) % RX_BUF_SIZE);
        out_buf[len++] = c;
        if (c == '\n') break;
    }
    out_buf[len] = '\0';
    g_line_ready = 0;
    return (int)len;
}

/* ============================================================
 * TASK 2 — SPSC Ring buffer
 * ============================================================ */

#define RING_BUF_MASK  0x3Fu
#define RING_BUF_SIZE  (RING_BUF_MASK + 1)

typedef struct {
    volatile uint8_t  buf[RING_BUF_SIZE];
    volatile uint8_t  head;
    volatile uint8_t  tail;
} RingBuf;

void ring_init(RingBuf *rb)
{
    memset((void*)rb->buf, 0, RING_BUF_SIZE);
    rb->head = 0;
    rb->tail = 0;
}

int ring_push(RingBuf *rb, uint8_t byte)
{
    uint8_t next_head = (rb->head + 1u) & RING_BUF_MASK;
    if (next_head == rb->tail) return -1;   /* full */
    rb->buf[rb->head] = byte;
    rb->head = next_head;
    return 0;
}

int ring_pop(RingBuf *rb, uint8_t *out)
{
    if (rb->head == rb->tail) return -1;   /* empty */
    *out = rb->buf[rb->tail];
    rb->tail = (rb->tail + 1u) & RING_BUF_MASK;
    return 0;
}

uint8_t ring_available(const RingBuf *rb)
{
    return (rb->head - rb->tail) & RING_BUF_MASK;
}

/* ============================================================
 * TASK 3 — Critical section for SensorSnapshot
 * ============================================================ */

static uint8_t g_irq_enabled = 1;
static void __disable_irq(void) { g_irq_enabled = 0; }
static void __enable_irq(void)  { g_irq_enabled = 1; }

typedef struct {
    uint32_t timestamp;
    float    temperature;
    float    pressure;
    uint8_t  valid;
} SensorSnapshot;

volatile SensorSnapshot g_sensor;

void sensor_isr(uint32_t ts, float temp, float pressure)
{
    __disable_irq();        /* prevent higher-priority ISR from reading partial write */
    g_sensor.timestamp   = ts;
    g_sensor.temperature = temp;
    g_sensor.pressure    = pressure;
    g_sensor.valid       = 1;
    __enable_irq();
}

SensorSnapshot sensor_get_snapshot(void)
{
    SensorSnapshot local;
    __disable_irq();
    local = g_sensor;       /* atomic struct copy inside critical section */
    __enable_irq();
    return local;
}

/* ============================================================
 * TASK 4 — DMA double-buffer
 * ============================================================ */

#define DMA_BUF_SIZE  256u

uint8_t g_dma_buf_a[DMA_BUF_SIZE];
uint8_t g_dma_buf_b[DMA_BUF_SIZE];

volatile uint8_t g_dma_buf_ready  = 0;   /* 1=buf_a ready, 2=buf_b ready */
volatile uint8_t g_dma_active_buf = 0;   /* 0=filling buf_a, 1=filling buf_b */

void DMA_IRQHandler(void)
{
    /* Signal which buffer just completed */
    g_dma_buf_ready = (g_dma_active_buf == 0) ? 1u : 2u;
    /* Switch active buffer */
    g_dma_active_buf = (g_dma_active_buf == 0) ? 1u : 0u;
    /* In real code: restart DMA transfer on the new active buffer here */
}

uint8_t *dma_get_ready_buffer(uint16_t *len)
{
    if (g_dma_buf_ready == 0) { *len = 0; return NULL; }
    uint8_t *buf = (g_dma_buf_ready == 1) ? g_dma_buf_a : g_dma_buf_b;
    *len = DMA_BUF_SIZE;
    g_dma_buf_ready = 0;
    return buf;
}

/* ============================================================
 * TASK 5 — Bug hunt FIXED
 *
 * Bug 1 & 2: g_pulse_width_ticks not volatile.
 *   The compiler may cache the write in a register and never flush to RAM.
 *   Main reads a stale value. FIX: volatile uint32_t g_pulse_width_ticks;
 *
 * Bug 3: busy-wait in get_pulse_width() (while (!g_pulse_ready){}).
 *   This blocks the main loop, wastes CPU cycles, prevents other tasks from
 *   running, and can starve lower-priority processing.
 *   FIX: use an RTOS event flag or binary semaphore; or return -1 if not ready
 *   and let the caller retry (non-blocking polling pattern).
 * ============================================================ */

volatile uint32_t g_rise_tick = 0;
volatile uint32_t g_fall_tick = 0;
volatile uint8_t  g_pulse_ready = 0;
volatile uint32_t g_pulse_width_ticks;   /* FIXED: now volatile */

void rising_edge_isr(uint32_t current_tick)  { g_rise_tick = current_tick; g_pulse_ready = 0; }
void falling_edge_isr(uint32_t current_tick) {
    g_fall_tick = current_tick;
    g_pulse_width_ticks = g_fall_tick - g_rise_tick;
    g_pulse_ready = 1;
}

int get_pulse_width_nonblocking(uint32_t *width_out)
{
    /* FIXED: non-blocking — caller decides what to do if not ready */
    if (!g_pulse_ready) return -1;
    *width_out = g_pulse_width_ticks;
    g_pulse_ready = 0;
    return 0;
}

/* ============================================================
 * INTERVIEW QUESTION ANSWERS
 * ============================================================

Q1: 5 things you must NEVER do inside an ISR, and why.

A: 1. printf() / sprintf() — uses malloc internally for buffer, calls locks
      (mutex) on the I/O stream. Can deadlock if main holds the lock.
   2. malloc() / free() — heap functions take mutexes; heap state may be
      inconsistent at the moment ISR fires; non-deterministic runtime.
   3. delay_ms() / vTaskDelay() — blocking ISR stops all other IRQs at
      same or lower priority. System appears hung.
   4. xSemaphoreTake() (blocking) — can attempt to block inside ISR which
      is undefined behavior in FreeRTOS; use xSemaphoreGiveFromISR() instead.
   5. Floating-point operations (without FPU context save) — on Cortex-M4F,
      FPU registers (S0-S15, FPSCR) are NOT saved on interrupt entry by
      default (lazy stacking). ISR that uses FP corrupts main's FP registers.

Q2: Which registers are saved automatically on Cortex-M4 interrupt entry?

A: Hardware automatically pushes 8 registers onto the current stack (MSP or PSP):
   R0, R1, R2, R3 — argument/return registers (caller-saved)
   R12            — scratch register
   LR (R14)       — link register (return address)
   PC (R15)       — program counter of interrupted instruction
   xPSR           — processor status register (flags, ISR number, Thumb state)
   These 8 registers × 4 bytes = 32 bytes minimum stack frame.
   NOT saved automatically: R4-R11 (callee-saved — compiler saves them in prologue
   if ISR uses them). FPU registers S0-S15 saved lazily (FPCCR.LSPEN).

Q3: What is a spurious interrupt? How do you handle it defensively?

A: A spurious interrupt fires with no identifiable source — the ISR vector
   is entered but the status register shows no pending flag.
   Causes: electrical noise on IRQ line, race condition clearing the flag,
   software re-enabling interrupts before flag is cleared.
   Defensive handling: always check the flag at the top of every ISR:
     if (!(PERIPH->SR & EXPECTED_FLAG)) return;  // spurious — ignore
   Never assume the ISR fired for the expected reason.

Q4: Sharing uint64_t between ISR and main on 32-bit MCU. Atomic?

A: No. A 64-bit read requires two 32-bit bus transactions (LDRD or two LDRs).
   Between the first and second read, the ISR could fire and update both halves.
   Result: main reads old_high + new_low — a corrupt value.
   Solutions:
   (a) Critical section: disable interrupts, read, enable interrupts.
   (b) Double-copy pattern: read until two consecutive reads agree.
   (c) Sequence counter: ISR increments counter before/after write;
       main reads until counter hasn't changed (seqlock pattern).

Q5: Difference between maskable and non-maskable interrupts on Cortex-M.

A: Maskable (IRQs): can be disabled globally with PRIMASK (CPSID I / __disable_irq())
   or selectively with BASEPRI. All peripheral interrupts (UART, TIM, DMA, etc.) are
   maskable. FreeRTOS uses BASEPRI to mask IRQs below a threshold.
   Non-Maskable Interrupt (NMI): cannot be disabled by software. Always responds.
   Uses: clock failure monitor, watchdog in NMI mode, catastrophic hardware fault.
   Also non-maskable: HardFault, Reset.
   HardFault: triggered by memory access violations, invalid instructions.
   On Cortex-M33 (TrustZone): SecureFault is also non-maskable from non-secure code.

Q6: What is tail-chaining on ARM Cortex-M? Why does it reduce latency?

A: When the CPU finishes an ISR and another IRQ is pending (equal or lower priority),
   instead of fully unstacking (restoring 8 registers) and restacking for the next ISR
   (saving 8 registers again), it goes directly from ISR exit to next ISR entry.
   Saves approximately 12 cycles (vs ~30 cycles for unstack+stack).
   The stack frame stays intact (SP doesn't change). EXC_RETURN mechanism
   recognizes the pending IRQ and "chains" to it directly.
   Practical impact: at 168 MHz, 12 cycles = ~71 ns saved per chained interrupt.
   Critical for systems with many simultaneous IRQs (CAN mailboxes, ADC oversampling).
*/

int main(void)
{
    /* Ring buffer test */
    RingBuf rb;
    ring_init(&rb);
    assert(ring_available(&rb) == 0);

    for (uint8_t i = 0; i < 10; i++) ring_push(&rb, i);
    assert(ring_available(&rb) == 10);

    uint8_t out;
    ring_pop(&rb, &out);
    assert(out == 0);
    assert(ring_available(&rb) == 9);

    /* Critical section test */
    sensor_isr(1000, 25.5f, 1013.25f);
    SensorSnapshot snap = sensor_get_snapshot();
    assert(snap.valid == 1 && snap.timestamp == 1000);

    /* DMA double buffer test */
    g_dma_active_buf = 0;
    DMA_IRQHandler();
    assert(g_dma_buf_ready == 1);
    assert(g_dma_active_buf == 1);
    uint16_t len;
    uint8_t *buf = dma_get_ready_buffer(&len);
    assert(buf == g_dma_buf_a && len == DMA_BUF_SIZE);
    assert(g_dma_buf_ready == 0);

    /* Pulse width test */
    rising_edge_isr(1000);
    falling_edge_isr(1500);
    uint32_t width;
    assert(get_pulse_width_nonblocking(&width) == 0);
    assert(width == 500);

    printf("All ISR design answers verified.\n");
    return 0;
}
