// ==============================================================================
// GenesisEmu - M68k Flow Control Execution Unit (Core Domain)
// ==============================================================================
// This file implements subroutine execution flows (JSR, BSR, RTS) and active
// stack-frame modifications.
//
// SOLID Compliance:
// 1. Single Responsibility Principle (SRP):
//    It manages strictly stack frames and program counter modifications.
// ==============================================================================

#include "M68kFlowControl.h"

namespace GenesisEmu::Core::Domain::M68k {

using namespace GenesisEmu::Core::Domain::Common;

int M68kFlowControl::ExecuteJSR(IBus* bus, Address& pc, Longword& sp, Address targetAddress) {
    // 1. Decrement the Stack Pointer by 4 bytes (Size of a 32-bit physical address)
    sp -= 4;

    // 2. Push the return address (the next instruction PC) onto the stack
    bus->WriteLongword(sp, pc);

    // 3. Update the Program Counter to point directly to the target destination
    pc = targetAddress;

    // Standard JSR (xxx).L execution consumes exactly 16 clock cycles
    return 16;
}

int M68kFlowControl::ExecuteRTS(IBus* bus, Address& pc, Longword& sp) {
    // 1. Read the 32-bit return address from the top of the stack
    Address returnAddress = bus->ReadLongword(sp);

    // 2. Increment the Stack Pointer back by 4 bytes
    sp += 4;

    // 3. Jump the execution flow back to the pulled address
    pc = returnAddress;

    // Standard RTS execution consumes exactly 16 clock cycles
    return 16;
}

int M68kFlowControl::ExecuteBSR(IBus* bus, Address& pc, Longword& sp, std::int16_t displacement, Address instAddress) {
    // 1. Decrement the Stack Pointer by 4 bytes
    sp -= 4;

    // 2. Push the return address (which already points past the displacement word) to stack
    bus->WriteLongword(sp, pc);

    // 3. Target address is calculated relative to the start of the instruction + 2
    pc = (instAddress + 2) + displacement;

    // Standard BSR.W execution consumes exactly 18 clock cycles
    return 18;
}

} // namespace GenesisEmu::Core::Domain::M68k