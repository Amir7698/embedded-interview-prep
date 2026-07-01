/*
 * ANSWERS: 06_Communication_Protocols/01_uart_framing_crc.c
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* ============================================================
 * INTERVIEW QUESTION ANSWERS
 * ============================================================

Q1: CRC vs checksum vs hash — which for embedded protocol?

A: Simple checksum (sum of bytes, XOR): O(n), no hardware needed.
   Detects all 1-bit errors; misses many multi-bit and burst errors.
   Good enough for: short packets, non-critical data, flash memory pages
   where bit errors are uncommon and verified at write time.

   CRC (Cyclic Redundancy Check): division by polynomial over GF(2).
   CRC-16 detects: all 1-bit and 2-bit errors, all odd-bit errors
   (CRC-CCITT), all burst errors of length ≤ 16 bits.
   Hardware CRC peripheral on STM32: single-cycle computation.
   Use for: communication protocols (Modbus, CANopen, UART frames).

   Cryptographic hash (SHA-256): 256-bit output, one-way, collision-resistant.
   Detects accidental AND intentional corruption. Expensive (1000s of cycles).
   Use for: firmware update integrity, secure boot image verification.

   For embedded protocols: CRC-16 is the sweet spot. Fast enough for ISR
   use, much stronger than checksum, no security overhead needed.

Q2: Why polynomial 0xA001 for Modbus CRC-16?

A: Modbus uses CRC-16/IBM (also called CRC-16/ARC).
   Generator polynomial: x^16 + x^15 + x^2 + 1 = 0x8005 in normal form.
   0xA001 is the REFLECTED (LSB-first) representation of 0x8005.
   Reflection: serial data is sent LSB-first, so the polynomial is
   mirrored to process bits from LSB to MSB in a simple bit loop.
   0x8005 reversed: bit15↔bit0, bit14↔bit1, ... = 0xA001.
   The reflected algorithm is simpler to implement in C (right-shift based)
   and identical in result to unreflected left-shift algorithm on reflected data.

Q3: How does byte stuffing work and why is it needed?

A: Problem: the SOF byte (0xAA in our protocol) can appear as data.
   A receiver scanning for SOF would false-trigger on data containing 0xAA.

   Byte stuffing: before transmission, scan the PAYLOAD for reserved bytes
   and escape them:
   0xAA → 0xBB 0x01  (SOF replacement)
   0xBB → 0xBB 0x02  (escape byte itself must be escaped)

   On receive: when 0xBB is seen, set escape flag, next byte decoded:
   0x01 → original was 0xAA
   0x02 → original was 0xBB

   CRC is computed on the UNSTUFFED data. Stuffing is transparent to CRC.
   Length field should reflect the ORIGINAL data length (before stuffing).
   Alternative: HDLC framing with 0x7E start/end and 0x7D escape.
   COBS (Consistent Overhead Byte Stuffing) is more efficient: overhead
   is at most 1 byte per 254 bytes, and null byte (0x00) can be SOF.

Q4: State machine parser vs linear parser. When to use state machine?

A: Linear parser: reads entire packet at once, indexes into buffer.
   Simple but requires complete packet to be in buffer before parsing.
   Problem: with streaming serial data, you can't assume packet boundaries.
   Incoming bytes arrive one at a time in an ISR.

   State machine parser: processes one byte at a time, remembers state.
   States: WAIT_SOF → CMD → LENGTH → PAYLOAD → CRC_LOW → CRC_HIGH
   Each state consumes one byte and transitions to next state.
   Naturally handles: partial packets, noise (WAIT_SOF resets on bad byte),
   back-to-back packets (returns to WAIT_SOF after complete packet).

   Always use state machine for UART/I2C/SPI streaming protocols.
   Linear parser only when you have a reliable framed transport (USB, TCP)
   that guarantees complete packet delivery.

Q5: CRC computed on wrong data — how would you find this bug?

A: Systematic approach:
   1. Confirm which bytes the CRC covers: just payload, or header+payload?
      Protocol spec is authoritative. Most protocols: CRC covers everything
      after SOF, before CRC field itself.
   2. Inject a known packet by hand. Compute CRC manually (online tool or
      Python: crcmod library). Compare with what your code produces.
   3. Add debug print: printf the bytes being CRC'd, byte by byte.
   4. Check byte order: CRC stored LE (low byte first) vs BE? Modbus: LE.
   5. Check initial value and final XOR: CRC-16/IBM: init=0xFFFF, no XOR.
      CRC-16/CCITT: init=0xFFFF or 0x0000 depending on variant.
   6. Use Wireshark or logic analyzer to capture the packet and verify.

Q6: Why is the length field included in the frame header, not inferred from CRC?

A: The receiver needs to know how many bytes to receive for the payload
   BEFORE it can compute the CRC. Without a length field:
   - Receiver would need to scan for end of packet somehow (timeout-based).
   - Timeout introduces variable latency and is error-prone.
   - With byte stuffing, end-of-frame can't be a fixed byte sequence
     unless it's escaped everywhere in the payload.
   Length field: receiver reads LEN bytes → computes CRC on known payload
   → verifies against received CRC. Clean and deterministic.
   Security note: the length field should be validated against a maximum
   before allocating a buffer (prevents buffer overflow on malformed packets).
*/

