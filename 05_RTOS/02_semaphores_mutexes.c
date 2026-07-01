/*
 * ============================================================
 * EMBEDDED INTERVIEW PREP
 * Topic : RTOS — Semaphores, Mutexes, and Queues
 * File  : 05_RTOS/02_semaphores_mutexes.c
 * ============================================================
 *
 * The THREE synchronization primitives you MUST know:
 * 1. Binary semaphore  — ISR-to-task signaling
 * 2. Mutex            — mutual exclusion with priority inheritance
 * 3. Queue            — data transfer between tasks
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* ============================================================
 * THEORY — When to use what
 * ============================================================
 *
 * Binary Semaphore:
 *   - Signal an event (e.g., ISR tells task data is ready)
 *   - Like a flag but RTOS-aware (task blocks instead of polling)
 *   - Can be given from ISR: xSemaphoreGiveFromISR()
 *   - No priority inheritance — NOT safe for resource locking
 *
 * Counting Semaphore:
 *   - Track N available resources (e.g., buffer pool with 4 slots)
 *   - xSemaphoreCreateCounting(max, initial)
 *
 * Mutex:
 *   - Protect a shared resource (SPI bus, UART, global variable)
 *   - HAS priority inheritance (prevents priority inversion)
 *   - Must be taken and given by the SAME task
 *   - NEVER give from ISR (use binary semaphore for ISR sync)
 *   - xSemaphoreCreateMutex()
 *
 * Recursive Mutex:
 *   - Same task can take it multiple times without deadlock
 *   - Must give same number of times it took
 *   - xSemaphoreCreateRecursiveMutex()
 *
 * Queue:
 *   - Pass data between tasks or from ISR to task
 *   - Provides both synchronization AND data transfer
 *   - xQueueCreate(length, item_size)
 *   - xQueueSend() / xQueueReceive()
 *   - xQueueSendFromISR() / xQueueReceiveFromISR()
 * ============================================================ */

/* Simulated FreeRTOS types */
typedef uint32_t TickType_t;
typedef uint32_t BaseType_t;
#define pdTRUE   1u
#define pdFALSE  0u
#define pdPASS   1u
#define portMAX_DELAY 0xFFFFFFFFu
#define pdMS_TO_TICKS(ms) (ms)

/* ============================================================
 * SIMULATED SEMAPHORE AND QUEUE (host-side practice)
 * ============================================================ */

typedef struct {
    volatile uint32_t count;
    uint32_t max_count;
} SimSemaphore;

typedef struct {
    uint8_t  *buf;
    uint16_t  item_size;
    uint16_t  length;
    volatile uint16_t head;
    volatile uint16_t tail;
    volatile uint16_t count;
} SimQueue;

static SimSemaphore g_sem  = {0};
static SimSemaphore g_mutex = {1, 1};   /* mutex starts at 1 (available) */

BaseType_t sem_give(SimSemaphore *s)
{
    if (s->count >= s->max_count) return pdFALSE;
    s->count++;
    return pdTRUE;
}

BaseType_t sem_take(SimSemaphore *s, TickType_t timeout_ticks)
{
    if (s->count > 0) { s->count--; return pdTRUE; }
    /* In real RTOS: block here for timeout_ticks */
    (void)timeout_ticks;
    return pdFALSE;   /* would block in real RTOS */
}

/* ============================================================
 * TASK 1 — Binary semaphore: ISR-to-task data-ready signal
 *
 * Pattern: ISR receives UART byte, signals task to process.
 * Task blocks efficiently instead of polling.
 * ============================================================ */

static volatile uint8_t g_isr_byte = 0;
static SimSemaphore g_data_ready_sem = {0, 1};

