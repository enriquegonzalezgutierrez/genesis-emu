# Software Design Document (SDD)
## 07. ROM Cartridge, SRAM, and Lock-on Technology Domain

**Project:** GenesisEmu  
**Version:** 1.0.0  

### 1. Purpose
This document defines the Domain Model for the Cartridge subsystem. A Sega Genesis cartridge is not just a static read-only memory; it often contains internal routing logic (mappers), save RAM (SRAM/EEPROM), and complex hardware extensions like the *Sonic & Knuckles Lock-on* system.

### 2. Cartridge Domain Model
In DDD, the `Cartridge` is an **Aggregate Root**. It implements the `IMemoryMappedDevice` interface (defined in SDD-03) and is attached to the `MainBus`, typically responding to the `$000000 - $3FFFFF` and `$A130xx` address ranges.

#### 2.1. Entities and Value Objects
*   **ROM Data (Entity):** The byte array containing the raw game data.
*   **SRAM / EEPROM (Entity):** The writable byte array used for saving games, backed by the host filesystem (Outer Hexagon).
*   **RomHeader (Value Object):** An immutable data structure parsed from ROM addresses `$100 - $1FF`.

### 3. ROM Header Parsing
Upon instantiation, the `Cartridge` adapter reads the `RomHeader`. This Value Object dictates how the emulator should configure itself for this specific game.
*   **System Type (`$100 - $10F`):** Must contain "SEGA" (e.g., "SEGA MEGA DRIVE"). *Architecture Note:* Crucial for emulating the TMSS (TradeMark Security System) boot sequence.
*   **Region Support (`$1F0`):** Defines "J" (Japan), "U" (US), "E" (Europe). The emulator host can use this to set the VDP to NTSC (60Hz) or PAL (50Hz).
*   **Extra Memory (`$1B0 - $1BB`):** If the string starts with "RA", the cartridge contains Save RAM.
    *   *Type:* Indicates if it's 8-bit/16-bit and if it saves persistently (e.g., `$F8` = 8-bit odd addresses, saves).
    *   *Start/End Address:* Dictates where the `Cartridge` entity should route memory requests (e.g., `$200001 - $20FFFF`).

### 4. Extra Memory Routing (SRAM)
If the `RomHeader` indicates SRAM exists, the `Cartridge` handles read/write requests differently based on the address.
*   If `Address < SRAM_Start`: Return ROM data.
*   If `Address >= SRAM_Start` AND `Address <= SRAM_End`: Route to the SRAM entity.
*   *Note on banking:* Many games share the `$200000` space between ROM and SRAM. A specific register (often in the `$A130xx` range, signaled by the `/TIME` pin) acts as a toggle switch.

### 5. Advanced Mappers: Sonic & Knuckles Lock-on
The S&K Lock-on system requires the `Cartridge` entity to dynamically change its memory map based on hardware registers. This is an excellent use case for the **State** or **Strategy Pattern**.

#### 5.1. Memory Layout (S&K + Sonic 3 / Get Blue Spheres)
Two ROMs are loaded into the `Cartridge` entity: the Base (S&K) and the Locked-on ROM.
*   `$000000 - $1FFFFF`: Reads are routed to the Base S&K ROM.
*   `$200000 - $3FFFFF`: Reads are routed to the *upper 2MB* of the Locked-on ROM.

#### 5.2. Sonic 2 Patch ROM Logic (`$A130F1`)
Sonic & Knuckles includes a 256KB patch ROM inside the physical cartridge to fix Sonic 2. The routing changes dynamically based on the register `$A130F1`.
*   **When `$A130F1` Bit 0 is `0`:** `$300000 - $3FFFFF` routes to the locked-on cartridge.
*   **When `$A130F1` Bit 0 is `1`:** `$300000 - $3FFFFF` routes to the internal 256KB Patch ROM (mirrored).

### 6. TDD Application
The Cartridge domain is highly deterministic and perfect for unit testing header extraction and dynamic routing.

**Scenario: ROM Header Parsing**
*   **Action:** Instantiate a `RomHeader` Value Object passing a byte array where bytes `$1B0 - $1B1` are `0x52 0x41` ("RA").
*   **Assert:** `RomHeader.HasExtraMemory` is `True`.

**Scenario: S&K Lock-on Routing (Sonic 2 Mode)**
*   **Setup:** `Cartridge` is initialized as an S&K Lock-on with Sonic 2.
*   **Action 1:** CPU writes `1` to `$A130F1`.
*   **Assert 1:** `Cartridge.ReadWord(0x300000)` returns data from the *Patch ROM*.
*   **Action 2:** CPU writes `0` to `$A130F1`.
*   **Assert 2:** `Cartridge.ReadWord(0x300000)` returns data from the *Sonic 2 ROM*.