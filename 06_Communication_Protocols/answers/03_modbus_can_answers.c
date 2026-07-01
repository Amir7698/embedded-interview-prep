/*
 * ANSWERS: 06_Communication_Protocols/03_modbus_can.c
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* ============================================================
 * INTERVIEW QUESTION ANSWERS
 * ============================================================

Q1: Modbus FC=03 vs FC=06 vs FC=16.

A: FC=03 (Read Holding Registers): master requests N registers starting
   at address ADDR. Slave responds with 2×N bytes (each reg = 2 bytes, BE).
   Used for: reading sensor values, status registers, counters.

   FC=06 (Write Single Register): master writes 2 bytes (one register) to ADDR.
   Slave echoes the request as acknowledgement.
   Used for: set a setpoint, write a single config value.

   FC=16 (Write Multiple Registers): master writes N registers starting at ADDR.
   Slave responds with ADDR + QTY (echo, 6 bytes).
   Used for: write a block of configuration, motor command (speed+direction+ramp).

Q2: Modbus RTU CRC byte order.

A: Modbus RTU CRC-16: LITTLE-ENDIAN (low byte first in frame).
   After computing uint16_t crc:
   frame[n]   = crc & 0xFF;     // low byte
   frame[n+1] = crc >> 8;       // high byte
   The CRC polynomial (0xA001) is already reflected for LSB-first processing.
   Register addresses and data values in Modbus frames are BIG-ENDIAN.
   Common mistake: storing CRC as big-endian → devices reject every frame.

Q3: CAN bus arbitration — who wins?

A: CAN uses non-destructive bitwise arbitration on the identifier field.
   Rule: dominant bit (0) wins over recessive bit (1).
   All nodes transmit simultaneously from bit 0 (MSB) of the identifier.
   Each node compares what it transmitted to what it reads back.
   If a node transmits recessive (1) but reads dominant (0): another node
   transmitted 0 → that node wins → our node stops transmitting and
   becomes a receiver. It will retry when the bus is idle.
   Lower CAN ID = higher priority (more leading 0 bits, wins arbitration earlier).
   Frame with ID=0x000 wins against all other frames.
   This is why safety-critical messages (e.g., brake command) get low IDs.

Q4: CAN error counters — TEC/REC. What triggers Bus-Off?

A: TEC (Transmit Error Counter): incremented on transmit error (+8 for
   most errors). Decremented by 1 on successful transmission.
   REC (Receive Error Counter): similar for receive errors.

   States:
   Error-Active (normal):   TEC < 128 && REC < 128
   Error-Passive:           TEC ≥ 128 OR REC ≥ 128
     In Error-Passive: node sends passive error frames (recessive bits),
     can still transmit.
   Bus-Off:                 TEC ≥ 256
     Node disconnects from bus completely. Cannot transmit or receive.
     Recovery: 128 occurrences of 11 consecutive recessive bits (bus idle).

   A node in Bus-Off is typically a sign of hardware failure (short circuit,
   wrong bit rate, or disconnected cable) and may require MCU reset.

Q5: CAN 11-bit vs 29-bit identifiers (CAN 2.0A vs 2.0B).

A: CAN 2.0A (Standard frame): 11-bit identifier → 2048 possible IDs.
   Arbitration field = 11 bits. Smaller SOF + ID + RTR + control = less overhead.
   CAN 2.0B (Extended frame): 29-bit identifier → 536 million IDs.
   Used when: large systems (automotive with hundreds of ECUs), SAE J1939
   (truck/bus), ISO 15765 (OBD-II).
   IDE bit in frame header distinguishes standard vs extended.
   A CAN controller set to 2.0B can receive both standard and extended frames.
   In the same network: extended frames have lower priority if the first 11 bits
   match a standard frame ID (IDE bit is recessive=1 which loses to dominant=0).

Q6: CAN Intel byte order for signal extraction — what does it mean?

A: CAN databases (DBC files) define signals in Intel (little-endian) or
   Motorola (big-endian) byte order.
   Intel byte order: the START BIT is the LSB of the signal.
   Signal spans multiple bytes: LSB is at start_bit position,
   and signal grows toward higher bit positions (including across bytes in
   little-endian order: bit 7→bit 8 means byte 0 bit 7 to byte 1 bit 0).
   Motorola byte order: the START BIT is the MSB of the signal.
   Extraction for Intel:
   1. Extract N bytes from frame starting at byte(start_bit/8).
   2. Treat as uint64_t little-endian.
   3. Shift right by (start_bit % 8).
   4. Mask to signal length: & ((1<<length)-1).
   5. Sign-extend if signed.
*/

