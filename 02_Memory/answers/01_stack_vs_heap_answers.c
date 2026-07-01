/*
 * ANSWERS: 02_Memory/01_stack_vs_heap.c
 * ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* ============================================================
 * TASK 1 — Variable classification answers
 * ============================================================
 *
 * int g_counter = 42;          → .data  (initialized global, copied from flash at startup)
 * int g_uninitialized;         → .bss   (zero-initialized global, zeroed at startup)
 * const int g_config_value=100;→ .rodata / flash (const global — stays in ROM)
 *
 * void task1_classify(void) {
 *   int local_var = 5;         → stack  (local variable, lives in stack frame)
 *   static int persist = 0;    → .data  (static local — survives calls, NOT on stack)
 *   int *dyn = malloc(64);     → pointer on stack; pointed-to memory on HEAP
 * }
 */

int       g_counter       = 42;       /* .data */
int       g_uninitialized;            /* .bss  */
const int g_config_value  = 100;      /* .rodata (flash) */

void task1_classify(void)
{
    int        local_var = 5;         /* stack */
    static int persist   = 0;         /* .data — static! */
    int       *dyn       = malloc(64);/* dyn → stack, *dyn → heap */
    (void)local_var; (void)persist;
    if (dyn) free(dyn);
}

/* ============================================================
 * TASK 2 — Iterative implementations
 * ============================================================ */

uint32_t fib_iterative(uint32_t n)
{
    if (n <= 1) return n;
    uint32_t prev = 0, curr = 1;
    for (uint32_t i = 2; i <= n; i++) {
        uint32_t next = prev + curr;
        prev = curr;
        curr = next;
    }
    return curr;
}

uint32_t power_iterative(uint32_t base, uint32_t exp)
{
    uint32_t result = 1;
    while (exp--) result *= base;
    return result;
}

/* ============================================================
 * TASK 3 — Memory pool
 * ============================================================ */

#define POOL_BLOCK_SIZE  32u
#define POOL_NUM_BLOCKS  16u

typedef struct {
    uint8_t  storage[POOL_NUM_BLOCKS][POOL_BLOCK_SIZE];
    uint8_t  used[POOL_NUM_BLOCKS];
    uint8_t  num_free;
} MemPool;

void pool_init(MemPool *pool)
{
    memset(pool->storage, 0, sizeof(pool->storage));
    memset(pool->used,    0, sizeof(pool->used));
    pool->num_free = POOL_NUM_BLOCKS;
}

void *pool_alloc(MemPool *pool)
{
    for (uint8_t i = 0; i < POOL_NUM_BLOCKS; i++) {
        if (!pool->used[i]) {
            pool->used[i] = 1;
            pool->num_free--;
            return pool->storage[i];
        }
    }
    return NULL;
}

void pool_free(MemPool *pool, void *ptr)
{
    uint8_t *p = (uint8_t *)ptr;
    for (uint8_t i = 0; i < POOL_NUM_BLOCKS; i++) {
        if (pool->storage[i] == p) {
            if (!pool->used[i]) return;  /* double-free protection */
            pool->used[i] = 0;
            pool->num_free++;
            return;
        }
    }
}

uint8_t pool_num_free(const MemPool *pool) { return pool->num_free; }

/* ============================================================
 * TASK 4 — Stack watermark
 * ============================================================ */

#define FAKE_STACK_SIZE  256u
static uint8_t fake_stack[FAKE_STACK_SIZE];

void stack_paint(void)
{
    memset(fake_stack, 0xA5, FAKE_STACK_SIZE);
}

uint32_t stack_get_high_water_mark(void)
{
    /* Count bytes from the END that are still 0xA5 (never touched) */
    uint32_t unused = 0;
    for (int i = (int)FAKE_STACK_SIZE - 1; i >= 0; i--) {
        if (fake_stack[i] == 0xA5u) unused++;
        else break;
    }
    return unused;
}

