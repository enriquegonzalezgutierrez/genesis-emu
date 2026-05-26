# Software Design Document (SDD)
## 05. CPU Domain and Execution Loop (M68k & Timing)

**Project:** GenesisEmu  
**Version:** 1.0.0  

### 1. Purpose
This document outlines the Domain Model for the Motorola 68000 (M68k) main processor and defines the architectural strategy for hardware synchronization (Timing). Accurate emulation requires that the CPU and auxiliary chips (VDP, Z80) run in lockstep relative to their hardware clock speeds.

### 2. M68k CPU Domain Model
The CPU is an entity that maintains its own internal state and has exactly **one external dependency**: the `IBus` interface (defined in SDD-03).

#### 2.1. Internal State (Registers)
The M68k contains the following internal state representations:
*   **Data Registers (D0 - D7):** Eight 32-bit registers used for general arithmetic and logic.
*   **Address Registers (A0 - A7):** Eight 32-bit registers used for memory addressing. (A7 acts as the Stack Pointer - SP).
*   **Program Counter (PC):** A 32-bit register (though only 24 bits are physically routed to the bus) pointing to the next instruction to execute.
*   **Status Register (SR):** A 16-bit register. 
    *   *System Byte:* Contains the Interrupt Mask and Supervisor State flag.
    *   *Condition Code Register (CCR):* The lower byte containing the flags `X` (Extend), `N` (Negative), `Z` (Zero), `V` (Overflow), and `C` (Carry).

### 3. The Execution Pipeline (Fetch-Decode-Execute)
The CPU operates in a continuous loop, modeled as a state machine.

#### 3.1. Fetch
1. The CPU requests a `Word` (16 bits) from the `IBus` at the current `PC` address.
2. The `PC` is incremented by 2.

#### 3.2. Decode
The fetched 16-bit word is the **Opcode**. The CPU must determine which instruction to execute based on bit-mask matching (e.g., if the highest 4 bits are `0100`, it might be an `LEA` or `MOVE` instruction depending on the rest of the bits). 
*   *Design Note (SOLID):* To avoid a massive `switch` statement, opcodes can be decoded into polymorphic `Instruction` objects, or routed via a pre-computed function pointer table (Lookup Table) for performance.

#### 3.3. Execute
The decoded instruction performs its action (reading from registers, reading/writing to the `IBus`, updating `CCR` flags). 
*   **Crucial Return Value:** Every instruction must return the number of **Clock Cycles** it took to execute. For example, a `NOP` takes 4 cycles; a 32-bit division `DIVU` can take up to 140 cycles.

### 4. Synchronization and Timing (The Emulator Loop)
A modern host PC runs at GHz speeds; the M68k runs at `~7.67 MHz` (NTSC). If we run the loop as fast as possible, games will be unplayable. Components must be synced using **Cycle Counting**.

#### 4.1. The Master Loop Strategy
Instead of syncing to real-world time on every instruction (which is inefficient), the emulator groups execution into "Frames" (60 per second for NTSC).

```text
// Conceptual System Loop
void EmulateFrame() {
    int cyclesPerFrame = 7670454 / 60; // Approx 127,840 CPU cycles per frame
    int currentCycles = 0;

    while (currentCycles < cyclesPerFrame) {
        int consumedCycles = M68k.Step(); // Fetch-Decode-Execute
        
        currentCycles += consumedCycles;
        
        // Sync the VDP based on CPU cycles
        Vdp.Update(consumedCycles);
        
        // Sync Audio
        Z80_YM2612.Update(consumedCycles);
    }
}
```

#### 4.2. Interrupt Handling (VDP to CPU Communication)
The VDP tells the CPU when to stop rendering and start logic. It does this by triggering **Exceptions/Interrupts**.
*   **Level 4 Interrupt (HBlank):** Triggered by the VDP after rendering a single horizontal scanline (if enabled in VDP Reg #00).
*   **Level 6 Interrupt (VBlank):** Triggered by the VDP after rendering the entire visible screen (if enabled in VDP Reg #01).
*   **CPU Response:** When an interrupt occurs, the M68k finishes its current instruction, pushes the `PC` and `SR` to the Stack (via `IBus`), and jumps to a specific Vector Address (read from ROM `0x000070` for HBlank, `0x000078` for VBlank).

### 5. TDD Application
The CPU is the most rewarding component to test using TDD. There are thousands of edge cases, but they are 100% deterministic.

**Scenario: Testing the `ADD.W D0, D1` Opcode**
*   **Setup:** 
    *   Initialize `M68k` with a `MockBus`.
    *   Set `M68k.D0 = 0x00000005`.
    *   Set `M68k.D1 = 0x0000000A`.
    *   MockBus is programmed to return the opcode `0xD240` (ADD.W D0, D1) at `PC = 0x000000`.
*   **Action:** Call `M68k.Step()`.
*   **Asserts:**
    1.  `M68k.D1` must equal `0x0000000F`.
    2.  `M68k.PC` must equal `0x000002`.
    3.  `M68k.CCR.Z` (Zero) must be `False`.
    4.  `M68k.CCR.X` (Extend) must be `False`.
    5.  The method must return exactly `4` cycles.