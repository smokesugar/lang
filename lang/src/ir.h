#pragma once

#include "base.h"

typedef struct {
    i64 _val; // Temporary
} IRAllocation;

typedef enum {
    IR_ILLEGAL,

    IR_IMM,

    IR_STORE,
    IR_LOAD,
    
    IR_ADD,
    IR_SUB,
    IR_MUL,
    IR_DIV,

    IR_LESS,
    IR_LEQUAL,
    IR_NEQUAL,
    IR_EQUAL,

    IR_RET,

    NUM_IR_KINDS
} IROpCode;

typedef enum {
    IR_OPERAND_ILLEGAL,
    IR_OPERAND_REG,
    IR_OPERAND_INTEGER,
    IR_OPERAND_ALLOCATION,
} IROperandKind;

typedef u32 IRReg;

typedef struct {
    IROperandKind kind;
    union {
        IRReg reg;
        u64 integer;
        IRAllocation* allocation;
    };
} IROperand;

typedef struct IRInstr IRInstr;
struct IRInstr {
    IRInstr* next;
    IROpCode op;
    union {
        struct {
            IRReg dest;
            IROperand val; 
        } imm;
        struct {
            IRReg dest;
            IROperand l;
            IROperand r;
        } bin;
        struct {
            IRReg dest;
            IROperand loc;
        } load;
        struct {
            IROperand loc;
            IROperand src;
        } store;
        IROperand ret_val;
    };
};

typedef struct {
    IRInstr* first_instr;
    IRReg next_reg;
} IR;

void print_ir(IR* ir);
