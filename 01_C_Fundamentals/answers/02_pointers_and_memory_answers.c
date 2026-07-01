/*
 * ANSWERS: 02_pointers_and_memory.c
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* ============================================================
 * TASK 1 — Parse UART frame using pointer arithmetic only
 * ============================================================ */

typedef struct {
    uint8_t  sof;
    uint8_t  cmd;
    uint16_t length;
    uint32_t payload;
    uint8_t  checksum;
} UartFrame;

UartFrame parse_frame(const uint8_t *raw)
{
    UartFrame f;
    f.sof      = *raw;
    f.cmd      = *(raw + 1);
    f.length   = (uint16_t)((uint16_t)(*(raw + 2)) | ((uint16_t)(*(raw + 3)) << 8));
    f.payload  = (uint32_t)(*(raw+4)) | ((uint32_t)(*(raw+5))<<8) |
                 ((uint32_t)(*(raw+6))<<16) | ((uint32_t)(*(raw+7))<<24);
    f.checksum = *(raw + 7);   /* last byte */
    return f;
}

/* ============================================================
 * TASK 2 — Memory-mapped peripheral access
 * ============================================================ */

typedef struct {
    volatile uint32_t CR;
    volatile uint32_t SR;
    volatile uint32_t DR;
    volatile uint32_t RESERVED;
} FakePeripheral;

static FakePeripheral _periph = {0};
FakePeripheral *PERIPH = &_periph;

#define PERIPH_CR_EN   (1u << 0)
#define PERIPH_SR_BUSY (1u << 1)
#define PERIPH_SR_DONE (1u << 0)

void periph_init(FakePeripheral *p)  { p->CR |= PERIPH_CR_EN; }
int  periph_wait_ready(FakePeripheral *p) {
    uint32_t t = 10000;
    while ((p->SR & PERIPH_SR_BUSY) && --t);
    return t ? 0 : -1;
}
void periph_write(FakePeripheral *p, uint32_t data) { p->DR = data; }

/* ============================================================
 * TASK 3 — memcpy and union type punning
 * ============================================================ */

void my_memcpy(uint8_t *dst, const uint8_t *src, uint16_t n)
{
    while (n--) *dst++ = *src++;
}

float bytes_to_float_union(const uint8_t *bytes)
{
    union { uint32_t u; float f; } pun;
    my_memcpy((uint8_t*)&pun.u, bytes, 4);
    return pun.f;
}

/* ============================================================
 * TASK 4 — Function pointer dispatch table
 * ============================================================ */

typedef void (*EventCallback)(void *data);

typedef struct {
    uint8_t       event_id;
    EventCallback callback;
} EventEntry;

static void on_button(void *d) { printf("Button event: %p\n", d); }
static void on_timer(void *d)  { printf("Timer event: %p\n", d); }
static void on_uart(void *d)   { printf("UART event: %p\n", d); }

static EventEntry dispatch_table[] = {
    {0x01, on_button},
    {0x02, on_timer},
    {0x03, on_uart},
};
#define DISPATCH_TABLE_SIZE (sizeof(dispatch_table)/sizeof(dispatch_table[0]))

void dispatch_event(uint8_t event_id, void *data)
{
    for (size_t i = 0; i < DISPATCH_TABLE_SIZE; i++) {
        if (dispatch_table[i].event_id == event_id) {
            dispatch_table[i].callback(data);
            return;
        }
    }
    printf("Unknown event: 0x%02X\n", event_id);
}

/* ============================================================
 * TASK 5 — const correctness
 * ============================================================ */

uint32_t sum_bytes(const uint8_t *data, uint16_t len)
{
    uint32_t sum = 0;
    while (len--) sum += *data++;
    return sum;
}

static const uint8_t crc_table[256] = { /* all zeros for demo */ 0 };

uint8_t crc8_lookup(const uint8_t *data, uint16_t len)
{
    uint8_t crc = 0;
    while (len--) crc = crc_table[crc ^ *data++];
    return crc;
}

/* ============================================================
 * TASK 6 — Bug hunt FIXED
 * ============================================================ */

