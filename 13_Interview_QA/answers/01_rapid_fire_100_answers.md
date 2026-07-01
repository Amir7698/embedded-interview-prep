# Answers: 100 Rapid-Fire Embedded Questions

---

## Section A — C Language

**1. What does `volatile` do? When MUST you use it?**
Tells the compiler: "every read/write must actually happen — no caching in registers, no reordering." Use it for: (a) variables shared with ISRs, (b) memory-mapped hardware registers, (c) variables changed by another CPU core or DMA.

**2. Can a variable be both `const` and `volatile`?**
Yes. `const volatile uint32_t *STATUS_REG = (uint32_t*)0x40020000;`
`const` = software must not write it. `volatile` = hardware changes it autonomously. Classic example: a read-only status register.

**3. `static` at file scope vs function scope.**
File scope: limits visibility to this translation unit — the embedded equivalent of "private." Function scope: variable persists across function calls (stored in `.data`/`.bss`, not stack). Use for debounce counters, state machines.

**4. Why is `int` dangerous? What to use?**
`int` size is implementation-defined (16 or 32 bits depending on platform/compiler). Use `<stdint.h>` types: `uint8_t`, `uint16_t`, `uint32_t`, `int32_t`. Never `int` in protocol code or hardware register access.

**5. 3 examples of undefined behavior (UB).**
(a) Signed integer overflow: `INT_MAX + 1`. (b) Shifting by width of type: `uint32_t x; x << 32`. (c) Dereferencing NULL or freed pointer. Also: reading uninitialized variable, out-of-bounds array access.

**6. Struct padding vs packed struct.**
Compiler inserts padding bytes to align members (e.g., `uint32_t` aligned to 4 bytes). `__attribute__((packed))` removes padding — useful for protocol frames but causes unaligned accesses which crash on strict-alignment CPUs or cause slow reads on ARM.

**7. `sizeof` for `struct { char c; int x; }`?**
Not 5 — it's 8 (on 32-bit). `char` at offset 0, 3 bytes padding, `int` at offset 4. Use `__attribute__((packed))` for 5 bytes, accepting unaligned access penalties.

**8. Union use case in embedded.**
Type punning: access the same bytes as different types.
```c
union { uint32_t raw; float f; } u;
u.raw = 0x3F800000;
// u.f == 1.0f
```
Also: overlay registers, deserialize protocol bytes into a struct.

**9. `++i` vs `i++` in for loop.**
In `for (int i = 0; i < N; i++)` and `for (int i = 0; i < N; ++i)` — identical generated code. Difference only matters when the expression's value is used: `a = i++` (a gets old value) vs `a = ++i` (a gets new value).

**10. Type punning with unions — safe in C?**
Yes, in C11 (TC3) reading a union member different from the last written is defined. In C++, it is UB — use `memcpy` instead for C++.

**11. `restrict` keyword.**
Tells the compiler: "this pointer is the only alias to this memory in this scope." Enables better optimization (no alias analysis needed). Common in `memcpy`-like functions. `void my_memcpy(uint8_t * restrict dst, const uint8_t * restrict src, size_t n)`.

**12. Why avoid `malloc` in embedded production?**
Non-deterministic time, fragmentation over time, no recovery from failure, heap size unknown at compile time, forbidden by MISRA/IEC 61508. Use static allocation: fixed-size buffers, memory pools.

**13. Detecting memory leak on bare metal.**
Track pool allocation counts (alloc_count - free_count). Stack painting (0xA5 pattern + watermark). Heap check functions in FreeRTOS (`xPortGetFreeHeapSize`). For Linux embedded: Valgrind, Valgrind massif, `/proc/self/status` VmRSS.

**14. Stack overflow detection.**
FreeRTOS: stack painting + `uxTaskGetStackHighWaterMark()`. Bare metal: fill stack with canary (0xA5A5A5A5), check at runtime. MPU: configure stack guard region (generates fault on overflow). Cortex-M: MSPLIM register (M33/M4 with TrustZone).