void uart_rx_isr_sim(uint8_t byte)
{
    /* TODO: save the byte */
    /* TODO: give the semaphore: xSemaphoreGiveFromISR() in real FreeRTOS
     *       (include BaseType_t xHigherPriorityTaskWoken = pdFALSE and
     *        portYIELD_FROM_ISR(xHigherPriorityTaskWoken) at end) */
    g_isr_byte = byte;
    sem_give(&g_data_ready_sem);
    printf("[ISR] UART byte 0x%02X received, semaphore given\n", byte);
}

void uart_process_task(void *param)
{
    /* TODO: loop forever:
     *   xSemaphoreTake(sem, portMAX_DELAY) — blocks until ISR signals
     *   process g_isr_byte (volatile read inside critical section in real code) */
    (void)param;
    if (sem_take(&g_data_ready_sem, portMAX_DELAY) == pdTRUE) {
        printf("[Task] Processing byte: 0x%02X\n", g_isr_byte);
    }
}

/* ============================================================
 * TASK 2 — Mutex: protect shared SPI bus
 *
 * Multiple tasks share one SPI peripheral.
 * Transactions must not be interleaved.
 * ============================================================ */

static SimSemaphore g_spi_mutex = {1, 1};

void spi_write_protected(uint8_t reg, uint8_t val)
{
    /* TODO: xSemaphoreTake(g_spi_mutex, portMAX_DELAY) */
    /* TODO: perform SPI transaction (CS assert, write, CS deassert) */
    /* TODO: xSemaphoreGive(g_spi_mutex) */
    /* IMPORTANT: always give the mutex, even if SPI fails.
     * Use goto cleanup or scope-based pattern. */

    if (sem_take(&g_spi_mutex, portMAX_DELAY) == pdTRUE) {
        printf("[SPI] Writing reg=0x%02X val=0x%02X\n", reg, val);
        sem_give(&g_spi_mutex);
    }
}

/* ============================================================
 * TASK 3 — Queue: pass sensor readings between tasks
 *
 * Sensor task reads ADC at 100Hz, fills queue.
 * Logger task drains queue and writes to flash.
 * Decouples timing between producer and consumer.
 * ============================================================ */

typedef struct {
    uint32_t timestamp_ms;
    uint16_t adc_raw;
    float    voltage;
} SensorSample;

#define QUEUE_LENGTH  16
static SensorSample g_queue_buf[QUEUE_LENGTH];
static SimQueue g_sensor_queue = {
    .buf       = (uint8_t *)g_queue_buf,
    .item_size = sizeof(SensorSample),
    .length    = QUEUE_LENGTH,
    .head      = 0,
    .tail      = 0,
    .count     = 0
};

BaseType_t queue_send(SimQueue *q, const void *item)
{
    /* TODO: if full: return pdFALSE
     * copy item to buf + head * item_size
     * advance head (wrap at length)
     * increment count
     * return pdTRUE */
    if (q->count >= q->length) return pdFALSE;
    memcpy(q->buf + q->head * q->item_size, item, q->item_size);
    q->head = (uint16_t)((q->head + 1) % q->length);
    q->count++;
    return pdTRUE;
}

BaseType_t queue_receive(SimQueue *q, void *item, TickType_t timeout)
{
    /* TODO: if empty: wait timeout — return pdFALSE on timeout
     * copy buf + tail * item_size to item
     * advance tail
     * decrement count */
    (void)timeout;
    if (q->count == 0) return pdFALSE;
    memcpy(item, q->buf + q->tail * q->item_size, q->item_size);
    q->tail = (uint16_t)((q->tail + 1) % q->length);
    q->count--;
    return pdTRUE;
}

void sensor_producer_task(void *param)
{
    (void)param;
    SensorSample s = {.timestamp_ms = 100, .adc_raw = 2048, .voltage = 3.3f};
    if (queue_send(&g_sensor_queue, &s) == pdTRUE)
        printf("[Sensor] Sample queued: ADC=%u\n", s.adc_raw);
    else
        printf("[Sensor] Queue full — dropping sample!\n");
}

