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
    IR_JMP,
    IR_BRANCH,

    NUM_IR_KINDS
} IROpCode;

typedef enum {
    IR_OPERAND_ILLEGAL,
    IR_OPERAND_REG,
    IR_OPERAND_INTEGER,
    IR_OPERAND_ALLOCATION,
} IROperandKind;

typedef struct IRBasicBlock IRBasicBlock;
struct IRBasicBlock {
    int id;
    IRBasicBlock* next;
    struct IRInstr* start;
    u32 len;
};

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
    IRInstr* prev;
    IROpCode op;
    IRBasicBlock* block;
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
        IRBasicBlock* jmp_loc;
        struct {
            IROperand cond;
            IRBasicBlock* then_loc;
            IRBasicBlock* els_loc;
        } branch;
        IROperand ret_val;
    };
};

typedef struct {
    IRInstr* first_instr;
    IRBasicBlock* first_block;
    IRReg next_reg;
} IR;

void print_ir(IR* ir);
void remove_ir_instr(IR* ir, IRInstr* instr);
