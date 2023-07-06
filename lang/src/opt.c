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

internal IRBasicBlock* first_common_dominator(IRBasicBlock** idom, PostOrder* po, IRBasicBlock* b1, IRBasicBlock* b2) {
    int f1 = po->indices[b1->id];
    int f2 = po->indices[b2->id];

    while (f1 != f2) {
        while (f1 < f2)
            f1 = po->indices[idom[po->order[f1]->id]->id];
        while (f2 < f1)
            f2 = po->indices[idom[po->order[f2]->id]->id];
    }

    return po->order[f1];
}

internal void get_post_order(PostOrder* po, IRBasicBlock* b) {
    if (po->traversed[b->id])
        return;
    po->traversed[b->id] = true;

    BBList succ = bb_get_succ( b);
    for (int i = 0; i < succ.count; ++i)
        get_post_order(po, succ.data[i]);

    int index = po->count++;
    po->order[index] = b;
    po->indices[b->id] = index;
}

typedef struct BBNode BBNode;
struct BBNode {
    IRBasicBlock* b;
    BBNode *l,  *r;
};

internal void bb_set_insert(Arena* arena, BBNode** node, IRBasicBlock* b) {
    if (!*node) {
        BBNode* n = arena_push_type(arena, BBNode);
        n->b = b;
        *node = n;
    }
    else {
        BBNode* n = *node;
        if (b->id < n->b->id) {
            bb_set_insert(arena, &n->l, b);
        }
        else if (b->id > n->b->id) {
            bb_set_insert(arena, &n->r, b);
        }
        else {
            assert(n->b == b);
        }
    }
}

internal void bb_set_print(BBNode* root) {
    if (root) {
        bb_set_print(root->l);
        printf("  bb.%d\n", root->b->id);
        bb_set_print(root->r);
    }
}

internal void mem2reg(IR* ir) {
    Scratch scratch = get_scratch(0, 0);

    int max_id = -1;
    FOREACH_IR_BB(b, ir->first_block)
        max_id = b->id > max_id ? b->id : max_id;

    assert(max_id >= 0);
    int nblock = max_id + 1;

    PostOrder po = {
        .traversed = arena_push_array(scratch.arena, bool, nblock),
        .indices = arena_push_array(scratch.arena, int, nblock),
        .order = arena_push_array(scratch.arena, IRBasicBlock*, nblock),
    };

    get_post_order(&po, ir->first_block);

    int* pred_count = arena_push_array(scratch.arena, int, nblock);
    IRBasicBlock*** pred = arena_push_array(scratch.arena, IRBasicBlock**, nblock);

    FOREACH_IR_BB(b, ir->first_block) {
        BBList succ = bb_get_succ(b);
        for (int i = 0; i < succ.count; ++i) {
            IRBasicBlock* s = succ.data[i];
            pred_count[s->id]++;
        }
    }

    FOREACH_IR_BB(b, ir->first_block) {
        pred[b->id] = arena_push_array(scratch.arena, IRBasicBlock*, pred_count[b->id]);
        pred_count[b->id] = 0;
    }

    FOREACH_IR_BB(b, ir->first_block) {
        BBList succ = bb_get_succ(b);
        for (int i = 0; i < succ.count; ++i) {
            IRBasicBlock* s = succ.data[i];
            pred[s->id][pred_count[s->id]++] = b;
        }
    }

    IRBasicBlock** idom = arena_push_array(scratch.arena, IRBasicBlock*, nblock);
    idom[ir->first_block->id] = ir->first_block;

    for (;;) {
        bool changed = false;

        for (int i = po.count-1; i >= 0; --i)
        {
            IRBasicBlock* b = po.order[i];
            if (b == ir->first_block)
                continue;

            IRBasicBlock* new_idom = pred[b->id][0];

            for (int j = 1; j < pred_count[b->id]; ++j)
            {
                IRBasicBlock* p = pred[b->id][j];
                if (idom[p->id])
                    new_idom = first_common_dominator(idom, &po, p, new_idom);
            }

            if (idom[b->id] != new_idom) {
                idom[b->id] = new_idom;
                changed = true;
            }
        }

        if (!changed)
            break;
    }

    idom[ir->first_block->id] = 0;

    #if 0
    FOREACH_IR_BB(b, ir->first_block) {
        if (idom[b->id])
            printf("bb.%d dominator: bb.%d\n", b->id, idom[b->id]->id);
    }
    printf("\n");
    #endif

    // Compute dominance frontiers
    BBNode** df = arena_push_array(scratch.arena, BBNode*, nblock);
    FOREACH_IR_BB(n, ir->first_block)
    {
        if (!idom[n->id] || pred_count[n->id] <= 1)
            continue;

        for (int i = 0; i < pred_count[n->id]; ++i)
        {
            IRBasicBlock* runner = pred[n->id][i];
            while (runner && runner != idom[n->id])
            {
                if (!idom[runner->id])
                    break;

                bb_set_insert(scratch.arena, &df[runner->id], n);
                runner = idom[runner->id];
            }
        }
    }

    FOREACH_IR_BB(b, ir->first_block) {
        printf("DF bb.%d:\n", b->id);
        bb_set_print(df[b->id]);
    }
    printf("\n");

    release_scratch(&scratch);
}

void optimize(IR* ir) {
    immediate_operands(ir);
    mem2reg(ir);
}