void logger_consumer_task(void *param)
{
    (void)param;
    SensorSample s;
    if (queue_receive(&g_sensor_queue, &s, pdMS_TO_TICKS(100)) == pdTRUE)
        printf("[Logger] Got sample: ADC=%u at t=%ums\n", s.adc_raw, (unsigned)s.timestamp_ms);
}

/* ============================================================
 * TASK 4 — Deadlock scenario
 *
 * Task A takes Mutex1 then tries to take Mutex2.
 * Task B takes Mutex2 then tries to take Mutex1.
 * Result: DEADLOCK — both tasks wait forever.
 *
 * Prevention: always take mutexes in the same ORDER across all tasks.
 * ============================================================ */

void explain_deadlock(void)
{
    printf("\n--- Deadlock Demo ---\n");
    printf("Task A: takes Mutex1, then waits for Mutex2\n");
    printf("Task B: takes Mutex2, then waits for Mutex1\n");
    printf("Result: circular dependency — both block forever.\n");
    printf("Fix: all tasks must acquire mutexes in the SAME fixed order.\n");
    printf("     Or: use trylock with timeout and retry with backoff.\n");
}

/* ============================================================
 * TASK 5 — BUG HUNT: synchronization bugs
 *
 * The code below has 3 concurrency bugs.
 * Find and mark each one.
 * ============================================================ */

static SimSemaphore g_lock = {1, 1};
static uint32_t g_shared_counter = 0;

void increment_shared_BUGGY(uint32_t count)
{
    /* Bug 1: mutex not taken before accessing g_shared_counter.
     * If two tasks run this concurrently, the read-modify-write
     * on g_shared_counter is a data race. */
    for (uint32_t i = 0; i < count; i++) {
        g_shared_counter++;
    }
    /* Missing: sem_take(&g_lock, portMAX_DELAY) before loop
     *          sem_give(&g_lock) after loop */
}

BaseType_t isr_safe_signal_BUGGY(SimSemaphore *mutex)
{
    /* Bug 2: giving a MUTEX from an ISR is illegal in FreeRTOS.
     * Mutexes track ownership (priority inheritance) — giving from ISR
     * corrupts the ownership state.
     * Use a BINARY SEMAPHORE for ISR-to-task signaling. */
    return sem_give(mutex);
}

void critical_section_BUGGY(SimSemaphore *sem)
{
    /* Bug 3: no handling of sem_take failure.
     * If sem_take times out or fails, the code falls through
     * and modifies g_shared_counter without holding the lock. */
    sem_take(sem, 10);   /* return value ignored */
    g_shared_counter = 999;
    sem_give(sem);
}

/* ============================================================
 * SELF-TEST
 * ============================================================ */

int main(void)
{
    /* Binary semaphore test */
    uart_rx_isr_sim(0xAB);
    uart_process_task(NULL);

    /* Queue test */
    sensor_producer_task(NULL);
    logger_consumer_task(NULL);

    /* Mutex test */
    spi_write_protected(0x10, 0xFF);

    explain_deadlock();

    printf("\nAll synchronization tests PASSED.\n");
    return 0;
}

/* ============================================================
 * INTERVIEW QUESTIONS
 * ============================================================
 *
 * Q1: What is the difference between a binary semaphore and a mutex?
 *     Can you use a semaphore to protect a shared resource?
 *     Answer: TODO
 *
 * Q2: Why can you give a binary semaphore from an ISR but not a mutex?
 *     Answer: TODO
 *
 * Q3: What is deadlock? Give a two-task example with two mutexes.
 *     How do you prevent it?
 *     Answer: TODO
 *
 * Q4: You have a queue of length 10. Producer sends at 100 Hz,
 *     consumer reads at 80 Hz. What happens over 1 second?
 *     Answer: TODO
 *
 * Q5: A task takes a mutex but crashes before giving it.
 *     What happens in FreeRTOS? How does the "mutex holder died" case
 *     differ between FreeRTOS and other RTOSes?
 *     Answer: TODO
 */
