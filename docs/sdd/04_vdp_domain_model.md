# Software Design Document (SDD)
## 04. VDP (Video Display Processor) Domain Model

**Project:** GenesisEmu  
**Version:** 1.0.0  

### 1. Purpose
This document defines the Domain Model for the Video Display Processor (VDP). The VDP is the most complex component of the Sega Genesis. By applying **Domain-Driven Design (DDD)**, we encapsulate its complexity behind a strict boundary (Aggregate Root) and model its internal behavior as a State Machine.

### 2. VDP as an Aggregate Root
In DDD terms, the `VDP` is an **Aggregate Root**. External components (like the M68k CPU via the `MainBus`) cannot directly access the VDP's internal memory (VRAM, CRAM, VSRAM). They can only interact with the VDP through its exposed Ports (Control Port and Data Port).

#### 2.1. Internal Entities (Memory Spaces)
The VDP encapsulates three private memory blocks:
*   **VRAM (Video RAM):** 64 KB (byte array). Stores tile graphics, sprite tables, and scroll maps.
*   **CRAM (Color RAM):** 128 Bytes (64 words). Stores 4 palettes of 16 colors each.
*   **VSRAM (Vertical Scroll RAM):** 80 Bytes (40 words). Stores vertical scrolling offsets.

#### 2.2. Value Objects
*   **Code/Command:** Defines the operation type (VRAM Read, VRAM Write, CRAM Write, DMA, etc.).
*   **TargetAddress:** A 16-bit internal address pointing to a location inside VRAM, CRAM, or VSRAM.

### 3. The Control Port State Machine (`$C00004`)
Based on the hardware specification, writing to the VdpCtrl port behaves differently depending on the value and the internal state. This will be modeled as a State Machine.

#### 3.1. Register Writes
If a 16-bit word is written to `$C00004` and its two most significant bits are `10` (Hex `$8000`), it is a **Register Write**.
*   **Format:** `$8000 OR (RegisterNumber << 8) OR Value`
*   **Behavior:** Updates the internal `VdpRegisters` entity.
*   **Side Effect:** Doing this resets the Control Port's "Write Pending" state (see below).

#### 3.2. Address and Command Writes (The Flip-Flop)
Setting a read/write/DMA address requires a 32-bit command. Since the data bus is 16-bit, it takes **two 16-bit writes**. The VDP contains an internal boolean flag (Flip-Flop or `firstWordWritten`).
*   **First Write (Flip-Flop = false):** VDP stores the high word in a temporary latch and sets `Flip-Flop = true`.
*   **Second Write (Flip-Flop = true):** VDP combines the temporary latch with the new low word to form the full 32-bit command. It extracts the `TargetAddress` and the `Code`, and sets `Flip-Flop = false`.

#### 3.3. Control Port Read
Reading from `$C00004` returns the **VDP Status Word** (e.g., VBlank active, HBlank active, DMA busy, Sprite collision). Reading the Control Port also resets the Flip-Flop.

### 4. The Data Port (`$C00000`)
When the CPU reads or writes to the Data Port, the VDP executes the operation defined by the last Command setup at the Control Port.
*   **Write Flow:** 
    1. Read word from CPU.
    2. Write word to `TargetAddress` in the selected memory (VRAM/CRAM/VSRAM).
    3. Increment `TargetAddress` by the value stored in VDP Register `$8F` (Auto-increment).
*   **Read Flow:** 
    Similar to Write, but fetches the word from internal memory and sends it to the CPU.

### 5. VDP Registers Entity
The VDP has 24 internal registers (`$00` to `$17`). This will be modeled as an internal Entity or data structure. Key registers include:
*   **Reg #00 & #01 (Mode Sets):** HBlank/VBlank interrupt enables, Display enable.
*   **Reg #02 to #05:** Base addresses for Plane A, Window, Plane B, and Sprite Tables.
*   **Reg #07:** Background Color index.
*   **Reg #0F (`$8F`):** Auto-increment step.
*   **Reg #13, #14, #15, #16, #17:** DMA Length and Source address registers.

### 6. TDD Scenarios for the VDP
The VDP is entirely testable without opening a graphics window. We will validate its logic using pure Unit Tests.

**Scenario 1: Register Write**
*   **Action:** Call `Vdp.WriteWord(0xC00004, 0x8F02)` (Set Auto-increment to 2).
*   **Assert:** VDP Internal Register `$0F` equals `0x02`.

**Scenario 2: VRAM Write via Data Port**
*   **Setup:** VDP Auto-increment is 2.
*   **Action 1 (Control):** `Vdp.WriteWord(0xC00004, 0x4000)` (First word of VRAM write to address $0000).
*   **Action 2 (Control):** `Vdp.WriteWord(0xC00004, 0x0000)` (Second word).
*   **Action 3 (Data):** `Vdp.WriteWord(0xC00000, 0xABCD)` (Write data).
*   **Assert 1:** VRAM at index `0x0000` contains `0xABCD`.
*   **Assert 2:** Internal `TargetAddress` is now `0x0002` (Auto-incremented).