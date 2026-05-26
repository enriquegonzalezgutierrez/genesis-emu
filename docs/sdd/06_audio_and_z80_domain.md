# Software Design Document (SDD)
## 06. Audio Subsystem and Z80 Coprocessor Domain

**Project:** GenesisEmu  
**Version:** 1.0.0  

### 1. Purpose
This document defines the Domain Model for the Audio Subsystem, which is driven by a secondary processor (the Zilog Z80). It establishes the rules for inter-processor communication, memory sharing (Bank Switching), and the encapsulation of the audio synthesizers (YM2612 and PSG).

### 2. The Coprocessor Domain (Z80)
While the M68k handles game logic and graphics, the Z80 runs independently at `~3.58 MHz` to manage audio. 

In DDD terms, the Z80 Subsystem is a distinct **Bounded Context**. It has its own CPU State (Registers AF, BC, DE, HL, PC, SP), its own Execution Loop (Fetch-Decode-Execute), and its own local memory bus (`IZ80Bus`).

#### 2.1. Z80 Local Memory Map (16-bit Address Space)
The Z80 can only address 64 KB of memory (`$0000` to `$FFFF`). Its internal `IZ80Bus` routes requests as follows:
*   `$0000 - $1FFF`: **Z80 RAM** (8 KB local work RAM).
*   `$4000 - $4003`: **YM2612** FM Synthesizer.
*   `$6000`: **Bank Register** (Used for Bank Switching, see 2.2).
*   `$7F11`: **PSG** (SN76489 Programmable Sound Generator).
*   `$8000 - $FFFF`: **M68k Memory Window** (32 KB window into the main 68000 address space, usually pointing to the Cartridge ROM to read PCM samples or music data).

#### 2.2. Bank Switching
To allow the 16-bit Z80 to read from the 24-bit M68k memory space (e.g., reading a drum sample at `$200000`), the Z80 uses a 9-bit **Bank Register**.
*   When the Z80 writes a 9-bit value to `$6000`, this value forms the upper 9 bits of a 24-bit address.
*   When the Z80 reads from its local `$8000 - $FFFF` window, the `IZ80Bus` translates the address: `ActualAddress = (BankRegister << 15) | (Z80_Address & 0x7FFF)`.
*   The `IZ80Bus` then delegates this read request to the `MainBus`.

### 3. Inter-Processor Communication (Bus Arbiter)
Both the M68k and the Z80 share physical traces to access the Z80 RAM and Audio chips. If both read/write at the exact same time, a hardware crash occurs. 
To prevent this, the architecture implements a **Bus Arbiter** via specific addresses on the `MainBus`.

#### 3.1. Z80 Bus Request (`$A11100`)
If the M68k wants to write a sound command to the Z80 RAM, it must first "request the bus".
*   **Requesting:** M68k writes `1` to `$A11100`. The Z80 finishes its current instruction and halts.
*   **Granting:** M68k reads `$A11100`. If it returns `0`, the bus is granted to the M68k. The M68k can now safely read/write to the `$A00000 - $A0FFFF` range.
*   **Releasing:** M68k writes `0` to `$A11100`. The Z80 resumes execution.

#### 3.2. Z80 Reset (`$A11200`)
The M68k can force the Z80 to reboot by writing `0` and then `1` to `$A11200`.

### 4. Audio Entities (YM2612 & PSG)
The audio chips are standard `IMemoryMappedDevice` entities. They receive writes from either the M68k (if bus is granted) or the Z80.

#### 4.1. YM2612 (FM Synthesizer)
Based on the hardware specs, it is a complex state machine with:
*   Two sets of registers (Bank 0: `$4000/1`, Bank 1: `$4002/3`).
*   6 Channels (Channel 6 can act as a PCM DAC).
*   Timers (Timer A and Timer B) that can trigger interrupts.
*   *Architecture Note:* Audio generation (mixing oscillators and envelopes) will be decoupled from memory access. The `YM2612` class will store register states, and an external `AudioAdapter` (in the Outer Hexagon) will pull samples at 44.1kHz.

### 5. TDD Application
Testing the Bus Arbiter is critical for system stability.

**Scenario: M68k accessing Z80 RAM without requesting the bus**
*   **Setup:** M68k and Z80 are running. Bus Request flag is `0` (Z80 owns the bus).
*   **Action:** M68k attempts to write `0xFF` to `MainBus` at `$A00000` (Z80 RAM).
*   **Assert:** The write is **ignored** (or stored in a dummy void), and Z80 RAM at `$0000` remains unchanged, mimicking hardware behavior.

**Scenario: M68k successfully requesting the bus**
*   **Action 1:** M68k writes `1` to `$A11100`.
*   **Assert 1:** Z80 state changes to `Halted`. `MainBus.ReadByte(0xA11100)` returns `0` (Bus granted).
*   **Action 2:** M68k writes `0xFF` to `$A00000`.
*   **Assert 2:** Z80 RAM at index `0x0000` now contains `0xFF`.