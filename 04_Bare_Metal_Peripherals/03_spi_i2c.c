/*
 * ============================================================
 * EMBEDDED INTERVIEW PREP
 * Topic : SPI and I2C — Register-Level Drivers
 * File  : 04_Bare_Metal_Peripherals/03_spi_i2c.c
 * ============================================================
 *
 * SPI and I2C are asked constantly. Know:
 * - The electrical difference (push-pull vs open-drain)
 * - Clock polarity/phase for SPI (CPOL/CPHA)
 * - I2C address/ACK/NACK sequence
 * - When to choose one over the other
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* ============================================================
 * THEORY — SPI vs I2C Side-by-Side
 * ============================================================
 *
 * SPI:
 *   Lines  : SCLK, MOSI, MISO, CS (one per slave)
 *   Wires  : 4+ (CS per slave = N+3 wires for N slaves)
 *   Driver : Push-pull (fast, no pull-ups needed)
 *   Speed  : Up to 100 Mbit/s typical, depends on MCU
 *   Duplex : Full duplex (simultaneous TX+RX)
 *   Address: No — CS selects slave
 *   Distance: Short (PCB), capacitance-limited
 *
 * I2C:
 *   Lines  : SDA (data), SCL (clock) — ONLY 2 wires total
 *   Wires  : 2 (all slaves share the same 2 wires)
 *   Driver : Open-drain with external pull-ups (MANDATORY)
 *   Speed  : 100 kHz (Standard), 400 kHz (Fast), 1 MHz (Fast+), 3.4 MHz (High)
 *   Duplex : Half duplex (SDA is bidirectional)
 *   Address: 7-bit or 10-bit device address on the bus
 *   Multi-master: Yes, with arbitration
 *
 * SPI Modes (CPOL/CPHA):
 *   Mode 0: CPOL=0, CPHA=0 → idle low, sample on rising edge   (most common)
 *   Mode 1: CPOL=0, CPHA=1 → idle low, sample on falling edge
 *   Mode 2: CPOL=1, CPHA=0 → idle high, sample on falling edge
 *   Mode 3: CPOL=1, CPHA=1 → idle high, sample on rising edge
 *
 * I2C Transaction sequence:
 *   Write: START → ADDR+W → ACK → DATA[0] → ACK → ... → DATA[n] → ACK → STOP
 *   Read:  START → ADDR+R → ACK → DATA[0] → ACK → ... → DATA[n] → NACK → STOP
 *   Combined (register read):
 *          START → ADDR+W → ACK → REG_ADDR → ACK →
 *          RESTART → ADDR+R → ACK → DATA → NACK → STOP
 * ============================================================ */

/* ============================================================
 * SPI — SIMULATED REGISTERS
 * ============================================================ */

typedef struct {
    volatile uint32_t CR1;
    volatile uint32_t CR2;
    volatile uint32_t SR;
    volatile uint32_t DR;
} SPI_TypeDef;

/* SPI CR1 bits */
#define SPI_CR1_BIDIMODE  (1u << 15)
#define SPI_CR1_DFF       (1u << 11)   /* 0=8-bit, 1=16-bit frame */
#define SPI_CR1_SSM       (1u << 9)    /* Software slave management */
#define SPI_CR1_SSI       (1u << 8)    /* Internal slave select */
#define SPI_CR1_SPE       (1u << 6)    /* SPI enable */
#define SPI_CR1_BR_MASK   (0b111u << 3) /* Baud rate bits [5:3] */
#define SPI_CR1_MSTR      (1u << 2)    /* Master mode */
#define SPI_CR1_CPOL      (1u << 1)
#define SPI_CR1_CPHA      (1u << 0)

/* SPI SR bits */
#define SPI_SR_BSY   (1u << 7)
#define SPI_SR_TXE   (1u << 1)
#define SPI_SR_RXNE  (1u << 0)

static SPI_TypeDef _SPI1 = {0};
SPI_TypeDef *SPI1 = &_SPI1;

/* Simulated CS GPIO */
static volatile uint32_t g_cs_pin = 1;   /* 1 = deasserted */

/* ============================================================
 * TASK 1 — SPI initialization
 * ============================================================ */

typedef struct {
    uint8_t mode;        /* 0-3 (CPOL/CPHA) */
    uint8_t br_div;      /* 0-7 → divides PCLK by 2^(br_div+1) */
    uint8_t data_16bit;  /* 0=8bit, 1=16bit */
    uint8_t msb_first;   /* 0=LSB first (non-standard), 1=MSB first */
} SpiConfig;

