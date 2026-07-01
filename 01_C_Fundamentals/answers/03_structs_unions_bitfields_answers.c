/*
 * ANSWERS: 03_structs_unions_bitfields.c
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* ============================================================
 * TASK 1 — sizeof quiz answers
 * ============================================================
 *
 * struct S1 { char a; int b; char c; }
 *   a @ offset 0 (1 byte) + 3 pad → b @ offset 4 (4 bytes) + c @ offset 8 (1 byte) + 3 pad
 *   sizeof = 12
 *
 * struct S2 { int b; char a; char c; }
 *   b @ offset 0 (4 bytes) + a @ offset 4 (1 byte) + c @ offset 5 (1 byte) + 2 pad
 *   sizeof = 8
 *
 * struct S3 { uint16_t x; uint32_t y; uint8_t z; }
 *   x @ offset 0 (2 bytes) + 2 pad → y @ offset 4 (4 bytes) → z @ offset 8 (1 byte) + 3 pad
 *   sizeof = 12
 *
 * struct S4 { uint8_t a; uint8_t b; uint16_t c; uint32_t d; } __attribute__((packed))
 *   No padding: sizeof = 1+1+2+4 = 8
 */

typedef struct { char a; int b; char c; }                            S1;
typedef struct { int b; char a; char c; }                            S2;
typedef struct { uint16_t x; uint32_t y; uint8_t z; }               S3;
typedef struct __attribute__((packed)) { uint8_t a,b; uint16_t c; uint32_t d; } S4;

/* ============================================================
 * TASK 2 — GPIO register map and functions
 * ============================================================ */

typedef struct {
    volatile uint32_t MODER, OTYPER, OSPEEDR, PUPDR;
    volatile uint32_t IDR, ODR, BSRR, LCKR, AFRL, AFRH;
} GPIO_TypeDef;

#define GPIO_MODE_INPUT   0b00u
#define GPIO_MODE_OUTPUT  0b01u
#define GPIO_MODE_AF      0b10u
#define GPIO_MODE_ANALOG  0b11u

static GPIO_TypeDef _GPIOA = {0};
GPIO_TypeDef *GPIOA = &_GPIOA;

void gpio_set_mode(GPIO_TypeDef *gpio, uint8_t pin, uint8_t mode)
{
    gpio->MODER = (gpio->MODER & ~(0b11u << (pin*2))) | ((uint32_t)mode << (pin*2));
}

void gpio_write_pin_atomic(GPIO_TypeDef *gpio, uint8_t pin, uint8_t value)
{
    /* BSRR: bits[15:0] = set, bits[31:16] = reset — single write, atomic */
    if (value)
        gpio->BSRR = (1u << pin);
    else
        gpio->BSRR = (1u << (pin + 16));
}

uint8_t gpio_read_input(GPIO_TypeDef *gpio, uint8_t pin)
{
    return (gpio->IDR >> pin) & 1u;
}

/* ============================================================
 * TASK 3 — ControlReg union
 * ============================================================ */

typedef union {
    uint32_t raw;
    struct {
        uint32_t PE     : 1;   /* bit 0 */
        uint32_t TXIE   : 1;   /* bit 1 */
        uint32_t RXIE   : 1;   /* bit 2 */
        uint32_t RSVD   : 5;   /* bits 7:3 */
        uint32_t SPEED  : 2;   /* bits 9:8 */
        uint32_t RSVD2  : 22;
    } bits;
} ControlReg;

/* PE=1, TXIE=0, RXIE=1 → bits 0,2 set = 0x05
 * SPEED=0b01 at bits[9:8] → 0x100
 * raw = 0x00000105 */
#define EXPECTED_CR_RAW 0x00000105u

/* ============================================================
 * TASK 4 — SensorWord union: big-endian int16 + uint16
 * ============================================================ */

typedef union {
    uint8_t  bytes[4];
    struct {
        int16_t  temperature;   /* big-endian in bytes[0:1] */
        uint16_t humidity;      /* big-endian in bytes[2:3] */
    } fields;
} SensorWord;

void sensor_parse(const uint8_t *raw, int16_t *temp, uint16_t *hum)
{
    /* Big-endian parse — never cast bytes directly (alignment + aliasing UB) */
    *temp = (int16_t)(((uint16_t)raw[0] << 8) | raw[1]);
    *hum  = (uint16_t)(((uint16_t)raw[2] << 8) | raw[3]);
}

/* ============================================================
 * TASK 5 — CommandFrame packed struct
 * ============================================================ */

typedef struct __attribute__((packed)) {
    uint8_t  sof;       /* 0xA5 */
    uint8_t  cmd;
    uint16_t value;     /* little-endian */
    uint8_t  checksum;  /* XOR of cmd ^ value_lo ^ value_hi */
    uint8_t  eof;       /* 0x5A */
} CommandFrame;

void frame_build(CommandFrame *f, uint8_t cmd, uint16_t value)
{
    f->sof      = 0xA5u;
    f->cmd      = cmd;
    f->value    = value;   /* stored LE on LE system */
    f->checksum = cmd ^ (uint8_t)(value & 0xFFu) ^ (uint8_t)(value >> 8);
    f->eof      = 0x5Au;
}

