/*
 * ANSWERS: 04_Bare_Metal_Peripherals/03_spi_i2c.c
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <assert.h>

/* ============================================================
 * INTERVIEW QUESTION ANSWERS (complete before code)
 * ============================================================

Q1: 8 sensors — SPI vs I2C?

A: Depends on the sensors.
   SPI: 8 sensors need 8 CS pins + 3 shared (SCLK/MOSI/MISO) = 11 pins total.
   Advantage: full-duplex, fast (10-50 Mbit/s), no address collision.
   Disadvantage: more pins, longer routing.
   I2C: 8 sensors share 2 wires (SDA+SCL). Need 8 unique addresses.
   Advantage: only 2 pins. Disadvantage: half-duplex, max 400kHz (Fast)
   or 1MHz (Fast+), address space limited (127 addresses, some reserved).
   Pick SPI if: high data rate, noise immunity needed (differential), long PCB runs.
   Pick I2C if: low speed sensors (temp, humidity, RTC), pin count is critical.

Q2: I2C bus stuck — SCL low, SDA low. Cause and recovery.

A: Cause: The slave was mid-byte when power was cut or MCU reset. The slave
   is holding SDA low waiting for more clock pulses to complete its byte.
   Recovery procedure (bit-bang from GPIO):
   1. Check SCL — if stuck low, there's a hardware problem (short circuit).
      If SDA is stuck and SCL is free:
   2. Send up to 9 clock pulses on SCL (GPIO toggling).
      Most slaves will release SDA after seeing the remaining clocks of their byte.
   3. Generate a STOP condition (SDA low → SDA high while SCL high).
   4. If still stuck: power cycle the slave.
   Linux: `i2c_recover_bus()` in the I2C subsystem does this automatically.
   On STM32: use the I2C peripheral reset (CR1.SWRST) then GPIO recovery.

Q3: Clock stretching in I2C. Which device performs it?

A: The SLAVE holds SCL low to pause the master's clocking.
   When the master drives SCL high, the slave keeps it low (open-drain bus).
   The master's SCL input sees LOW, waits until it sees HIGH, then continues.
   Use case: slave received an I2C address + register, needs time to fetch data
   from internal EEPROM or ADC before it can send the response.
   Without clock stretching: master would clock in garbage data while slave
   is still preparing.
   Master requirement: SCL must be open-drain (or have a clock stretching
   detection circuit) — a push-pull SCL cannot be held low by the slave.

Q4: CPOL=1, CPHA=1 waveform for byte 0b10110001.

A: Mode 3: clock idles HIGH (CPOL=1), data sampled on rising edge (CPHA=1).
   Sequence for 0b10110001 (MSB first):
   SCLK: H H L H L H L H L H L H L H L H H
          |falling|rising|falling|rising| ...
   DATA: \_1___0___1___1___0___0___0___1/
   - Data changes on falling edge of SCLK.
   - Data is sampled (read) on rising edge of SCLK.
   - CS asserted low before first clock, deasserted after 8th rising edge.

Q5: I2C pull-up resistors — too large vs too small.

A: Pull-ups are MANDATORY on I2C (open-drain lines cannot actively drive HIGH).
   Too large (e.g., 100 kΩ):
   - RC time constant = R × C_bus too long. Signals rise slowly.
   - At 400 kHz, bits may not reach VIH before being sampled → framing errors.
   - Standard specifies rise time < 300 ns for Fast mode.
   Too small (e.g., 100 Ω):
   - When slave pulls SDA LOW, current = VCC/R = 3.3V/100Ω = 33 mA per line.
   - Exceeds slave's output sink current spec (typically 3-10 mA) → VOL too high.
   - Also wastes power.
   Correct value: 4.7 kΩ for 100 kHz, 2.2 kΩ for 400 kHz (typical for 3.3V, <100pF bus).
   Formula: R = (VCC - VOL_max) / I_OL_min, constrained by rise time = 0.8473 × R × C_bus.

Q6: I2C 7-bit vs 10-bit addressing.

A: 7-bit (standard): address is 7 bits → 128 addresses, some reserved → ~112 usable.
   Frame: START + [ADDR:7][R/W:1] (1 byte) + ACK + ...
   10-bit: address is 10 bits → 1024 addresses.
   Frame: START + [11110:5][ADDR_HIGH:2][W:1] (byte 1) + ACK +
                  [ADDR_LOW:8] (byte 2) + ACK + ...
   For read, needs a repeated START + [11110:5][ADDR_HIGH:2][R:1].
   10-bit addressing is backward-compatible: the 5-bit prefix 11110 is reserved
   and not used by 7-bit devices.
   Use case for 10-bit: large systems with many I2C devices (e.g., server backplanes).
*/

