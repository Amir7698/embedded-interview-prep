/*
 * ANSWERS: 05_RTOS/02_semaphores_mutexes.c
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* ============================================================
 * INTERVIEW QUESTION ANSWERS
 * ============================================================

Q1: Binary semaphore vs mutex — key differences.

A: Binary semaphore:
   - Signaling mechanism. Used for ISR→task or task→task notification.
   - No ownership: any task (or ISR) can give it.
   - No priority inheritance: if high-priority task waits, lower-priority
     holder is NOT elevated → risk of priority inversion.
   - xSemaphoreGiveFromISR() is allowed.

   Mutex:
   - Mutual exclusion (resource protection).
   - Has OWNERSHIP: the task that takes it must be the one to give it.
   - Priority inheritance built-in: if higher-priority task blocks on
     the mutex, the holder's priority is temporarily raised.
   - xSemaphoreGiveFromISR() is NOT allowed (undefined behavior).
   - Cannot be given from ISR.

   Rule: semaphore for signaling, mutex for shared resource protection.

Q2: Counting semaphore — what it models.

A: A counting semaphore has an integer count [0..MAX].
   xSemaphoreTake: decrements count. Blocks if count == 0.
   xSemaphoreGive: increments count. Unblocks one waiter.
   Models: resource pools.
   Example: 3 SPI buses available. COUNT=3.
   Task takes SPI bus: COUNT→2. Another task: COUNT→1. Third: COUNT→0.
   Fourth task: BLOCKS until one of the first three gives back.
   Also models: event queues (ISR fires multiple times between task runs;
   each fire calls Give; task wakes once per Give).
   Counting semaphore ≠ queue: no data transferred, only count.

Q3: Deadlock — define and give an embedded example.

A: Deadlock: two or more tasks are each waiting for a resource held by
   the other → circular wait → all blocked forever.

   Embedded example:
   Task A:  takes Mutex_SPI, then tries to take Mutex_DMA.
   Task B:  takes Mutex_DMA, then tries to take Mutex_SPI.
   If A takes SPI and B takes DMA simultaneously:
   A blocks waiting for DMA (held by B).
   B blocks waiting for SPI (held by A).
   Neither can proceed. System frozen.

   Prevention — Lock Ordering:
   Globally define: always acquire SPI before DMA.
   Both Task A and B: take SPI first, then DMA.
   B must try to take SPI first → sees A holds it → blocks → A can
   take DMA → completes → gives both → B runs.

Q4: Can a mutex be given from an ISR in FreeRTOS?

A: No. xSemaphoreTake() and xSemaphoreGive() on a mutex may not be
   called from an ISR.
   Reason: Mutex give involves priority inheritance logic — it may need
   to change a task's priority, which is not ISR-safe.
   ISR must use: xSemaphoreGiveFromISR() with a binary or counting semaphore.
   Pattern:
   ISR: xSemaphoreGiveFromISR(sem, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);  // trigger context switch
   Task: xSemaphoreTake(sem, portMAX_DELAY);

Q5: Queue vs semaphore for ISR→task data transfer.

A: Semaphore: only signals that something happened. No data.
   If ISR fires faster than task processes, count accumulates but
   the ISR's data may have been overwritten before task reads it.
   Appropriate when: task can re-read the data itself (e.g., read ADC register).

   Queue: transfers the actual data value. ISR copies data into queue entry.
   Task wakes and pops data off. If queue has depth N, up to N events can
   be buffered. If queue is full, xQueueSendFromISR returns pdFALSE (drop).
   Appropriate when: data values matter and must not be lost.

   Rule: for ISR→task with data → use queue.
         for ISR→task signaling only → use binary semaphore.

Q6: What is xHigherPriorityTaskWoken and why must you call portYIELD_FROM_ISR?

A: When an ISR gives a semaphore or sends to a queue, a higher-priority
   task may be unblocked. The ISR cannot switch context immediately
   (it's in interrupt context). Instead:
   1. FreeRTOS sets *pxHigherPriorityTaskWoken = pdTRUE if a higher-priority
      task was unblocked.
   2. At the END of the ISR, portYIELD_FROM_ISR(val) checks val:
      If pdTRUE: pends a PendSV interrupt. When ISR returns, PendSV fires
      and does the context switch → high-priority task runs immediately.
      If pdFALSE: ISR returns normally, no context switch.
   Without portYIELD_FROM_ISR: the unblocked high-priority task won't run
   until the next SysTick (up to 1 ms later) → unnecessary latency.
*/

/* ============================================================
 * Simulated types
 * ============================================================ */

typedef struct {
    volatile int32_t  count;
    int32_t           max_count;
    const char       *name;
} SimSemaphore;

typedef struct {
    uint8_t  buf[64];
    uint8_t  head, tail, count;
    uint8_t  item_size;
    uint8_t  depth;
} SimQueue;

/* ============================================================
 * TASK 1 — Binary semaphore (ISR → task signaling)
 * ============================================================ */

static SimSemaphore g_uart_rx_sem = {0, 1, "uart_rx"};

void sim_sem_init_binary(SimSemaphore *s) { s->count = 0; s->max_count = 1; }

int sim_sem_give(SimSemaphore *s)
{
    if (s->count >= s->max_count) return 0;  /* would overflow */
    s->count++;
    return 1;
}

int sim_sem_give_from_isr(SimSemaphore *s, uint8_t *higher_prio_woken)
{
    int r = sim_sem_give(s);
    if (r && higher_prio_woken) *higher_prio_woken = 1;
    return r;
}