**15. Circular (ring) buffer.**
Head = write index (producer), tail = read index (consumer). Size must be power-of-2 for mask trick. Full: `(head+1) & MASK == tail`. Empty: `head == tail`. Lock-free for SPSC (single producer, single consumer).

**16. Endianness. ARM Cortex-M default.**
Little-endian by default (can be configured BE8 in some variants, but virtually all real-world ARM embedded is LE). Network byte order is big-endian. Use `htons()`/`ntohl()` or manual `(buf[0]<<8)|buf[1]` for protocol parsing.

**17. Bit-banding.**
Maps each bit in SRAM/peripheral to its own 32-bit word in a 32MB alias region. Enables atomic bit set/clear without read-modify-write (eliminates race condition with ISR). Available on Cortex-M3/M4. Not on M0/M0+.

**18. `uint8_t *p` vs `const uint8_t *p`.**
`uint8_t *p`: pointer to mutable uint8_t. `const uint8_t *p`: pointer to read-only uint8_t (can't write `*p`). For a const pointer: `uint8_t * const p`. For both const: `const uint8_t * const p`.

**19. `__attribute__((packed))` risks.**
Removes struct padding. Risks: unaligned access — on Cortex-M0/M0+ this causes HardFault. On M3/M4 it works but slower (byte-by-byte loads). Pointer to packed member is not safe to dereference via unaligned pointer.

**20. `memcpy` vs `memmove`.**
`memcpy`: source and destination must NOT overlap (UB if they do). `memmove`: handles overlapping regions correctly (uses intermediate buffer or copies in reverse). Use `memmove` when shifting data within a buffer.

---

## Section B — Interrupts and ISR

**21. 5 things NEVER to do in ISR.**
(1) `printf` (uses heap, locks, blocking). (2) `malloc`/`free`. (3) `delay_ms()` or any blocking wait. (4) Lock a mutex (xSemaphoreTake — can block). (5) Floating-point without saving FPU context.

**22. Spurious interrupt.**
An interrupt that fires with no apparent source (cleared flag, no pending source). Causes: electrical noise, race condition in flag clearing. Handle defensively: check flag in ISR, ignore if no source found, do NOT loop indefinitely.

**23. What `volatile` fixes in ISR-to-main communication.**
Without volatile, compiler may cache the flag in a register across loop iterations and never re-read from memory. Main loop sees stale value forever. `volatile` forces every read to go to memory.

**24. Registers saved automatically on Cortex-M interrupt entry.**
Hardware pushes 8 registers to stack: R0, R1, R2, R3, R12, LR, PC, xPSR. You must manually save/restore R4-R11 (callee-saved) if ISR uses them (compiler does this automatically for ISR functions via the EXC_RETURN mechanism).

**25. Tail-chaining.**
When an interrupt occurs while the CPU is handling another IRQ of lower priority, it chains directly without unstacking/restacking. Saves ~12 cycles. Effective when multiple IRQs are pending.

**26. Preemption in interrupts.**
A higher-priority IRQ can interrupt (preempt) a lower-priority ISR currently executing. On Cortex-M: configurable via NVIC priority levels. A pending IRQ preempts if its priority group > current active group.

**27. NMI (Non-Maskable Interrupt).**
Cannot be disabled by software. Always executes. Use cases: clock failure monitor, watchdog NMI mode, power-fail detection. On STM32: BXCAN errors can generate NMI, watchdog can be configured to trigger NMI.

**28. NVIC. Setting interrupt priority.**
NVIC = Nested Vectored Interrupt Controller (Cortex-M core peripheral). Set priority: `NVIC_SetPriority(IRQn, priority)`. Enable: `NVIC_EnableIRQ(IRQn)`. Lower number = higher priority. Number of bits implemented varies (typically 4 bits = 16 levels on STM32).

**29. Interrupt priority vs preemption priority.**
STM32 uses group priority and sub-priority. Group priority determines preemption. Sub-priority breaks ties between same group-priority IRQs (no preemption, just ordering). Set via `NVIC_SetPriorityGrouping()`.

**30. Clear interrupt flag in ISR.**
If you don't clear the flag, the IRQ fires again immediately after returning → infinite ISR loop → system hang. Some peripherals clear on read of data register (UART RXNE cleared by reading DR). Others require explicit clear (TIM SR UIF cleared by `SR &= ~TIF_UIF`).

**31. Lazy FPU context saving (Cortex-M4F).**
By default, FPU registers (S0-S15, FPSCR) are NOT saved on interrupt entry to save cycles. Saving is deferred until the ISR actually uses FP instructions. If your ISR uses float: either set FPCCR.LSPEN=0 (eager), or don't use float in ISRs.

**32. Priority inversion. Cause.**
High-priority task H blocks waiting for a resource held by low-priority task L. A medium-priority task M preempts L (M > L). H starves even though H > M > L. Solution: priority inheritance in mutex — L is temporarily elevated to H's priority.

**33. Binary semaphore vs mutex for ISR signaling.**
Binary semaphore: no ownership tracking, can be given from ISR. Mutex: tracks which task owns it (for priority inheritance). You CANNOT give a mutex from an ISR (corrupts ownership state). Use `xSemaphoreGiveFromISR()` with binary semaphore.

**34. Critical section on bare metal.**
Disable interrupts: `__disable_irq()`, access shared data, re-enable: `__enable_irq()`. On Cortex-M with priorities: use BASEPRI register to mask interrupts below a threshold without blocking NMI/HardFault.

**35. `__disable_irq()` vs BASEPRI.**
`__disable_irq()` sets PRIMASK — disables ALL maskable interrupts. BASEPRI = N masks only interrupts with priority < N. BASEPRI is preferred in FreeRTOS (`taskENTER_CRITICAL()`) so NMI and HardFault still respond.

---

## Section C — Memory

**36. ARM Cortex-M memory layout.**
```
0x00000000: Code (flash) — .text, .rodata
0x20000000: SRAM — .data (initialized), .bss (zeroed),
                   heap (grows up), stack (grows down)
0x40000000: Peripherals (memory-mapped registers)
0xE0000000: System (SysTick, NVIC, SCB, DWT, ITM)
```

**37. .data and .bss initialization.**
`.data`: initialized globals (e.g., `int x = 5`). Values are stored in flash at startup and copied to SRAM by the startup code (Reset_Handler). `.bss`: zero-initialized globals (e.g., `int y;`). Startup code writes zeros to this region. `.bss` is NOT stored in flash (saves flash space — only start address and size are needed).

**38. Heap vs stack.**
Heap: dynamic memory (malloc/free), grows upward from bottom of free RAM. Stack: function call frames, local variables, return addresses, grows downward from top of RAM. They grow toward each other — collision = silent corruption.

**39. Stack overflow on bare metal.**
Silent corruption. Stack grows into heap, .bss, or .data. Variables get overwritten with garbage. System may run normally for a while then crash randomly. No automatic protection unless MPU is configured with a stack guard region.

**40. Dangling pointer.**
A pointer that references memory that has been freed or gone out of scope. `int *p = malloc(4); free(p); *p = 5;` — UB. On bare metal: function returns address of local variable — stack frame gone, pointer points to garbage.

**41. Use-after-free.**
Accessing memory after `free()`. Common bug: `free(p); if (p->flag) {...}`. The memory may have been reused by another allocation. Fix: `free(p); p = NULL;` — then NULL dereference is detectable.

**42. NULL vs wild pointer.**
NULL: pointer value of 0, explicitly invalid. Dereferencing causes fault (if MPU/MMU enabled). Wild pointer: uninitialized pointer with random value — can point anywhere, dereference corrupts random memory. Always initialize pointers.

**43. Memory fragmentation.**
After many alloc/free cycles, free memory exists in many small non-contiguous holes. A large allocation fails even though total free bytes > requested size. Embedded mitigation: fixed-size memory pools, allocate at startup only, never free.

**44. Memory pool.**
Pre-allocated array of N fixed-size blocks. `alloc()`: O(1), returns next free block. `free()`: marks block as available. No fragmentation (all blocks same size). Used in FreeRTOS queues, CAN message buffers, sensor data buffers.

**45. `__attribute__((section(".ccm")))`.**
Places the variable in a named linker section. The linker script maps `.ccm` to Core-Coupled Memory (CCM SRAM) on STM32F4 (64 KB at 0x10000000). CCM is connected directly to CPU D-bus — fastest possible data access. DMA cannot access CCM.

**46. CCMRAM. What you CANNOT do.**
DMA cannot access CCMRAM. Only CPU can. Use for: fast lookup tables, stack, RTOS data. Don't put: DMA buffers, shared peripherals buffers.

**47. Startup code initializes .data and .bss.**
Reset_Handler in startup_stm32xxxx.s:
1. Copy `.data` from flash (`_sidata` to `_edata`) to SRAM (`_sdata`).
2. Zero `.bss` from `_sbss` to `_ebss`.
3. Call `SystemInit()`.
4. Call `main()`.

**48. Linker script role.**
Defines memory regions (FLASH, RAM sizes and addresses), sections (.text, .data, .bss, .stack, .heap), and symbols (`_estack`, `_sidata`) used by startup code. Also places code in specific sections (`.ramfunc`, `.ccm`).

**49. SRAM1 vs SRAM2 on STM32.**
SRAM1 (112 KB): general purpose, DMA accessible, slower. SRAM2 (16 KB): parity-protected on some variants. Both at 0x20000000 region but physically separate — matters for MPU region configuration.

**50. DMA.**
Direct Memory Access controller moves data between peripherals and memory without CPU involvement. CPU sets up source, destination, count, and starts. DMA fires interrupt when done. Frees CPU for computation while data transfers in background.

---

## Section D — Peripherals and Protocols

**51. UART vs SPI vs I2C.**
UART: async, 2 wires (TX/RX), point-to-point, up to ~10 Mbit/s. SPI: sync, 4+ wires (SCLK/MOSI/MISO/CS), full duplex, multi-slave with 1 CS per slave, up to 100+ Mbit/s. I2C: sync, 2 wires (SDA/SCL), half duplex, multi-master multi-slave via 7-bit address, open-drain, up to 3.4 Mbit/s.

**52. Baud rate. BRR for 115200 on 84 MHz.**
`BRR = FCLK / (16 × BAUD) = 84,000,000 / (16 × 115200) = 45.57 → round to 45` (error ~1%).

**53. TXE vs TC flags.**
TXE (TX Empty): DR is empty, ready to write next byte. TC (Transmission Complete): all bits shifted out onto the line (shift register empty). Use TC for RS-485 direction control (must not deassert DE until TC).

**54. 4 SPI modes.**
Mode 0 (CPOL=0,CPHA=0): idle low, sample rising. Mode 1 (CPOL=0,CPHA=1): idle low, sample falling. Mode 2 (CPOL=1,CPHA=0): idle high, sample falling. Mode 3 (CPOL=1,CPHA=1): idle high, sample rising. Mode 0 is most common.

**55. Clock stretching in I2C.**
Slave holds SCL low to pause the transaction — gives slave time to prepare data. Master must support clock stretching (not poll/timeout on SCL). Needed when slave has slow processing (e.g., reading flash before responding).

**56. I2C bus lockup and recovery.**
Slave started sending but was reset mid-byte — holds SDA low. Master sends 9 clock pulses: most slaves will release SDA after seeing clocks. If not: assert STOP condition, then hardware reset. Kernel: `i2c_recover_bus()`.

**57. I2C ACK/NACK.**
After each byte, receiver pulls SDA low during 9th clock = ACK. If receiver releases SDA = NACK. Master sends NACK before the STOP on the last read byte to signal "no more data needed."

**58. PWM. Duty cycle formula.**
`Duty% = CCRx / (ARR + 1) × 100`. Frequency = `FCLK / ((PSC+1) × (ARR+1))`.

**59. Input capture mode.**
Timer captures the value of CNT register into CCRx when a specified edge occurs on the input pin. Used to measure pulse width, period, or frequency.

**60. ADC oversampling.**
Average N consecutive ADC readings to reduce noise and increase effective resolution. 4× oversampling → +1 bit resolution. 16× → +2 bits. Hardware oversampling on STM32: set OVSR and OSR bits in CFGR2.

**61. DMA double-buffering.**
DMA alternates between two buffers: while filling buffer B, CPU processes buffer A. When DMA completes A, ISR signals CPU, DMA switches to fill A while CPU processes B. Continuous data with no gaps.

**62. RS-485 vs RS-232.**
RS-232: single-ended, ±12V, point-to-point, max ~15m. RS-485: differential pair (A/B), ±5V, multi-drop up to 32 nodes, 1200m at 100 kbps. RS-485 requires direction control (DE pin).

**63. Modbus RTU. FC for holding registers.**
FC=03 reads holding registers. Frame: `[ADDR][0x03][START_HI][START_LO][COUNT_HI][COUNT_LO][CRC_LO][CRC_HI]`.

**64. CRC-16/IBM.**
Polynomial: 0x8005 (normal) = 0xA001 (reflected). Init: 0xFFFF. Input/output reflected. Used by Modbus RTU, USB.

**65. CAN frame. DLC.**
DLC = Data Length Code (4 bits), 0–8. Specifies number of payload bytes. Classic CAN: max 8 bytes. CAN FD: up to 64 bytes.

**66. CAN arbitration.**
Non-destructive bitwise OR. All nodes transmit simultaneously. Dominant bit (0) overrides recessive (1). Node loses arbitration when it tries to send 1 but sees 0 on bus — stops transmitting and retries. Lower ID wins (more dominant bits in ID field).

**67. CAN bus-off.**
Transmit Error Counter (TEC) > 255. Node stops transmitting (bus-off). Recovery: wait 128 × 11 recessive bits, then re-enter error active state. Or application reset.

**68. CAN FD.**
Flexible Data-rate CAN. Two-phase frame: arbitration at classical speed (up to 1 Mbit/s), data phase at up to 8 Mbit/s. Payload up to 64 bytes. Requires CAN FD controller and transceiver.

**69. SOME/IP.**
Scalable service-Oriented MiddlewarE over IP. Automotive middleware protocol (AUTOSAR). Runs over UDP/TCP. Used for service discovery and RPC in automotive Ethernet. Replaces CAN in high-bandwidth applications.

**70. GPIO push-pull vs open-drain.**
Push-pull: driver actively drives high AND low — fast, no pull-up needed. Open-drain: driver can only pull low; high state requires external pull-up resistor. I2C SDA/SCL must be open-drain (allows multiple masters/slaves to share the bus without short-circuit).

---

## Section E — RTOS

**71. Preemptive vs cooperative RTOS.**
Preemptive: scheduler can interrupt any task at any time when a higher-priority task becomes ready. Cooperative: tasks run until they voluntarily yield. Preemptive provides bounded latency; cooperative is simpler but one hung task stops everything.

**72. Tick. `configTICK_RATE_HZ`.**
Periodic interrupt (SysTick) that drives the RTOS scheduler. `configTICK_RATE_HZ=1000` → 1 ms per tick. All `vTaskDelay()` and timeout values are in ticks. Higher rate = better resolution but more CPU overhead.

**73. `vTaskDelay` vs `vTaskDelayUntil`.**
`vTaskDelay(100)`: delay 100 ticks from NOW. `vTaskDelayUntil(&lastWake, 100)`: delay until 100 ticks from LAST WAKE. `vTaskDelayUntil` is for precise periodic tasks — it compensates for processing time so the period is exact.

**74. Task stack sizing.**
Each task has its own stack. Size in words (4 bytes each). Include: local variables, function call depth, interrupt context (64 bytes), string buffers. Measure with `uxTaskGetStackHighWaterMark()`. Never less than `configMINIMAL_STACK_SIZE`. Add 20% margin.

**75. `uxTaskGetStackHighWaterMark()` return value.**
Returns minimum number of WORDS remaining since task started. Lower = closer to overflow. Return of 0 = overflow occurred. Check periodically in debug builds.

**76. 3 semaphore types in FreeRTOS.**
(1) Binary semaphore: 0 or 1, for event signaling. (2) Counting semaphore: 0 to N, tracks multiple resources. (3) Mutex: binary + priority inheritance, for resource protection.

**77. Why can't you give a mutex from ISR?**
Mutex tracks ownership for priority inheritance. `xSemaphoreGive()` checks and updates the owner task's priority. In ISR, there's no valid task context — accessing task control block from ISR corrupts RTOS internals. Use binary semaphore + `xSemaphoreGiveFromISR()`.

**78. Deadlock — two-task, two-mutex example.**
Task A: takes M1, then waits for M2. Task B: takes M2, then waits for M1. Neither can proceed. Both block forever. Prevention: always acquire locks in the same global order.

**79. Livelock vs deadlock.**
Deadlock: tasks are blocked, waiting forever (no progress). Livelock: tasks are NOT blocked — they keep running and changing state, but still make no overall progress (e.g., two tasks keep yielding to each other in a loop).

**80. Message queue in RTOS.**
FIFO buffer with built-in synchronization. `xQueueSend()` blocks if full. `xQueueReceive()` blocks if empty. Provides both data transfer and task synchronization. Can be used from ISR with `xQueueSendFromISR()`.

**81. `portMAX_DELAY` risk.**
Wait forever. Risk: if the event never comes (bug, deadlock), the task is stuck forever. In production, use finite timeouts and handle the timeout case (log error, reset peripheral, etc.).

**82. heap_4.c vs heap_1.c.**
heap_1.c: simplest — only allocates, never frees. No fragmentation. heap_4.c: first-fit with block merging — supports `free()`, merges adjacent free blocks. heap_5.c: heap_4 but across multiple discontinuous memory regions.

---

## Section F — Linux Embedded

**83. sysfs. 3 things controlled through it.**
Virtual filesystem at `/sys` exposing kernel objects as files. Control: (1) GPIO direction/value (`/sys/class/gpio`), (2) LED brightness/trigger (`/sys/class/leds`), (3) CPU frequency governor (`/sys/devices/system/cpu`).

**84. Toggle GPIO from Linux userspace.**
Option A (sysfs): `echo 17 > /sys/class/gpio/export; echo out > /sys/class/gpio/gpio17/direction; echo 1 > /sys/class/gpio/gpio17/value`. Option B (libgpiod): `gpioset gpiochip0 17=1`. Libgpiod is the modern preferred approach.

**85. Kernel module — insert/remove.**
`insmod mydriver.ko`: load into kernel. `rmmod mydriver`: remove. `modprobe`: handles dependencies. `lsmod`: list loaded modules. `dmesg`: check kernel log for module messages.

**86. Device tree.**
Hardware description file (DTS/DTB) that tells the Linux kernel what hardware exists and how it's connected (address, interrupts, clock sources, pin configuration). Replaces board-specific code. Loaded by bootloader, parsed by kernel at boot.

**87. systemd service unit file.**
```ini
[Unit]
Description=My Sensor App
After=network.target

[Service]
ExecStart=/usr/bin/sensor-app
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
```
`systemctl enable myapp.service` — start on boot.

**88. Debug kernel Oops.**
`dmesg | tail -50` — find the oops. Look for: PC (program counter), LR (link register), call stack. Use `addr2line -e vmlinux <address>` to find source line. Or `gdb vmlinux` with core dump.

**89. strace.**
Traces system calls of a running process. `strace -p PID` — attach to running process. `strace -e open,read ./app` — trace specific syscalls. `strace -o log.txt ./app` — save to file. Useful: find which file fails to open, which socket call blocks.

**90. Character device vs block device.**
Character device (`/dev/ttyS0`, `/dev/i2c-0`): byte-stream, no buffering, accessed sequentially. Block device (`/dev/sda`, `/dev/mmcblk0`): fixed-size blocks, buffered, random access, supports filesystems.

**91. TCP keepalive in Linux.**
```c
int enable = 1;
setsockopt(fd, SOL_SOCKET,  SO_KEEPALIVE,  &enable, sizeof(enable));
int idle = 60;    /* start after 60s idle */
setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE,  &idle,   sizeof(idle));
int interval = 10;
setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
int count = 3;
setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT,   &count,  sizeof(count));
```

---

## Section G — Testing and Debug

**92. Unit testing in embedded.**
Test individual functions in isolation. Tools: Unity (C), CppUTest, cmocka. Mock hardware dependencies (GPIO, SPI) with stub functions. Run on host PC for speed. Separate HAL from business logic to enable mocking.

**93. MC/DC coverage.**
Modified Condition/Decision Coverage. Each boolean condition in a decision must independently affect the outcome. Required by DO-178C (avionics Level A). Example: `if (A && B)` — test A=T,B=T; A=T,B=F; A=F,B=T. Not just branch true/false.

**94. HIL (Hardware-in-the-Loop) test setup.**
Real ECU/firmware running, with its I/O connected to a simulation model that mimics the physical plant (engine, motor, sensors). Stimulus injected by test PC via CAN/analog signals. Used to test without physical hardware, validate timing, inject faults.

**95. Find stack overflow at runtime.**
FreeRTOS: `uxTaskGetStackHighWaterMark()` < safety margin → alarm. Stack painting: fill with 0xA5 at startup, scan backwards for first non-0xA5. MPU: configure guard region below stack → HardFault on overflow → log PC.

**96. Valgrind on embedded targets.**
Valgrind runs on x86 Linux only. For embedded: run logic-only code on host under Valgrind (requires HAL abstraction). For ARM Linux embedded (Raspberry Pi, iMX8): Valgrind is available and runs natively. Not for bare-metal MCUs.

**97. ASAN (Address Sanitizer).**
Compiler instrumentation (`-fsanitize=address`) that detects: use-after-free, buffer overflow, use-after-return, double-free. Adds ~2× memory overhead and ~2× runtime overhead. Supported on ARM Linux. Not available on bare-metal without runtime support.

**98. Debug hard fault on ARM Cortex-M.**
1. In HardFault handler: read CFSR, HFSR, MMFAR, BFAR from SCB.
2. PC from stacked frame (MSP or PSP + 6th word) = faulting instruction.
3. Common causes: CFSR.BFARVALID + BFAR = bad memory address, CFSR.INVPC = invalid EXC_RETURN, CFSR.UNDEFINSTR = undefined instruction.
4. Use `addr2line -e firmware.elf <PC_value>` to find source line.

**99. JTAG vs SWD.**
JTAG: 4-wire (TCK, TMS, TDI, TDO) + TRST optional. Industry standard, supports chain of devices, can test board connections (boundary scan). SWD (Serial Wire Debug): 2-wire (SWDCLK, SWDIO). ARM-specific, saves pins, same debug capability as JTAG for single device. Most embedded boards use SWD.

**100. Lab works, field crashes — 5 likely causes.**
(1) **Timing/interrupt**: longer cable, slower sensor response, missed interrupt. Use oscilloscope. (2) **Power supply**: voltage sag under load, EMI from motors. Check with scope on VCC. (3) **Temperature**: component out of spec, crystal drift, MOSFET threshold shifts. Test at temperature extremes. (4) **Stack overflow**: more data in field (full log buffers, etc.). Check watermark. (5) **Race condition**: different traffic patterns expose rare concurrency bug. Add logging, increase tick for debug build.
