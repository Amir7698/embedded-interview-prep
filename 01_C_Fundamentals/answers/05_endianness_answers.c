/*
 * ANSWERS: 05_endianness.c
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* ============================================================
 * TASK 1 — Endianness detection and byte swap
 * ============================================================ */

int system_is_little_endian(void)
{
    /* Cast a uint16_t 1 to uint8_t* — on LE, byte[0]==1 */
    uint16_t x = 1;
    return *((uint8_t *)&x) == 1;
}

uint16_t bswap16(uint16_t x)
{
    return (uint16_t)((x << 8) | (x >> 8));
}

uint32_t bswap32(uint32_t x)
{
    return ((x & 0x000000FFu) << 24) |
           ((x & 0x0000FF00u) <<  8) |
           ((x & 0x00FF0000u) >>  8) |
           ((x & 0xFF000000u) >> 24);
}

uint64_t bswap64(uint64_t x)
{
    return ((uint64_t)bswap32((uint32_t)(x & 0xFFFFFFFFu)) << 32) |
           ((uint64_t)bswap32((uint32_t)(x >> 32)));
}

/* ============================================================
 * TASK 2 — Parse big/little-endian from byte buffer
 * ============================================================ */

uint16_t read_be16(const uint8_t *buf)
{
    return (uint16_t)(((uint16_t)buf[0] << 8) | buf[1]);
}

uint32_t read_be32(const uint8_t *buf)
{
    return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] <<  8) |  (uint32_t)buf[3];
}

uint16_t read_le16(const uint8_t *buf)
{
    return (uint16_t)(buf[0] | ((uint16_t)buf[1] << 8));
}

uint32_t read_le32(const uint8_t *buf)
{
    return (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) |
           ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
}

/* ============================================================
 * TASK 3 — Write big/little-endian to byte buffer
 * ============================================================ */

void write_be16(uint8_t *buf, uint16_t val)
{
    buf[0] = (uint8_t)(val >> 8);
    buf[1] = (uint8_t)(val & 0xFFu);
}

void write_be32(uint8_t *buf, uint32_t val)
{
    buf[0] = (uint8_t)(val >> 24);
    buf[1] = (uint8_t)(val >> 16);
    buf[2] = (uint8_t)(val >>  8);
    buf[3] = (uint8_t)(val & 0xFFu);
}

void write_le16(uint8_t *buf, uint16_t val)
{
    buf[0] = (uint8_t)(val & 0xFFu);
    buf[1] = (uint8_t)(val >> 8);
}

/* ============================================================
 * TASK 4 — Modbus TCP header
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
    out->transaction_id = read_be16(frame + 0);
    out->protocol_id    = read_be16(frame + 2);
    if (out->protocol_id != 0x0000u) return -1;   /* must be 0 for Modbus */
    out->length         = read_be16(frame + 4);
    out->unit_id        = frame[6];
    out->function_code  = frame[7];
    return 0;
}

void build_modbus_tcp_request(uint8_t *out, uint16_t transaction_id,
                               uint8_t unit_id, const uint8_t *pdu, uint8_t pdu_len)
{
    write_be16(out + 0, transaction_id);
    write_be16(out + 2, 0x0000u);                        /* protocol ID */
    write_be16(out + 4, (uint16_t)(1u + pdu_len));       /* unit_id byte + PDU */
    out[6] = unit_id;
    memcpy(out + 7, pdu, pdu_len);
}

/* ============================================================
 * TASK 5 — CAN Intel signal extraction
 * ============================================================ */

uint64_t can_extract_intel_signal(const uint8_t *payload, uint8_t dlc,
                                   uint8_t start_bit, uint8_t bit_length)
{
    if (dlc > 8 || bit_length > 64) return 0;
    uint64_t raw = 0;
    for (uint8_t i = 0; i < dlc; i++)
        raw |= ((uint64_t)payload[i] << (i * 8));
    uint64_t mask = (bit_length == 64) ? 0xFFFFFFFFFFFFFFFFULL : ((1ULL << bit_length) - 1);
    return (raw >> start_bit) & mask;
}

/* ============================================================
 * TASK 6 — Bug hunt FIXED
 *
 * Bug 1: buf[0] | (buf[1] << 8) — this is LITTLE-endian not big-endian.
 *        For big-endian: (buf[0] << 8) | buf[1].
 *        FIX: pkt.temp_centideg = (int16_t)(((uint16_t)buf[0] << 8) | buf[1]);
 *
 * Bug 2: *(uint16_t*)(buf + 2) — two problems:
 *        (a) Strict aliasing violation: accessing uint8_t array via uint16_t*.
 *        (b) Potential misalignment: buf+2 may not be 2-byte aligned.
 *        Both are UB. Use explicit byte-by-byte reads.
 *        FIX: raw_pressure = (uint16_t)(((uint16_t)buf[2] << 8) | buf[3]);
 *
 * Bug 3: (buf[4] << 8) — buf[4] is uint8_t. When buf[4] >= 0x80 (bit 7 set),
 *        the integer promotion makes it a signed int before shifting,
 *        and sign extension gives 0xFFFFxx80, making the OR result wrong.
 *        FIX: ((uint16_t)buf[4] << 8) | buf[5]  — cast to uint16_t first.
 * ============================================================ */

