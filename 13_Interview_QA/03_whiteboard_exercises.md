# Whiteboard Exercises — Live Coding in Interviews

> These are the problems interviewers hand you a marker and say:
> "Write this on the board." Practice writing clean C without IDE help.

---

## Exercise 1 — Implement memcpy

```c
// Write memcpy without using the standard library.
void *my_memcpy(void *dst, const void *src, size_t n);
```

**Key points to mention:**
- Use `uint8_t *` for byte-by-byte copy
- `dst` and `src` must not overlap (use `memmove` for overlapping)
- Return `dst` (matches standard signature)
- Bonus: optimize with 32-bit copies for aligned data

**Solution:**
```c
void *my_memcpy(void *dst, const void *src, size_t n) {
    uint8_t       *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (n--) *d++ = *s++;
    return dst;
}
```

---

## Exercise 2 — Count Set Bits (popcount)

```c
// Count number of 1-bits in a 32-bit value.
// Two approaches: naive loop and Brian Kernighan's trick.
uint8_t count_bits(uint32_t x);
```

**Naive:**
```c
uint8_t count_bits_naive(uint32_t x) {
    uint8_t count = 0;
    while (x) { count += (x & 1); x >>= 1; }
    return count;
}
```

**Brian Kernighan (faster — only iterates for each SET bit):**
```c
uint8_t count_bits(uint32_t x) {
    uint8_t count = 0;
    while (x) { x &= (x - 1); count++; }  // clears the lowest set bit each time
    return count;
}
```

**Interview follow-up:** On ARM Cortex-M there's a hardware instruction: `__builtin_popcount(x)` or the `VCNT` instruction on M4. When would you use it?

---

## Exercise 3 — Reverse Bits

```c
// Reverse all 32 bits: 0b10110000...00 → 0b00...00001101
uint32_t reverse_bits(uint32_t x);
```

**Solution:**
```c
uint32_t reverse_bits(uint32_t x) {
    uint32_t result = 0;
    for (int i = 0; i < 32; i++) {
        result = (result << 1) | (x & 1);
        x >>= 1;
    }
    return result;
}
```

---

## Exercise 4 — Ring Buffer (SPSC)

```c
// Implement a lock-free single-producer single-consumer ring buffer.
// Size must be power of 2.
typedef struct { uint8_t buf[64]; uint8_t head; uint8_t tail; } Ring;
int  ring_push(Ring *r, uint8_t byte);
int  ring_pop(Ring *r, uint8_t *out);
```

**Solution:**
```c
#define MASK 0x3Fu

int ring_push(Ring *r, uint8_t byte) {
    if (((r->head + 1) & MASK) == r->tail) return -1;  // full
    r->buf[r->head & MASK] = byte;
    r->head = (r->head + 1) & MASK;
    return 0;
}

int ring_pop(Ring *r, uint8_t *out) {
    if (r->head == r->tail) return -1;  // empty
    *out = r->buf[r->tail & MASK];
    r->tail = (r->tail + 1) & MASK;
    return 0;
}
```

**Interview follow-up:** Why must head and tail be `volatile`? What is the memory ordering concern on multi-core?

---

## Exercise 5 — CRC-16 Modbus

```c
// Implement CRC-16/IBM (used by Modbus RTU).
// Polynomial: 0xA001 (reflected), Init: 0xFFFF
uint16_t crc16_modbus(const uint8_t *data, uint16_t len);
```

**Solution:**
```c
uint16_t crc16_modbus(const uint8_t *data, uint16_t len) {
    uint16_t crc = 0xFFFF;
    while (len--) {
        crc ^= *data++;
        for (int i = 0; i < 8; i++)
            crc = (crc & 1) ? (crc >> 1) ^ 0xA001u : crc >> 1;
    }
    return crc;
}
```

---

## Exercise 6 — Endianness Conversion

```c
// Read a big-endian uint32 from a byte buffer.
// No memcpy, no casting through pointer (UB).
uint32_t read_be32(const uint8_t *buf);
```

**Solution:**
```c
uint32_t read_be32(const uint8_t *buf) {
    return ((uint32_t)buf[0] << 24) |
           ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] <<  8) |
           ((uint32_t)buf[3]);
}
```

