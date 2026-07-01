/*
 * ============================================================
 * EMBEDDED INTERVIEW PREP
 * Topic : RTOS — Tasks, Scheduling, and Priority
 * File  : 05_RTOS/01_tasks_and_scheduling.c
 * ============================================================
 *
 * FreeRTOS is the most common RTOS in embedded interviews.
 * This file simulates the API concepts without requiring
 * a real FreeRTOS port. Compile and run on host.
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* ============================================================
 * THEORY — RTOS Scheduling
 * ============================================================
 *
 * Task states in FreeRTOS:
 *   RUNNING   — currently executing on the CPU
 *   READY     — ready to run, waiting for scheduler
 *   BLOCKED   — waiting for event (delay, semaphore, queue)
 *   SUSPENDED — explicitly suspended (vTaskSuspend)
 *
 * Scheduler types:
 *   Preemptive: higher-priority task immediately preempts
 *               lower-priority task (default FreeRTOS)
 *   Cooperative: task runs until it yields (no preemption)
 *   Time-sliced: equal-priority tasks share time in round-robin
 *
 * Priority: higher number = higher priority in FreeRTOS
 *   (opposite to some other RTOSes like OSEK/AUTOSAR!)
 *
 * Stack: each task has its own stack.
 *   FreeRTOS stack size is in WORDS (4 bytes each on 32-bit)
 *   configMINIMAL_STACK_SIZE is typically 128 words = 512 bytes
 *
 * Tick: the RTOS tick is a periodic interrupt (SysTick on ARM)
 *   configTICK_RATE_HZ = 1000 → 1 ms per tick
 *   vTaskDelay(100) = delay 100 ticks = 100 ms
 *   pdMS_TO_TICKS(250) converts ms to ticks portably
 *
 * Context switch:
 *   On Cortex-M: PendSV handler saves/restores R4-R11
 *   The CPU hardware auto-saves R0-R3, R12, LR, PC, xPSR
 * ============================================================ */

/* ============================================================
 * SIMULATED FreeRTOS API (for host-side practice)
 * ============================================================ */

typedef void* TaskHandle_t;
typedef void* QueueHandle_t;
typedef void* SemaphoreHandle_t;
typedef uint32_t TickType_t;
typedef uint32_t BaseType_t;
typedef uint32_t UBaseType_t;

#define pdTRUE   1u
#define pdFALSE  0u
#define pdPASS   1u
#define pdFAIL   0u
#define portMAX_DELAY  0xFFFFFFFFu

#define configTICK_RATE_HZ    1000u
#define pdMS_TO_TICKS(ms)     ((TickType_t)((ms) * configTICK_RATE_HZ / 1000u))

/* Simulated task creation (no real scheduling on host) */
static int g_task_count = 0;

BaseType_t xTaskCreate_sim(void (*task_fn)(void *),
                           const char *name,
                           uint16_t stack_words,
                           void *param,
                           UBaseType_t priority,
                           TaskHandle_t *handle)
{
    printf("[RTOS] Task created: %s (stack=%u words, priority=%u)\n",
           name, stack_words, (unsigned)priority);
    g_task_count++;
    if (handle) *handle = (void *)(uintptr_t)g_task_count;
    (void)task_fn; (void)param;
    return pdPASS;
}

/* ============================================================
 * TASK 1 — Task creation and lifecycle
 *
 * Create two tasks: one LED blinker, one sensor reader.
 * The sensor task has higher priority.
 * ============================================================ */

volatile uint8_t g_led_state   = 0;
volatile uint8_t g_sensor_data = 0;

/* LED blinker task — runs at low priority, 500ms blink */
void led_task(void *param)
{
    /* TODO: in a real system, infinite loop:
     *   while (1) {
     *       gpio_toggle_pin(GPIOA, 5);
     *       vTaskDelay(pdMS_TO_TICKS(500));
     *   }
     * On host: just toggle the flag once */
    (void)param;
    g_led_state ^= 1;
    printf("LED task: LED %s\n", g_led_state ? "ON" : "OFF");
}

