// ==============================================================================
// GenesisEmu - M68k Addressing Mode Resolver Header (Core Domain)
// ==============================================================================
// This file declares the M68kAddressing utility class. It manages effective
// address (EA) resolution and operand reading/writing based on standard M68k modes.
//
// SOLID Compliance:
// 1. Single Responsibility Principle (SRP):
//    It is solely responsible for managing register/memory operand calculations
//    and transfers. It prevents repetitive address-calculation boilerplate from
//    cluttering active instruction execution classes.
// 2. Dependency Inversion Principle (DIP):
//    It interacts with the system memory using the abstract Common::IBus interface.
// ==============================================================================

#pragma once

#include "../Common/IMemoryMappedDevice.h"
#include "../Common/IBus.h"
#include "M68kInstruction.h"

namespace GenesisEmu::Core::Domain::M68k {

// Forward declaration of M68k processor class to prevent circular inclusions
class M68k;

/**
 * @class M68kAddressing
 * @brief Stateless utility managing register and memory operand accesses.
 */
class M68kAddressing {
public:
    M68kAddressing() = delete; // Enforce pure static utility design
    ~M68kAddressing() = delete;

    /**
     * @brief Resolves the target physical memory address for memory-indirect modes.
     * @param mode Target addressing mode.
     * @param reg Target register index (0 to 7).
     * @param size Operand data size.
     * @param cpu Reference to the CPU execution core.
     * @param bus Pointer to system bus interface.
     * @return Resolved 32-bit physical address.
     */
    static Common::Address ResolveAddress(AddressingMode mode, Common::Byte reg, OperandSize size, M68k& cpu, Common::IBus* bus);

    /**
     * @brief Reads an operand value (Byte, Word, or Long) according to its addressing mode.
     */
    static Common::Longword ReadOperand(AddressingMode mode, Common::Byte reg, OperandSize size, M68k& cpu, Common::IBus* bus);

    /**
     * @brief Writes an operand value to register or memory locations.
     */
    static void WriteOperand(AddressingMode mode, Common::Byte reg, OperandSize size, Common::Longword value, M68k& cpu, Common::IBus* bus);
};

} // namespace GenesisEmu::Core::Domain::M68k