int sim_sem_take(SimSemaphore *s, uint32_t timeout_ticks)
{
    (void)timeout_ticks;
    if (s->count > 0) { s->count--; return 1; }
    return 0;  /* would block in real RTOS */
}

/* ISR: new UART byte received */
static volatile uint8_t g_uart_byte = 0;
void uart_rx_isr_sim(uint8_t byte)
{
    g_uart_byte = byte;
    uint8_t woken = 0;
    sim_sem_give_from_isr(&g_uart_rx_sem, &woken);
    /* portYIELD_FROM_ISR(woken) — would trigger PendSV on real MCU */
}

/* Task: processes received byte */
void uart_process_task_sim(void)
{
    if (sim_sem_take(&g_uart_rx_sem, 1000)) {
        printf("  Received byte: 0x%02X\n", g_uart_byte);
    }
}

/* ============================================================
 * TASK 2 — Mutex (resource protection)
 * ============================================================ */

typedef struct { uint8_t locked; const char *owner; } SimMutex;

int mutex_take(SimMutex *m, const char *caller)
{
    if (m->locked) return 0;  /* would block in real RTOS */
    m->locked = 1;
    m->owner  = caller;
    return 1;
}

void mutex_give(SimMutex *m, const char *caller)
{
    if (!m->locked || m->owner != caller) {
        printf("ERROR: %s trying to give mutex owned by %s\n", caller, m->owner);
        return;
    }
    m->locked = 0;
    m->owner  = NULL;
}

static SimMutex g_spi_mutex = {0, NULL};
static uint8_t  g_spi_reg_value = 0;

void spi_write_protected(uint8_t reg, uint8_t val)
{
    if (!mutex_take(&g_spi_mutex, "spi_writer")) return;
    g_spi_reg_value = val;  /* critical section */
    mutex_give(&g_spi_mutex, "spi_writer");
}

/* ============================================================
 * TASK 3 — Queue (producer/consumer with data)
 * ============================================================ */

typedef struct { uint32_t timestamp_ms; float temperature; float humidity; } SensorSample;

#define QUEUE_DEPTH  4u
typedef struct {
    SensorSample buf[QUEUE_DEPTH];
    uint8_t head, tail, count;
} SensorQueue;

static SensorQueue g_sensor_q = {0};

int queue_send(SensorQueue *q, const SensorSample *s)
{
    if (q->count >= QUEUE_DEPTH) return 0;  /* full */
    q->buf[q->head] = *s;
    q->head = (q->head + 1) % QUEUE_DEPTH;
    q->count++;
    return 1;
}

int queue_recv(SensorQueue *q, SensorSample *out)
{
    if (q->count == 0) return 0;  /* empty */
    *out = q->buf[q->tail];
    q->tail = (q->tail + 1) % QUEUE_DEPTH;
    q->count--;
    return 1;
}

/* ============================================================
 * TASK 4 — Bug hunt FIXED
 *
 * Bug 1: spi_write() accesses g_spi_data WITHOUT mutex.
 *        Two tasks calling spi_write() simultaneously may interleave
 *        their byte sequences → corrupted SPI transaction.
 *        FIX: take mutex before modifying shared SPI state, give after.
 *
 * Bug 2: uart_isr() calls xSemaphoreTake() (blocking) from ISR context.
 *        ISRs must NEVER call blocking FreeRTOS API. The ISR may be
 *        entered with the scheduler suspended or in a critical section.
 *        Calling Take can corrupt FreeRTOS internal state → HardFault.
 *        FIX: ISR should call xSemaphoreGiveFromISR(). The waiting task
 *        calls xSemaphoreTake() with a timeout.
 *
 * Bug 3: return value of sem_take ignored.
 *        If sem_take returns pdFALSE (timeout expired), the code proceeds
 *        to use stale data as if fresh data arrived.
 *        FIX: check return value and skip processing on timeout/failure.
 * ============================================================ */

int main(void)
{
    /* Binary semaphore test */
    sim_sem_init_binary(&g_uart_rx_sem);
    assert(g_uart_rx_sem.count == 0);

    uart_rx_isr_sim(0x42);
    assert(g_uart_rx_sem.count == 1);

    uart_process_task_sim();
    assert(g_uart_rx_sem.count == 0);

    /* Mutex test */
    spi_write_protected(0x01, 0xAB);
    assert(g_spi_reg_value == 0xAB);
    assert(!g_spi_mutex.locked);  /* released after write */

    /* Queue test */
    SensorSample s1 = {1000, 25.5f, 60.0f};
    SensorSample s2 = {2000, 26.0f, 61.0f};
    assert(queue_send(&g_sensor_q, &s1));
    assert(queue_send(&g_sensor_q, &s2));
    assert(g_sensor_q.count == 2);

    SensorSample out;
    assert(queue_recv(&g_sensor_q, &out));
    assert(out.timestamp_ms == 1000 && out.temperature == 25.5f);
    assert(g_sensor_q.count == 1);

    /* Queue full test */
    SensorSample dummy = {0};
    queue_send(&g_sensor_q, &dummy);
    queue_send(&g_sensor_q, &dummy);
    queue_send(&g_sensor_q, &dummy);
    assert(g_sensor_q.count == QUEUE_DEPTH);
    assert(queue_send(&g_sensor_q, &dummy) == 0);  /* must fail when full */

    printf("All RTOS semaphore/mutex answers verified.\n");
    return 0;
}
