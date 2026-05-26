// ==============================================================================
// GenesisEmu - M68k Flow Control Execution Unit Implementation (Core Domain)
// ==============================================================================
// This file implements the stack push/pop procedures for subroutine execution
// matching the Motorola 68000 micro-architecture requirements.
// ==============================================================================

#include "M68kFlowControl.h"

namespace GenesisEmu::Core {

int M68kFlowControl::ExecuteJSR(IBus* bus, Address& pc, Longword& sp, Address targetAddress) {
    // 1. Decrement stack pointer to hold a 32-bit return address (Longword)
    sp -= 4;

    // 2. Push current Program Counter (pointing to the next instruction) onto the stack
    bus->WriteLongword(sp, pc);

    // 3. Set the Program Counter to the target destination
    pc = targetAddress;

    // JSR (xxx).L standard execution takes exactly 16 clock cycles
    return 16;
}

int M68kFlowControl::ExecuteRTS(IBus* bus, Address& pc, Longword& sp) {
    // 1. Pull the 32-bit return address from the top of the stack
    Address returnAddress = bus->ReadLongword(sp);

    // 2. Increment stack pointer by 4 bytes (Longword size popped)
    sp += 4;

    // 3. Jump execution flow back to the pulled address
    pc = returnAddress;

    // RTS execution takes exactly 16 clock cycles
    return 16;
}

int M68kFlowControl::ExecuteBSR(IBus* bus, Address& pc, Longword& sp, std::int16_t displacement, Address instAddress) {
    // 1. Decrement stack pointer to hold return address
    sp -= 4;

    // 2. Push current Program Counter (already pointing past the displacement word) to stack
    bus->WriteLongword(sp, pc);

    // 3. Target address is relative to the start of the instruction plus 2, added with the signed displacement
    pc = (instAddress + 2) + displacement;

    // BSR.W takes exactly 18 clock cycles
    return 18;
}

} // namespace GenesisEmu::Core