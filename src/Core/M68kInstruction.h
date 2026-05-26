// ==============================================================================
// GenesisEmu - Motorola 68000 Decoded Instruction Structure
// ==============================================================================
// This file defines the types and data structures used to represent a decoded
// M68k instruction, facilitating structured decoding and execution (DDD/SOLID).
// ==============================================================================

#pragma once

#include "IMemoryMappedDevice.h"

namespace GenesisEmu::Core {

// ------------------------------------------------------------------------------
// 1. Instruction Operation Types (Opcodes)
// ------------------------------------------------------------------------------
enum class OpType {
    UNKNOWN,
    NOP,        // No Operation
    MOVE,       // Move Source to Destination
    ADD,        // Add Source to Destination
    SUB,        // Subtract Source from Destination
    JMP,        // Jump to Address
    RTS,        // Return from Subroutine
    // More instructions (AND, OR, Bcc, etc.) will be added here progressively
};

// ------------------------------------------------------------------------------
// 2. Data Size Specification
// ------------------------------------------------------------------------------
enum class OperandSize {
    BYTE,  // 8-bit operations (.B)
    WORD,  // 16-bit operations (.W)
    LONG,  // 32-bit operations (.L)
    NONE   // For instructions that don't operate on data sizes (e.g., NOP, JMP)
};

// ------------------------------------------------------------------------------
// 3. M68k Addressing Modes (Standard 12 modes)
// ------------------------------------------------------------------------------
enum class AddressingMode {
    DataRegisterDirect,          // Dn (e.g., D0)
    AddressRegisterDirect,       // An (e.g., A0)
    AddressRegisterIndirect,     // (An)
    AddressRegisterPostincrement,// (An)+
    AddressRegisterPredecrement, // -(An)
    AddressRegisterDisplacement, // (d16, An)
    AddressRegisterIndex,        // (d8, An, Xn)
    AbsoluteShort,               // (xxx).W
    AbsoluteLong,                // (xxx).L
    ProgramCounterDisplacement,  // (d16, PC)
    ProgramCounterIndex,         // (d8, PC, Xn)
    Immediate                    // #<data>
};

// ------------------------------------------------------------------------------
// 4. Decoded Instruction Value Object
// ------------------------------------------------------------------------------
// Holds the completely parsed metadata of an instruction word fetched from ROM.
// ------------------------------------------------------------------------------
struct DecodedInstruction {
    OpType       type            = OpType::UNKNOWN;
    OperandSize  size            = OperandSize::NONE;
    
    // Source Operand Metadata
    AddressingMode srcMode       = AddressingMode::Immediate;
    Byte           srcRegister   = 0; // 0-7 (Register index if applicable)
    
    // Destination Operand Metadata
    AddressingMode destMode      = AddressingMode::Immediate;
    Byte           destRegister  = 0; // 0-7 (Register index if applicable)
    
    // Extracted raw immediate data or displacement values if the mode requires it
    Longword     immediateData   = 0; 
};

} // namespace GenesisEmu::Core