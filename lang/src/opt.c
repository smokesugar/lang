#include "opt.h"
#include "core.h"

enum {
    FLAG_NONE,
    FLAG_IS_IMM = BIT(0),
};

typedef struct {
    u32 flags;
    IRValue imm;
} RegData;

typedef struct {
    u32 size;
    IRReg* keys;
    RegData* vals;
} RegDataTable;

internal RegData* get_reg_data(RegDataTable* table, IRReg reg) {
    u32 index = fnv_1_a_hash(&reg, sizeof(reg)) % table->size;

    for (u32 i = 0; i < table->size; ++i)
    {
        if (!table->keys[index] ||
             table->keys[index] == reg)
        {
            table->keys[index] = reg;
            return table->vals + index;
        }

        index = (index + 1) % table->size;
    }

    assert(false && "hash table is full");
    return 0;
}

internal RegDataTable make_reg_data_table(Arena* arena, u32 num_regs) {
    u32 table_size = num_regs * 2;
    return (RegDataTable) {
        .size = table_size,
        .keys = arena_push_array(arena, IRReg, table_size),
        .vals = arena_push_array(arena, RegData, table_size)
    };
}

internal void opt_value(RegDataTable* table, IRValue* value) {
    if (value->kind == IR_VALUE_REG) {
        RegData* data = get_reg_data(table, value->reg);
        if (data->flags & FLAG_IS_IMM)
           *value = data->imm;
    }
}

internal void immediate_operands(IR* ir) {
    Scratch scratch = get_scratch(0, 0);

    RegDataTable reg_table = make_reg_data_table(scratch.arena, ir->num_regs);

    for (IRInstr* instr = ir->first_instr; instr; instr = instr->next)
    {
        static_assert(NUM_IR_OPS == 18, "not all ir ops handled");
        switch (instr->op) {
            case IR_OP_IMM: {
                RegData* data = get_reg_data(&reg_table, instr->imm.dest);
                data->flags |= FLAG_IS_IMM;
                data->imm = instr->imm.val;
                remove_ir_instr(ir, instr);
                --ir->num_regs;
            } break;

            case IR_OP_LOAD:
                opt_value(&reg_table, &instr->load.loc);
                break;

            case IR_OP_STORE:
                opt_value(&reg_table, &instr->store.loc);
                opt_value(&reg_table, &instr->store.src);
                break;

            case IR_OP_SEXT:
            case IR_OP_ZEXT:
            case IR_OP_TRUNC:
                opt_value(&reg_table, &instr->cast.src);
                break;

            case IR_OP_ADD:
            case IR_OP_SUB:
            case IR_OP_MUL:
            case IR_OP_DIV:
            case IR_OP_LESS:
            case IR_OP_LEQUAL:
            case IR_OP_NEQUAL:
            case IR_OP_EQUAL:
            {
                opt_value(&reg_table, &instr->bin.l);
                opt_value(&reg_table, &instr->bin.r);
            } break;

            case IR_OP_RET: {
                opt_value(&reg_table, &instr->ret.val);
            } break;

            case IR_OP_JMP: {
            } break;

            case IR_OP_BRANCH: {
                opt_value(&reg_table, &instr->branch.cond);
            } break;
        }
    }

    release_scratch(&scratch);
}

internal void mem2reg(IR* ir) {
    (void)ir;
}

void optimize(IR* ir) {
    immediate_operands(ir);
    mem2reg(ir);
}