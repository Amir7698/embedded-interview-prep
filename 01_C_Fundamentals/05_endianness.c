/*
 * ============================================================
 * EMBEDDED INTERVIEW PREP
 * Topic : Endianness — Detection, Conversion, Protocol Parsing
 * File  : 01_C_Fundamentals/05_endianness.c
 * ============================================================
 *
 * You WILL be asked about endianness when working with:
 * CAN, Ethernet, UART protocols, Modbus, SOME/IP, USB, BLE.
 * Most MCUs are little-endian. Most network protocols are big-endian.
 * Getting this wrong = silent data corruption.
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* ============================================================
 * THEORY
 * ============================================================
 *
 * Little-endian (LE): LSB at lowest address
 *   Value 0x12345678 stored as: [78][56][34][12]
 *   Used by: x86, ARM (default), RISC-V (default)
 *
 * Big-endian (BE): MSB at lowest address
 *   Value 0x12345678 stored as: [12][34][56][78]
 *   Used by: network protocols (htons/htonl), Motorola, SPARC
 *            CAN signals can be either (DBC specifies per-signal)
 *
 * Mixed (PDP-endian): rare, obsolete
 *
 * Network byte order = big-endian
 *   htons() = host-to-network short (16-bit)
 *   htonl() = host-to-network long  (32-bit)
 *   ntohs() = network-to-host short
 *   ntohl() = network-to-host long
 *
 * CAN signals:
 *   Intel byte order = little-endian (start bit = LSB position)
 *   Motorola byte order = big-endian (start bit = MSB position)
 * ============================================================ */


/* ============================================================
 * TASK 1 — Endianness detection and manual byte swap
 * ============================================================ */

int system_is_little_endian(void)
{
    /* TODO: detect at runtime — no library calls */
    return -1;
}

uint16_t bswap16(uint16_t x)
{
    /* TODO: swap bytes: 0xAABB → 0xBBAA */
    (void)x; return 0;
}

uint32_t bswap32(uint32_t x)
{
    /* TODO: swap bytes: 0xAABBCCDD → 0xDDCCBBAA */
    (void)x; return 0;
}

uint64_t bswap64(uint64_t x)
{
    /* TODO: swap bytes of 64-bit value
     * Hint: use bswap32 on each half and swap halves */
    (void)x; return 0;
}

/* ============================================================
 * TASK 2 — Parse big-endian multi-byte values from a byte buffer
 *
 * This is the MOST COMMON operation when parsing protocol frames:
 * Modbus TCP, SOME/IP, UDP headers, BLE ATT PDUs.
 * ============================================================ */

uint16_t read_be16(const uint8_t *buf)
{
    /* TODO: read 2 bytes big-endian from buf
     * buf[0] = MSB, buf[1] = LSB
     * return as host uint16_t */
    (void)buf; return 0;
}

uint32_t read_be32(const uint8_t *buf)
{
    /* TODO: read 4 bytes big-endian */
    (void)buf; return 0;
}

uint16_t read_le16(const uint8_t *buf)
{
    /* TODO: read 2 bytes little-endian
     * buf[0] = LSB, buf[1] = MSB */
    (void)buf; return 0;
}

uint32_t read_le32(const uint8_t *buf)
{
    /* TODO: read 4 bytes little-endian */
    (void)buf; return 0;
}

/* ============================================================
 * TASK 3 — Write big-endian values into a byte buffer
 * (Serialising outgoing protocol frames)
 * ============================================================ */

void write_be16(uint8_t *buf, uint16_t val)
{
    /* TODO: buf[0] = MSB, buf[1] = LSB */
    (void)buf; (void)val;
}

void write_be32(uint8_t *buf, uint32_t val)
{
    /* TODO: buf[0..3] big-endian */
    (void)buf; (void)val;
}

void write_le16(uint8_t *buf, uint16_t val)
{
    /* TODO: buf[0] = LSB, buf[1] = MSB */
    (void)buf; (void)val;
}

/* ============================================================
 * TASK 4 — Modbus TCP MBAP header parsing
 *
 * Modbus TCP header (6 bytes, ALL fields big-endian):
 *   [0:1] Transaction ID  — uint16_t
 *   [2:3] Protocol ID     — uint16_t (always 0x0000)
 *   [4:5] Length          — uint16_t (bytes following)
 *
 * Then the PDU:
 *   [6]   Unit ID         — uint8_t
 *   [7]   Function Code   — uint8_t
 *   [8..] Data
 * ============================================================ */

typedef struct {
    uint16_t transaction_id;
    uint16_t protocol_id;
    uint16_t length;
    uint8_t  unit_id;
    uint8_t  function_code;
} ModbusTCPHeader;

int parse_modbus_tcp_header(const uint8_t *frame, uint16_t frame_len,
                             ModbusTCPHeader *out)
{
    if (frame_len < 8) return -1;

    /* TODO: out->transaction_id = read_be16(frame + 0) */
    /* TODO: out->protocol_id    = read_be16(frame + 2) */
    /* TODO: validate protocol_id == 0 */
    /* TODO: out->length         = read_be16(frame + 4) */
    /* TODO: out->unit_id        = frame[6] */
    /* TODO: out->function_code  = frame[7] */

    (void)frame; (void)out;
    return 0;
}