/* ============================================================
 * TASK 5 — Linker section attributes ANSWERS
 * ============================================================
 *
 * __attribute__((section(".ccm"))) uint8_t fast_dma_buffer[1024];
 *   → Places buffer in Core-Coupled Memory on STM32F4 (64 KB at 0x10000000).
 *   → CPU access is faster than SRAM1; DMA CANNOT access CCM.
 *
 * __attribute__((section(".ramfunc"))) void flash_unlock_sequence(void)
 *   → Places function in SRAM so it executes from RAM during flash erase.
 *   → During flash programming, executing from flash is undefined behavior.
 *   → Startup code must copy .ramfunc section from flash to SRAM.
 */

__attribute__((section(".ccm_sim")))
uint8_t fast_dma_buffer[1024];         /* ".ccm" on real target */

__attribute__((section(".ramfunc_sim")))
void flash_unlock_sequence(void) {}    /* ".ramfunc" on real target */

/* ============================================================
 * TASK 6 — Bug hunt FIXED
 *
 * Bug 1: no NULL check after malloc(sizeof(Packet))
 *        If heap is full, p is NULL. p->data = malloc(len) → crash (NULL deref).
 *        FIX: if (!p) return NULL;
 *
 * Bug 2: malloc(0) is implementation-defined (may return NULL or unique pointer).
 *        Calling memcpy with NULL src/dst is UB.
 *        FIX: if (len == 0) { p->data = NULL; p->len = 0; return p; }
 *             Or: malloc(len > 0 ? len : 1);
 *
 * Bug 3 (in destroy): no NULL check on p before accessing p->data.
 *        If p is NULL: p->data crash.
 *        FIX: if (!p) return;
 *
 * Bug 4: p->data = NULL after free(p). Writing to freed memory is UB.
 *        The memory at p's location may already be reused by another allocation.
 *        FIX: the caller should null their pointer: caller_ptr = NULL;
 *             Or pass pointer-to-pointer: void destroy(Packet **pp)
 *             { free((*pp)->data); free(*pp); *pp = NULL; }
 * ============================================================ */

typedef struct { uint8_t *data; uint16_t len; } Packet;

Packet *create_packet_fixed(const uint8_t *src, uint16_t len)
{
    Packet *p = malloc(sizeof(Packet));
    if (!p) return NULL;                    /* Bug 1 fix */
    if (len == 0) {
        p->data = NULL; p->len = 0;
        return p;                           /* Bug 2 fix */
    }
    p->data = malloc(len);
    if (!p->data) { free(p); return NULL; }
    memcpy(p->data, src, len);
    p->len = len;
    return p;
}

void destroy_packet_fixed(Packet **pp)
{
    if (!pp || !*pp) return;               /* Bug 3 fix */
    free((*pp)->data);
    free(*pp);
    *pp = NULL;                            /* Bug 4 fix: caller's pointer nulled */
}

