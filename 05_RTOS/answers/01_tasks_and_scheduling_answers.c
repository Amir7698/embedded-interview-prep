/*
 * ANSWERS: 05_RTOS/01_tasks_and_scheduling.c
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* ============================================================
 * INTERVIEW QUESTION ANSWERS
 * ============================================================

Q1: FreeRTOS preemptive scheduling — how does it work?

A: FreeRTOS uses a fixed-priority preemptive scheduler with round-robin
   for tasks of equal priority.
   On every SysTick interrupt (configTICK_RATE_HZ, typically 1 kHz):
   1. SysTick_Handler calls xPortSysTickHandler().
   2. The tick count increments; any task with a matching wake time is
      moved from blocked → ready state.
   3. If a higher-priority task is now ready, PendSV is pended.
   4. PendSV fires (lowest priority, always deferred) and does the context
      switch: saves current task's context (R4-R11 + PSP), loads next task's.
   Rule: the HIGHEST priority ready task ALWAYS runs.
   If two tasks have the same priority: time-sliced round-robin, each runs
   for one tick time slice then yields to the other.

Q2: vTaskDelay vs vTaskDelayUntil. When to prefer each?

A: vTaskDelay(n): blocks for n ticks FROM THE TIME OF THE CALL.
   If the task was woken up 5 ms late (e.g., a higher priority task ran),
   it then delays n ticks → actual period becomes n + jitter. Periods drift.

   vTaskDelayUntil(&xLastWakeTime, n): blocks until the ABSOLUTE wake time
   (xLastWakeTime + n). The scheduler sets xLastWakeTime on wake-up.
   Even if the task woke up 5 ms late this cycle, the next cycle starts
   at the correct absolute time. No drift accumulation.

   Use vTaskDelay: tasks that don't need precise timing (UI, logging).
   Use vTaskDelayUntil: periodic control loops, sensor sampling, PID,
   any task where jitter accumulation matters.

Q3: Stack size of 128 words — why words, not bytes?

A: "Words" = 32-bit values on Cortex-M. 128 words = 512 bytes.
   FreeRTOS stack depth is specified in words because the stack stores
   register values (32-bit) and local variables which are naturally aligned.
   Stack size in bytes = depth × sizeof(StackType_t) = depth × 4.
   Minimum stack = ISR frame (8 regs × 4B = 32B) + task prologue registers
   (R4-R11 × 4B = 32B) + local variables.
   Rule: measure with uxTaskGetStackHighWaterMark(). If < 20 words → increase.

Q4: Priority inversion — concrete example and two solutions.

A: Scenario:
   TaskL (priority 1) takes mutex M → runs.
   TaskH (priority 3) becomes ready → preempts TaskL → tries to take M → BLOCKS.
   TaskM (priority 2) becomes ready → preempts TaskL (higher priority than L) → runs.
   TaskH STARVES because TaskM prevents TaskL from releasing M.

   Solution 1 — Priority Inheritance (xSemaphoreCreateMutex):
   When TaskH blocks on M, TaskL's priority is temporarily raised to TaskH's
   priority (3). TaskM cannot preempt TaskL anymore. TaskL finishes, releases M,
   priority returns to 1, TaskH runs.

   Solution 2 — Priority Ceiling Protocol:
   Mutex M has a predefined "ceiling priority" = max priority of any task that
   might take it. When any task takes M, it runs at the ceiling priority.
   Prevents inversion even without knowing which high-priority task will contend.
   More predictable but requires knowing all mutex users at design time.

Q5: How many tasks can run simultaneously on a single-core MCU? Why?

A: Exactly ONE task runs at any given instant (single core = single pipeline).
   The RTOS creates the ILLUSION of concurrency by rapidly switching tasks
   (context switching, typically every 1 ms = SysTick interval).
   An ISR can interrupt the running task, but it's still one execution thread.
   Total CREATED tasks: limited by RAM (each needs its stack + TCB ~100 bytes).
   At 128 KB SRAM: ~100 tasks of 512-byte stacks + overhead = 50-60 tasks max.
   But in practice: 5-15 tasks is typical for embedded systems.

Q6: Task won't start after xTaskCreate. Debug approach.

A: 1. Check return value: if xTaskCreate returns pdFAIL, RAM is too small for
      the stack + TCB. Reduce stack size or free other memory.
   2. Check vTaskStartScheduler was called. Tasks don't run until scheduler starts.
   3. Check priority: if priority=0 (same as Idle task), the task may not run
      if Idle never yields (configUSE_IDLE_HOOK=1 with infinite loop in hook).
   4. Check stack overflow: if another task overflowed its stack, heap/TCB
      corruption can prevent task from running. Use configCHECK_FOR_STACK_OVERFLOW=2.
   5. Check for an assertion/hardfault in vTaskStartScheduler: insufficient heap
      for idle task stack, or configMINIMAL_STACK_SIZE too small.
   6. Add trace: configUSE_TRACE_FACILITY=1, then use Segger SystemView or
      similar to see task state transitions.
*/

/* ============================================================
 * Simulated FreeRTOS types for compilation without RTOS
 * ============================================================ */

typedef uint32_t TickType_t;
typedef void (*TaskFunction_t)(void *);

typedef struct {
    char     name[16];
    uint32_t priority;
    uint32_t stack_depth_words;
    uint8_t  running;
} SimTask;

#define MAX_TASKS  8
static SimTask g_tasks[MAX_TASKS];
static uint8_t g_task_count = 0;
static uint8_t g_scheduler_started = 0;

/* ============================================================
 * TASK 1 — xTaskCreate simulation
 * ============================================================ */