/* ============================================================
 * TASK 1 — CRC-16/Modbus bit-by-bit
 * ============================================================ */

uint16_t crc16_modbus(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFFu;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            if (crc & 1u)
                crc = (crc >> 1) ^ 0xA001u;
            else
                crc >>= 1;
        }
    }
    return crc;
}

/* ============================================================
 * TASK 2 — CRC-16 table-driven
 * ============================================================ */

static uint16_t g_crc_table[256];
static uint8_t  g_crc_table_ready = 0;

void crc16_build_table(void)
{
    for (uint16_t i = 0; i < 256; i++) {
        uint16_t crc = i;
        for (int b = 0; b < 8; b++) {
            if (crc & 1u) crc = (crc >> 1) ^ 0xA001u;
            else           crc >>= 1;
        }
        g_crc_table[i] = crc;
    }
    g_crc_table_ready = 1;
}

uint16_t crc16_modbus_fast(const uint8_t *data, uint16_t len)
{
    if (!g_crc_table_ready) crc16_build_table();
    uint16_t crc = 0xFFFFu;
    for (uint16_t i = 0; i < len; i++)
        crc = (crc >> 8) ^ g_crc_table[(crc ^ data[i]) & 0xFFu];
    return crc;
}

/* ============================================================
 * TASK 3 — Frame encode
 * ============================================================ */

#define SOF_BYTE   0xAAu
#define FRAME_HDR  4u   /* SOF + CMD + LEN_HI + LEN_LO */
#define FRAME_CRC  2u

uint16_t frame_encode(uint8_t cmd, const uint8_t *payload, uint16_t payload_len,
                      uint8_t *out_buf, uint16_t out_max)
{
    uint16_t total = FRAME_HDR + payload_len + FRAME_CRC;
    if (total > out_max) return 0;

    out_buf[0] = SOF_BYTE;
    out_buf[1] = cmd;
    out_buf[2] = (uint8_t)(payload_len >> 8);
    out_buf[3] = (uint8_t)(payload_len & 0xFF);
    memcpy(&out_buf[4], payload, payload_len);

    /* CRC over CMD + LEN_HI + LEN_LO + PAYLOAD */
    uint16_t crc = crc16_modbus(&out_buf[1], 1u + 2u + payload_len);
    out_buf[4 + payload_len]     = (uint8_t)(crc & 0xFF);  /* CRC low */
    out_buf[4 + payload_len + 1] = (uint8_t)(crc >> 8);    /* CRC high */

    return total;
}

/* ============================================================
 * TASK 4 — Frame parser state machine
 * ============================================================ */

typedef enum {
    PARSER_WAIT_SOF = 0,
    PARSER_CMD,
    PARSER_LEN_HI,
    PARSER_LEN_LO,
    PARSER_PAYLOAD,
    PARSER_CRC_LO,
    PARSER_CRC_HI
} ParserState;

typedef struct {
    ParserState state;
    uint8_t     cmd;
    uint16_t    payload_len;
    uint16_t    payload_idx;
    uint8_t     payload_buf[256];
    uint8_t     crc_lo;
    uint8_t     frame_ready;
    uint8_t     frame_error;
} FrameParser;

void parser_init(FrameParser *p)
{
    memset(p, 0, sizeof(*p));
    p->state = PARSER_WAIT_SOF;
}