/* Sensor reader task — runs at high priority every 100ms */
void sensor_task(void *param)
{
    /* TODO: in a real system:
     *   while (1) {
     *       g_sensor_data = read_adc_channel(0);
     *       vTaskDelay(pdMS_TO_TICKS(100));
     *   }
     * On host: simulate a reading */
    (void)param;
    g_sensor_data = 42;
    printf("Sensor task: data = %u\n", g_sensor_data);
}

void create_tasks(void)
{
    /* TODO: create led_task with priority 1, stack 256 words */
    /* TODO: create sensor_task with priority 3, stack 512 words */
    /* TODO: start the scheduler: vTaskStartScheduler() */
    TaskHandle_t led_handle, sensor_handle;

    xTaskCreate_sim(led_task, "LED", 256, NULL, 1, &led_handle);
    xTaskCreate_sim(sensor_task, "Sensor", 512, NULL, 3, &sensor_handle);
    (void)led_handle; (void)sensor_handle;
}

/* ============================================================
 * TASK 2 — Priority inversion scenario
 *
 * Classic embedded interview question: what is priority inversion?
 * How does a mutex with priority inheritance fix it?
 *
 * Scenario:
 *   Task H (high priority)  needs mutex M
 *   Task L (low  priority)  holds mutex M
 *   Task M (medium priority) is runnable (doesn't need M)
 *
 * Without priority inheritance:
 *   L runs but M preempts L (M > L) → H starves!
 *   H is waiting for M which L holds, but L can't run because M preempts it.
 *
 * With priority inheritance (xSemaphoreCreateMutex in FreeRTOS):
 *   When H blocks on mutex held by L, L is temporarily raised to H's priority.
 *   L > M, so L runs and releases the mutex.
 *   H unblocks and runs immediately.
 * ============================================================ */

void priority_inversion_demo(void)
{
    printf("\n--- Priority Inversion Demo ---\n");
    printf("Task H (prio=3) needs mutex.\n");
    printf("Task L (prio=1) holds mutex, is preempted by Task M (prio=2).\n");
    printf("Result WITHOUT priority inheritance: H starves behind M behind L.\n");
    printf("Result WITH priority inheritance (xSemaphoreCreateMutex):\n");
    printf("  L is boosted to prio=3, completes, H runs. M runs last.\n");
}

/* ============================================================
 * TASK 3 — Tick rate and delay accuracy
 * ============================================================ */

uint32_t ms_to_ticks(uint32_t ms)
{
    /* TODO: return pdMS_TO_TICKS(ms) — use the macro */
    return ms * configTICK_RATE_HZ / 1000;
}

uint32_t ticks_to_ms(uint32_t ticks)
{
    /* TODO: return ticks * 1000 / configTICK_RATE_HZ */
    return ticks * 1000 / configTICK_RATE_HZ;
}

/* ============================================================
 * TASK 4 — Stack size estimation
 *
 * Common interview question: how do you choose task stack size?
 * Rules of thumb:
 *   - Count local variables, function call depth, string buffers
 *   - FreeRTOS uxTaskGetStackHighWaterMark() reports unused words
 *   - Start with 512 words, tune down after measuring
 *   - Always add 20% safety margin
 *   - On Cortex-M: each context save = 16 registers × 4 bytes = 64 bytes
 * ============================================================ */

uint32_t estimate_task_stack_bytes(uint32_t max_local_vars_bytes,
                                   uint8_t max_call_depth,
                                   uint32_t interrupt_context_bytes)
{
    /* TODO: Rough estimate:
     * frame_per_call = max_local_vars_bytes / max_call_depth (average)
     * total = frame_per_call * max_call_depth
     *       + interrupt_context_bytes (context saved on ISR entry: 8 regs = 32 bytes)
     *       + 20% safety
     * Round up to nearest power of 2 (common practice)
     * Return in BYTES (divide by 4 to get words for FreeRTOS stack parameter) */
    (void)max_local_vars_bytes; (void)max_call_depth; (void)interrupt_context_bytes;
    return 512;   /* placeholder */
}