int frame_validate(const CommandFrame *f)
{
    if (f->sof != 0xA5u || f->eof != 0x5Au) return 0;
    uint8_t expected = f->cmd ^ (uint8_t)(f->value & 0xFFu) ^ (uint8_t)(f->value >> 8);
    return (f->checksum == expected) ? 1 : 0;
}

/* ============================================================
 * TASK 6 — Bug hunt FIXED
 *
 * Bug 1: sizeof(PacketHeader*) — size of POINTER (4 or 8 bytes), not struct.
 *        FIX: sizeof(*hdr) or sizeof(PacketHeader)
 *
 * Bug 2: length field doesn't include header size — receiver underestimates total size.
 *        FIX: hdr->length = (uint16_t)(sizeof(*hdr) + payload_len)
 *
 * Bug 3: sequence not byte-swapped — protocol header should be big-endian (network order).
 *        FIX: hdr->sequence = htonl(seq) — or manual BE write
 * ============================================================ */

/* ============================================================
 * INTERVIEW QUESTION ANSWERS
 * ============================================================

Q1: Why does struct S1 { char a; int b; char c; } have sizeof = 12, not 6?

A: The compiler inserts padding to align each member to its natural alignment.
   - `a` (char, 1 byte) at offset 0.
   - 3 bytes padding so `b` (int, 4 bytes) starts at offset 4 (4-byte aligned).
   - `b` occupies offsets 4–7.
   - `c` (char, 1 byte) at offset 8.
   - 3 bytes tail padding so the overall struct size is a multiple of 4
     (required so an array of S1 keeps all `b` members aligned).
   Total: 12 bytes.
   To get 6 bytes: reorder fields (int, char, char) or use __attribute__((packed)).

Q2: What is the risk of packed structs?

A: Packed structs remove padding. Any multi-byte field may be misaligned.
   On Cortex-M0/M0+: misaligned access causes HardFault.
   On Cortex-M3/M4: works but slower (LDR becomes 2 byte loads).
   Taking a pointer to a packed field (e.g., &pkt->length) gives a misaligned
   pointer — passing it to functions expecting aligned pointers is UB.
   Rule: use packed structs only to describe wire formats; copy fields out
   before arithmetic: uint16_t len; memcpy(&len, &pkt->length, 2);

Q3: Why use bit-fields in embedded?

A: Bit-fields describe hardware registers concisely:
   struct { uint32_t PE:1; uint32_t TXIE:1; uint32_t SPEED:2; } CR;
   Saves code vs manual shift/mask. Readable.
   Risks: (1) Bit order is implementation-defined (compiler/endianness dependent).
          (2) Multi-bit fields crossing a word boundary are UB.
          (3) Not safe for hardware registers on all compilers.
   Rule: use bit-fields for readability in non-safety code; use explicit
   shift/mask macros when bit layout MUST be exact (e.g., protocol frames).

Q4: When does union type-punning cause UB in C vs C++?

A: In C (since C99 TC3/C11): reading a union member different from the last
   written is defined behavior — the bytes are reinterpreted. Safe.
   In C++: it is technically UB (strict aliasing rule). Use memcpy instead:
     float f; uint32_t u; memcpy(&u, &f, 4);  // defined in both C and C++

Q5: How do you serialize a struct for transmission over UART?

A: Don't cast the struct to uint8_t* directly — alignment and padding make
   the layout platform-dependent.
   Correct approach: serialize field by field:
     out[0] = pkt.cmd;
     out[1] = pkt.len;
     out[2] = pkt.value & 0xFF;   // LSB first
     out[3] = pkt.value >> 8;
   Or use a packed struct + memcpy (with the caveats above).
   Always document the wire format independently of the C struct.
*/

int main(void)
{
    /* sizeof checks */
    printf("sizeof(S1)=%zu (expected 12)\n", sizeof(S1));
    printf("sizeof(S2)=%zu (expected 8)\n",  sizeof(S2));
    printf("sizeof(S4)=%zu (expected 8)\n",  sizeof(S4));

    /* GPIO test */
    gpio_set_mode(GPIOA, 5, GPIO_MODE_OUTPUT);
    assert((GPIOA->MODER & (0b11u << 10)) == (GPIO_MODE_OUTPUT << 10u));
    gpio_write_pin_atomic(GPIOA, 5, 1);
    assert(GPIOA->BSRR & (1u << 5));

    /* ControlReg test */
    ControlReg cr = {0};
    cr.bits.PE    = 1;
    cr.bits.RXIE  = 1;
    cr.bits.SPEED = 1;
    assert(cr.raw == EXPECTED_CR_RAW);

    /* SensorWord test */
    uint8_t raw[] = {0x01, 0x2C, 0x01, 0xF4}; /* temp=300 (0x012C), hum=500 (0x01F4) */
    int16_t t; uint16_t h;
    sensor_parse(raw, &t, &h);
    assert(t == 300 && h == 500);

    /* CommandFrame test */
    CommandFrame f;
    frame_build(&f, 0x10, 0x1234);
    assert(frame_validate(&f) == 1);
    f.checksum ^= 0xFF;
    assert(frame_validate(&f) == 0);

    printf("All struct/union/bitfield answers verified.\n");
    return 0;
}