void parser_feed(FrameParser *p, uint8_t byte)
{
    p->frame_ready = 0;
    p->frame_error = 0;

    switch (p->state) {
    case PARSER_WAIT_SOF:
        if (byte == SOF_BYTE) p->state = PARSER_CMD;
        break;
    case PARSER_CMD:
        p->cmd   = byte;
        p->state = PARSER_LEN_HI;
        break;
    case PARSER_LEN_HI:
        p->payload_len = (uint16_t)byte << 8;
        p->state = PARSER_LEN_LO;
        break;
    case PARSER_LEN_LO:
        p->payload_len |= byte;
        if (p->payload_len == 0) { p->state = PARSER_CRC_LO; break; }
        if (p->payload_len > sizeof(p->payload_buf)) {
            p->frame_error = 1; p->state = PARSER_WAIT_SOF; break;
        }
        p->payload_idx = 0;
        p->state = PARSER_PAYLOAD;
        break;
    case PARSER_PAYLOAD:
        p->payload_buf[p->payload_idx++] = byte;
        if (p->payload_idx >= p->payload_len) p->state = PARSER_CRC_LO;
        break;
    case PARSER_CRC_LO:
        p->crc_lo = byte;
        p->state  = PARSER_CRC_HI;
        break;
    case PARSER_CRC_HI: {
        uint16_t rx_crc = (uint16_t)p->crc_lo | ((uint16_t)byte << 8);
        /* Reconstruct what CRC was computed over: [cmd, len_hi, len_lo, payload] */
        uint8_t hdr[3] = { p->cmd,
                           (uint8_t)(p->payload_len >> 8),
                           (uint8_t)(p->payload_len & 0xFF) };
        uint16_t calc = crc16_modbus(hdr, 3u);
        if (p->payload_len > 0)
            calc = crc16_modbus_fast(p->payload_buf, p->payload_len);
        /* Simpler: recompute over full header+payload as in encoder */
        /* Re-encode to verify (matches frame_encode logic) */
        uint8_t verify[260];
        verify[0] = p->cmd;
        verify[1] = (uint8_t)(p->payload_len >> 8);
        verify[2] = (uint8_t)(p->payload_len & 0xFF);
        memcpy(&verify[3], p->payload_buf, p->payload_len);
        calc = crc16_modbus(verify, 3u + p->payload_len);
        if (calc == rx_crc) p->frame_ready = 1;
        else                p->frame_error = 1;
        p->state = PARSER_WAIT_SOF;
        break;
    }
    }
}

/* ============================================================
 * TASK 5 — Byte stuffing
 * ============================================================ */

#define STUFF_ESC   0xBBu
#define STUFF_SOF   0xAAu

uint16_t stuff_encode(const uint8_t *in, uint16_t len, uint8_t *out, uint16_t out_max)
{
    uint16_t j = 0;
    for (uint16_t i = 0; i < len; i++) {
        if (in[i] == STUFF_SOF || in[i] == STUFF_ESC) {
            if (j + 2 > out_max) return 0;
            out[j++] = STUFF_ESC;
            out[j++] = (in[i] == STUFF_SOF) ? 0x01u : 0x02u;
        } else {
            if (j + 1 > out_max) return 0;
            out[j++] = in[i];
        }
    }
    return j;
}

uint16_t stuff_decode(const uint8_t *in, uint16_t len, uint8_t *out, uint16_t out_max)
{
    uint16_t j = 0;
    uint8_t  esc = 0;
    for (uint16_t i = 0; i < len; i++) {
        if (esc) {
            if (j >= out_max) return 0;
            out[j++] = (in[i] == 0x01u) ? STUFF_SOF : STUFF_ESC;
            esc = 0;
        } else if (in[i] == STUFF_ESC) {
            esc = 1;
        } else {
            if (j >= out_max) return 0;
            out[j++] = in[i];
        }
    }
    return j;
}

int main(void)
{
    crc16_build_table();

    /* CRC known value: "123456789" → 0xBB3D for CRC-16/IBM */
    const uint8_t test_str[] = "123456789";
    uint16_t crc1 = crc16_modbus(test_str, 9);
    uint16_t crc2 = crc16_modbus_fast(test_str, 9);
    assert(crc1 == crc2);  /* both algorithms must agree */
    assert(crc1 == 0xBB3Du);

    /* Frame encode/decode round trip */
    uint8_t payload[] = {0x01, 0x02, 0x03};
    uint8_t frame[32];
    uint16_t flen = frame_encode(0x10, payload, 3, frame, sizeof(frame));
    assert(flen > 0 && frame[0] == SOF_BYTE && frame[1] == 0x10);

    FrameParser parser;
    parser_init(&parser);
    for (uint16_t i = 0; i < flen; i++) parser_feed(&parser, frame[i]);
    assert(parser.frame_ready);
    assert(parser.cmd == 0x10);
    assert(parser.payload_len == 3);
    assert(parser.payload_buf[0] == 0x01);

    /* Byte stuffing */
    uint8_t raw[]     = {0xAA, 0xBB, 0x01, 0xAA};
    uint8_t stuffed[16], unstuffed[16];
    uint16_t slen = stuff_encode(raw, 4, stuffed, sizeof(stuffed));
    assert(slen == 4 + 3);  /* 2×AA→BB01 + 1×BB→BB02, one 0x01 unchanged */
    uint16_t ulen = stuff_decode(stuffed, slen, unstuffed, sizeof(unstuffed));
    assert(ulen == 4);
    assert(memcmp(raw, unstuffed, 4) == 0);

    printf("All UART framing/CRC answers verified.\n");
    return 0;
}
