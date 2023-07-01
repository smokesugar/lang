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
    IR_VALUE_ILLEGAL,
    IR_VALUE_REG,
    IR_VALUE_INTEGER,
    IR_VALUE_ALLOCATION,
} IRValueKind;

typedef struct IRBasicBlock IRBasicBlock;
struct IRBasicBlock {
    int id;
    IRBasicBlock* next;
    struct IRInstr* start;
    u32 len;

    u32 succ_count;
    IRBasicBlock* succ[2];
};

typedef u32 IRReg;

typedef struct {
    IRValueKind kind;
    union {
        IRReg reg;
        u64 integer;
        IRAllocation* allocation;
    };
} IRValue;

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
            IRValue val; 
        } imm;
        struct {
            IRType type;
            IRReg dest;
            IRValue l;
            IRValue r;
        } bin;
        struct {
            IRType type;
            IRReg dest;
            IRValue loc;
        } load;
        struct {
            IRType type;
            IRValue loc;
            IRValue src;
        } store;
        struct {
            IRType type_src;
            IRType type_dest;
            IRReg dest;
            IRValue src;
        } cast;
        IRBasicBlock* jmp_loc;
        struct {
            IRType type;
            IRValue cond;
            IRBasicBlock* then_loc;
            IRBasicBlock* els_loc;
        } branch;
        struct {
            IRType type;
            IRValue val;
        } ret;
    };
};

typedef struct {
    IRInstr* first_instr;
    IRBasicBlock* first_block;
    IRReg next_reg;
    IRReg num_regs;
} IR;

void print_ir(IR* ir);
void remove_ir_instr(IR* ir, IRInstr* instr);
