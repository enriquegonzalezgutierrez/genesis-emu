# Software Design Document (SDD)
## 02. Ubiquitous Language and Memory Map

**Project:** GenesisEmu  
**Version:** 1.0.0  

### 1. Purpose
In accordance with Domain-Driven Design (DDD), this document establishes the **Ubiquitous Language**—a strict vocabulary derived directly from the Sega Genesis hardware specifications. It also defines the **Memory Map**, which acts as the central routing logic (Main Bus) connecting all hardware components.

### 2. Ubiquitous Language (Glossary)
To avoid ambiguity, the following terms must be used exactly as defined in both discussions and source code (classes, variables, methods).

#### 2.1. General Data Types & Endianness
The Motorola 68000 CPU is **Big-Endian**. Memory addresses and data must be handled accordingly.
*   **Byte:** An 8-bit unsigned value.
*   **Word:** A 16-bit unsigned value.
*   **Longword:** A 32-bit unsigned value.
*   **Address:** A 24-bit value used by the 68000 to locate data in memory (effectively addressing up to 16 Megabytes).

#### 2.2. Core Components
*   **M68k:** The main processor (Motorola 68000).
*   **Z80:** The coprocessor, primarily used for audio control.
*   **VDP (Video Display Processor):** The graphics chip.
*   **YM2612:** The FM synthesis audio chip.
*   **PSG (Programmable Sound Generator):** The SN76489 secondary audio chip.
*   **MainBus:** The software abstraction representing the physical motherboard traces. It routes M68k read/write requests to the appropriate components.

#### 2.3. Video Domain (VDP)
*   **VRAM (Video RAM):** 64KB memory storing tiles, sprites, and tilemaps.
*   **CRAM (Color RAM):** Memory storing the color palettes (up to 64 colors in 4 palettes).
*   **VSRAM (Vertical Scroll RAM):** Memory storing vertical scrolling data.
*   **VdpCtrl:** The VDP Control Port (Mapped at `$C00004`). Used to send commands, set addresses, and write to internal registers.
*   **VdpData:** The VDP Data Port (Mapped at `$C00000`). Used to read/write raw data to VRAM/CRAM/VSRAM.
*   **HBlank (Horizontal Blank):** The interval when the CRT beam returns to the left of the screen.
*   **VBlank (Vertical Blank):** The interval when the CRT beam returns to the top of the screen.
*   **DMA (Direct Memory Access):** High-speed data transfer bypassing the CPU (e.g., from ROM to VRAM).

#### 2.4. Cartridge Domain
*   **ROM Header:** Cartridge metadata located from `$100` to `$1FF`.
*   **SRAM (Save RAM) / EEPROM:** On-cartridge memory used for saving game progress.

### 3. The Main Bus (Memory Map Routing)
The M68k CPU does not have direct references to the ROM, RAM, or VDP. It only knows how to read and write Bytes, Words, or Longwords to a 24-bit **Address**. 

The `MainBus` implements the **Mediator Pattern** (Single Responsibility Principle). When the CPU performs a memory operation, the `MainBus` intercepts it and routes it to the specific Bounded Context (hardware component) based on the following standard memory map:

| Address Range (Hex) | Size | Target Component / Bounded Context | Description |
| :--- | :--- | :--- | :--- |
| `$000000 - $3FFFFF` | 4 MB | **Cartridge ROM** | The game code and data. Read-only. (Vector table is at `$000000 - $0000FF`). |
| `$400000 - $7FFFFF` | 4 MB | **Expansion Port / Mega CD** | Usually ignored unless Sega CD is attached (Controlled by `/CART` signal). |
| `$800000 - $9FFFFF` | 2 MB | **32X Memory** | Ignored in base Genesis emulation. |
| `$A00000 - $A0FFFF` | 64 KB | **Z80 Memory Space / Audio** | Includes Z80 RAM and YM2612 registers (Mapped at `$A04000`). |
| `$A10000 - $A1001F` | 32 B | **I/O Ports** | Controller inputs (DE-9 ports) and system control. |
| `$A11100 - $A112FF` | 512 B| **Z80 Control** | Z80 Bus Request (`$A11100`) and Z80 Reset (`$A11200`). |
| `$A13000 - $A130FF` | 256 B| **Cartridge I/O (`/TIME`)** | Used for SRAM/EEPROM banking and Sonic & Knuckles lock-on registers. |
| `$C00000 - $C0001F` | 32 B | **VDP Ports** | `$C00000` (Data), `$C00004` (Control), `$C00008` (HV Counter). |
| `$E00000 - $FFFFFF` | 64 KB | **Work RAM (WRAM)** | System RAM. M68k mirrors this memory (e.g., `$FF0000` points to the same physical RAM as `$E00000`). |

### 4. Implementation Guidelines (SOLID & TDD)
*   **Dependency Inversion:** Components (ROM, VDP, WRAM) will implement an interface (e.g., `IMemoryMappedDevice`). The `MainBus` will hold a collection of these interfaces, not concrete implementations.
*   **Testability:** The `MainBus` can be tested in isolation by injecting "Mock Devices" into specific address ranges and verifying that read/write operations are correctly routed.