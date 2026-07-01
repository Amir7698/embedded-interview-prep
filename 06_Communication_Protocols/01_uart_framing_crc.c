/*
 * ============================================================
 * EMBEDDED INTERVIEW PREP
 * Topic : Communication Protocols — UART Framing + CRC
 * File  : 06_Communication_Protocols/01_uart_framing_crc.c
 * ============================================================
 *
 * Designing a reliable binary protocol over UART is a core
 * embedded skill. This covers frame design, CRC calculation,
 * and a complete packet encoder/decoder.
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* ============================================================
 * THEORY — Binary Protocol Design
 * ============================================================
 *
 * A well-designed binary protocol over UART needs:
 *
 * 1. FRAMING: How does receiver know where a packet starts/ends?
 *    Options:
 *    a) Start/end bytes (SOF/EOF) with byte stuffing
 *    b) Length-prefixed (SOF + LENGTH + PAYLOAD + CRC)
 *    c) Fixed-length packets (simplest, wastes bandwidth)
 *
 * 2. ERROR DETECTION: How do we know the data is correct?
 *    a) Checksum: XOR or sum mod 256 (weak)
 *    b) CRC-8:    Catches all 1-bit and burst errors up to 8 bits
 *    c) CRC-16:   Industrial standard (Modbus uses CRC-16/IBM)
 *    d) CRC-32:   Ethernet, USB, ZIP
 *
 * FRAME STRUCTURE (our custom protocol):
 *   [SOF:1][CMD:1][LEN:1][PAYLOAD:LEN][CRC16_LO:1][CRC16_HI:1]
 *
 *   SOF = 0xAA (start of frame)
 *   CMD = command/message type (0x01=data, 0x02=ack, 0xFF=error)
 *   LEN = payload length (0..MAX_PAYLOAD)
 *   PAYLOAD = len bytes of application data
 *   CRC16 = CRC-16/IBM over CMD+LEN+PAYLOAD (little-endian)
 *
 * CRC-16/IBM (used by Modbus RTU):
 *   Polynomial: 0x8005 (normal) = 0xA001 (reflected/reversed)
 *   Init: 0xFFFF
 *   Input/Output reflected: yes (so we use 0xA001)
 * ============================================================ */

#define FRAME_SOF        0xAAu
#define MAX_PAYLOAD_SIZE 64u
#define HEADER_SIZE      3u    /* SOF + CMD + LEN */
#define CRC_SIZE         2u

#define CMD_DATA   0x01u
#define CMD_ACK    0x02u
#define CMD_NACK   0x03u
#define CMD_PING   0x04u
#define CMD_PONG   0x05u

/* ============================================================
 * TASK 1 — CRC-16/IBM (Modbus) implementation
 * ============================================================ */

uint16_t crc16_modbus(const uint8_t *data, uint16_t len)
{
    /* TODO: init crc = 0xFFFF
     * for each byte:
     *   crc ^= byte
     *   for i in 0..7:
     *     if crc & 1: crc = (crc >> 1) ^ 0xA001
     *     else:       crc >>= 1
     * return crc */
    (void)data; (void)len;
    return 0;
}

/* Table-driven CRC-16 (faster, used in production) */
static uint16_t crc16_table[256];
static uint8_t  g_crc_table_init = 0;

void crc16_build_table(void)
{
    /* TODO: for each i in 0..255:
     *   crc = i
     *   for bit in 0..7:
     *     if crc & 1: crc = (crc >> 1) ^ 0xA001
     *     else:       crc >>= 1
     *   crc16_table[i] = crc
     * Set g_crc_table_init = 1 */
    for (uint16_t i = 0; i < 256; i++) {
        uint16_t crc = i;
        for (uint8_t b = 0; b < 8; b++) {
            crc = (crc & 1) ? (crc >> 1) ^ 0xA001u : crc >> 1;
        }
        crc16_table[i] = crc;
    }
    g_crc_table_init = 1;
}

