# embedded-interview-prep

> **From GPIO registers to Linux BSP.**
> Real coding exercises, bug hunts, and design problems
> from embedded systems interviews — across every level.

---

## The Story Behind This Repo

I applied to **12 embedded companies** in Italy this year.

For each one, I researched their tech stack and built real practice problems —
not theory, not LeetCode. Actual interview exercises:
- Write a CRC-16 from scratch
- Find the 3 bugs in this ISR
- Design a bootloader with OTA and rollback
- Implement Modbus RTU FC=03 request builder

This repo is everything I built. It goes from beginner C bit manipulation
all the way to Linux BSP, AUTOSAR, and full system design.

---

## What's Inside

| Folder | Topic | Files |
|--------|-------|-------|
| `01_C_Fundamentals/` | Bit manipulation, pointers, structs, volatile/const/static, endianness | 5 problems + answers |
| `02_Memory/` | Stack/heap, memory pools, linker sections, MMIO | 4 problems + answers |
| `03_Interrupts_and_ISR/` | ISR design, ring buffers, critical sections, DMA double-buffer | 4 problems + answers |
| `04_Bare_Metal_Peripherals/` | GPIO registers, UART, SPI/I2C, timers/PWM | 4 problems + answers |
| `05_RTOS/` | Tasks, semaphores/mutexes/queues, priority inversion, deadlock | 2 problems + answers |
| `06_Communication_Protocols/` | UART framing + CRC, Modbus RTU/TCP, CAN bus | 3 problems + answers |
| `07_Linux_Embedded/` | sysfs/GPIO, hwmon, /proc parsing, LED triggers, net stats | 1 problem + answers |
| `08_IoT_Protocols/` | MQTT packet builder, CoAP, JSON, TLS concepts | 1 problem + answers |
| `12_System_Design/` | Bootloader architecture, OTA, A/B flash, watchdog | 1 deep-dive markdown |
| `13_Interview_QA/` | 100 rapid-fire Q&A, 20 bug hunts, whiteboard exercises | 3 files + answers |

---

## How to Use This Repo

### Method 1: Practice First
1. Open a problem file (e.g., `01_C_Fundamentals/01_bit_manipulation.c`)
2. Read the THEORY block at the top
3. Implement all TODO sections
4. Try the bug hunt at the end
5. Check `answers/` folder to compare

### Method 2: Read and Understand
1. Read problem + answer side by side
2. Focus on the WHY in the comments
3. Close the file and reproduce from memory

### Method 3: Interview Simulation
1. Set a 30-minute timer
2. Open ONE file, implement WITHOUT reading answer
3. After 30 min, compare with answer, note gaps
4. Do this daily for 2 weeks

---

## File Naming Convention

```
XX_Topic/
├── NN_problem_name.c       ← TODO stubs + theory + bug hunt
└── answers/
    └── NN_problem_name_answers.c  ← complete implementations + explanations
```

---

## Build and Run (Host PC)

Every C file compiles on Linux/Mac/Windows (GCC/Clang):

```bash
gcc -Wall -Wextra -o test 01_C_Fundamentals/01_bit_manipulation.c && ./test
```

Python files (Linux Embedded section):
```bash
python3 07_Linux_Embedded/some_file.py
```

---

## Topics Covered

**C Programming**
- Bit manipulation (set/clear/toggle, field extraction, popcount, reverse bits)
- Pointer arithmetic, function pointers, memory-mapped registers
- Struct padding, packed structs, bit-fields, union type-punning
- `volatile`, `const`, `static` — with ISR and hardware register use cases
- Endianness detection, byte-swap functions, protocol parsing

**Memory**
- Stack vs heap, .data vs .bss, flash regions
- Fixed-size memory pools (O(1) alloc/free, no fragmentation)
- Stack watermark / painting (FreeRTOS-style)
- Linker section attributes (`__attribute__((section(...)))`)

**Interrupts**
- ISR golden rules (what you MUST NOT do)
- SPSC ring buffer (lock-free)
- Critical sections (disable/enable IRQ)
- DMA double-buffering pattern
- Priority inversion, nested interrupts

**Peripherals (STM32 register-level)**
- GPIO: MODER, OTYPER, OSPEEDR, PUPDR, IDR, ODR, BSRR, AFRL/AFRH
- UART: BRR calculation, polling TX, ISR-driven ring buffer TX
- SPI: 4 modes (CPOL/CPHA), full-duplex transfer, CS management
- I2C: init, write transaction, combined read transaction, ACK/NACK
- Timers: PSC/ARR calculation, PWM mode 1, input capture, SysTick

**RTOS (FreeRTOS)**
- Task creation, priorities, `vTaskDelayUntil`
- Binary semaphore (ISR→task), mutex (priority inheritance)
- Message queues (producer/consumer)
- Deadlock, livelock, priority inversion

**Communication Protocols**
- UART framing: start/end byte, length-prefixed, byte stuffing
- CRC-16/IBM (Modbus): bit-by-bit and table-driven
- Modbus RTU: FC=03 request/response, FC=10 write multiple
- CAN bus: arbitration, error counters, TEC/REC, Bus-Off
- CAN signal extraction (Intel byte order)

**Linux Embedded**
- sysfs GPIO (export/direction/value/unexport)
- LED trigger control (timer trigger, delay_on/delay_off)
- /proc/meminfo parsing
- hwmon temperature reading
- Network interface statistics (/sys/class/net)

**IoT Protocols**
- MQTT: QoS levels, topic wildcards, PUBLISH packet binary format
- CoAP vs MQTT comparison
- JSON payload construction and simple parsing
- TLS concepts (mbedTLS, mTLS, PSK)

**System Design**
- Bootloader: flash layout, A/B slots, integrity + signature verification
- OTA update flow with rollback
- Security: read protection, anti-rollback, OTP fuses

**Interview Practice**
- 100 rapid-fire questions with full answers
- 20 real embedded bugs (volatile miss, race condition, CRC byte order, etc.)
- Whiteboard exercises: memcpy, ring buffer, CRC, state machine, endianness

---

## Who This Is For

- Embedded engineers preparing for technical interviews
- Students transitioning from university projects to industry
- Senior engineers brushing up on fundamentals before a new role
- Anyone who wants to go deeper than tutorials

---

## License

MIT — use it, fork it, share it.

---

## Author

Built by an embedded engineer who spent months preparing for interviews
across Italian embedded companies (automotive, industrial IoT, medical devices).

Every exercise here came from a real interview question or a real production bug.
