/*
 * ============================================================
 * EMBEDDED INTERVIEW PREP
 * Topic : Modbus RTU/TCP and CAN Bus
 * File  : 06_Communication_Protocols/03_modbus_can.c
 * ============================================================
 *
 * Modbus is the standard in industrial embedded (PLC, SCADA).
 * CAN is mandatory for automotive and industrial IoT.
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* ============================================================
 * THEORY — Modbus RTU
 * ============================================================
 *
 * Modbus RTU frame:
 *   [ADDR:1][FC:1][DATA:N][CRC_LO:1][CRC_HI:1]
 *
 *   ADDR : slave address (1-247, 0=broadcast)
 *   FC   : function code
 *   DATA : depends on FC
 *   CRC  : CRC-16/IBM (0xA001), little-endian (LO first)
 *
 * Common function codes:
 *   0x01 Read Coils
 *   0x02 Read Discrete Inputs
 *   0x03 Read Holding Registers  ← most common
 *   0x04 Read Input Registers
 *   0x05 Write Single Coil
 *   0x06 Write Single Register
 *   0x10 Write Multiple Registers
 *
 * FC=03 Request (8 bytes total):
 *   [ADDR][0x03][START_ADDR_HI][START_ADDR_LO][QUANTITY_HI][QUANTITY_LO][CRC_LO][CRC_HI]
 *
 * FC=03 Response:
 *   [ADDR][0x03][BYTE_COUNT][REG0_HI][REG0_LO]...[REGn_HI][REGn_LO][CRC_LO][CRC_HI]
 *   BYTE_COUNT = QUANTITY * 2
 *
 * Silent interval between frames: >= 3.5 character times
 *   At 9600 baud: 3.5 * (1/9600) * 11 bits ≈ 4 ms
 * ============================================================ */

/* ============================================================
 * THEORY — CAN Bus
 * ============================================================
 *
 * CAN (Controller Area Network) properties:
 *   - Multi-master differential bus (CAN_H / CAN_L)
 *   - Non-destructive bitwise arbitration (ID arbitration)
 *   - Built-in error detection: CRC-15, bit stuffing, ACK
 *   - Max 8 bytes payload (Classical CAN), 64 bytes (CAN FD)
 *
 * CAN frame structure (Standard 11-bit ID):
 *   SOF[1] + ID[11] + RTR[1] + IDE[1] + r0[1] + DLC[4] + DATA[0-64bits] +
 *   CRC[15] + CRC_DEL[1] + ACK[1] + ACK_DEL[1] + EOF[7]
 *
 * Arbitration: if two nodes transmit simultaneously, the one with
 * the lower ID "wins" (dominant bits override recessive).
 * High-priority frames have LOW IDs.
 *
 * Error frames:
 *   Active error  : 6 dominant bits + 8 recessive (bus functional)
 *   Passive error : 6 recessive bits (node reducing impact)
 *   Bus-off       : TEC > 255 → node goes silent
 *
 * TEC/REC: Transmit/Receive Error Counter
 *   Successful TX: TEC -= 1
 *   Failed TX:     TEC += 8
 *   TEC > 127: Error Passive
 *   TEC > 255: Bus-Off
 * ============================================================ */

/* ============================================================
 * MODBUS IMPLEMENTATION
 * ============================================================ */

/* Reuse CRC from earlier (inline for self-contained file) */
static uint16_t modbus_crc16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; b++)
            crc = (crc & 1) ? (crc >> 1) ^ 0xA001u : crc >> 1;
    }
    return crc;
}

/* ============================================================
 * TASK 1 — Build FC=03 request
 * ============================================================ */

int modbus_build_fc03_request(uint8_t slave_addr, uint16_t start_reg,
                               uint16_t quantity, uint8_t *out)
{
    /* TODO: out[0] = slave_addr
     * TODO: out[1] = 0x03
     * TODO: out[2] = start_reg >> 8
     * TODO: out[3] = start_reg & 0xFF
     * TODO: out[4] = quantity >> 8
     * TODO: out[5] = quantity & 0xFF
     * TODO: crc = modbus_crc16(out, 6)
     * TODO: out[6] = crc & 0xFF (low byte FIRST)
     * TODO: out[7] = crc >> 8
     * TODO: return 8 */
    out[0] = slave_addr;
    out[1] = 0x03;
    out[2] = (uint8_t)(start_reg >> 8);
    out[3] = (uint8_t)(start_reg & 0xFFu);
    out[4] = (uint8_t)(quantity >> 8);
    out[5] = (uint8_t)(quantity & 0xFFu);
    uint16_t crc = modbus_crc16(out, 6);
    out[6] = (uint8_t)(crc & 0xFFu);
    out[7] = (uint8_t)(crc >> 8);
    return 8;
}

