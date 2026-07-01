# Bug Hunt Collection — 20 Real Embedded Bugs

> Each bug below is the type that appears in embedded interviews and
> real production incidents. Some are subtle, some are obvious.
> The skill is EXPLAINING what's wrong and why it matters.

---

## Bug 1 — The Volatile Miss

```c
uint8_t g_flag = 0;

void UART_IRQHandler(void) {
    g_flag = 1;
}

int main(void) {
    while (!g_flag) {
        // wait for ISR
    }
    process_data();
}
```

**What's wrong?**
`g_flag` is missing `volatile`. The compiler sees `g_flag` never changes in `main()` (from its perspective), optimizes the loop to `while (1)`, and `main()` never exits.

**Fix:** `volatile uint8_t g_flag = 0;`

---

## Bug 2 — Integer Overflow in Timeout

```c
uint8_t start = get_tick_ms();   // returns uint8_t
// ... some processing ...
if ((get_tick_ms() - start) > 1000) {
    timeout_handler();
}
```

**What's wrong?**
`get_tick_ms()` returns `uint8_t` (max 255). The difference wraps correctly for values ≤ 255, but a timeout of 1000 ms can never be reached since the subtraction result (uint8_t) can never exceed 255. Also: 1000 > 255 so the condition is always false at compile time (depending on types).

**Fix:** Use `uint32_t` for tick counters. `uint32_t` at 1kHz wraps every 49 days.

---

## Bug 3 — Missing NULL check after malloc

```c
void process_message(uint8_t len) {
    uint8_t *buf = malloc(len);
    memcpy(buf, g_uart_buf, len);
    parse_message(buf);
    free(buf);
}
```

**What's wrong?**
If `malloc` fails (heap full), `buf` is NULL. `memcpy(NULL, ...)` is UB — HardFault on bare metal.

**Fix:** `if (!buf) { log_error(ERR_OOM); return; }`

---

## Bug 4 — Race Condition on Multi-Byte Read

```c
// Written by ISR:
volatile uint16_t g_timestamp_high;
volatile uint16_t g_timestamp_low;

// Read by main:
uint32_t get_timestamp(void) {
    return ((uint32_t)g_timestamp_high << 16) | g_timestamp_low;
}
```

**What's wrong?**
Between reading `g_timestamp_high` and `g_timestamp_low`, the ISR may fire and update both. Main reads old high + new low → garbage timestamp.

**Fix:** Disable interrupts around the two reads, or use a 32-bit atomic type if MCU supports it.

---

## Bug 5 — I2C Bus Stuck After Power Cycle

```c
void i2c_init(void) {
    // Configure GPIO for SDA/SCL
    gpio_set_af(I2C_SDA_PIN, AF4);
    gpio_set_mode(I2C_SDA_PIN, GPIO_MODE_AF);
    // Initialize I2C peripheral
    i2c_configure(400000);
}
```

**What's wrong?**
On power cycle, if the slave was in the middle of sending data, SDA may be held low by the slave. The I2C peripheral init doesn't handle this. Bus is stuck (SDA = 0 = arbitration lost forever).

**Fix:** Before `i2c_configure()`, toggle SCL 9 times with GPIO to clock out the stuck slave, then send STOP condition.

---

## Bug 6 — CRC Byte Order

```c
uint16_t crc = compute_crc16(payload, len);
frame[len]   = crc >> 8;       // high byte first
frame[len+1] = crc & 0xFF;     // low byte second
```

**What's wrong?**
Modbus RTU specifies CRC as LITTLE-ENDIAN (low byte first). This code sends big-endian. The receiver's CRC check will fail.

**Fix:** `frame[len] = crc & 0xFF; frame[len+1] = crc >> 8;`

---

## Bug 7 — printf in ISR

```c
void TIM2_IRQHandler(void) {
    TIM2->SR &= ~TIM_SR_UIF;
    g_tick++;
    if (g_tick % 1000 == 0) {
        printf("1 second elapsed\n");  // BUG
    }
}
```