/* ============================================================
 * TASK 5 — vTaskDelayUntil pattern (fixed-frequency loop)
 *
 * vTaskDelay(N) = delay N ticks from NOW
 * vTaskDelayUntil(&last_wake, N) = delay until N ticks from LAST WAKE
 *
 * Use vTaskDelayUntil for precise periodic tasks.
 * vTaskDelay drifts because processing time is excluded.
 * ============================================================ */

void periodic_task_correct(void *param)
{
    /* Simulated — shows the pattern */
    TickType_t last_wake = 0;   /* in real code: xTaskGetTickCount() at start */
    const TickType_t period = pdMS_TO_TICKS(10);   /* 10ms period */

    for (int i = 0; i < 3; i++) {
        /* TODO: on real HW: vTaskDelayUntil(&last_wake, period) */
        last_wake += period;   /* simulation */
        printf("Periodic task tick at t=%u ms\n", (unsigned)ticks_to_ms(last_wake));
    }
    (void)param;
}

/* ============================================================
 * TASK 6 — BUG HUNT: task creation and scheduling bugs
 *
 * The code below creates tasks incorrectly.
 * Find 3 bugs.
 * ============================================================ */

void worker_task_fn(void *p) { (void)p; }

void setup_tasks_BUGGY(void)
{
    TaskHandle_t h;

    /* Bug 1: stack size of 32 words (128 bytes) is far too small.
     * FreeRTOS overhead + ISR context = ~100 bytes minimum.
     * Any function call will overflow. Minimum recommended: 128 words. */
    xTaskCreate_sim(worker_task_fn, "Worker", 32, NULL, 2, &h);

    /* Bug 2: priority = 0 is the idle task priority.
     * A real task should have priority >= 1, otherwise it
     * competes with (and can starve) the idle task,
     * preventing heap cleanup and stack watermark checks. */
    xTaskCreate_sim(worker_task_fn, "Worker2", 256, NULL, 0, &h);

    /* Bug 3: vTaskStartScheduler() not called — tasks created but never run.
     * After creating all tasks, MUST call vTaskStartScheduler().
     * Code after this call never executes (scheduler takes over). */
}

/* ============================================================
 * SELF-TEST
 * ============================================================ */

int main(void)
{
    create_tasks();
    led_task(NULL);
    sensor_task(NULL);
    priority_inversion_demo();
    periodic_task_correct(NULL);

    assert(ms_to_ticks(500)  == 500);
    assert(ticks_to_ms(1000) == 1000);

    printf("\nAll RTOS task tests PASSED.\n");
    return 0;
}

/* ============================================================
 * INTERVIEW QUESTIONS
 * ============================================================
 *
 * Q1: What is the difference between vTaskDelay and vTaskDelayUntil?
 *     When does it matter?
 *     Answer: TODO
 *
 * Q2: Explain priority inversion. What RTOS primitive prevents it?
 *     Answer: TODO
 *
 * Q3: On FreeRTOS with preemption enabled, two tasks at the same priority:
 *     does one preempt the other? What configures this behavior?
 *     Answer: TODO
 *
 * Q4: A task's stack high-water mark is 8 words. Is this safe?
 *     What do you do about it?
 *     Answer: TODO
 *
 * Q5: What happens to the heap and idle task if you never call
 *     vTaskStartScheduler()?
 *     Answer: TODO
 *
 * Q6: Your system has 5 tasks. The highest-priority task runs every 1ms.
 *     Draw a timeline showing how lower-priority tasks get CPU time.
 *     Answer: TODO
 */
