/*
 * ============================================================
 * EMBEDDED INTERVIEW PREP
 * Topic : Stack vs Heap — Memory Layout in Embedded Systems
 * File  : 02_Memory/01_stack_vs_heap.c
 * ============================================================
 *
 * Memory is finite and precious on MCUs.
 * Getting this wrong = stack overflow, heap fragmentation, or crash.
 * ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* ============================================================
 * THEORY — Embedded memory regions
 * ============================================================
 *
 * Typical ARM Cortex-M memory map:
 *
 *  0x00000000  ┌──────────────┐
 *              │    FLASH     │  .text (code), .rodata (const data)
 *  0x0007FFFF  └──────────────┘
 *
 *  0x20000000  ┌──────────────┐
 *              │    .data     │  initialized globals (copied from flash at startup)
 *              ├──────────────┤
 *              │    .bss      │  zero-initialized globals (zeroed at startup)
 *              ├──────────────┤
 *              │    HEAP      │  malloc/free (grows upward ↑)
 *              │     ↑↑↑      │
 *              │              │
 *              │     ↓↓↓      │
 *              │    STACK     │  local variables, return addresses (grows downward ↓)
 *  0x2001FFFF  └──────────────┘
 *
 * Stack overflow: stack grows into heap or .bss → silent corruption
 * Heap fragmentation: many small alloc/free cycles leave unusable holes
 *
 * Embedded rule of thumb:
 *   Avoid dynamic allocation (malloc/free) in production firmware.
 *   Use static allocation: fixed-size buffers, memory pools.
 *   If you must use dynamic: allocate at startup, never free.
 * ============================================================ */


/* ============================================================
 * TASK 1 — Classify variables: where does each one live?
 *
 * For each variable below, identify: flash, .data, .bss, stack, heap
 * ============================================================ */

int       g_counter      = 42;          /* lives in: TODO */
int       g_uninitialized;              /* lives in: TODO */
const int g_config_value = 100;         /* lives in: TODO */

void task1_classify(void)
{
    int      local_var   = 5;           /* lives in: TODO */
    static int persist   = 0;           /* lives in: TODO */
    int     *dyn         = malloc(64);  /* pointer in stack; data in: TODO */

    (void)local_var; (void)persist;
    if (dyn) free(dyn);
}

/* ============================================================
 * TASK 2 — Stack depth analysis
 *
 * Recursive functions can overflow the stack on MCUs.
 * Implement iterative versions of common recursive algorithms.
 * ============================================================ */

/* Recursive fibonacci — DANGEROUS on MCUs with small stacks */
uint32_t fib_recursive(uint32_t n)
{
    if (n <= 1) return n;
    return fib_recursive(n-1) + fib_recursive(n-2);
}

/* TODO: Implement iterative fibonacci — O(1) stack usage */
uint32_t fib_iterative(uint32_t n)
{
    /* TODO: no recursion, constant stack usage */
    (void)n;
    return 0;
}

/* TODO: Implement iterative power function (base^exp) */
uint32_t power_iterative(uint32_t base, uint32_t exp)
{
    /* TODO: multiply base by itself exp times, no recursion */
    (void)base; (void)exp;
    return 0;
}

/* ============================================================
 * TASK 3 — Memory pool: fixed-size block allocator
 *
 * A memory pool pre-allocates N blocks of fixed size.
 * alloc: O(1), no fragmentation, deterministic.
 * Used in RTOS message queues, packet buffers, sensor data.
 * ============================================================ */

#define POOL_BLOCK_SIZE  32u
#define POOL_NUM_BLOCKS  16u

typedef struct {
    uint8_t  storage[POOL_NUM_BLOCKS][POOL_BLOCK_SIZE];
    uint8_t  used[POOL_NUM_BLOCKS];   /* 1=in use, 0=free */
    uint8_t  num_free;
} MemPool;

void pool_init(MemPool *pool)
{
    /* TODO: zero storage and used arrays
     * TODO: num_free = POOL_NUM_BLOCKS */
    (void)pool;
}

void *pool_alloc(MemPool *pool)
{
    /* TODO: find first block where used[i] == 0
     * mark it as used, decrement num_free
     * return pointer to storage[i]
     * return NULL if no free blocks */
    (void)pool;
    return NULL;
}

void pool_free(MemPool *pool, void *ptr)
{
    /* TODO: find which block ptr points to (pointer arithmetic)
     * validate it's within the pool range
     * mark it as free, increment num_free
     * if ptr not from this pool: return without doing anything */
    (void)pool; (void)ptr;
}

uint8_t pool_num_free(const MemPool *pool)
{
    return pool->num_free;
}