typedef struct { int16_t temp_centideg; uint16_t pressure_pa; uint16_t humid_tenth; } SensorPacket;

SensorPacket parse_sensor_packet_FIXED(const uint8_t *buf)
{
    SensorPacket pkt;
    pkt.temp_centideg = (int16_t)(((uint16_t)buf[0] << 8) | buf[1]);   /* BE, signed */
    pkt.pressure_pa   = (uint16_t)(((uint16_t)buf[2] << 8) | buf[3]);  /* BE, unsigned */
    pkt.humid_tenth   = (uint16_t)(((uint16_t)buf[4] << 8) | buf[5]);  /* BE, cast first */
    return pkt;
}

/* ============================================================
 * INTERVIEW QUESTION ANSWERS
 * ============================================================

Q1: One-liner to detect endianness at runtime.

A: int is_le = (*(uint8_t *)&(uint16_t){1}) == 1;
   Explanation: compound literal (uint16_t){1} stores value 0x0001.
   On LE: stored as [01][00] → byte[0] == 1 → true.
   On BE: stored as [00][01] → byte[0] == 0 → false.

Q2: You receive a uint32_t over TCP. What must you do before using it?

A: Convert from network byte order (big-endian) to host byte order:
   uint32_t host_val = ntohl(network_val);
   On a big-endian host: ntohl() is a no-op.
   On a little-endian host (x86, ARM): ntohl() reverses bytes.
   Never use the raw value directly — it will be wrong on LE hosts.

Q3: CAN signal EngineSpeed: start_bit=0, length=16, Intel, factor=0.25, offset=0.
    Payload [0xE8][0x03]. Physical value?

A: Intel byte order, start_bit=0, length=16.
   raw = payload as uint64_t LE = 0x03E8 (from bytes [E8][03] → LE = 0x03E8 = 1000)
   Signal = (raw >> 0) & 0xFFFF = 1000
   Physical = 1000 * 0.25 + 0 = 250 RPM... wait:
   Actually 0x03E8 = 1000. Physical = 1000 * 0.25 = 250.
   (But typical EngineSpeed: factor=0.25, so 4000 RPM = raw 16000 = 0x3E80)

Q4: Why is *(uint16_t*)(buf+1) dangerous even on little-endian ARM?

A: Two reasons:
   (1) Strict aliasing: the compiler assumes uint8_t* and uint16_t* never alias.
       Reading via uint16_t* what was stored via uint8_t* is undefined behavior —
       the compiler may use a cached register value instead of reading from memory.
   (2) Misalignment: buf+1 is at an odd address. On Cortex-M0/M0+, a misaligned
       halfword load causes HardFault. On M3/M4 it works but may be slow.
   Safe alternative: memcpy(&val, buf+1, 2);  — always defined, always aligned.

Q5: What is htonl() and why do you need it for UDP?

A: htonl() = Host TO Network Long. Converts a 32-bit value from host byte order
   to network byte order (big-endian).
   POSIX sockets expect all multi-byte header fields in network byte order.
   If you send a uint32_t without htonl() on a little-endian host:
   value 0x00000001 is stored as [01][00][00][00] in memory,
   but you need [00][00][00][01] on the wire.
   ntohl() is the reverse: Network TO Host Long (used after recvfrom()).
*/

int main(void)
{
    const uint8_t be_data[] = {0x12, 0x34, 0x56, 0x78};
    const uint8_t le_data[] = {0x78, 0x56, 0x34, 0x12};

    assert(read_be16(be_data) == 0x1234u);
    assert(read_be32(be_data) == 0x12345678u);
    assert(read_le16(le_data) == 0x1234u);
    assert(read_le32(le_data) == 0x12345678u);

    assert(bswap16(0xAABBu) == 0xBBAAu);
    assert(bswap32(0xAABBCCDDu) == 0xDDCCBBAAu);
    assert(bswap64(0x0102030405060708ULL) == 0x0807060504030201ULL);

    uint8_t out[4] = {0};
    write_be32(out, 0x12345678u);
    assert(out[0]==0x12 && out[1]==0x34 && out[2]==0x56 && out[3]==0x78);

    /* CAN signal test */
    uint8_t can_data[] = {0xE8, 0x03, 0,0,0,0,0,0};
    assert(can_extract_intel_signal(can_data, 8, 0, 16) == 0x03E8u);

    /* Sensor packet fixed parse */
    uint8_t sp[] = {0x01, 0x2C, 0x00, 0x64, 0x01, 0xF4}; /* temp=300, pres=100, hum=500 */
    SensorPacket pkt = parse_sensor_packet_FIXED(sp);
    assert(pkt.temp_centideg == 300);
    assert(pkt.pressure_pa   == 100);
    assert(pkt.humid_tenth   == 500);

    printf("Endianness: %s\n", system_is_little_endian() ? "little-endian" : "big-endian");
    printf("All endianness answers verified.\n");
    return 0;
}