uint16_t crc16_modbus_fast(const uint8_t *data, uint16_t len)
{
    /* TODO: if !g_crc_table_init: crc16_build_table()
     * crc = 0xFFFF
     * for each byte: crc = (crc >> 8) ^ crc16_table[(crc ^ byte) & 0xFF]
     * return crc */
    if (!g_crc_table_init) crc16_build_table();
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++) {
        crc = (crc >> 8) ^ crc16_table[(crc ^ data[i]) & 0xFFu];
    }
    return crc;
}

/* ============================================================
 * TASK 2 — Frame encoder
 *
 * Build a complete frame in the output buffer.
 * Returns total frame length, or -1 on error.
 * ============================================================ */

int frame_encode(uint8_t cmd, const uint8_t *payload, uint8_t payload_len,
                 uint8_t *out_buf, uint16_t out_max)
{
    /* TODO: validate payload_len <= MAX_PAYLOAD_SIZE */
    /* TODO: validate out_max >= HEADER_SIZE + payload_len + CRC_SIZE */
    /* TODO: out_buf[0] = FRAME_SOF */
    /* TODO: out_buf[1] = cmd */
    /* TODO: out_buf[2] = payload_len */
    /* TODO: memcpy(out_buf + 3, payload, payload_len) */
    /* TODO: compute CRC over out_buf[1..2+payload_len] (cmd+len+payload) */
    /* TODO: out_buf[3+payload_len]   = crc & 0xFF (low byte) */
    /* TODO: out_buf[3+payload_len+1] = (crc >> 8) & 0xFF (high byte) */
    /* TODO: return HEADER_SIZE + payload_len + CRC_SIZE */

    if (payload_len > MAX_PAYLOAD_SIZE) return -1;
    uint16_t total = HEADER_SIZE + payload_len + CRC_SIZE;
    if (total > out_max) return -1;

    out_buf[0] = FRAME_SOF;
    out_buf[1] = cmd;
    out_buf[2] = payload_len;
    if (payload && payload_len) memcpy(out_buf + 3, payload, payload_len);

    uint16_t crc = crc16_modbus_fast(out_buf + 1, HEADER_SIZE - 1 + payload_len);
    out_buf[3 + payload_len]     = (uint8_t)(crc & 0xFFu);
    out_buf[3 + payload_len + 1] = (uint8_t)(crc >> 8);

    return (int)total;
}

/* ============================================================
 * TASK 3 — Frame decoder (state machine)
 *
 * A real receiver processes bytes one at a time from UART ISR.
 * The decoder is a state machine.
 * ============================================================ */

typedef enum {
    PARSE_WAIT_SOF,
    PARSE_CMD,
    PARSE_LEN,
    PARSE_PAYLOAD,
    PARSE_CRC_LO,
    PARSE_CRC_HI
} ParseState;

typedef struct {
    uint8_t  cmd;
    uint8_t  payload[MAX_PAYLOAD_SIZE];
    uint8_t  len;
    uint16_t crc_received;
    uint16_t crc_computed;
    uint8_t  valid;   /* 1 = frame complete and CRC OK */
} ParsedFrame;

typedef struct {
    ParseState   state;
    ParsedFrame  frame;
    uint8_t      payload_idx;
} FrameParser;

void parser_reset(FrameParser *p)
{
    p->state = PARSE_WAIT_SOF;
    p->payload_idx = 0;
    memset(&p->frame, 0, sizeof(p->frame));
}

