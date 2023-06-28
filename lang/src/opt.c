#include "opt.h"
#include "core.h"

enum {
    FLAG_NONE,
    FLAG_IS_IMM = BIT(0),
};

typedef struct {
    u32 flags;
    IROperand imm;
} RegData;

internal RegData* get_reg_data(IRReg* keys, RegData* vals, u32 table_size, IRReg reg) {
    u32 index = fnv_1_a_hash(&reg, sizeof(reg)) % table_size;

    for (u32 i = 0; i < table_size; ++i) {
        if (!keys[index] || keys[index] == reg) {
            keys[index] = reg;
            return vals + index;
        }
        index = (index + 1) % table_size;
    }

    assert(false && "hash table is full");
    return 0;
}

internal void opt_operand(IRReg* keys, RegData* vals, u32 table_size, IROperand* operand) {
    if (operand->kind == IR_OPERAND_REG) {
        RegData* data = get_reg_data(keys, vals, table_size, operand->reg);
        if (data->flags & FLAG_IS_IMM)
           *operand = data->imm;
    }
}

void optimize(IR* ir) {
    Scratch scratch = get_scratch(0, 0);

    u32 table_size = ir->next_reg * 2;

    IRReg* reg_keys = arena_push_array(scratch.arena, IRReg, table_size);
    RegData* reg_data  = arena_push_array(scratch.arena, RegData, table_size);

    for (IRInstr** p_instr = &ir->first_instr; *p_instr;)
    {
        IRInstr* instr = *p_instr;

        static_assert(NUM_IR_KINDS == 9, "not all ir ops handled");
        switch (instr->op) {
            case IR_IMM: {
                RegData* data = get_reg_data(reg_keys, reg_data, table_size, instr->imm.dest);
                data->flags |= FLAG_IS_IMM;
                data->imm = instr->imm.val;
                *p_instr = instr->next;
            } continue;

            case IR_LOAD:
                opt_operand(reg_keys, reg_data, table_size, &instr->load.loc);
                break;

            case IR_STORE:
                opt_operand(reg_keys, reg_data, table_size, &instr->store.loc);
                opt_operand(reg_keys, reg_data, table_size, &instr->store.src);
                break;

            case IR_ADD:
            case IR_SUB:
            case IR_MUL:
            case IR_DIV: {
                opt_operand(reg_keys, reg_data, table_size, &instr->bin.l);
                opt_operand(reg_keys, reg_data, table_size, &instr->bin.r);
            } break;

            case IR_RET: {
                opt_operand(reg_keys, reg_data, table_size, &instr->ret_val);
            } break;
        }

        p_instr = &(*p_instr)->next;
    }

    #undef FLAGS

    release_scratch(&scratch);
}