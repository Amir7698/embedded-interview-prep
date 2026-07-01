/*
 * ============================================================
 * EMBEDDED INTERVIEW PREP
 * Topic : Structs, Unions, Bit-fields, Packed Structs
 * File  : 01_C_Fundamentals/03_structs_unions_bitfields.c
 * ============================================================
 *
 * In embedded C, structs map directly to register layouts.
 * Unions enable type-punning. Bit-fields let you name register bits.
 * Knowing alignment & padding is essential — it decides whether
 * your protocol parser works or produces garbage.
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* ============================================================
 * THEORY — Struct padding and alignment
 * ============================================================
 *
 * The compiler inserts padding to align members to their size:
 *   struct { uint8_t a; uint32_t b; }  →  sizeof = 8 (3 bytes padding after a)
 *   struct { uint32_t b; uint8_t a; }  →  sizeof = 8 (3 bytes padding after a)
 *   struct { uint8_t a; uint8_t b; uint16_t c; uint32_t d; } → sizeof = 8 (no padding)
 *
 * To force no padding (for protocol frames / register maps):
 *   __attribute__((packed))   ← GCC/Clang
 *   #pragma pack(1)           ← MSVC and most compilers
 *
 * WARNING: packed structs may cause unaligned access faults on
 *           ARM Cortex-M0/M0+ (no hardware unaligned support).
 *           Use memcpy() to read multi-byte fields from packed structs.
 *
 * Union type punning:
 *   union { float f; uint32_t u; } x;
 *   x.f = 3.14f;
 *   printf("%08X\n", x.u);   ← reads float as raw bytes — legal in C
 * ============================================================ */


/* ============================================================
 * TASK 1 — Predict sizeof()
 *
 * Before running, calculate the size of each struct.
 * Then verify with the assert.
 * ============================================================ */

struct S1 { uint8_t a; uint32_t b; uint8_t c; };
struct S2 { uint32_t b; uint8_t a; uint8_t c; };
struct S3 { uint8_t a; uint8_t b; uint16_t c; uint32_t d; };
struct S4 { uint8_t a; uint16_t b; uint8_t c; uint32_t d; } __attribute__((packed));

void task1_sizeof_quiz(void)
{
    /* TODO: Fill in the expected sizes before running */
    printf("S1: %zu (expected: TODO)\n", sizeof(struct S1));
    printf("S2: %zu (expected: TODO)\n", sizeof(struct S2));
    printf("S3: %zu (expected: TODO)\n", sizeof(struct S3));
    printf("S4: %zu (expected: TODO)\n", sizeof(struct S4));

    /* TODO: uncomment and fix the expected values
    assert(sizeof(struct S1) == ???);
    assert(sizeof(struct S2) == ???);
    assert(sizeof(struct S3) == ???);
    assert(sizeof(struct S4) == ???);
    */
}

/* ============================================================
 * TASK 2 — Register map as a struct (STM32 GPIO style)
 *
 * Map the STM32 GPIO peripheral register layout.
 * Each register is 32-bit, at 4-byte offsets.
 * Then implement GPIO direction and output functions.
 * ============================================================
 *
 * Offset  Register  Purpose
 * 0x00    MODER     mode (input/output/AF/analog) 2 bits per pin
 * 0x04    OTYPER    output type (push-pull/open-drain) 1 bit per pin
 * 0x08    OSPEEDR   speed 2 bits per pin
 * 0x0C    PUPDR     pull-up/pull-down 2 bits per pin
 * 0x10    IDR       input data register (read-only)
 * 0x14    ODR       output data register
 * 0x18    BSRR      bit set/reset register (atomic)
 * 0x1C    LCKR      lock register
 * 0x20    AFRL      alternate function low
 * 0x24    AFRH      alternate function high
 */

typedef struct {
    volatile uint32_t MODER;
    volatile uint32_t OTYPER;
    volatile uint32_t OSPEEDR;
    volatile uint32_t PUPDR;
    volatile uint32_t IDR;
    volatile uint32_t ODR;
    volatile uint32_t BSRR;
    volatile uint32_t LCKR;
    volatile uint32_t AFRL;
    volatile uint32_t AFRH;
} GPIO_TypeDef;

#define GPIO_MODER_INPUT   0b00u
#define GPIO_MODER_OUTPUT  0b01u
#define GPIO_MODER_AF      0b10u
#define GPIO_MODER_ANALOG  0b11u

void gpio_set_mode(GPIO_TypeDef *gpio, uint8_t pin, uint8_t mode)
{
    /* TODO: MODER has 2 bits per pin at position (pin*2)
     * Clear the 2 bits, then set the mode value
     * mode: GPIO_MODER_INPUT, GPIO_MODER_OUTPUT, etc. */
    (void)gpio; (void)pin; (void)mode;
}

void gpio_write_pin_atomic(GPIO_TypeDef *gpio, uint8_t pin, uint8_t value)
{
    /* TODO: use BSRR register for atomic set/clear (no read-modify-write)
     * BSRR[15:0]  = set bits  (write 1 to set)
     * BSRR[31:16] = reset bits (write 1 to clear)
     * If value == 1: set   pin by writing (1u << pin)       to BSRR
     * If value == 0: clear pin by writing (1u << (pin + 16)) to BSRR */
    (void)gpio; (void)pin; (void)value;
}

uint8_t gpio_read_input(GPIO_TypeDef *gpio, uint8_t pin)
{
    /* TODO: read from IDR, return bit at position 'pin' */
    (void)gpio; (void)pin;
    return 0;
}