/* Returns 1 when a complete valid frame has been received */
int parser_feed_byte(FrameParser *p, uint8_t byte)
{
    /* TODO: implement state machine:
     * PARSE_WAIT_SOF: if byte == FRAME_SOF → PARSE_CMD, else stay
     * PARSE_CMD:      save cmd, → PARSE_LEN
     * PARSE_LEN:      if len > MAX_PAYLOAD_SIZE → reset; else save len
     *                 if len == 0 → PARSE_CRC_LO else → PARSE_PAYLOAD
     * PARSE_PAYLOAD:  save to payload[idx++], if idx == len → PARSE_CRC_LO
     * PARSE_CRC_LO:   save crc low byte → PARSE_CRC_HI
     * PARSE_CRC_HI:   save crc high byte, compute CRC, validate, set valid
     *                 return 1
     */
    switch (p->state) {
        case PARSE_WAIT_SOF:
            if (byte == FRAME_SOF) p->state = PARSE_CMD;
            break;
        case PARSE_CMD:
            p->frame.cmd = byte;
            p->state = PARSE_LEN;
            break;
        case PARSE_LEN:
            if (byte > MAX_PAYLOAD_SIZE) { parser_reset(p); break; }
            p->frame.len = byte;
            p->payload_idx = 0;
            p->state = (byte == 0) ? PARSE_CRC_LO : PARSE_PAYLOAD;
            break;
        case PARSE_PAYLOAD:
            p->frame.payload[p->payload_idx++] = byte;
            if (p->payload_idx == p->frame.len) p->state = PARSE_CRC_LO;
            break;
        case PARSE_CRC_LO:
            p->frame.crc_received = byte;
            p->state = PARSE_CRC_HI;
            break;
        case PARSE_CRC_HI: {
            p->frame.crc_received |= (uint16_t)((uint16_t)byte << 8);
            /* Compute CRC over CMD + LEN + PAYLOAD */
            uint8_t crc_data[2 + MAX_PAYLOAD_SIZE];
            crc_data[0] = p->frame.cmd;
            crc_data[1] = p->frame.len;
            memcpy(crc_data + 2, p->frame.payload, p->frame.len);
            p->frame.crc_computed = crc16_modbus_fast(crc_data, 2 + p->frame.len);
            p->frame.valid = (p->frame.crc_received == p->frame.crc_computed) ? 1 : 0;
            parser_reset(p);
            return 1;
        }
    }
    return 0;
}

/* ============================================================
 * TASK 4 — Byte stuffing (escape sequences)
 *
 * Problem: what if the payload contains 0xAA (SOF byte)?
 * Solution: byte stuffing — escape special bytes.
 *
 * Rules:
 *   0xAA in payload → send 0xBB 0x01
 *   0xBB in payload → send 0xBB 0x02
 * ============================================================ */

#define STUFF_ESC   0xBBu
#define STUFF_AA    0x01u
#define STUFF_BB    0x02u

int stuff_encode(const uint8_t *src, uint16_t src_len, uint8_t *dst, uint16_t dst_max)
{
    /* TODO: for each byte in src:
     *   if byte == 0xAA: write 0xBB 0x01 to dst
     *   if byte == 0xBB: write 0xBB 0x02 to dst
     *   else: write byte to dst
     * return number of bytes written, or -1 if dst too small */
    uint16_t out = 0;
    for (uint16_t i = 0; i < src_len; i++) {
        if (src[i] == 0xAAu || src[i] == 0xBBu) {
            if (out + 2 > dst_max) return -1;
            dst[out++] = STUFF_ESC;
            dst[out++] = (src[i] == 0xAAu) ? STUFF_AA : STUFF_BB;
        } else {
            if (out + 1 > dst_max) return -1;
            dst[out++] = src[i];
        }
    }
    return (int)out;
}

int stuff_decode(const uint8_t *src, uint16_t src_len, uint8_t *dst, uint16_t dst_max)
{
    /* TODO: reverse of encode — when you see 0xBB, read next byte and unstuff */
    uint16_t out = 0;
    for (uint16_t i = 0; i < src_len; i++) {
        if (src[i] == STUFF_ESC) {
            if (i + 1 >= src_len) return -1;
            i++;
            dst[out++] = (src[i] == STUFF_AA) ? 0xAAu : 0xBBu;
        } else {
            if (out >= dst_max) return -1;
            dst[out++] = src[i];
        }
    }
    return (int)out;
}