void spi_init(SPI_TypeDef *spi, const SpiConfig *cfg)
{
    /* TODO: disable SPI before config (clear SPE) */
    /* TODO: build CR1:
     *   CPOL = (cfg->mode >> 1) & 1
     *   CPHA = (cfg->mode >> 0) & 1
     *   BR   = cfg->br_div << 3
     *   MSTR = 1 (master mode)
     *   SSM  = 1, SSI = 1 (software CS management)
     *   DFF  = cfg->data_16bit
     *   LSBFIRST = !cfg->msb_first (bit 7 of CR1) */
    /* TODO: enable SPI (set SPE) */
    (void)spi; (void)cfg;
}

/* ============================================================
 * TASK 2 — SPI transfer (8-bit)
 *
 * On STM32, SPI is full-duplex: every byte sent = byte received.
 * ============================================================ */

void spi_cs_assert(void)   { g_cs_pin = 0; }
void spi_cs_deassert(void) { g_cs_pin = 1; }

uint8_t spi_transfer_byte(SPI_TypeDef *spi, uint8_t tx)
{
    /* TODO: wait until TXE (SR bit 1) — TX register empty */
    /* TODO: write tx to spi->DR */
    /* TODO: wait until RXNE (SR bit 0) — RX has data */
    /* TODO: return (uint8_t)spi->DR */
    (void)spi; (void)tx;

    /* Simulation: echo the byte */
    spi->SR |= SPI_SR_TXE | SPI_SR_RXNE;
    spi->DR  = tx;
    return (uint8_t)spi->DR;
}

void spi_write_register(SPI_TypeDef *spi, uint8_t reg, uint8_t value)
{
    /* TODO: CS assert, send reg (write bit=0), send value, CS deassert */
    (void)spi; (void)reg; (void)value;
}

uint8_t spi_read_register(SPI_TypeDef *spi, uint8_t reg)
{
    /* TODO: CS assert, send reg | 0x80 (read bit=1), send 0x00 dummy, CS deassert
     * return the byte received during the dummy transfer */
    (void)spi; (void)reg;
    return 0;
}

void spi_transfer_buf(SPI_TypeDef *spi, const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    /* TODO: for each byte: spi_transfer_byte, store in rx if rx != NULL */
    (void)spi; (void)tx; (void)rx; (void)len;
}

/* ============================================================
 * I2C — SIMULATED REGISTERS
 * ============================================================ */

typedef struct {
    volatile uint32_t CR1;
    volatile uint32_t CR2;
    volatile uint32_t OAR1;
    volatile uint32_t SR1;
    volatile uint32_t SR2;
    volatile uint32_t CCR;
    volatile uint32_t TRISE;
    volatile uint32_t DR;
} I2C_TypeDef;

/* I2C CR1 bits */
#define I2C_CR1_SWRST  (1u << 15)
#define I2C_CR1_ACK    (1u << 10)
#define I2C_CR1_STOP   (1u << 9)
#define I2C_CR1_START  (1u << 8)
#define I2C_CR1_PE     (1u << 0)

/* I2C SR1 bits */
#define I2C_SR1_RXNE   (1u << 6)
#define I2C_SR1_TXE    (1u << 7)
#define I2C_SR1_BTF    (1u << 2)   /* Byte transfer finished */
#define I2C_SR1_ADDR   (1u << 1)   /* Address sent/matched */
#define I2C_SR1_SB     (1u << 0)   /* Start bit generated */
#define I2C_SR1_AF     (1u << 10)  /* Acknowledge failure */
#define I2C_SR1_BERR   (1u << 8)   /* Bus error */

static I2C_TypeDef _I2C1 = {0};
I2C_TypeDef *I2C1 = &_I2C1;

/* ============================================================
 * TASK 3 — I2C initialization
 * ============================================================ */

void i2c_init(I2C_TypeDef *i2c, uint32_t pclk_hz, uint32_t speed_hz)
{
    /* TODO: disable I2C (clear PE) */
    /* TODO: set CR2.FREQ = pclk_hz / 1_000_000 (in MHz, max 42) */
    /* TODO: compute CCR:
     *   standard mode (<=100kHz): CCR = pclk_hz / (2 * speed_hz)
     *   fast mode (>100kHz):      CCR = pclk_hz / (3 * speed_hz) with DUTY=0 */
    /* TODO: compute TRISE:
     *   standard: (pclk_mhz * 1000 / 1000) + 1 = pclk_mhz + 1
     *   fast:     (pclk_mhz * 300  / 1000) + 1          */
    /* TODO: enable I2C (set PE) */
    (void)i2c; (void)pclk_hz; (void)speed_hz;
}

/* ============================================================
 * TASK 4 — I2C write transaction
 *
 * Write N bytes to a device register:
 *   START → ADDR+W → REG → DATA[0..n-1] → STOP
 * ============================================================ */

#define I2C_TIMEOUT  10000u

