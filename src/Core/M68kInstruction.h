// ==============================================================================
// GenesisEmu - Motorola 68000 Decoded Instruction Structure (Updated)
// ==============================================================================
// Added OpType::LSR and OpType::LSL to support bit shifting logic.
// ==============================================================================

#pragma once

#include "IMemoryMappedDevice.h"

namespace GenesisEmu::Core {

// ------------------------------------------------------------------------------
// 1. Instruction Operation Types (Opcodes)
// ------------------------------------------------------------------------------
enum class OpType {
    UNKNOWN,
    NOP,        
    MOVE,       
    MOVE_TO_SR, 
    MOVE_USP,   
    ADD,        
    ADDQ,       // Quick ADD (immediate embedded in opcode, no extension word)
    SUB,        
    SUBQ,       // Quick SUB (immediate embedded in opcode, no extension word)
    JMP,        
    BRA,        
    
    // --- Conditional Branches (Bcc family) ---
    BNE,        
    BEQ,        
    BPL,        
    BMI,        
    BHI,        
    
    // --- Arithmetic & Logic Group ---
    AND,        
    OR,         
    EOR,        
    
    // --- Subroutines & Stack Flow ---
    BSR,        
    JSR,        
    RTS,

    // --- Comparisons & Tests ---
    TST,
    CMPI,       

    // --- Loops & Branching ---
    DBF,

    // --- Unary Operations ---
    CLR,
    SWAP,       // Swap high/low words of a data register
    EXT,        // Sign-extend byte->word or word->long

    // --- Pointer & Address Operations ---
    PEA,

    // --- Bit Manipulation Operations ---
    BTST,
    
    // --- Shift & Rotate Operations ---
    LSR,        // Logical Shift Right
    LSL         // Logical Shift Left
};

// ------------------------------------------------------------------------------
// 2. Data Size Specification
// ------------------------------------------------------------------------------
enum class OperandSize {
    BYTE,  
    WORD,  
    LONG,  
    NONE   
};

// ------------------------------------------------------------------------------
// 3. M68k Addressing Modes (Standard 12 modes)
// ------------------------------------------------------------------------------
enum class AddressingMode {
    DataRegisterDirect,          
    AddressRegisterDirect,       
    AddressRegisterIndirect,     
    AddressRegisterPostincrement,
    AddressRegisterPredecrement, 
    AddressRegisterDisplacement, 
    AddressRegisterIndex,        
    AbsoluteShort,               
    AbsoluteLong,                
    ProgramCounterDisplacement,  
    ProgramCounterIndex,         
    Immediate                    
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