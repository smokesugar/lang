#pragma once

#include "base.h"

typedef enum {
    IR_ILLEGAL,

    IR_IMM,
    
    IR_ADD,
    IR_SUB,
    IR_MUL,
    IR_DIV,

    IR_RET,

    NUM_IR_KINDS
} IROpCode;

typedef enum {
    IR_OPERAND_ILLEGAL,
    IR_OPERAND_REG,
    IR_OPERAND_INTEGER,
} IROperandKind;

typedef u32 IRReg;

typedef struct {
    IROperandKind kind;
    union {
        IRReg reg;
        u64 integer;
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
        IROperand ret_val;
    };
};

typedef struct {
    IRInstr* first_instr;
    IRReg next_reg;
} IR;

void print_ir(IR* ir);
