// ==============================================================================
// GenesisEmu - Motorola 68000 Decoded Instruction Structure (Updated)
// ==============================================================================
// Added ANDI_TO_SR, ORI_TO_SR, and EORI_TO_SR to allow the CPU to manipulate
// Status Register interrupt masks and trigger horizontal/vertical blanking.
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
    ADDX,       // Add with Extend (multi-precision carry)
    SUB,        
    SUBQ,       // Quick SUB (immediate embedded in opcode, no extension word)
    SUBX,       // Subtract with Extend (multi-precision borrow)
    JMP,        
    BRA,        
    
    // --- Conditional Branches (Bcc family) ---
    BCC,        // Branch on Carry Clear
    BCS,        // Branch on Carry Set
    BEQ,        // Branch on Equal
    BGE,        // Branch on Greater or Equal
    BGT,        // Branch on Greater Than
    BHI,        // Branch on Higher
    BLE,        // Branch on Less or Equal
    BLS,        // Branch on Lower or Same
    BLT,        // Branch on Less Than
    BMI,        // Branch on Minus
    BNE,        // Branch on Not Equal
    BPL,        // Branch on Plus
    BVC,        // Branch on Overflow Clear
    BVS,        // Branch on Overflow Set
    
    // --- Set Conditionally (Scc family) ---
    SCC,        // Set Conditionally (resolves any of the 16 condition codes)

    // --- Arithmetic & Logic Group ---
    AND,        
    OR,         
    EOR,        
    
    // --- Status Register Logical Operators ---
    ANDI_TO_SR, // AND Immediate to Status Register
    ORI_TO_SR,  // OR Immediate to Status Register
    EORI_TO_SR, // EOR Immediate to Status Register

    // --- Subroutines & Stack Flow ---
    BSR,        
    JSR,        
    RTS,

    // --- Comparisons & Tests ---
    TST,
    CMP,
    CMPI,       

    // --- Loops & Branching ---
    DBF,

    // --- Unary Operations ---
    CLR,
    SWAP,       // Swap high/low words of a data register
    EXT,        // Sign-extend byte->word or word->long

    // --- Pointer & Address Operations ---
    PEA,
    LEA,
    MOVEQ,
    MOVEM,

    // --- Bit Manipulation Operations ---
    BTST,
    
    // --- Shift & Rotate Operations ---
    LSR,        // Logical Shift Right
    LSL,        // Logical Shift Left
    ASR,        // Arithmetic Shift Right
    ASL,        // Arithmetic Shift Left
    ROR,        // Rotate Right
    ROL         // Rotate Left
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