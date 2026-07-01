# 100 Rapid-Fire Embedded Interview Questions

> These are real questions from real embedded interviews.
> Drill until you can answer each in under 30 seconds.
> Answers in `answers/01_rapid_fire_100_answers.md`

---

## Section A — C Language (1–20)

1. What does `volatile` do? When MUST you use it?
2. Can a variable be both `const` and `volatile`? Give an example.
3. What is `static` at file scope vs function scope?
4. Why is `int` dangerous for embedded code? What should you use?
5. What is undefined behavior? Give 3 examples.
6. What is the difference between `struct padding` and `packed` struct?
7. `sizeof(struct S)` — will it be 5 for a struct with `char` + `int`? Why?
8. What is a `union`? Give a use case in embedded.
9. What is the difference between `++i` and `i++` in a for loop?
10. What is type punning? Is it safe with unions in C?
11. What is `restrict` keyword? Where is it useful in embedded?
12. Why is `malloc` avoided in embedded production firmware?
13. What is a memory leak? How do you detect it on a bare-metal MCU?
14. What is stack overflow? How do FreeRTOS and bare-metal detect it?
15. How do you implement a circular (ring) buffer?
16. What is endianness? Is ARM Cortex-M little or big endian by default?
17. What is bit-banding? What problem does it solve?
18. What is the difference between `uint8_t *p` and `const uint8_t *p`?
19. What does `__attribute__((packed))` do? What risks does it carry?
20. What is the difference between `memcpy` and `memmove`?

---

## Section B — Interrupts and ISR (21–35)

21. List 5 things you must NEVER do inside an ISR.
22. What is a spurious interrupt?
23. What does `volatile` fix in ISR-to-main communication?
24. On ARM Cortex-M4, which registers are saved automatically on interrupt entry?
25. What is tail-chaining? Why is it efficient?
26. What is preemption in the context of interrupts?
27. What is an NMI (Non-Maskable Interrupt)? Give a use case.
28. What is the NVIC? How do you set interrupt priority on Cortex-M?
29. What is the difference between interrupt priority and preemption priority?
30. Why must you clear the interrupt flag inside the ISR?
31. What is lazy FPU context saving on Cortex-M4F?
32. What is priority inversion? What causes it?
33. How does a binary semaphore differ from a mutex for ISR signaling?
34. What is a critical section? How is it implemented on bare metal?
35. What is the difference between `__disable_irq()` and `BASEPRI`?

---

## Section C — Memory (36–50)

36. Draw the memory layout of a typical ARM Cortex-M application.
37. What is `.data` section? What is `.bss`? How are they initialized?
38. What is the heap? What is the stack? Where does each grow?
39. What happens at stack overflow on bare metal?
40. What is a dangling pointer?
41. What is use-after-free?
42. What is the difference between `NULL` and a wild (uninitialized) pointer?
43. What is memory fragmentation? Why does it matter in long-running systems?
44. What is a memory pool? What are its advantages?
45. What is `__attribute__((section(".ccm")))` used for?
46. What is CCMRAM on STM32? What can you NOT do with it?
47. How does the startup code initialize `.data` and `.bss`?
48. What is the linker script's role in embedded?
49. What is SRAM1 vs SRAM2 on STM32? Why are they separate?
50. What is DMA? How does it avoid CPU involvement in data transfer?

---

## Section D — Peripherals and Protocols (51–70)

51. What is the difference between UART, SPI, and I2C?
52. What is baud rate? Calculate BRR for 115200 baud on 84 MHz clock.
53. What is the difference between TXE and TC flags in USART?
54. What are the 4 SPI modes? Which is most common?
55. What is clock stretching in I2C?
56. What is an I2C bus lockup? How do you recover?
57. What is the I2C ACK/NACK mechanism?
58. What is PWM? Write the formula for duty cycle with ARR and CCR.
59. What is input capture mode on a timer?
60. What is ADC oversampling? When do you use it?
61. What is DMA double-buffering? Draw the pattern.
62. What is RS-485? How does it differ from RS-232?
63. What is Modbus RTU? What function code reads holding registers?
64. CRC-16/IBM — what is the polynomial? What is the initial value?
65. What is a CAN frame? What is DLC?
66. How does CAN arbitration work?
67. When does a CAN node go to Bus-Off?
68. What is CAN FD? What's the max payload?
69. What is SOME/IP?
70. What is the difference between GPIO push-pull and open-drain?

---

## Section E — RTOS (71–82)

71. What is the difference between a preemptive and cooperative RTOS?
72. What is a tick? What does `configTICK_RATE_HZ` control?
73. `vTaskDelay(100)` vs `vTaskDelayUntil`. What is the difference?
74. What is a task's stack? How do you size it?
75. What does `uxTaskGetStackHighWaterMark()` return?
76. What is a semaphore? What are the 3 types in FreeRTOS?
77. Why can you NOT give a mutex from an ISR?
78. What is deadlock? Give a two-task, two-mutex example.
79. What is livelock? How does it differ from deadlock?
80. What is a message queue in an RTOS?
81. What is `portMAX_DELAY`? What risk comes with using it?
82. What is heap_4.c vs heap_1.c in FreeRTOS?

---

## Section F — Linux Embedded (83–91)

83. What is sysfs? Name 3 things you control through it.
84. How do you toggle a GPIO from a Linux userspace application?
85. What is a kernel module? How do you insert/remove it?
86. What is a device tree? What does it describe?
87. What is systemd? How do you write a service unit file?
88. How do you debug a kernel crash (Oops)?
89. What is strace? Give a useful strace command for debugging an embedded app.
90. What is a character device? How does it differ from a block device?
91. How do you set up TCP keepalive in Linux?

---

## Section G — Testing and Debug (92–100)

92. What is unit testing in embedded? What tool do you use?
93. What is MC/DC coverage? Why is it required in DO-178C?
94. What is a HIL (Hardware-in-the-Loop) test setup?
95. How do you find a stack overflow bug at runtime?
96. What is Valgrind? Can you use it on embedded targets?
97. What is ASAN (Address Sanitizer)?
98. How do you debug a hard fault on ARM Cortex-M?
99. What is a JTAG debugger? What is SWD? How do they differ?
100. You deploy firmware and it works in the lab but crashes in the field.
     List the 5 most likely causes and how you would investigate each.