/* ============================================================
 * CRC-16/Modbus
 * ============================================================ */

uint16_t crc16_modbus(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFFu;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++)
            crc = (crc & 1u) ? ((crc >> 1) ^ 0xA001u) : (crc >> 1);
    }
    return crc;
}

/* ============================================================
 * TASK 1 — Modbus FC=03 request builder
 * ============================================================ */

uint8_t modbus_build_fc03_request(uint8_t slave_id, uint16_t start_addr,
                                  uint16_t quantity, uint8_t *out)
{
    out[0] = slave_id;
    out[1] = 0x03;
    out[2] = (uint8_t)(start_addr >> 8);    /* Big-endian */
    out[3] = (uint8_t)(start_addr & 0xFF);
    out[4] = (uint8_t)(quantity >> 8);
    out[5] = (uint8_t)(quantity & 0xFF);
    uint16_t crc = crc16_modbus(out, 6u);
    out[6] = (uint8_t)(crc & 0xFF);         /* CRC: low byte first (LE) */
    out[7] = (uint8_t)(crc >> 8);
    return 8u;
}

/* ============================================================
 * TASK 2 — Modbus FC=03 response parser
 * ============================================================ */

int modbus_parse_fc03_response(const uint8_t *resp, uint16_t resp_len,
                               uint16_t *regs_out, uint8_t max_regs)
{
    if (resp_len < 5) return -1;

    uint8_t byte_count = resp[2];
    if (byte_count % 2 != 0) return -2;
    uint8_t num_regs = byte_count / 2u;
    if (num_regs > max_regs) return -3;
    if (resp_len < (uint16_t)(3u + byte_count + 2u)) return -4;

    /* Verify CRC */
    uint16_t calc_crc = crc16_modbus(resp, (uint16_t)(3u + byte_count));
    uint16_t rx_crc   = (uint16_t)resp[3 + byte_count] |
                        ((uint16_t)resp[3 + byte_count + 1] << 8);
    if (calc_crc != rx_crc) return -5;

    for (uint8_t i = 0; i < num_regs; i++) {
        regs_out[i] = ((uint16_t)resp[3 + i*2] << 8) | resp[3 + i*2 + 1];
    }
    return num_regs;
}

/* ============================================================
 * TASK 3 — Modbus FC=16 builder
 * ============================================================ */

uint16_t modbus_build_fc10_request(uint8_t slave_id, uint16_t start_addr,
                                   const uint16_t *regs, uint8_t num_regs,
                                   uint8_t *out, uint16_t out_max)
{
    uint16_t total = (uint16_t)(7u + 2u * num_regs + 2u);
    if (total > out_max) return 0;

    out[0] = slave_id;
    out[1] = 0x10;
    out[2] = (uint8_t)(start_addr >> 8);
    out[3] = (uint8_t)(start_addr & 0xFF);
    out[4] = 0x00;
    out[5] = num_regs;
    out[6] = (uint8_t)(num_regs * 2u);   /* byte count */

    for (uint8_t i = 0; i < num_regs; i++) {
        out[7 + i*2]     = (uint8_t)(regs[i] >> 8);
        out[7 + i*2 + 1] = (uint8_t)(regs[i] & 0xFF);
    }
    uint16_t crc = crc16_modbus(out, (uint16_t)(7u + 2u * num_regs));
    out[7 + 2*num_regs]     = (uint8_t)(crc & 0xFF);
    out[7 + 2*num_regs + 1] = (uint8_t)(crc >> 8);
    return total;
}

/* ============================================================
 * TASK 4 — CAN frame validation
 * ============================================================ */

typedef struct {
    uint32_t id;
    uint8_t  dlc;
    uint8_t  data[8];
    uint8_t  is_extended;
} CanFrame;

typedef enum { CAN_OK=0, CAN_ERR_DLC, CAN_ERR_ID } CanValidResult;

CanValidResult can_frame_validate(const CanFrame *f)
{
    if (f->dlc > 8) return CAN_ERR_DLC;
    if (f->is_extended && f->id > 0x1FFFFFFFu) return CAN_ERR_ID;
    if (!f->is_extended && f->id > 0x7FFu)     return CAN_ERR_ID;
    return CAN_OK;
}

