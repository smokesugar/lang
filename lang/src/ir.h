#pragma once

#include "base.h"

typedef u32 IRReg;
#define IR_EMPTY_REG UINT32_MAX

typedef enum {
    IR_TYPE_ILLEGAL,

    IR_TYPE_I8,
    IR_TYPE_I16,
    IR_TYPE_I32,
    IR_TYPE_I64,

    NUM_IR_TYPES
} IRType;

typedef struct IRAllocation IRAllocation;
struct IRAllocation {
    int id;
    IRAllocation* next;
    IRType type;
    i64 _val;
};

typedef enum {
    IR_OP_ILLEGAL,

    IR_OP_IMM,
    IR_OP_PHI,
    IR_OP_COPY,

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
    struct IRInstr* end;
    int len;
};

typedef struct {
    IRValueKind kind;
    union {
        IRReg reg;
        u64 integer;
        IRAllocation* allocation;
    };
} IRValue;

typedef struct {
    IRBasicBlock* block;
    IRReg reg;
} IRPhiParam;

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
            int param_count;
            IRPhiParam* params;
            IRAllocation* a;
        } phi;
        struct {
            IRType type;
            IRReg dest;
            IRValue src;
        } copy;
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
    IRAllocation* first_allocation;
    IRReg next_reg;
    IRReg num_regs;
} IR;

typedef struct {
    int count;
    IRBasicBlock* data[2];
} BBList;

BBList bb_get_succ(IRBasicBlock* block);

void bb_update_end(IRBasicBlock* block);

void print_ir(IR* ir);

void remove_ir_instr(IR* ir, IRInstr* instr);
void insert_ir_instr_before(IR* ir, IRInstr* after, IRInstr* instr);
void insert_ir_instr_at_block_start(IR* ir, IRBasicBlock* b, IRInstr* instr);

void output_cfg_graphviz(IR* ir, char* path);

IRInstr* new_ir_instr(Arena* arena, IROpCode op);

IRValue ir_integer_value(u64 val);
IRValue ir_reg_value(IRReg reg);
IRValue ir_allocation_value(IRAllocation* allocation);

#define FOREACH_IR_BB(name, head) for (IRBasicBlock* name = (head); name; name = name->next)