static int i2c_wait_flag(I2C_TypeDef *i2c, uint32_t sr1_flag)
{
    uint32_t timeout = I2C_TIMEOUT;
    while (!(i2c->SR1 & sr1_flag)) {
        if (--timeout == 0) return -1;
    }
    /* simulation: set the flag */
    i2c->SR1 |= sr1_flag;
    return 0;
}

int i2c_write(I2C_TypeDef *i2c, uint8_t dev_addr, uint8_t reg_addr,
              const uint8_t *data, uint16_t len)
{
    /* TODO: generate START (CR1 |= START), wait for SB flag */
    /* TODO: send (dev_addr << 1) | 0 (write), wait for ADDR flag */
    /* TODO: clear ADDR by reading SR1 then SR2 */
    /* TODO: send reg_addr, wait for TXE */
    /* TODO: for each byte in data: write to DR, wait for TXE */
    /* TODO: wait for BTF (all bytes transmitted), generate STOP */
    /* TODO: return 0 on success, -1 on timeout */
    (void)i2c; (void)dev_addr; (void)reg_addr; (void)data; (void)len;
    return 0;
}

/* ============================================================
 * TASK 5 — I2C read transaction (combined write-then-read)
 *
 *   START → ADDR+W → REG → RESTART → ADDR+R → DATA[0..n-1] → STOP
 * ============================================================ */

int i2c_read(I2C_TypeDef *i2c, uint8_t dev_addr, uint8_t reg_addr,
             uint8_t *data, uint16_t len)
{
    /* TODO: START, send addr+W, send reg_addr */
    /* TODO: REPEATED START */
    /* TODO: send addr+R (dev_addr<<1)|1, wait ADDR */
    /* TODO: for each byte:
     *   if second-to-last: clear ACK (CR1 &= ~I2C_CR1_ACK) to NACK last byte
     *   if last: set STOP before reading last byte
     *   wait RXNE, read DR into data[i] */
    /* TODO: re-enable ACK for next transaction */
    (void)i2c; (void)dev_addr; (void)reg_addr; (void)data; (void)len;
    return 0;
}

/* ============================================================
 * TASK 6 — BUG HUNT: I2C read transaction mistakes
 *
 * The code below reads 2 bytes from a temperature sensor.
 * It has 3 bugs. Find and mark each one.
 * ============================================================ */

int i2c_read_temp_BUGGY(I2C_TypeDef *i2c, uint8_t addr, uint8_t *out)
{
    /* Bug 1: START condition not generated before sending address */
    /* Missing: i2c->CR1 |= I2C_CR1_START; wait for SB; */

    /* Bug 2: wrong address format — address should be shifted left by 1
     * and OR'd with 1 for read */
    i2c->DR = addr;   /* should be (addr << 1) | 1 */
    i2c_wait_flag(i2c, I2C_SR1_ADDR);
    (void)(i2c->SR1); (void)(i2c->SR2);   /* clear ADDR */

    /* Bug 3: ACK not cleared before last byte — receiver won't NACK,
     * master will clock in extra bytes and lose sync */
    out[0] = (uint8_t)i2c->DR;
    i2c_wait_flag(i2c, I2C_SR1_RXNE);
    out[1] = (uint8_t)i2c->DR;

    i2c->CR1 |= I2C_CR1_STOP;
    return 0;
}

/* ============================================================
 * SELF-TEST
 * ============================================================ */

int main(void)
{
    /* Smoke test: SPI transfer returns the dummy byte */
    SPI1->SR = SPI_SR_TXE | SPI_SR_RXNE;
    SPI1->DR = 0xAB;
    uint8_t rx = spi_transfer_byte(SPI1, 0x55);
    assert(rx == 0xAB);

    printf("All SPI/I2C tests PASSED.\n");
    return 0;
}

/* ============================================================
 * INTERVIEW QUESTIONS
 * ============================================================
 *
 * Q1: You need to connect 8 sensors to one MCU. Compare using SPI vs I2C.
 *     Which would you pick and why?
 *     Answer: TODO
 *
 * Q2: An I2C bus is stuck — SCL is low and SDA is low.
 *     What caused this and how do you recover?
 *     Answer: TODO
 *
 * Q3: What is "clock stretching" in I2C? Which device performs it?
 *     Answer: TODO
 *
 * Q4: You see CPOL=1, CPHA=1 in a sensor datasheet.
 *     Draw the waveform for sending byte 0b10110001.
 *     Answer: TODO
 *
 * Q5: Why must I2C SDA and SCL have pull-up resistors?
 *     What happens if they're too large? Too small?
 *     Answer: TODO
 *
 * Q6: Explain the difference between I2C 7-bit and 10-bit addressing.
 *     How does the frame change for 10-bit?
 *     Answer: TODO
 */
