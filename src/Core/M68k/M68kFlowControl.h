// ==============================================================================
// GenesisEmu - M68k Flow Control Execution Unit (Core Domain)
// ==============================================================================
// This component manages execution flow modifications, specifically subroutines
// (JSR, BSR, RTS) and stack manipulations.
//
// DESIGN STRATEGY:
// 1. SRP: Isolates stack and subroutine calling mechanism from the main decoder.
// 2. OOP & DDD: Models the Stack Pointer interactions using physical M68k rules.
// ==============================================================================

#pragma once

#include "IMemoryMappedDevice.h"
#include "IBus.h"

namespace GenesisEmu::Core {

/**
 * @class M68kFlowControl
 * @brief Handles stack frame generation, subroutine calls, and returns.
 */
class M68kFlowControl {
public:
    M68kFlowControl() = delete;

    /**
     * @brief Executes a Jump to Subroutine (JSR).
     *        Pushes the return address (PC) onto the stack and jumps to target.
     * @param bus Pointer to the system memory bus.
     * @param pc Reference to the Program Counter.
     * @param sp Reference to the Stack Pointer (Address Register A7).
     * @param targetAddress The target execution address.
     * @return Clock cycles consumed.
     */
    static int ExecuteJSR(IBus* bus, Address& pc, Longword& sp, Address targetAddress);

    /**
     * @brief Executes a Return from Subroutine (RTS).
     *        Pulls the return address from the stack and places it back in PC.
     * @param bus Pointer to the system memory bus.
     * @param pc Reference to the Program Counter.
     * @param sp Reference to the Stack Pointer (A7).
     * @return Clock cycles consumed.
     */
    static int ExecuteRTS(IBus* bus, Address& pc, Longword& sp);

    /**
     * @brief Executes a Branch to Subroutine (BSR).
     * @param bus Pointer to the system memory bus.
     * @param pc Reference to the Program Counter.
     * @param sp Reference to the Stack Pointer (A7).
     * @param displacement Signed 16-bit offset.
     * @param instAddress The address where the BSR instruction started.
     * @return Clock cycles consumed.
     */
    static int ExecuteBSR(IBus* bus, Address& pc, Longword& sp, std::int16_t displacement, Address instAddress);
};

} // namespace GenesisEmu::Core