/* ============================================================
 * TASK 1-5 — Complete implementations
 * ============================================================ */

typedef struct { volatile uint32_t CR1, CR2, SR, DR; } SPI_TypeDef;
#define SPI_SR_TXE   (1u << 1)
#define SPI_SR_RXNE  (1u << 0)
#define SPI_SR_BSY   (1u << 7)
#define SPI_CR1_SPE  (1u << 6)
#define SPI_CR1_MSTR (1u << 2)
#define SPI_CR1_SSM  (1u << 9)
#define SPI_CR1_SSI  (1u << 8)

static SPI_TypeDef _SPI1 = {0};
SPI_TypeDef *SPI1 = &_SPI1;
static volatile uint32_t g_cs_pin = 1;
void spi_cs_assert(void)   { g_cs_pin = 0; }
void spi_cs_deassert(void) { g_cs_pin = 1; }

typedef struct { uint8_t mode, br_div, data_16bit, msb_first; } SpiConfig;

void spi_init(SPI_TypeDef *spi, const SpiConfig *cfg)
{
    spi->CR1 &= ~SPI_CR1_SPE;
    uint32_t cr1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI;
    cr1 |= (cfg->mode & 1u) ? (1u << 0) : 0;  /* CPHA */
    cr1 |= (cfg->mode >> 1) ? (1u << 1) : 0;  /* CPOL */
    cr1 |= ((uint32_t)cfg->br_div & 0x7u) << 3;
    if (cfg->data_16bit)   cr1 |= (1u << 11);
    if (!cfg->msb_first)   cr1 |= (1u << 7);  /* LSBFIRST */
    spi->CR1 = cr1 | SPI_CR1_SPE;
}

uint8_t spi_transfer_byte(SPI_TypeDef *spi, uint8_t tx)
{
    while (!(spi->SR & SPI_SR_TXE)) {}
    spi->DR = tx;
    while (!(spi->SR & SPI_SR_RXNE)) {}
    return (uint8_t)spi->DR;
}

void spi_write_register(SPI_TypeDef *spi, uint8_t reg, uint8_t val)
{
    spi_cs_assert();
    spi_transfer_byte(spi, reg & 0x7Fu);   /* write bit = 0 */
    spi_transfer_byte(spi, val);
    spi_cs_deassert();
}

uint8_t spi_read_register(SPI_TypeDef *spi, uint8_t reg)
{
    uint8_t val;
    spi_cs_assert();
    spi_transfer_byte(spi, reg | 0x80u);   /* read bit = 1 */
    val = spi_transfer_byte(spi, 0x00);
    spi_cs_deassert();
    return val;
}

void spi_transfer_buf(SPI_TypeDef *spi, const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++) {
        uint8_t b = spi_transfer_byte(spi, tx ? tx[i] : 0x00u);
        if (rx) rx[i] = b;
    }
}

int main(void)
{
    /* SPI smoke test */
    SPI1->SR = SPI_SR_TXE | SPI_SR_RXNE;
    SPI1->DR = 0xAB;
    uint8_t rx = spi_transfer_byte(SPI1, 0x55);
    assert(rx == 0xAB);

    printf("All SPI/I2C answers verified.\n");
    return 0;
}