/* ============================================================
 * TASK 2 — Parse FC=03 response
 *
 * Returns number of registers parsed, or -1 on error.
 * ============================================================ */

int modbus_parse_fc03_response(const uint8_t *resp, uint16_t resp_len,
                                uint8_t expected_slave, uint16_t *regs, uint16_t max_regs)
{
    /* TODO: minimum frame = 5 bytes (addr+fc+count+crc) */
    if (resp_len < 5) return -1;

    /* TODO: validate resp[0] == expected_slave */
    if (resp[0] != expected_slave) return -1;

    /* TODO: validate resp[1] == 0x03 */
    if (resp[1] != 0x03) return -1;

    uint8_t byte_count = resp[2];

    /* TODO: validate byte_count is even */
    if (byte_count % 2 != 0) return -1;

    /* TODO: validate resp_len >= 3 + byte_count + 2 */
    if (resp_len < (uint16_t)(5 + byte_count)) return -1;

    /* TODO: validate CRC over resp[0..2+byte_count] */
    uint16_t crc_calc = modbus_crc16(resp, 3 + byte_count);
    uint16_t crc_recv = (uint16_t)(resp[3 + byte_count] | ((uint16_t)resp[4 + byte_count] << 8));
    if (crc_calc != crc_recv) return -1;

    /* TODO: extract registers (big-endian pairs) */
    uint16_t num_regs = byte_count / 2;
    if (num_regs > max_regs) num_regs = max_regs;
    for (uint16_t i = 0; i < num_regs; i++) {
        regs[i] = (uint16_t)((uint16_t)resp[3 + i*2] << 8) | resp[4 + i*2];
    }
    return (int)num_regs;
}

/* ============================================================
 * TASK 3 — Build FC=10 (write multiple registers) request
 * ============================================================ */

int modbus_build_fc10_request(uint8_t slave_addr, uint16_t start_reg,
                               const uint16_t *regs, uint16_t count, uint8_t *out)
{
    /* FC=10 PDU:
     * [ADDR][0x10][START_HI][START_LO][COUNT_HI][COUNT_LO][BYTE_COUNT][DATA...][CRC_LO][CRC_HI]
     * BYTE_COUNT = count * 2
     * DATA: each register big-endian */
    out[0] = slave_addr;
    out[1] = 0x10;
    out[2] = (uint8_t)(start_reg >> 8);
    out[3] = (uint8_t)(start_reg & 0xFFu);
    out[4] = (uint8_t)(count >> 8);
    out[5] = (uint8_t)(count & 0xFFu);
    out[6] = (uint8_t)(count * 2);
    for (uint16_t i = 0; i < count; i++) {
        out[7 + i*2]     = (uint8_t)(regs[i] >> 8);
        out[7 + i*2 + 1] = (uint8_t)(regs[i] & 0xFFu);
    }
    uint16_t pdu_len = 7 + count * 2;
    uint16_t crc = modbus_crc16(out, pdu_len);
    out[pdu_len]     = (uint8_t)(crc & 0xFFu);
    out[pdu_len + 1] = (uint8_t)(crc >> 8);
    return (int)(pdu_len + 2);
}

/* ============================================================
 * CAN BUS IMPLEMENTATION
 * ============================================================ */

typedef struct {
    uint32_t id;       /* 11-bit (standard) or 29-bit (extended) */
    uint8_t  dlc;      /* data length code (0-8) */
    uint8_t  data[8];
    uint8_t  is_extended;   /* 0=standard 11-bit, 1=extended 29-bit */
    uint8_t  is_rtr;        /* 1 = remote transmission request (no data) */
} CanFrame;

/* ============================================================
 * TASK 4 — CAN frame validation and signal extraction
 * ============================================================ */

int can_frame_validate(const CanFrame *f)
{
    /* TODO: DLC must be 0-8
     * TODO: if is_extended: ID must be <= 0x1FFFFFFF (29 bits)
     *       else:           ID must be <= 0x7FF       (11 bits)
     * TODO: if is_rtr: data is don't care (RTR frames carry no data)
     * TODO: return 1 if valid, 0 if not */
    if (f->dlc > 8) return 0;
    if (f->is_extended && f->id > 0x1FFFFFFFu) return 0;
    if (!f->is_extended && f->id > 0x7FFu) return 0;
    return 1;
}

