// ==============================================================================
// GenesisEmu - M68k Decoded Instruction Structures (Core Domain)
// ==============================================================================
// This file declares the structures and value objects used to represent
// decoded instructions for the Motorola 68000.
// ==============================================================================

#pragma once

#include "../Common/IMemoryMappedDevice.h"

namespace GenesisEmu::Core::Domain::M68k {

// ------------------------------------------------------------------------------
// 1. Instruction Operation Families (Opcodes)
// ------------------------------------------------------------------------------
enum class OpType {
    UNKNOWN,
    NOP,        
    MOVE,       
    MOVE_TO_SR, 
    MOVE_FROM_SR, 
    MOVE_TO_CCR,  // Move to Condition Codes Register (User byte of SR)
    MOVE_USP,   
    ADD,        
    ADDQ,       // Quick Addition (immediate inside opcode)
    ADDX,       // Addition with Extend (multi-precision carry)
    SUB,        
    SUBQ,       // Quick Subtraction (immediate inside opcode)
    SUBX,       // Subtraction with Extend (multi-precision borrow)
    NEG,        // Negate (Subtract destination from 0)
    NEGX,       // Negate with Extend (Subtract destination and X flag from 0)
    MULU,       // Unsigned Multiplication
    MULS,       // Signed Multiplication
    DIVU,       // Unsigned Division
    DIVS,       // Signed Division
    EXG,        // Exchange Registers (swaps two registers)
    LINK,       // Link and Allocate (allocates stack frame)
    UNLK,       // Unlink (deallocates stack frame)
    JMP,        
    BRA,        
    
    // --- Conditional Branch Families (Bcc) ---
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
    
    // --- Condition Setters (Scc) ---
    SCC,        // Set byte conditionally (handles all 16 condition codes)

    // --- Logical Operations ---
    AND,        
    OR,         
    EOR,        
    
    // --- SR Logical Operations ---
    ANDI_TO_SR, // AND Immediate to SR
    ORI_TO_SR,  // OR Immediate to SR
    EORI_TO_SR, // EOR Immediate to SR

    // --- Flow Controls ---
    BSR,        
    JSR,        
    RTS,
    RTE,        // Return from Exception (Pops SR and PC, restores state)

    // --- Comparisons & Tests ---
    TST,
    CMP,
    CMPI,       

    // --- Loops & Branching ---
    DBCC,       // Decrement and Branch Conditionally (16 variants)
    DBF,        // Decrement and Branch (Always False / Decrement loop counter)

    // --- Unary Operations ---
    CLR,
    NOT,        // Logical Inversion (Complement to 1)
    SWAP,       // Swap high and low words of a data register
    EXT,        // Sign-extend byte to word, or word to long

    // --- Pointer & Advanced Loading ---
    PEA,
    LEA,
    MOVEQ,
    MOVEM,

    // --- Bit Manipulation Operations ---
    BTST,
    BCHG,       // Bit Change (Invert/Toggle bit)
    BCLR,       // Bit Clear (Set bit to 0)
    BSET,       // Bit Set (Set bit to 1)
    
    // --- Shift and Rotate Operations ---
    LSR,        // Logical Shift Right
    LSL,        // Logical Shift Left
    ASR,        // Arithmetic Shift Right
    ASL,        // Arithmetic Shift Left
    ROR,        // Rotate Right (circular shift)
    ROL,        // Rotate Left (circular shift)
    ROXR,       // Rotate Right with Extend
    ROXL        // Rotate Left with Extend
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
// 4. Decoded Instruction Value Packet
// ------------------------------------------------------------------------------
struct DecodedInstruction {
    OpType       type            = OpType::UNKNOWN;
    OperandSize  size            = OperandSize::NONE;
    
    // Source Operand Specifications
    AddressingMode srcMode       = AddressingMode::Immediate;
    Common::Byte   srcRegister   = 0; 
    
    // Destination Operand Specifications
    AddressingMode destMode      = AddressingMode::Immediate;
    Common::Byte   destRegister  = 0; 
    
    Common::Longword immediateData = 0; // Pre-loaded immediate value or step counter
};

} // namespace GenesisEmu::Core::Domain::M68k