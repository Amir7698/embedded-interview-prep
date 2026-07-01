# Personal Project Interview Answers

Real answers drawn from your own project experience.
Use these verbatim or adapt for behavioral / technical deep-dive questions.

---

## "Describe a communication protocol you designed."

**Q (EN):** "Describe the UART protocol you designed — the framing and CRC16."
**Q (IT):** "Descrivi il protocollo UART che hai progettato — il framing e il CRC16."

### EN Answer

> "We needed a structured binary-over-UART protocol between the PIC32 and the TI MCU.
> I designed ASCII-hex framing:
>
> - **Start byte:** `0x3A` (`':'`, like Intel HEX format)
> - **Command byte**, payload length, payload bytes encoded as ASCII-hex pairs
>   (`0xAB` → `'A','B'`)
> - **CRC16** of the payload (two bytes, also ASCII-hex), using the Modbus polynomial `0x8005`
> - **Terminated by CRLF**
>
> ASCII-hex makes the stream human-readable in a terminal — useful when you're
> sniffing the UART line during debug without a logic analyzer.
> CRC16 catches bit errors over the physical link.
> For synchronization, the UART ISR uses a dual-buffer ping-pong scheme:
> the ISR fills one buffer while the application task processes the other,
> avoiding any shared-state race condition."

### IT Answer

> "Avevo bisogno di un protocollo UART strutturato tra il PIC32 e il MCU TI.
> Ho progettato un framing ASCII-hex:
>
> - **Start byte:** `0x3A` (`':'`, come il formato Intel HEX)
> - **Byte comando**, lunghezza payload, bytes payload come coppie ASCII-hex
>   (`0xAB` → `'A','B'`)
> - **CRC16** del payload (due byte, ASCII-hex), con il polinomio Modbus `0x8005`
> - **Terminato da CRLF**
>
> L'ASCII-hex rende il flusso leggibile in un terminale — utile per fare sniffing
> sulla linea UART durante il debug senza un analizzatore logico.
> Il CRC16 rileva errori di bit sul link fisico.
> Per la sincronizzazione, l'ISR UART usa lo schema dual-buffer ping-pong:
> l'ISR riempie un buffer mentre il task applicativo processa l'altro,
> evitando race condition sullo stato condiviso."

### Likely follow-up questions

| Follow-up | Key point to hit |
|---|---|
| "Why ASCII-hex instead of raw binary?" | Debug visibility without logic analyzer; slight overhead (2× bytes) was acceptable at our baud rate |
| "Why `0x3A` as start byte?" | Borrowed from Intel HEX — familiar convention, rare in random noise |
| "Why CRC16 and not a simple checksum?" | CRC-16 detects all 1-bit, all 2-bit, and all burst errors ≤ 16 bits; checksum misses many multi-bit patterns |
| "What polynomial — 0x8005 or 0xA001?" | Same polynomial, different form: `0x8005` is normal (MSB-first); `0xA001` is reflected (LSB-first), used in the bit-loop implementation |
| "What happens if CRC fails?" | Receiver discards the frame and sends a NACK command byte back; sender retransmits up to 3 times |
| "How did the ping-pong work exactly?" | Two fixed buffers A and B. ISR pointer toggles on frame-complete flag. App task reads whichever buffer ISR is NOT currently writing to. A volatile flag signals buffer-ready |
