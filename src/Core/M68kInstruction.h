// ==============================================================================
// GenesisEmu - Motorola 68000 Decoded Instruction Structure (Updated)
// ==============================================================================
// Added subroutine control opcodes (JSR, BSR) to support structured execution.
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
    BRA,        // Branch Always (Relative Jump)
    
    // --- Conditional Branches (Bcc family) ---
    BNE,        // Branch if Not Equal / Not Zero (Z flag == 0)
    BEQ,        // Branch if Equal / Zero (Z flag == 1)
    BPL,        // Branch if Plus / Positive (N flag == 0)
    BMI,        // Branch if Minus / Negative (N flag == 1)
    
    // --- Arithmetic & Logic Group ---
    AND,        // Logical AND (Source & Destination)
    OR,         // Logical OR (Source | Destination)
    EOR,        // Logical Exclusive OR (Source ^ Destination)
    
    // --- Subroutines & Stack Flow ---
    BSR,        // Branch to Subroutine
    JSR,        // Jump to Subroutine
    RTS         // Return from Subroutine
};

// ------------------------------------------------------------------------------
// 2. Data Size Specification
// ------------------------------------------------------------------------------
enum class OperandSize {
    BYTE,  // 8-bit operations (.B)
    WORD,  // 16-bit operations (.W)
    LONG,  // 32-bit operations (.L)
    NONE   // For instructions that don't operate on data sizes
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
struct DecodedInstruction {
    OpType       type            = OpType::UNKNOWN;
    OperandSize  size            = OperandSize::NONE;
    
    // Source Operand Metadata
    AddressingMode srcMode       = AddressingMode::Immediate;
    Byte           srcRegister   = 0; 
    
    // Destination Operand Metadata
    AddressingMode destMode      = AddressingMode::Immediate;
    Byte           destRegister  = 0; 
    
    Longword     immediateData   = 0; 
};

} // namespace GenesisEmu::Core