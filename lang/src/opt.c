#include <stdio.h>

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

typedef struct {
    bool* traversed;
    int* indices;
    IRBasicBlock** order;
    int count;
} PostOrder;

internal IRBasicBlock* intersect(IRBasicBlock** doms, PostOrder* po, IRBasicBlock* b1, IRBasicBlock* b2) {
    int f1 = po->indices[b1->id];
    int f2 = po->indices[b2->id];

    while (f1 != f2) {
        while (f1 < f2)
            f1 = po->indices[doms[po->order[f1]->id]->id];
        while (f2 < f1)
            f2 = po->indices[doms[po->order[f2]->id]->id];
    }

    return po->order[f1];
}

internal void get_post_order(PostOrder* po, IRBasicBlock* b) {
    if (po->traversed[b->id])
        return;
    po->traversed[b->id] = true;

    if (b->len > 0) {
        IRInstr* last_instr = b->start;
        for (u32 i = 1; i < b->len; ++i)
            last_instr = last_instr->next;

        switch (last_instr->op) {
            default:
                if (last_instr->next)
                    get_post_order(po, last_instr->next->block);
                break;
            case IR_OP_RET:
                break;
            case IR_OP_BRANCH:
                get_post_order(po, last_instr->branch.then_loc);
                get_post_order(po, last_instr->branch.els_loc);
                break;
            case IR_OP_JMP:
                get_post_order(po, last_instr->jmp_loc);
                break;
        }
    }
    else {
        if (b->start)
            get_post_order(po, b->start->block);
    }

    int index = po->count++;
    po->order[index] = b;
    po->indices[b->id] = index;
}

typedef struct {
    int count;
    IRBasicBlock** ptr;
} Preds;

internal Preds get_preds(Arena* arena, IR* ir, int nblock, IRBasicBlock* block) {
    IRBasicBlock** preds = arena_push_array(arena, IRBasicBlock*, nblock);
    int count = 0;

    for (IRBasicBlock* b = ir->first_block; b; b = b->next) {
        if (b->len > 0) {
            IRInstr* last_instr = b->start;
            for (u32 i = 1; i < b->len; ++i)
                last_instr = last_instr->next;

            switch (last_instr->op) {
                default:
                    if (last_instr->next && block == last_instr->next->block)
                        preds[count++] = b;
                    break;
                case IR_OP_RET:
                    break;
                case IR_OP_BRANCH:
                    if (block == last_instr->branch.then_loc)
                        preds[count++] = b;
                    else if (block == last_instr->branch.els_loc)
                        preds[count++] = b;
                    break;
                case IR_OP_JMP:
                    if (block == last_instr->jmp_loc)
                        preds[count++] = b;
                    break;
            }
        }
        else {
            if (b->start && block == b->start->block) {
                preds[count++] = b;
            }
        }
    }

    arena_realloc(arena, preds, count * sizeof(IRBasicBlock*));
    if (count == 0)
        preds = 0;
    
    return (Preds) {
        .count = count,
        .ptr = preds
    };
}

internal void mem2reg(IR* ir) {
    Scratch scratch = get_scratch(0, 0);

    int max_id = -1;
    for (IRBasicBlock* b = ir->first_block; b; b = b->next) {
        max_id = b->id > max_id ? b->id : max_id;
    }

    assert(max_id >= 0);
    int nblock = max_id + 1;

    PostOrder po = {
        .traversed = arena_push_array(scratch.arena, bool, nblock),
        .indices = arena_push_array(scratch.arena, int, nblock),
        .order = arena_push_array(scratch.arena, IRBasicBlock*, nblock),
    };

    get_post_order(&po, ir->first_block);

    Preds* preds = arena_push_array(scratch.arena, Preds, nblock);
    for (IRBasicBlock* b = ir->first_block; b; b = b->next) {
        preds[b->id] = get_preds(scratch.arena, ir, nblock, b);
    }

    IRBasicBlock** doms = arena_push_array(scratch.arena, IRBasicBlock*, nblock);
    doms[ir->first_block->id] = ir->first_block;

    for (;;) {
        bool changed = false;

        for (int i = po.count-1; i >= 0; --i)
        {
            IRBasicBlock* b = po.order[i];
            if (b == ir->first_block)
                continue;

            IRBasicBlock* new_idom = preds[b->id].ptr[0];

            for (int j = 1; j < preds[b->id].count; ++j)
            {
                IRBasicBlock* p = preds[b->id].ptr[j];
                if (doms[p->id])
                    new_idom = intersect(doms, &po, p, new_idom);
            }

            if (doms[b->id] != new_idom) {
                doms[b->id] = new_idom;
                changed = true;
            }
        }

        if (!changed)
            break;
    }

    for (IRBasicBlock* b = ir->first_block; b; b = b->next) {
        if (doms[b->id])
            printf("bb.%d dominator: bb.%d\n", b->id, doms[b->id]->id);
    }
    printf("\n");

    release_scratch(&scratch);
}

void optimize(IR* ir) {
    immediate_operands(ir);
    mem2reg(ir);
}