**What's wrong?**
`printf` uses `malloc` internally, accesses locks (mutex), and may block waiting for UART TX. Calling it from an ISR can deadlock the system (if main holds a printf lock), corrupt the heap, or simply take milliseconds blocking your ISR.

**Fix:** Set a flag in the ISR. Print in main loop.

---

## Bug 8 — Stack Allocated Buffer Returned

```c
const char *get_version_string(void) {
    char buf[32];
    snprintf(buf, sizeof(buf), "v%d.%d.%d", MAJOR, MINOR, PATCH);
    return buf;  // BUG: returning pointer to stack
}
```

**What's wrong?**
`buf` is a local variable. When the function returns, the stack frame is gone. The returned pointer is dangling. The string may appear valid briefly (stack not yet overwritten) then corrupt silently.

**Fix:** Use `static char buf[32]` (but not thread-safe) or caller-provided buffer.

---

## Bug 9 — Unaligned Access via Cast

```c
uint8_t frame[16];
// ...receive frame via UART...
uint16_t length = *(uint16_t*)(frame + 1);  // BUG: offset 1 is not 2-byte aligned
```

**What's wrong?**
On Cortex-M0/M0+: HardFault (strict alignment enforced). On M3/M4: works but compiler may generate 2 byte-reads instead of 1 halfword load (slower, potential tearing on non-atomic access).

**Fix:** `uint16_t length = (uint16_t)((uint16_t)frame[1] << 8) | frame[2];` (no alignment assumption).

---

## Bug 10 — Watchdog Not Fed in Slow Branch

```c
void main_loop(void) {
    while (1) {
        watchdog_feed();
        
        if (sensor_available()) {
            process_sensor();   // this takes 500ms
        }
    }
}
```

**What's wrong?**
Watchdog fed at top of loop. If `sensor_available()` is true and `process_sensor()` takes 500 ms while the watchdog timeout is 400 ms → system resets mid-processing.

**Fix:** Feed watchdog inside long operations, or increase watchdog timeout, or break `process_sensor()` into smaller pieces.

---

## Bug 11 — Modbus Register Byte Order

```c
uint16_t register_val = 1500;  // RPM

response[3] = register_val & 0xFF;    // BUG: LSB first
response[4] = register_val >> 8;      // MSB second
```

**What's wrong?**
Modbus registers are BIG-ENDIAN (MSB first). This sends little-endian. The master reads `(0x05 << 8) | 0xDC = 0xDC05 = 56325` instead of `0x05DC = 1500`.

**Fix:** `response[3] = register_val >> 8; response[4] = register_val & 0xFF;`

---

## Bug 12 — Double Free

```c
void cleanup(uint8_t *buf1, uint8_t *buf2) {
    free(buf1);
    free(buf2);
    if (buf1) free(buf1);   // BUG: freed twice
}
```

**What's wrong?**
`free(buf1)` twice: after the first free, the heap metadata for that block may be reused. Freeing it again corrupts heap internals — crash or silent corruption later.

**Fix:** After `free(p)`, always set `p = NULL`. Second `if (p) free(p)` will be skipped.

---

## Bug 13 — Signed/Unsigned Comparison Bug

```c
void process_buffer(uint8_t *buf, int8_t len) {
    if (len < 0) return;
    for (uint8_t i = 0; i < len; i++) {  // BUG: mixing signed/unsigned
        process(buf[i]);
    }
}
```

**What's wrong?**
`i` is `uint8_t`, `len` is `int8_t`. Comparison `i < len` — if `len` is negative, it gets sign-extended to a large unsigned value (due to integer promotion), and the loop runs indefinitely.

**Fix:** Use consistent types. `uint8_t len` or explicit cast: `i < (uint8_t)len`.

---

## Bug 14 — ISR Clears Wrong Flag

```c
void USART1_IRQHandler(void) {
    if (USART1->SR & USART_SR_RXNE) {
        uint8_t data = USART1->DR;
        ring_push(&g_rx_buf, data);
        USART1->SR &= ~USART_SR_RXNE;  // BUG
    }
}
```