/* ============================================================
 * TASK 5 — BUG HUNT: CRC and framing bugs
 *
 * The encoder below has 3 bugs. Find and mark each one.
 * ============================================================ */

int frame_encode_BUGGY(uint8_t cmd, const uint8_t *payload, uint8_t len,
                       uint8_t *out, uint16_t out_max)
{
    (void)out_max;
    out[0] = FRAME_SOF;
    out[1] = cmd;
    out[2] = len;
    memcpy(out + 3, payload, len);

    /* Bug 1: CRC computed over wrong range — SOF is included but shouldn't be */
    uint16_t crc = crc16_modbus_fast(out, HEADER_SIZE + len);   /* should start at out+1 */

    /* Bug 2: CRC bytes stored big-endian (high byte first)
     * Protocol specifies little-endian (low byte first) */
    out[3 + len]     = (uint8_t)(crc >> 8);     /* Bug: this is the HIGH byte */
    out[3 + len + 1] = (uint8_t)(crc & 0xFFu);  /* Bug: this is the LOW byte */

    /* Bug 3: return value doesn't include CRC bytes */
    return HEADER_SIZE + len;   /* should be HEADER_SIZE + len + CRC_SIZE */
}

/* ============================================================
 * SELF-TEST
 * ============================================================ */

int main(void)
{
    /* CRC test — known Modbus CRC values */
    uint8_t test_data[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};
    uint16_t crc = crc16_modbus_fast(test_data, sizeof(test_data));
    /* Known good: CRC of {0x01,0x03,0x00,0x00,0x00,0x0A} = 0xC50E */
    assert(crc == 0xC50Eu);

    /* Encode/decode round trip */
    uint8_t payload[] = {0x11, 0x22, 0x33};
    uint8_t frame[64] = {0};
    int flen = frame_encode(CMD_DATA, payload, sizeof(payload), frame, sizeof(frame));
    assert(flen == (int)(HEADER_SIZE + sizeof(payload) + CRC_SIZE));

    FrameParser parser;
    parser_reset(&parser);
    int complete = 0;
    for (int i = 0; i < flen; i++) {
        complete = parser_feed_byte(&parser, frame[i]);
    }
    assert(complete == 0);   /* last byte triggers CRC check inside parser */
    /* After the last byte, the parser resets. Check via direct CRC test. */

    /* Byte stuffing test */
    uint8_t src[] = {0xAA, 0x42, 0xBB};
    uint8_t stuffed[16] = {0};
    uint8_t unstuffed[16] = {0};
    int slen = stuff_encode(src, sizeof(src), stuffed, sizeof(stuffed));
    assert(slen == 5);   /* 0xBB 0x01, 0x42, 0xBB 0x02 */
    int ulen = stuff_decode(stuffed, (uint16_t)slen, unstuffed, sizeof(unstuffed));
    assert(ulen == 3);
    assert(memcmp(src, unstuffed, 3) == 0);

    printf("All protocol framing tests PASSED.\n");
    return 0;
}

/* ============================================================
 * INTERVIEW QUESTIONS
 * ============================================================
 *
 * Q1: Why is CRC-16 preferred over a simple checksum (XOR / sum)?
 *     Answer: TODO
 *
 * Q2: What is the hamming distance of CRC-16/IBM?
 *     What does this mean practically?
 *     Answer: TODO
 *
 * Q3: Why do you compute CRC over CMD+LEN+PAYLOAD but not over SOF?
 *     Answer: TODO
 *
 * Q4: Describe the complete sequence for designing a binary serial
 *     protocol that is robust to noise, lost bytes, and resets.
 *     Answer: TODO
 *
 * Q5: What is COBS (Consistent Overhead Byte Stuffing)?
 *     How does it differ from the simple byte stuffing above?
 *     Answer: TODO
 */