/* ============================================================
 * TASK 4 — Stack watermark measurement
 *
 * FreeRTOS uses stack painting: fill stack with 0xA5 at startup,
 * later count trailing 0xA5 bytes to find high-water mark.
 * Implement the same concept.
 * ============================================================ */

#define FAKE_STACK_SIZE  256u
static uint8_t fake_stack[FAKE_STACK_SIZE];

void stack_paint(void)
{
    /* TODO: fill fake_stack with 0xA5 */
}

uint32_t stack_get_high_water_mark(void)
{
    /* TODO: scan from end of fake_stack toward start (index FAKE_STACK_SIZE-1 down to 0)
     * count how many bytes are STILL 0xA5 (never used)
     * return that count — it is the "slack" remaining
     * (FreeRTOS returns this as unused words, we use bytes) */
    return 0;
}

/* ============================================================
 * TASK 5 — Linker section placement
 *
 * On embedded: you sometimes need to place a buffer at a specific
 * address or in a specific section.
 *
 * Fill in the correct GCC attributes:
 * ============================================================ */

/* TODO: place this 1KB buffer in the ".ccm" section (Core-Coupled Memory on STM32F4)
 * Hint: __attribute__((section(".ccm"))) */
uint8_t fast_dma_buffer[1024];

/* TODO: this function should always be placed in SRAM (not flash)
 * so it can run during flash erase operations
 * Hint: __attribute__((section(".ramfunc"))) or __attribute__((noinline, long_call)) */
void flash_unlock_sequence(void)
{
    /* Flash programming sequence — must not execute from flash */
}

/* ============================================================
 * TASK 6 — BUG HUNT: memory management bugs
 *
 * The code below manages a packet buffer. It has 4 bugs.
 * Find and mark each one.
 * ============================================================ */

typedef struct {
    uint8_t *data;
    uint16_t len;
} Packet;

Packet *create_packet_BUGGY(const uint8_t *src, uint16_t len)
{
    Packet *p = malloc(sizeof(Packet));

    /* Bug 1: ??? */
    /* no NULL check after malloc */

    /* Bug 2: ??? */
    p->data = malloc(len);     /* if len == 0, malloc(0) is implementation-defined */
    memcpy(p->data, src, len);
    p->len = len;
    return p;
}

void destroy_packet_BUGGY(Packet *p)
{
    /* Bug 3: ??? */
    free(p->data);
    free(p);
    p->data = NULL;    /* Bug 4: ??? — p is already freed, writing to it is UB */
}

/* ============================================================
 * SELF-TEST
 * ============================================================ */

static void test_memory(void)
{
    /* Test fibonacci */
    assert(fib_iterative(0)  == 0);
    assert(fib_iterative(1)  == 1);
    assert(fib_iterative(10) == 55);

    /* Test memory pool */
    MemPool pool;
    pool_init(&pool);
    assert(pool_num_free(&pool) == POOL_NUM_BLOCKS);

    void *a = pool_alloc(&pool);
    void *b = pool_alloc(&pool);
    assert(a != NULL && b != NULL && a != b);
    assert(pool_num_free(&pool) == POOL_NUM_BLOCKS - 2);

    pool_free(&pool, a);
    assert(pool_num_free(&pool) == POOL_NUM_BLOCKS - 1);
    pool_free(&pool, a);   /* double-free — pool_free should ignore this */
    assert(pool_num_free(&pool) == POOL_NUM_BLOCKS - 1);

    /* Test stack watermark */
    stack_paint();
    assert(stack_get_high_water_mark() == FAKE_STACK_SIZE);

    printf("All memory tests PASSED.\n");
}

int main(void)
{
    test_memory();
    return 0;
}

/* ============================================================
 * INTERVIEW QUESTIONS
 * ============================================================
 *
 * Q1: Why do most embedded safety standards (IEC 61508, MISRA)
 *     prohibit dynamic memory allocation in production firmware?
 *     Answer: TODO
 *
 * Q2: What is the difference between .data and .bss sections?
 *     Why does .bss not take space in the flash image?
 *     Answer: TODO
 *
 * Q3: FreeRTOS reports a task's stack high-water mark as 12 words.
 *     What does this mean? Is this safe?
 *     Answer: TODO
 *
 * Q4: How can a stack overflow corrupt the heap on a bare-metal system?
 *     Answer: TODO
 *
 * Q5: What is memory fragmentation and why does it matter in a
 *     long-running embedded system?
 *     Answer: TODO
 *
 * Q6: You have 4KB of SRAM and need to handle 10 concurrent sensor
 *     readings of 64 bytes each. Design the memory strategy.
 *     Answer: TODO
 */
