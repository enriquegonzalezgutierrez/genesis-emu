// ==============================================================================
// GenesisEmu - M68k Flow Control Execution Unit Header (Core Domain)
// ==============================================================================
// This file declares the M68kFlowControl class. It executes flow modification
// subroutines, managing program jump bounds and stack frame compositions.
//
// SOLID Compliance:
// 1. Single Responsibility Principle (SRP):
//    It is solely responsible for managing program jumps and active subroutine
//    stack manipulations. It does not execute arithmetic or bitwise instructions.
// 2. Dependency Inversion Principle (DIP):
//    It executes memory mutations using the abstract Common::IBus interface.
// ==============================================================================

#pragma once

#include "../Common/IMemoryMappedDevice.h"
#include "../Common/IBus.h"

namespace GenesisEmu::Core::Domain::M68k {

/**
 * @class M68kFlowControl
 * @brief Stateless flow controller managing jumps, branches, and stack frames.
 */
class M68kFlowControl {
public:
    M68kFlowControl() = delete; // Enforce pure static class design
    ~M68kFlowControl() = delete;

    /**
     * @brief Executes a Jump to Subroutine (JSR).
     *        Decrements the stack pointer, pushes PC, and sets PC to target.
     * @param bus Pointer to the system memory bus.
     * @param pc Reference to the CPU Program Counter.
     * @param sp Reference to the CPU Stack Pointer (A7).
     * @param targetAddress Destination physical address.
     * @return Cycles consumed by the JSR.
     */
    static int ExecuteJSR(Common::IBus* bus, Common::Address& pc, Common::Longword& sp, Common::Address targetAddress);

    /**
     * @brief Executes a Return from Subroutine (RTS).
     *        Pops the 32-bit return address from the stack back into PC.
     */
    static int ExecuteRTS(Common::IBus* bus, Common::Address& pc, Common::Longword& sp);

    /**
     * @brief Executes a Branch to Subroutine (BSR).
     *        Pushes the return address and branches using a signed offset.
     * @param instAddress The starting address of the active BSR instruction.
     */
    static int ExecuteBSR(Common::IBus* bus, Common::Address& pc, Common::Longword& sp, std::int16_t displacement, Common::Address instAddress);
};

} // namespace GenesisEmu::Core::Domain::M68k