/*
 * Bug 1: g_rx_pos was not volatile — compiler caches in register.
 *        ISR increments it but main() never sees the change.
 *        FIX: volatile uint8_t g_rx_pos
 *
 * Bug 2: no bounds check before indexing g_rx_buf[g_rx_pos]
 *        If ISR receives more bytes than buffer size, buffer overflow.
 *        FIX: if (g_rx_pos < sizeof(g_rx_buf)) g_rx_buf[g_rx_pos++] = ...
 *
 * Bug 3: arr == NULL not checked before find_max accesses arr[0]
 *        FIX: if (!arr || count == 0) return 0;
 */

static volatile uint8_t g_rx_pos_fixed = 0;
static uint8_t g_rx_buf_fixed[64];

void rx_isr_fixed(uint8_t byte)
{
    if (g_rx_pos_fixed < sizeof(g_rx_buf_fixed))
        g_rx_buf_fixed[g_rx_pos_fixed++] = byte;
}

uint8_t find_max_fixed(const uint8_t *arr, uint8_t count)
{
    if (!arr || count == 0) return 0;
    uint8_t max = arr[0];
    for (uint8_t i = 1; i < count; i++)
        if (arr[i] > max) max = arr[i];
    return max;
}

/* ============================================================
 * INTERVIEW QUESTION ANSWERS
 * ============================================================

Q1: What is the difference between:
    uint8_t *p, const uint8_t *p, uint8_t * const p, const uint8_t * const p?

A: uint8_t *p              — mutable pointer to mutable data. Both p and *p can change.
   const uint8_t *p        — mutable pointer to const data. Can change p (point elsewhere),
                              cannot write *p. Use for read-only input buffers.
   uint8_t * const p       — const pointer to mutable data. Cannot change p (always points
                              to same address), can write *p. Use for memory-mapped registers.
   const uint8_t * const p — const pointer to const data. Neither p nor *p can change.
                              Use for ROM lookup tables via pointer.

Q2: Why is void *p dangerous in embedded?

A: No type safety — any pointer can be cast to void* without warning. The compiler cannot
   detect size mismatches (e.g., passing a uint8_t* where uint32_t* is expected). Also,
   void* cannot be dereferenced directly — you must cast first. In embedded register access
   code, a wrong cast means you access wrong-size data and corrupt adjacent registers.

Q3: What is a dangling pointer? 3 ways to create one.

A: A pointer that refers to memory no longer valid.
   Way 1: Free then use: free(p); *p = 5;
   Way 2: Return address of local: int *f() { int x=1; return &x; }
   Way 3: Pointer to expired stack frame (function returned, another function
           reused the stack — *p now reads stack data from a different context).

Q4: Function pointer call overhead vs regular call?

A: Regular call: compiler knows the address at compile time → direct branch (BL instruction).
   Function pointer: address loaded from memory at runtime → indirect branch (BLX Rn).
   Overhead: 1-2 extra cycles for register load + potential branch predictor miss.
   For dispatch tables called frequently, the table fits in cache and overhead is negligible.
   Don't use function pointers inside tight DSP loops.

Q5: When is memcpy not safe? What do you use instead?

A: memcpy is unsafe when source and destination regions overlap. Writing to dst may corrupt
   src data before it's been copied. Use memmove() — it handles overlapping regions safely
   (copies through an intermediate buffer or reverses direction if needed).
   Example: sliding a buffer left by N bytes: memmove(buf, buf+N, len-N);
*/

int main(void)
{
    /* Test type punning */
    uint8_t ieee754[] = {0x00, 0x00, 0x80, 0x3F};  /* little-endian 1.0f */
    float f = bytes_to_float_union(ieee754);
    assert(f == 1.0f);

    /* Test dispatch */
    dispatch_event(0x01, NULL);
    dispatch_event(0x99, NULL);

    /* Test sum_bytes */
    uint8_t data[] = {1, 2, 3, 4};
    assert(sum_bytes(data, 4) == 10);

    /* Test find_max */
    uint8_t arr[] = {3, 7, 2, 9, 1};
    assert(find_max_fixed(arr, 5) == 9);
    assert(find_max_fixed(NULL, 5) == 0);

    printf("All pointer/memory answers verified.\n");
    return 0;
}