/* ============================================================
 * TASK 3 — Bit-fields for register access
 *
 * Bit-fields let you name individual bits inside a register.
 * Convenient for reading, BUT: bit ordering is implementation-defined.
 * Never use bit-fields for protocol parsing across different compilers/machines.
 * Safe use: local-only register abstraction.
 * ============================================================ */

typedef union {
    uint32_t raw;
    struct {
        uint32_t PE    :  1;   /* bit 0: Peripheral enable */
        uint32_t TXIE  :  1;   /* bit 1: TX interrupt enable */
        uint32_t RXIE  :  1;   /* bit 2: RX interrupt enable */
        uint32_t       :  5;   /* bits 3-7: reserved */
        uint32_t SPEED :  3;   /* bits 8-10: speed selection */
        uint32_t       : 21;   /* bits 11-31: reserved */
    } bits;
} ControlReg;

void task3_bitfield_demo(void)
{
    ControlReg cr = {0};

    /* TODO: enable peripheral using bit-field */
    /* TODO: enable TX interrupt using bit-field */
    /* TODO: set speed to 5 using bit-field */
    /* TODO: print cr.raw in hex and verify expected value:
     *       PE=1 (bit0), TXIE=1 (bit1), SPEED=5 (bits 10:8)
     *       expected raw = 0x00000503 */

    printf("Control register raw = 0x%08X (expected 0x00000503)\n", cr.raw);
    assert(cr.raw == 0x00000503u);
}

/* ============================================================
 * TASK 4 — Union for protocol frame parsing
 *
 * A 4-byte sensor data word from a CAN frame:
 *   Bytes [3:2] = temperature in 0.1°C units (int16_t, big-endian)
 *   Bytes [1:0] = humidity in 0.1% units (uint16_t, big-endian)
 *
 * Parse using a union — no pointer casts, no memcpy.
 * ============================================================ */

typedef union {
    uint8_t  raw[4];
    struct {
        /* TODO: declare fields here — remember big-endian means
         * the high byte comes first in the raw array */
        /* Hint: you may need to swap bytes after reading */
    } fields;
} SensorWord;

void task4_parse_sensor_word(const uint8_t data[4],
                              int16_t *temperature_tenth_C,
                              uint16_t *humidity_tenth_pct)
{
    SensorWord sw;
    /* TODO: copy data[0..3] into sw.raw */
    /* TODO: temperature = (int16_t)((sw.raw[0] << 8) | sw.raw[1]) */
    /* TODO: humidity    = (uint16_t)((sw.raw[2] << 8) | sw.raw[3]) */
    (void)data; (void)temperature_tenth_C; (void)humidity_tenth_pct;
}

/* ============================================================
 * TASK 5 — Packed struct for UART frame serialisation
 *
 * Serialise/deserialise a 6-byte command frame:
 *   [0]   SOF     = 0xA5
 *   [1]   CMD     = uint8_t
 *   [2:3] VALUE   = uint16_t, little-endian
 *   [4]   CHECKSUM= XOR of bytes [0:3]
 *   [5]   EOF     = 0x5A
 * ============================================================ */

typedef struct __attribute__((packed)) {
    uint8_t  sof;
    uint8_t  cmd;
    uint16_t value;
    uint8_t  checksum;
    uint8_t  eof;
} CommandFrame;

void build_command_frame(uint8_t cmd, uint16_t value, uint8_t out[6])
{
    CommandFrame *f = (CommandFrame *)out;
    /* TODO: fill sof, cmd, value fields */
    /* TODO: checksum = sof ^ cmd ^ (value & 0xFF) ^ (value >> 8) */
    /* TODO: fill eof */
    (void)cmd; (void)value; (void)f;
}

int validate_command_frame(const uint8_t in[6])
{
    const CommandFrame *f = (const CommandFrame *)in;
    /* TODO: check sof == 0xA5 and eof == 0x5A */
    /* TODO: recompute checksum and compare to f->checksum */
    /* TODO: return 1 if valid, 0 if not */
    (void)f;
    return 0;
}

/* ============================================================
 * TASK 6 — BUG HUNT
 *
 * The function below should build a network packet header.
 * It has 3 bugs. Find and mark each one.
 * ============================================================ */

typedef struct {
    uint8_t  version;
    uint8_t  type;
    uint16_t length;
    uint32_t sequence;
} PacketHeader;

void build_header_BUGGY(PacketHeader *hdr, uint8_t type,
                         uint16_t payload_len, uint32_t seq)
{
    /* Bug 1: ??? */
    memset(hdr, 0, sizeof(PacketHeader *));   /* wrong size — should be sizeof(*hdr) */

    hdr->version  = 1;
    hdr->type     = type;

    /* Bug 2: ??? */
    hdr->length   = payload_len;              /* should include header size:
                                               * payload_len + sizeof(PacketHeader) */

    /* Bug 3: ??? */
    hdr->sequence = seq;                      /* for network protocols should be
                                               * htonl(seq) — big-endian byte order */
}

/* ============================================================
 * INTERVIEW QUESTIONS
 * ============================================================
 *
 * Q1: Why should you NOT use bit-fields to parse a received CAN frame?
 *     Answer: TODO
 *
 * Q2: How do you ensure a struct maps exactly to a hardware register block
 *     with no compiler-inserted padding?
 *     Answer: TODO
 *
 * Q3: What is the safe way to read a float from a byte array in C?
 *     What is the UNSAFE way (that still works but violates strict aliasing)?
 *     Answer: TODO
 *
 * Q4: A packed struct member uint32_t at an odd address — what happens
 *     on ARM Cortex-M0 vs Cortex-M4?
 *     Answer: TODO
 *
 * Q5: You want to overlay a register map struct starting at address 0x40020000.
 *     Write the exact one-liner in C.
 *     Answer: TODO
 */