**What's wrong?**
RXNE in USART1_SR is cleared by reading DR, NOT by writing to SR. The `SR &= ~RXNE` write does nothing useful (and on some MCUs, writing 0 to other bits accidentally clears error flags). Moreover, the read of DR already cleared it.

**Fix:** Remove the `SR &= ~RXNE` line. Reading `DR` is sufficient to clear `RXNE`.

---

## Bug 15 — Timer Overflow in Subtraction

```c
uint32_t start = TIM2->CNT;
do_something();  // takes ~50ms
uint32_t elapsed = TIM2->CNT - start;  // BUG when counter wraps
if (elapsed > TIMEOUT_TICKS) timeout();
```

**What's wrong?**
If the timer wraps (CNT overflows from 0xFFFFFFFF to 0), `TIM2->CNT - start` gives a huge number, falsely triggering timeout.

**Fix:** **Actually this is fine** if both are `uint32_t`. Unsigned subtraction wraps correctly: `(0x00000005 - 0xFFFFFFF0) = 0x00000015` (correct elapsed time). The key: BOTH must be the same unsigned type. If start is `int32_t`, it breaks.

---

## Bug 16 — Mutex Taken, Never Released on Error Path

```c
int read_sensor(float *out) {
    xSemaphoreTake(g_spi_mutex, portMAX_DELAY);
    
    if (spi_exchange(0x80) != 0x55) {
        return -1;  // BUG: mutex never released!
    }
    
    *out = decode_reading(spi_exchange(0x00));
    xSemaphoreGive(g_spi_mutex);
    return 0;
}
```

**What's wrong?**
On error path, function returns without giving the mutex. Next task that tries to take the mutex blocks forever → deadlock.

**Fix:** Always give the mutex before every return, or use goto cleanup pattern.

---

## Bug 17 — Bit Manipulation: Forgot to Clear Before Set

```c
void set_gpio_mode(GPIO_TypeDef *gpio, uint8_t pin, uint8_t mode) {
    gpio->MODER |= (mode << (pin * 2));  // BUG: ORs without clearing first
}
```

**What's wrong?**
If the pin was previously in `AF` mode (0b10) and you try to set `OUTPUT` (0b01), you OR 0b01 into 0b10 = 0b11 (Analog mode). Always clear the field first.

**Fix:** `gpio->MODER = (gpio->MODER & ~(0b11u << (pin*2))) | ((uint32_t)mode << (pin*2));`

---

## Bug 18 — sprintf Buffer Overflow

```c
char msg[16];
sprintf(msg, "Error code: %d, module: %s", error_code, module_name);
uart_send(msg);
```

**What's wrong?**
If `error_code` has many digits and/or `module_name` is long, `sprintf` writes past the end of `msg[16]`. Silent stack corruption (overwrites return address, other locals, etc.).

**Fix:** `snprintf(msg, sizeof(msg), ...)`. Or use a larger buffer. Check the max possible string length.

---

## Bug 19 — Missing `break` in switch

```c
void handle_command(uint8_t cmd) {
    switch (cmd) {
        case CMD_START:
            start_motor();
            // BUG: missing break — falls through to stop!
        case CMD_STOP:
            stop_motor();
            break;
        case CMD_RESET:
            system_reset();
            break;
    }
}
```

**What's wrong?**
Missing `break` after `CMD_START`. The code falls through and calls `stop_motor()` immediately after starting it.

**Fix:** Add `break;` after `start_motor();`. In C17, add `[[fallthrough]];` where intentional fallthrough is desired.

---

## Bug 20 — printf Blocks System

```c
volatile uint8_t g_overrun = 0;

void UART_IRQHandler(void) {
    if (USART1->SR & USART_SR_ORE) {  // overrun error
        g_overrun = 1;
        printf("UART overrun!\n");  // BUG: printf IN ISR
    }
    // ...
}
```

**What's wrong?**
Same as Bug 7 — `printf` in ISR. This case is especially dangerous: the overrun error means UART is already overwhelmed, and calling `printf` (which internally uses UART) makes it worse. Classic feedback loop.

**Fix:** Set `g_overrun = 1` only. Log in main loop. Increment a counter. Never print from ISR.
