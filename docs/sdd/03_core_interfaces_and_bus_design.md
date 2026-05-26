# Software Design Document (SDD)
## 03. Core Interfaces and Bus Design

**Project:** GenesisEmu  
**Version:** 1.0.0  

### 1. Purpose
This document defines the architectural contracts (Interfaces / Ports) that connect the M68k CPU to the rest of the hardware. By defining these interfaces before writing any concrete logic, we ensure strict adherence to **SOLID principles** and enable isolated unit testing (**TDD**).

### 2. SOLID Principles Application
*   **Single Responsibility Principle (SRP):** The `MainBus` is solely responsible for routing memory requests. It does not know *how* the VDP processes a write, nor does it know *what* data is inside the ROM.
*   **Open/Closed Principle (OCP):** The `MainBus` will be open for extension (we can plug in new hardware like a 32X dummy or an SRAM chip) but closed for modification (we won't need to change the `MainBus` routing code to support a new device).
*   **Dependency Inversion Principle (DIP):** The M68k CPU will not depend on `WorkRAM` or `VDP` concrete classes. It will depend entirely on an `IBus` interface.

### 3. Core Interfaces Definition (The Contracts)

To achieve the decoupling described above, the Core Domain exposes the following interfaces. *(Note: Code snippets are language-agnostic representations of the contracts).*

#### 3.1. `IMemoryMappedDevice`
Every physical hardware component that occupies a space in the Memory Map (ROM, WRAM, VDP, Z80 Space, I/O) MUST implement this interface.

```text
interface IMemoryMappedDevice {
    // Read operations
    Byte ReadByte(Address offset);
    Word ReadWord(Address offset);
    
    // Write operations
    void WriteByte(Address offset, Byte data);
    void WriteWord(Address offset, Word data);
}
```
*Design Note:* The `offset` passed to the device is relative to its mapped memory space, or the raw address, depending on the routing strategy. We will pass the raw `Address` so the device knows exactly what was requested (useful for mirrored memory).

#### 3.2. `IBus`
The interface consumed by the CPU. The CPU only knows that it has an attached Bus.

```text
interface IBus {
    // Read operations
    Byte ReadByte(Address address);
    Word ReadWord(Address address);
    Longword ReadLongword(Address address);
    
    // Write operations
    void WriteByte(Address address, Byte data);
    void WriteWord(Address address, Word data);
    void WriteLongword(Address address, Longword data);
    
    // Hardware integration
    void AttachDevice(IMemoryMappedDevice device, Address startAddress, Address endAddress);
}
```

### 4. The Routing Mechanism and Endianness
#### 4.1. Big-Endian Architecture
The Sega Genesis (M68k) is a **Big-Endian** system (Most Significant Byte first). However, the host environment (Docker on Windows/x86_64) is **Little-Endian**.
*   **Responsibility:** The `IBus` implementation (or a dedicated Memory utility class) is responsible for byte-swapping when reading/writing `Word` (16-bit) or `Longword` (32-bit) values from the underlying host memory arrays to simulate the 68000's perspective.

#### 4.2. Attach and Route Strategy
The `MainBus` implementation of `IBus` will maintain a collection (e.g., a list or a routing tree) of connected devices. 
When `WriteWord(0xC00004, 0x8010)` is called:
1. The `MainBus` scans its attached devices.
2. It finds that the `VDP` is registered between `0xC00000` and `0xC0001F`.
3. It delegates the call: `vdp.WriteWord(0xC00004, 0x8010)`.

### 5. TDD Application (Mocking the Bus)
Because of this interface design, testing the CPU or the VDP becomes trivial.

**Example CPU Test Context:**
To test the M68k opcode `MOVE.B #$FF, $E00000` (Write the value 255 to WRAM):
1. Create a `MockBus` implementing `IBus`.
2. Inject `MockBus` into the `M68k` CPU instance.
3. Tell the CPU to execute the opcode.
4. **Assert** that `MockBus.WriteByte(0xE00000, 0xFF)` was called exactly once.
No actual RAM or VDP is required to test the CPU logic.