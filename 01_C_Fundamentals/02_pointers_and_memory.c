/*
 * ============================================================
 * EMBEDDED INTERVIEW PREP
 * Topic : Pointers & Memory in Embedded C
 * File  : 01_C_Fundamentals/02_pointers_and_memory.c
 * ============================================================
 *
 * Pointers are WHERE most interview candidates fail.
 * This file covers: pointer arithmetic, void*, function pointers,
 * const correctness, pointer-to-register, and common pitfalls.
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* ============================================================
 * THEORY
 * ============================================================
 *
 * Pointer basics:
 *   uint8_t  *p8  → points to 1-byte value
 *   uint16_t *p16 → points to 2-byte value, p16+1 = p16+2 bytes
 *   uint32_t *p32 → points to 4-byte value, p32+1 = p32+4 bytes
 *
 * Memory-mapped registers in embedded:
 *   #define GPIOA_ODR  (*((volatile uint32_t*)0x40020014))
 *   volatile tells the compiler: NEVER cache this in a register.
 *
 * const correctness:
 *   const uint8_t *p     → pointer to const (data can't change)
 *   uint8_t *const p     → const pointer (address can't change)
 *   const uint8_t *const p → both immutable
 *
 * Function pointers:
 *   void (*isr_handler)(void)   → pointer to function taking void, returning void
 *   int  (*cmp)(const void*, const void*)  → qsort comparator
 * ============================================================ */


/* ============================================================
 * TASK 1 — Pointer arithmetic on a byte buffer
 *
 * A UART receive buffer contains a raw protocol frame.
 * Parse it using only pointer arithmetic — no array indexing [].
 *
 * Frame format (8 bytes):
 *   [0]     start byte  = 0xAA
 *   [1]     command     = uint8_t
 *   [2:3]   payload_len = uint16_t, big-endian
 *   [4:7]   payload     = 4 bytes of data
 * ============================================================ */

typedef struct {
    uint8_t  command;
    uint16_t payload_len;
    uint8_t  payload[4];
    int      valid;
} Frame;

Frame parse_frame(const uint8_t *buf)
{
    Frame f = {0};

    /* TODO: check *(buf + 0) == 0xAA, set f.valid = 0 if not */
    /* TODO: f.command     = *(buf + 1) */
    /* TODO: f.payload_len = (uint16_t)(*(buf + 2) << 8) | *(buf + 3)  ← big-endian */
    /* TODO: copy 4 bytes from (buf + 4) to f.payload using pointer arithmetic,
     *       no memcpy, no array indexing — just *(dst+i) = *(src+i) */
    /* TODO: f.valid = 1 */

    (void)buf;
    return f;
}

/* ============================================================
 * TASK 2 — Memory-mapped register access
 *
 * On a real MCU you would have:
 *   #define PERIPH_BASE 0x40000000
 * Here we use a local struct to simulate registers.
 *
 * Rule: ALWAYS use volatile for memory-mapped registers.
 *       Without it the compiler may eliminate the read/write.
 * ============================================================ */

typedef struct {
    volatile uint32_t CTRL;   /* offset 0x00 */
    volatile uint32_t STATUS; /* offset 0x04 */
    volatile uint32_t DATA;   /* offset 0x08 */
} FakePeripheral;

#define CTRL_ENABLE   (1u << 0)
#define CTRL_RESET    (1u << 1)
#define STATUS_READY  (1u << 0)
#define STATUS_ERROR  (1u << 1)

void periph_init(FakePeripheral *p)
{
    /* TODO: reset the peripheral (set CTRL_RESET bit) */
    /* TODO: clear reset (clear CTRL_RESET bit) */
    /* TODO: enable the peripheral (set CTRL_ENABLE bit) */
    (void)p;
}

int periph_wait_ready(FakePeripheral *p, uint32_t timeout_loops)
{
    /* TODO: poll STATUS_READY bit until set or timeout_loops exhausted
     * Return 0 on ready, -1 on timeout */
    (void)p; (void)timeout_loops;
    return -1;
}

void periph_write(FakePeripheral *p, uint32_t value)
{
    /* TODO: wait for STATUS_READY (call periph_wait_ready with 1000 loops)
     * if not ready: return without writing
     * write value to DATA register */
    (void)p; (void)value;
}

/* ============================================================
 * TASK 3 — void* and type punning
 *
 * memcpy() takes void* — the standard way to copy any type.
 * Type punning via union is defined behaviour in C (not C++).
 * ============================================================ */