/* ============================================================
 * INTERVIEW QUESTION ANSWERS
 * ============================================================

Q1: Why do safety standards (IEC 61508, MISRA) prohibit dynamic allocation?

A: (1) Non-deterministic timing: malloc() runtime depends on heap state — can
       take microseconds or milliseconds, unacceptable in real-time systems.
   (2) Fragmentation: after many alloc/free cycles, free memory exists in
       non-contiguous blocks. A large allocation may fail even when total
       free bytes > requested size.
   (3) No failure recovery: embedded systems can't show "out of memory" dialog.
       malloc failure → NULL → crash if unchecked.
   (4) Hard to analyze: static analysis tools can't prove memory safety with
       dynamic allocation.
   Alternative: static allocation at startup, fixed-size memory pools.

Q2: Difference between .data and .bss? Why doesn't .bss take flash space?

A: .data: initialized globals (int x = 5). The value 5 must be in flash so
   startup code can copy it to SRAM. Flash contains: [initial values for .data].
   .bss: zero-initialized globals (int y;). The entire .bss region is zeroed
   by startup code — no need to store zeros in flash (zeros are implicit).
   Flash only stores start address and size of .bss so startup code knows
   how much to zero. Saves flash proportional to the size of .bss.

Q3: FreeRTOS stack watermark = 12 words. Safe?

A: 12 words = 48 bytes remaining. Whether it's safe depends on context:
   - If the task never recurses deeper or creates larger locals: probably OK.
   - 48 bytes is tight — a single nested function call with local arrays
     could overflow it under unusual conditions.
   Rule: watermark should be > 10% of total stack, or at least 64 bytes.
   Action: increase stack size by 50% and re-measure. Never ship at < 20 words.

Q4: How can stack overflow corrupt the heap on bare metal?

A: Stack grows downward. Heap grows upward. On a typical embedded layout:
   [.bss][HEAP→][...free...][←STACK]
   If the stack grows past the bottom of the stack region, it enters the
   free space and then the heap. malloc() internal metadata (block headers,
   free list pointers) gets overwritten by stack frames.
   Result: next malloc() or free() call corrupts the heap → crash or silent
   memory corruption that appears in a completely unrelated code path.

Q5: What is memory fragmentation and why does it matter in long-running systems?

A: After many malloc/free cycles with varying sizes:
   Free: [4KB][2KB][4KB][2KB][4KB][2KB] (total 18KB free)
   Request: malloc(6KB) → FAILS (no single 6KB block exists)
   This is fragmentation — free memory exists but in unusable pieces.
   In embedded: a device running for months/years (industrial, medical) will
   eventually fragment its heap to the point where allocations fail, even
   though there's technically enough memory. The system must be rebooted —
   unacceptable for critical systems.
   Solution: memory pools (all blocks same size → no fragmentation).

Q6: 4KB SRAM, 10 concurrent sensor readings of 64 bytes each. Design?

A: Use a static memory pool:
   static uint8_t sensor_pool[10][64];  // 640 bytes = 15.6% of 4KB
   static uint8_t pool_used[10] = {0};
   alloc(): scan pool_used for free slot, return pointer. O(1) average.
   free():  clear pool_used bit.
   Remaining 3456 bytes: stack (512B per task × 5 tasks = 2560B) + .bss + heap.
   Never malloc() in a system with 4KB SRAM.
*/

int main(void)
{
    assert(fib_iterative(0)  == 0);
    assert(fib_iterative(1)  == 1);
    assert(fib_iterative(10) == 55);
    assert(fib_iterative(20) == 6765);

    assert(power_iterative(2, 10) == 1024);
    assert(power_iterative(3, 3)  == 27);

    MemPool pool;
    pool_init(&pool);
    assert(pool_num_free(&pool) == POOL_NUM_BLOCKS);

    void *a = pool_alloc(&pool);
    void *b = pool_alloc(&pool);
    assert(a && b && a != b);
    assert(pool_num_free(&pool) == POOL_NUM_BLOCKS - 2);

    pool_free(&pool, a);
    assert(pool_num_free(&pool) == POOL_NUM_BLOCKS - 1);
    pool_free(&pool, a);   /* double-free — must be ignored */
    assert(pool_num_free(&pool) == POOL_NUM_BLOCKS - 1);

    stack_paint();
    assert(stack_get_high_water_mark() == FAKE_STACK_SIZE);
    fake_stack[255] = 0x00;   /* simulate stack usage at top */
    assert(stack_get_high_water_mark() == FAKE_STACK_SIZE - 1);

    uint8_t src[] = {1,2,3,4};
    Packet *pkt = create_packet_fixed(src, 4);
    assert(pkt && pkt->data && pkt->len == 4);
    assert(pkt->data[2] == 3);
    destroy_packet_fixed(&pkt);
    assert(pkt == NULL);

    printf("All memory answers verified.\n");
    return 0;
}