/* ============================================================
 * TASK 5 — CAN Intel byte order signal extraction
 * ============================================================ */

int32_t can_signal_extract(const uint8_t *data, uint8_t dlc,
                           uint8_t start_bit, uint8_t length,
                           uint8_t is_signed)
{
    (void)dlc;
    /* Intel byte order: start_bit is the LSB position.
       Build a 64-bit value from the data bytes, then extract. */
    uint64_t raw = 0;
    for (int i = 0; i < 8; i++)
        raw |= (uint64_t)data[i] << (i * 8);

    uint64_t mask = (length == 64) ? ~0ULL : ((1ULL << length) - 1ULL);
    uint64_t val  = (raw >> start_bit) & mask;

    if (is_signed && (val >> (length - 1))) {
        /* Sign extend */
        val |= ~mask;
        return (int32_t)(int64_t)val;
    }
    return (int32_t)val;
}

/* ============================================================
 * TASK 6 — CAN error state
 * ============================================================ */

typedef enum { CAN_STATE_ACTIVE=0, CAN_STATE_PASSIVE, CAN_STATE_BUS_OFF } CanErrorState;

CanErrorState can_get_error_state(uint8_t tec, uint8_t rec)
{
    if (tec >= 255)           return CAN_STATE_BUS_OFF;
    if (tec >= 128 || rec >= 128) return CAN_STATE_PASSIVE;
    return CAN_STATE_ACTIVE;
}

/* ============================================================
 * Bug hunt FIXED
 *
 * Bug 1: modbus_fc03_BUGGY uses LE byte order for register address.
 *        frame[2] = addr & 0xFF; frame[3] = addr >> 8;
 *        Modbus uses BIG-ENDIAN for register addresses and data.
 *        FIX: frame[2] = addr >> 8; frame[3] = addr & 0xFF;
 *
 * Bug 2: CRC stored as BIG-ENDIAN.
 *        frame[6] = crc >> 8; frame[7] = crc & 0xFF;
 *        Modbus CRC is LITTLE-ENDIAN (low byte first).
 *        FIX: frame[6] = crc & 0xFF; frame[7] = crc >> 8;
 *
 * Bug 3: returns 7 instead of 8.
 *        FC=03 request: 1(ID) + 1(FC) + 2(ADDR) + 2(QTY) + 2(CRC) = 8 bytes.
 *        FIX: return 8;
 * ============================================================ */

int main(void)
{
    /* FC=03 request */
    uint8_t req[8];
    uint8_t n = modbus_build_fc03_request(0x01, 0x0064, 0x0002, req);
    assert(n == 8);
    assert(req[0] == 0x01 && req[1] == 0x03);
    assert(req[2] == 0x00 && req[3] == 0x64);  /* BE address */
    assert(req[4] == 0x00 && req[5] == 0x02);
    /* Verify CRC */
    uint16_t crc = crc16_modbus(req, 6);
    assert(req[6] == (crc & 0xFF) && req[7] == (crc >> 8));

    /* FC=03 response parse */
    uint8_t resp[] = {0x01, 0x03, 0x04, 0x00, 0x0A, 0x00, 0x14, 0, 0};
    uint16_t resp_crc = crc16_modbus(resp, 7);
    resp[7] = (uint8_t)(resp_crc & 0xFF);
    resp[8] = (uint8_t)(resp_crc >> 8);
    uint16_t regs[4];
    int cnt = modbus_parse_fc03_response(resp, 9, regs, 4);
    assert(cnt == 2);
    assert(regs[0] == 10 && regs[1] == 20);

    /* CAN signal extraction */
    uint8_t can_data[8] = {0xAB, 0xCD, 0, 0, 0, 0, 0, 0};
    /* 16-bit Intel signal at start_bit=0, length=16 */
    int32_t sig = can_signal_extract(can_data, 8, 0, 16, 0);
    assert(sig == 0xCDAB);  /* Intel LE: 0xAB + 0xCD<<8 */

    /* CAN error state */
    assert(can_get_error_state(100, 50)  == CAN_STATE_ACTIVE);
    assert(can_get_error_state(130, 50)  == CAN_STATE_PASSIVE);
    assert(can_get_error_state(255, 50)  == CAN_STATE_BUS_OFF);

    printf("All Modbus/CAN answers verified.\n");
    return 0;
}