int xTaskCreate_sim(TaskFunction_t fn, const char *name,
                    uint32_t stack_depth, void *param,
                    uint32_t priority, void **handle)
{
    (void)fn; (void)param; (void)handle;

    /* Validate parameters (catches common bugs) */
    if (priority == 0)        { printf("WARN: priority 0 = Idle priority\n"); }
    if (stack_depth < 64)     { printf("WARN: stack too small (%u words)\n", stack_depth); }
    if (g_task_count >= MAX_TASKS) return 0;  /* pdFAIL */

    SimTask *t = &g_tasks[g_task_count++];
    strncpy(t->name, name, 15);
    t->priority = priority;
    t->stack_depth_words = stack_depth;
    t->running = 0;
    return 1;  /* pdPASS */
}

void vTaskStartScheduler_sim(void)
{
    g_scheduler_started = 1;
    /* Find highest priority task */
    uint32_t max_pri = 0;
    for (int i = 0; i < g_task_count; i++)
        if (g_tasks[i].priority > max_pri) max_pri = g_tasks[i].priority;
    for (int i = 0; i < g_task_count; i++)
        if (g_tasks[i].priority == max_pri) g_tasks[i].running = 1;
}

/* ============================================================
 * TASK 2 — vTaskDelayUntil pattern
 * ============================================================ */

static TickType_t g_simulated_tick = 0;

TickType_t xTaskGetTickCount_sim(void) { return g_simulated_tick; }

/* Correct periodic task pattern */
void periodic_sensor_task_sim(uint32_t period_ticks)
{
    TickType_t xLastWakeTime = xTaskGetTickCount_sim();
    for (int i = 0; i < 5; i++) {
        /* vTaskDelayUntil(&xLastWakeTime, period_ticks) would go here */
        xLastWakeTime += period_ticks;  /* simulate the absolute-time advance */
        /* do work... */
    }
}

/* ============================================================
 * TASK 3 — Priority inversion demo
 * ============================================================ */

typedef struct { uint8_t locked; uint8_t holder_priority; } SimMutex;

void mutex_take_sim(SimMutex *m, uint8_t caller_priority)
{
    if (m->locked) {
        /* Priority inheritance: raise holder to caller's priority if needed */
        if (caller_priority > m->holder_priority)
            m->holder_priority = caller_priority;
        printf("Task pri=%u BLOCKED on mutex (holder raised to %u)\n",
               caller_priority, m->holder_priority);
    } else {
        m->locked = 1;
        m->holder_priority = caller_priority;
    }
}

void mutex_give_sim(SimMutex *m)
{
    m->locked = 0;
    m->holder_priority = 0;
}

/* ============================================================
 * TASK 4 — ms_to_ticks / ticks_to_ms
 * ============================================================ */

#define CONFIG_TICK_RATE_HZ  1000u

TickType_t ms_to_ticks(uint32_t ms)
{
    return (TickType_t)((ms * CONFIG_TICK_RATE_HZ) / 1000u);
}

uint32_t ticks_to_ms(TickType_t ticks)
{
    return (uint32_t)((ticks * 1000u) / CONFIG_TICK_RATE_HZ);
}

/* ============================================================
 * TASK 5 — Bug hunt analysis
 *
 * Bug 1: Stack size = 32 words = 128 bytes.
 *        FreeRTOS minimum is configMINIMAL_STACK_SIZE (typically 128 words).
 *        Even an empty task needs ~40 words for context save.
 *        Result: stack overflow on first function call → HardFault.
 *        FIX: increase to at least 128 words (512 bytes) per task.
 *
 * Bug 2: Priority = 0 = same as Idle task.
 *        In FreeRTOS, idle task runs at priority 0.
 *        Tasks at priority 0 time-share with idle. If idle hook has work
 *        or configIDLE_SHOULD_YIELD=0, user task may barely run.
 *        FIX: user tasks should use priority >= 1. Reserve 0 for idle.
 *
 * Bug 3: vTaskStartScheduler() not called.
 *        xTaskCreate creates tasks in suspended state; they only run
 *        once the scheduler starts. Without vTaskStartScheduler():
 *        tasks never execute, infinite loop at the end of main().
 *        FIX: add vTaskStartScheduler() after all xTaskCreate() calls.
 *        Note: vTaskStartScheduler() should never return. If it does:
 *        heap was too small for the idle task stack.
 * ============================================================ */

int main(void)
{
    /* Task creation tests */
    int r1 = xTaskCreate_sim(NULL, "LED_Task",    128, NULL, 2, NULL);
    int r2 = xTaskCreate_sim(NULL, "Sensor_Task", 256, NULL, 3, NULL);
    assert(r1 == 1 && r2 == 1);
    assert(g_task_count == 2);

    vTaskStartScheduler_sim();
    assert(g_scheduler_started);
    /* Sensor_Task has higher priority → should run first */
    assert(g_tasks[1].running == 1);

    /* ms_to_ticks tests */
    assert(ms_to_ticks(1000) == 1000);
    assert(ms_to_ticks(100)  == 100);
    assert(ticks_to_ms(500)  == 500);

    /* Priority inversion demo */
    SimMutex m = {0};
    mutex_take_sim(&m, 1);  /* Low priority task takes mutex */
    assert(m.locked && m.holder_priority == 1);
    mutex_take_sim(&m, 3);  /* High priority task blocked → raises holder */
    assert(m.holder_priority == 3);  /* priority inherited */
    mutex_give_sim(&m);
    assert(!m.locked);

    printf("All RTOS task/scheduling answers verified.\n");
    return 0;
}