**Interview follow-up:** Why is `*(uint32_t*)buf` wrong? (Strict aliasing violation + potential misalignment.)

---

## Exercise 7 — State Machine for UART Packet Parser

```c
// Draw a state machine for parsing this frame:
// [0xAA][CMD:1][LEN:1][DATA:LEN][CRC_LO:1][CRC_HI:1]
// Write the feed_byte() function.
```

**States:** WAIT_SOF → CMD → LEN → DATA → CRC_LO → CRC_HI

```c
typedef enum { ST_SOF, ST_CMD, ST_LEN, ST_DATA, ST_CRC_LO, ST_CRC_HI } State;

typedef struct { State s; uint8_t cmd, len, idx, payload[64], crc_lo; } Parser;

int feed_byte(Parser *p, uint8_t b) {
    switch (p->s) {
        case ST_SOF:    if (b == 0xAA) p->s = ST_CMD; break;
        case ST_CMD:    p->cmd = b; p->s = ST_LEN; break;
        case ST_LEN:    p->len = b; p->idx = 0;
                        p->s = b ? ST_DATA : ST_CRC_LO; break;
        case ST_DATA:   p->payload[p->idx++] = b;
                        if (p->idx == p->len) p->s = ST_CRC_LO; break;
        case ST_CRC_LO: p->crc_lo = b; p->s = ST_CRC_HI; break;
        case ST_CRC_HI: p->s = ST_SOF; return 1;  // frame complete
    }
    return 0;
}
```

---

## Exercise 8 — Detect Endianness at Runtime

```c
// Write a one-liner or short function that detects
// whether the system is little-endian at runtime.
int is_little_endian(void);
```

**Solution:**
```c
int is_little_endian(void) {
    uint16_t x = 1;
    return *(uint8_t *)&x == 1;
}
// On LE: value 0x0001 stored as [01][00] — byte[0] == 1 ✓
// On BE: value 0x0001 stored as [00][01] — byte[0] == 0 ✗
```

---

## Exercise 9 — Fixed-Size Memory Pool

```c
// Implement a memory pool for 8 blocks of 32 bytes each.
// O(1) alloc and free.
```

```c
#define POOL_N     8
#define POOL_SIZE  32

typedef struct { uint8_t storage[POOL_N][POOL_SIZE]; uint8_t used[POOL_N]; } Pool;

void *pool_alloc(Pool *p) {
    for (int i = 0; i < POOL_N; i++) {
        if (!p->used[i]) { p->used[i] = 1; return p->storage[i]; }
    }
    return NULL;
}

void pool_free(Pool *p, void *ptr) {
    for (int i = 0; i < POOL_N; i++) {
        if (p->storage[i] == (uint8_t *)ptr) { p->used[i] = 0; return; }
    }
}
```

---

## Exercise 10 — GPIO Bit Manipulation Quiz

Answer WITHOUT running the code:

```c
uint32_t reg = 0b00101010;

// Q1: Set bit 3
reg |= (1u << 3);   // Answer: 0b00101010 | 0b00001000 = 0b00101010... = 0x32

// Q2: Clear bit 1
reg &= ~(1u << 1);  // Answer: clear bit 1

// Q3: Toggle bit 5
reg ^= (1u << 5);   // Answer: flip bit 5

// Q4: Read bit 3
uint8_t val = (reg >> 3) & 1u;  // Answer: 0 or 1

// Q5: Set bits [5:3] to value 0b101
reg = (reg & ~(0b111u << 3)) | (0b101u << 3);
```

---

## General Whiteboard Tips

1. **Narrate your thinking** — "I'll use a ring buffer here because it's lock-free for single-producer single-consumer..."
2. **Handle edge cases aloud** — "What if len is 0? I'll add a guard here."
3. **Name your variables clearly** — The interviewer reads your code; `head` beats `h`.
4. **Write the test case first** — "For input `[0xAA, 0x01, 0x00, 0xC5, 0x0E]`, expected output is ..."
5. **Admit uncertainty precisely** — "I know the polynomial is 0xA001 but I'd double-check the initial value from the Modbus spec."
6. **Don't start coding immediately** — spend 1 minute planning. Say what you're going to write before you write it.