/* Extract a signal (Intel byte order, unsigned) from CAN payload */
uint64_t can_signal_extract(const uint8_t *data, uint8_t start_bit, uint8_t length)
{
    /* Intel byte order: start_bit = LSB position in the 64-bit word
     * Build uint64_t from 8 bytes (little-endian), then shift+mask */
    uint64_t raw = 0;
    for (int i = 0; i < 8; i++)
        raw |= ((uint64_t)data[i] << (i * 8));
    uint64_t mask = (length == 64) ? 0xFFFFFFFFFFFFFFFFULL : ((1ULL << length) - 1);
    return (raw >> start_bit) & mask;
}

/* ============================================================
 * TASK 5 — CAN error handling states
 * ============================================================ */

typedef enum {
    CAN_ERROR_ACTIVE,
    CAN_ERROR_PASSIVE,
    CAN_BUS_OFF
} CanErrorState;

typedef struct {
    uint16_t tec;   /* transmit error counter */
    uint16_t rec;   /* receive error counter */
} CanErrorCounters;

CanErrorState can_get_error_state(const CanErrorCounters *ec)
{
    /* TODO: if tec > 255 || rec > 255: return CAN_BUS_OFF
     * TODO: if tec > 127 || rec > 127: return CAN_ERROR_PASSIVE
     * TODO: return CAN_ERROR_ACTIVE */
    if (ec->tec > 255 || ec->rec > 255) return CAN_BUS_OFF;
    if (ec->tec > 127 || ec->rec > 127) return CAN_ERROR_PASSIVE;
    return CAN_ERROR_ACTIVE;
}

/* ============================================================
 * TASK 6 — BUG HUNT: Modbus request builder
 *
 * The function below builds a Modbus FC=03 request.
 * Find 3 bugs.
 * ============================================================ */

int modbus_fc03_BUGGY(uint8_t addr, uint16_t reg, uint16_t qty, uint8_t *out)
{
    out[0] = addr;
    out[1] = 0x03;

    /* Bug 1: byte order wrong — should be big-endian (HIGH byte first) */
    out[2] = (uint8_t)(reg & 0xFFu);    /* should be reg >> 8 */
    out[3] = (uint8_t)(reg >> 8);       /* should be reg & 0xFF */

    out[4] = (uint8_t)(qty >> 8);
    out[5] = (uint8_t)(qty & 0xFFu);

    uint16_t crc = modbus_crc16(out, 6);

    /* Bug 2: CRC stored big-endian — Modbus RTU spec requires little-endian (LO first) */
    out[6] = (uint8_t)(crc >> 8);    /* should be crc & 0xFF */
    out[7] = (uint8_t)(crc & 0xFFu); /* should be crc >> 8   */

    /* Bug 3: return 7 instead of 8 */
    return 7;
}

/* ============================================================
 * SELF-TEST
 * ============================================================ */

int main(void)
{
    uint8_t req[8];
    int len = modbus_build_fc03_request(0x01, 0x0000, 0x000A, req);
    assert(len == 8);
    assert(req[0] == 0x01);
    assert(req[1] == 0x03);
    assert(req[2] == 0x00 && req[3] == 0x00);
    assert(req[4] == 0x00 && req[5] == 0x0A);

    /* Known CRC for this Modbus message: 0xC50E */
    uint16_t crc_check = (uint16_t)(req[6] | ((uint16_t)req[7] << 8));
    assert(crc_check == 0xC50Eu);

    /* CAN signal test */
    uint8_t can_data[] = {0xE8, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint64_t sig = can_signal_extract(can_data, 0, 16);
    /* 0x03E8 = 1000 */
    assert(sig == 0x03E8u);

    printf("All Modbus/CAN tests PASSED.\n");
    return 0;
}

/* ============================================================
 * INTERVIEW QUESTIONS
 * ============================================================
 *
 * Q1: What is the difference between Modbus RTU and Modbus TCP?
 *     Answer: TODO
 *
 * Q2: Modbus is master/slave. CAN is multi-master. Explain CAN arbitration.
 *     What happens when two nodes transmit the same ID simultaneously?
 *     Answer: TODO
 *
 * Q3: When does a CAN node enter "bus-off" state?
 *     How does it recover?
 *     Answer: TODO
 *
 * Q4: You receive a Modbus FC=03 response. CRC check fails.
 *     List the possible causes in order of likelihood.
 *     Answer: TODO
 *
 * Q5: What is CAN FD and how does it differ from Classical CAN?
 *     Answer: TODO
 */
