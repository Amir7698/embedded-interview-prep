# System Design: Bootloader Architecture

## The Interview Question

> "Design a bootloader for a safety-critical embedded device.
> It must support OTA updates, cryptographic verification, and
> rollback on failure. Walk me through your design."

This is a whiteboard system design question. There is no single
correct answer — the interviewer wants to see your thought process.

---

## Step 1: Ask Clarifying Questions

Before drawing anything, ask:

- What MCU? (STM32, iMX8, bare-metal or Linux?)
- What's the flash layout? (Internal flash? External SPI flash?)
- What update channel? (UART, USB, Ethernet, cellular?)
- What's the safety standard? (IEC 61508, ISO 26262, none?)
- What verification is required? (SHA256 hash? RSA signature? ECDSA?)
- How large can firmware be? (Determines sector allocation)
- Power-loss safety required? (Single vs dual-bank flash)

---

## Step 2: Flash Memory Layout

```
┌────────────────────────────────────────────────────┐  0x0800_0000 (STM32 example)
│                   BOOTLOADER                        │  64 KB — NEVER updated via OTA
│  (read-protected, physically separate from app)     │
├────────────────────────────────────────────────────┤  0x0801_0000
│              Boot Descriptor Block (BDB)            │  4 KB
│  magic, active_slot (0/1), attempt_count, flags    │
├────────────────────────────────────────────────────┤  0x0801_1000
│                  APPLICATION SLOT A                 │  192 KB
│  [Header: version, size, CRC, signature, date]     │
│  [Firmware binary]                                  │
├────────────────────────────────────────────────────┤  0x0804_1000
│                  APPLICATION SLOT B                 │  192 KB
│  (same structure as Slot A)                         │
├────────────────────────────────────────────────────┤  0x0807_1000
│              Persistent Data / NVM                  │  Remaining flash
│  (device config, calibration — never touched by BL)│
└────────────────────────────────────────────────────┘
```

**Why two slots?**
- Slot A = currently running firmware
- Slot B = new firmware being downloaded
- Download complete → verify B → set active_slot = B → reset
- If B fails to boot N times: revert to A
- This is called A/B (dual-bank) or ping-pong update

**Alternative: single-bank with scratch area**
- Saves flash but unsafe: if power fails during overwrite, brick
- Only acceptable for non-critical devices

---

## Step 3: Bootloader Boot Flow

```
RESET
  │
  ▼
[1] Hardware Init
    - Clock, watchdog, minimal GPIO
    - Do NOT init peripherals not needed for boot

  │
  ▼
[2] Read Boot Descriptor Block (BDB)
    - Validate magic number
    - If BDB corrupt → use factory defaults (Slot A)

  │
  ▼
[3] Integrity Check on Active Slot
    - Compute SHA-256 over [header + firmware]
    - Compare against header.sha256
    - If mismatch → mark slot bad → try other slot

  │
  ▼
[4] Signature Verification (if required)
    - Verify RSA-2048 / ECDSA-P256 signature over firmware hash
    - Public key stored in bootloader flash (write-protected)
    - If signature invalid → reject firmware, do NOT boot

  │
  ▼
[5] Version Downgrade Check
    - Check firmware version >= minimum_allowed_version
    - Prevents rollback attack (installing old vulnerable firmware)
    - minimum_allowed_version stored in OTP (one-time programmable) fuses

  │
  ▼
[6] Jump to Application
    - Set stack pointer: __set_MSP(app_header->stack_top)
    - Jump to reset handler: app_reset_handler()
    - On Cortex-M: must disable interrupts, set VTOR, jump

  │
  ▼
[7] Watchdog started in bootloader — app must pet it
    - If app never starts petting: watchdog resets
    - Bootloader increments attempt_count
    - After N failures: roll back to previous slot
```

---

## Step 4: OTA Update Flow

```
App receives update command (MQTT, UART, etc.)
  │
  ▼
[1] Open update channel, receive firmware binary + metadata
    (version, size, SHA-256, signature)

  │
  ▼
[2] Write to INACTIVE slot in chunks
    - Erase sector before writing (flash write rules)
    - Write 256-byte pages
    - Compute running CRC/hash as you go

  │
  ▼
[3] After complete download:
    - Verify running hash == expected hash
    - Write header to inactive slot

  │
  ▼
[4] Signal bootloader: "install this slot"
    - Update BDB: pending_slot = B, attempt_count = 0
    - Set UPDATE_PENDING flag in BDB

  │
  ▼
[5] Reset → Bootloader picks up UPDATE_PENDING
    - Verifies Slot B integrity + signature
    - If OK: active_slot = B
    - If not OK: clear flag, keep active_slot = A

  │
  ▼
[6] App boots from new slot
    - On first successful boot: set BDB.confirmed = 1
    - If not confirmed within N boots: rollback
```

---

## Step 5: Security Considerations

| Threat | Mitigation |
|--------|-----------|
| Unsigned firmware | Cryptographic signature verification (ECDSA-P256) |
| Replay attack (old firmware) | Version monotonicity + OTP fuses |
| Physical flash read | RDP (Read-out Protection) Level 2 on STM32 |
| Bootloader modification | Separate flash bank with write protection |
| Downgrade attack | Minimum version in OTP, anti-rollback counter |
| Supply chain attack | Secure boot chain, attestation certificate |

---

## Step 6: Watchdog Strategy

```c
/* Bootloader starts watchdog with 5-second window */
IWDG->KR  = 0xCCCC;  /* start */
IWDG->KR  = 0x5555;  /* unlock */
IWDG->PR  = 0x04;    /* prescaler /64 */
IWDG->RLR = 1953;    /* reload: 1953 * 64 / 32000Hz ≈ 3.9 s */
IWDG->KR  = 0xAAAA;  /* reload */

/* Bootloader kicks WDT during hash computation (can take >1s for 1MB) */

/* On jump to app: watchdog is STILL RUNNING */
/* App must call: IWDG->KR = 0xAAAA; within 3.9 seconds */
/* If app hangs: watchdog fires, bootloader increments fail count */
```

---

## Common Interview Follow-ups

**Q: What if flash erase fails mid-update?**
> The inactive slot header is only written AFTER verifying the full image.
> A partial erase/write leaves an invalid slot header → bootloader rejects it.

**Q: How do you handle a 1 MB firmware on a device with 128 KB RAM?**
> Stream and compute SHA-256 incrementally (sliding window hash).
> Never buffer the whole image in RAM.

**Q: Why is ECDSA-P256 better than RSA-2048 for embedded?**
> Smaller key size (256 vs 2048 bits), faster verification, smaller code.
> RSA signature verification requires ~2 KB RAM for the key; ECDSA < 500 bytes.

**Q: What happens if someone cuts power during the slot swap?**
> With A/B design: nothing lost. The BDB update (active_slot = B) is a
> single 4-byte write — atomic on most flash controllers.
> Slot A is never erased until B is confirmed working.

---

## Interview Checklist

Before saying "I'm done designing":

- [ ] Flash layout defined (bootloader, BDB, slot A, slot B, NVM)
- [ ] Integrity check (hash) + authentication (signature)
- [ ] Rollback mechanism (attempt counter, confirmation)
- [ ] Watchdog integration
- [ ] Anti-rollback (version check + OTP)
- [ ] Read protection / secure boot
- [ ] Power-loss safety (A/B or atomic update)
- [ ] Update channel and protocol defined
