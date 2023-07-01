#pragma once

#include "base.h"

typedef struct {
    i64 _val; // Temporary
} IRAllocation;

typedef enum {
    IR_TYPE_ILLEGAL,

    IR_TYPE_I8,
    IR_TYPE_I16,
    IR_TYPE_I32,
    IR_TYPE_I64,

    NUM_IR_TYPES
} IRType;

typedef enum {
    IR_OP_ILLEGAL,

    IR_OP_IMM,

    IR_OP_STORE,
    IR_OP_LOAD,

    IR_OP_SEXT,
    IR_OP_ZEXT,
    IR_OP_TRUNC,

    IR_OP_ADD,
    IR_OP_SUB,
    IR_OP_MUL,
    IR_OP_DIV,

    IR_OP_LESS,
    IR_OP_LEQUAL,
    IR_OP_NEQUAL,
    IR_OP_EQUAL,

    IR_OP_RET,
    IR_OP_JMP,
    IR_OP_BRANCH,

    NUM_IR_OPS
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
            IRType type;
            IRReg dest;
            IROperand val; 
        } imm;
        struct {
            IRType type;
            IRReg dest;
            IROperand l;
            IROperand r;
        } bin;
        struct {
            IRType type;
            IRReg dest;
            IROperand loc;
        } load;
        struct {
            IRType type;
            IROperand loc;
            IROperand src;
        } store;
        struct {
            IRType type_src;
            IRType type_dest;
            IRReg dest;
            IROperand src;
        } cast;
        IRBasicBlock* jmp_loc;
        struct {
            IRType type;
            IROperand cond;
            IRBasicBlock* then_loc;
            IRBasicBlock* els_loc;
        } branch;
        struct {
            IRType type;
            IROperand val;
        } ret;
    };
};

typedef struct {
    IRInstr* first_instr;
    IRBasicBlock* first_block;
    IRReg next_reg;
} IR;

void print_ir(IR* ir);
void remove_ir_instr(IR* ir, IRInstr* instr);