/* Implement your own memcpy using uint8_t pointer iteration */
void *my_memcpy(void *dst, const void *src, size_t n)
{
    /* TODO: cast dst and src to uint8_t*
     * copy n bytes one-by-one
     * return original dst pointer */
    (void)dst; (void)src; (void)n;
    return dst;
}

/* Read a float from a byte buffer (no memcpy, use union) */
float bytes_to_float_union(const uint8_t buf[4])
{
    /* TODO: declare a union { float f; uint8_t b[4]; } u;
     * copy 4 bytes from buf into u.b
     * return u.f
     * This is legal in C99 and later — NOT in C++ */
    (void)buf;
    return 0.0f;
}

/* ============================================================
 * TASK 4 — Function pointers (callbacks and dispatch tables)
 *
 * Embedded systems use function pointers for:
 *   - Interrupt vector tables
 *   - Command dispatch (avoid long if-else chains)
 *   - Plugin / callback patterns
 * ============================================================ */

typedef void (*EventCallback)(uint8_t event_id, uint32_t data);

typedef struct {
    uint8_t        event_id;
    EventCallback  handler;
} EventEntry;

static void on_button_press(uint8_t id, uint32_t data)
{
    printf("Button event %u: %u\n", id, (unsigned)data);
}

static void on_sensor_alert(uint8_t id, uint32_t data)
{
    printf("Sensor alert %u: %u\n", id, (unsigned)data);
}

static void on_comm_error(uint8_t id, uint32_t data)
{
    printf("Comm error %u: code=%u\n", id, (unsigned)data);
}

/* Dispatch table — maps event IDs to handlers */
static const EventEntry g_dispatch[] = {
    { 0x01, on_button_press },
    { 0x02, on_sensor_alert },
    { 0x10, on_comm_error   },
};
#define DISPATCH_SIZE (sizeof(g_dispatch) / sizeof(g_dispatch[0]))

void dispatch_event(uint8_t event_id, uint32_t data)
{
    /* TODO: iterate g_dispatch, find matching event_id, call handler
     * If no match: print "Unknown event: <id>" */
    (void)event_id; (void)data;
}

/* ============================================================
 * TASK 5 — const correctness
 *
 * const is a contract. Getting it right prevents bugs and
 * allows the compiler to place data in flash (ROM) on MCUs.
 * ============================================================ */

/* Classify each declaration — which pointer and/or data is const? */

/*  const uint8_t *p1  → data is const, pointer is mutable
 *  uint8_t *const p2  → pointer is const, data is mutable
 *  const uint8_t *const p3 → both const
 */

/* Implement: sum all bytes in a read-only buffer */
uint32_t sum_bytes(const uint8_t *buf, size_t len)
{
    /* TODO: iterate, sum — buf is read-only, you must not write to it */
    (void)buf; (void)len;
    return 0;
}

/* Flash lookup table — should live in ROM on an MCU.
 * TODO: add the correct qualifiers so this is placed in .rodata */
uint8_t /* TODO: qualifiers here */ crc_table[8] = {
    0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15
};

/* ============================================================
 * TASK 6 — BUG HUNT: pointer pitfalls
 *
 * The function below should return the maximum value in an array.
 * It has 3 bugs. Find and mark each one.
 * ============================================================ */

uint8_t find_max_BUGGY(uint8_t *arr, int len)
{
    uint8_t *max = arr;           /* Bug 1: ??? */

    for (int i = 0; i < len; i++) {
        if (arr[i] > *max) {
            max = arr + i;
        }
    }

    return *max;                  /* Bug 2: ??? */
    /* Bug 3: No check for arr == NULL or len == 0 before dereferencing */
}

/* ============================================================
 * INTERVIEW QUESTIONS
 * ============================================================
 *
 * Q1: What is the difference between NULL, 0, and '\0' in C?
 *     Answer: TODO
 *
 * Q2: Why must hardware register pointers be declared volatile?
 *     What specific optimisation does volatile prevent?
 *     Answer: TODO
 *
 * Q3: A firmware function receives a buffer pointer. What checks
 *     should you always perform before using it?
 *     Answer: TODO
 *
 * Q4: What is a dangling pointer? Give an embedded example
 *     where this causes a hard-to-find bug.
 *     Answer: TODO
 *
 * Q5: How do you represent the address 0x20000000 as a pointer
 *     to a uint32_t register in C? Write the exact declaration.
 *     Answer: TODO
 *
 * Q6: What is strict aliasing and why does it matter in embedded C?
 *     Answer: TODO
 */