void build_modbus_tcp_request(uint8_t *out, uint16_t transaction_id,
                               uint8_t unit_id, const uint8_t *pdu, uint8_t pdu_len)
{
    /* TODO: write_be16(out + 0, transaction_id) */
    /* TODO: write_be16(out + 2, 0x0000)          — protocol ID */
    /* TODO: write_be16(out + 4, (uint16_t)(1 + pdu_len))  — unit_id byte + PDU */
    /* TODO: out[6] = unit_id */
    /* TODO: memcpy(out + 7, pdu, pdu_len) */
    (void)out; (void)transaction_id; (void)unit_id; (void)pdu; (void)pdu_len;
}

/* ============================================================
 * TASK 5 — CAN Intel vs Motorola signal extraction
 *
 * In CAN DBC files, each signal specifies:
 *   - start_bit : bit position of LSB (Intel) or MSB (Motorola)
 *   - bit_length: number of bits
 *   - byte_order: 0=Motorola, 1=Intel
 *
 * Intel (little-endian):
 *   start_bit = LSB position, counting from bit 0 of byte 0
 *   Extract: (raw >> start_bit) & ((1 << bit_length) - 1)
 *
 * Motorola is more complex — see answers/ for full implementation.
 * ============================================================ */

uint64_t can_extract_intel_signal(const uint8_t *payload, uint8_t dlc,
                                   uint8_t start_bit, uint8_t bit_length)
{
    if (dlc > 8 || bit_length > 64) return 0;

    /* Copy payload into a uint64_t (little-endian) */
    uint64_t raw = 0;
    /* TODO: for i in 0..dlc-1: raw |= ((uint64_t)payload[i] << (i*8)) */

    /* TODO: extract: (raw >> start_bit) & mask
     * where mask = (bit_length == 64) ? 0xFFFFFFFFFFFFFFFF : (1ULL << bit_length) - 1 */

    (void)payload; (void)dlc; (void)start_bit; (void)bit_length;
    return raw;
}

/* ============================================================
 * TASK 6 — BUG HUNT: endianness bugs in a sensor data parser
 *
 * The function below parses a 6-byte sensor packet from a network device.
 * Packet format (all fields big-endian):
 *   [0:1] Temperature in 0.01°C (int16_t)
 *   [2:3] Pressure in Pa (uint16_t)
 *   [4:5] Humidity in 0.1% (uint16_t)
 * It has 3 bugs. Find and mark each one.
 * ============================================================ */

typedef struct { int16_t temp_centideg; uint16_t pressure_pa; uint16_t humid_tenth; } SensorPacket;

SensorPacket parse_sensor_packet_BUGGY(const uint8_t *buf)
{
    SensorPacket pkt;

    /* Bug 1: ??? */
    pkt.temp_centideg = (int16_t)(buf[0] | (buf[1] << 8));   /* LE not BE */

    /* Bug 2: ??? */
    uint16_t raw_pressure = *(uint16_t*)(buf + 2);            /* unaligned + aliasing UB */

    /* Bug 3: ??? */
    pkt.humid_tenth = (uint16_t)((buf[4] << 8) | buf[5]);    /* buf[4] is uint8_t,
                                                                * shifting by 8 is fine,
                                                                * BUT: if buf[4] >= 0x80
                                                                * sign extension before OR.
                                                                * FIX: cast to uint16_t first */
    pkt.pressure_pa  = raw_pressure;
    return pkt;
}

/* ============================================================
 * SELF-TEST
 * ============================================================ */

static void test_endianness(void)
{
    const uint8_t be_data[] = {0x12, 0x34, 0x56, 0x78};
    const uint8_t le_data[] = {0x78, 0x56, 0x34, 0x12};

    assert(read_be16(be_data) == 0x1234);
    assert(read_be32(be_data) == 0x12345678);
    assert(read_le16(le_data) == 0x1234);
    assert(read_le32(le_data) == 0x12345678);

    assert(bswap16(0xAABB) == 0xBBAA);
    assert(bswap32(0xAABBCCDD) == 0xDDCCBBAA);

    uint8_t out[4] = {0};
    write_be32(out, 0x12345678);
    assert(out[0]==0x12 && out[1]==0x34 && out[2]==0x56 && out[3]==0x78);

    printf("All endianness tests PASSED.\n");
}

int main(void)
{
    test_endianness();
    return 0;
}

/* ============================================================
 * INTERVIEW QUESTIONS
 * ============================================================
 *
 * Q1: Write a one-liner to detect endianness at runtime.
 *     Answer: TODO
 *
 * Q2: You receive a uint32_t over a TCP socket. What must you do
 *     before using its value on an x86 host?
 *     Answer: TODO
 *
 * Q3: A CAN signal "EngineSpeed" is defined as:
 *     start_bit=0, length=16, byte_order=Intel, factor=0.25, offset=0.
 *     Payload bytes: [0xE8][0x03][...].
 *     What is the physical value?
 *     Answer: TODO
 *
 * Q4: Why is *(uint16_t*)(buf+1) dangerous even on little-endian ARM?
 *     Answer: TODO
 *
 * Q5: What is htonl() and why do you need it when sending a uint32_t
 *     over a UDP socket?
 *     Answer: TODO
 */
