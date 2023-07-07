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
            break;
        }

        index = (index + 1) % table->size;
    }

    assert(table->keys[index] == reg && "hash table is full");
    return &table->vals[index];
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

enum {
    RBT_RED,
    RBT_BLACK,
};

typedef struct BBSetNode BBSetNode;
struct BBSetNode {
    u8 color;
    IRBasicBlock* b;
    BBSetNode *p, *l, *r;
};

internal void bb_set_right_rotate(BBSetNode** root, BBSetNode* n) {
    BBSetNode* left = n->l;

    n->l = left->r;
    if (n->l)
        n->l->p = n;

    left->p = n->p;
    if (!n->p) {
        *root = left;
    }
    else if (n == n->p->l) {
        n->p->l = left;
    }
    else {
        assert(n == n->p->r);
        n->p->r = left;
    }

    left->r = n;
    n->p = left;
}

internal void bb_set_left_rotate(BBSetNode** root, BBSetNode* n) {
    BBSetNode* right = n->r;

    n->r = right->l;
    if (n->r)
        n->r->p = n;

    right->p = n->p;
    if (!n->p) {
        *root = right;
    }
    else if (n == n->p->l) {
        n->p->l = right;
    }
    else {
        assert(n == n->p->r);
        n->p->r = right;
    }

    right->l = n;
    n->p = right;
}

internal BBSetNode* bb_bst_insert(Arena* arena, BBSetNode** node, IRBasicBlock* b, BBSetNode* parent) {
    if (!*node) {
        BBSetNode* n = arena_push_type(arena, BBSetNode);
        n->b = b;
        n->p = parent;
        *node = n;
        return n;
    }
    else {
        BBSetNode* n = *node;
        if (b->id < n->b->id) {
            return bb_bst_insert(arena, &n->l, b, n);
        }
        else if (b->id > n->b->id) {
            return bb_bst_insert(arena, &n->r, b, n);
        }
        else {
            assert(n->b == b);
            return 0;
        }
    }
}

internal void bb_set_swap_colors(BBSetNode* a, BBSetNode* b) {
    u8 temp = a->color;
    a->color = b->color;
    b->color = temp;
}

internal void bb_set_repair(BBSetNode** root, BBSetNode* x) {
    if (x == *root) {
        x->color = RBT_BLACK;
    }
    else if (x->p->color != RBT_BLACK) {
        BBSetNode* p = x->p;
        BBSetNode* g = p->p;
        BBSetNode* u = p == g->l ? g->r : g->l;

        u8 u_color = u ? u->color : RBT_BLACK;

        if (u_color == RBT_RED) {
            p->color = RBT_BLACK;
            u->color = RBT_BLACK;
            g->color = RBT_RED;
            bb_set_repair(root, g);
        }
        else {
            bool left_a = p == g->l;
            bool left_b = x == p->l;

            if (left_a && left_b) {
                bb_set_right_rotate(root, g);
                bb_set_swap_colors(p, g);
            }
            else if (!left_a && !left_b) {
                bb_set_left_rotate(root, g);
                bb_set_swap_colors(p, g);
            }
            else if (left_a && !left_b) {
                bb_set_left_rotate(root, p);
                bb_set_right_rotate(root, g);
                bb_set_swap_colors(g, x);
            }
            else if (!left_a && left_b) {
                bb_set_right_rotate(root, p);
                bb_set_left_rotate(root, g);
                bb_set_swap_colors(g, x);
            }
        }
    }
}

internal bool bb_set_insert(Arena* arena, BBSetNode** root, IRBasicBlock* b) {
    assert(!(*root) || (*root)->color == RBT_BLACK);
    BBSetNode* x = bb_bst_insert(arena, root, b, 0);
    bb_set_repair(root, x);
    return x != 0;
}

typedef struct {
    BBSetNode* cur;
    BBSetNode* right_most;
    IRBasicBlock* val;
} BBSetIter;

internal void bb_set_next(BBSetIter* it) {
    if (!it->cur) {
        it->val = 0;
        return;
    }

    if (!it->cur->l) {
        it->val = it->cur->b;
        it->cur = it->cur->r;
    }
    else {
        it->right_most = it->cur->l;

        while (it->right_most->r && it->right_most->r != it->cur)
            it->right_most = it->right_most->r;

        if (!it->right_most->r) {
            it->right_most->r = it->cur;
            it->cur = it->cur->l;
            bb_set_next(it);
        }
        else {
            it->right_most->r = 0; // Repair the tree
            it->val = it->cur->b;
            it->cur = it->cur->r;
        }
    }
}

internal BBSetIter bb_set_begin(BBSetNode* root) {
    BBSetIter it = {
        .cur = root
    };

    bb_set_next(&it);

    return it;
}

internal int bb_set_count(BBSetNode* root) {
    if (!root) {
        return 0;
    }

    return bb_set_count(root->l) + bb_set_count(root->r) + 1;
}

#define FOREACH_BB_SET(it, set) for (BBSetIter it = bb_set_begin(set); it.val; bb_set_next(&it))

enum {
    BB_WORKED = BIT(0),
    BB_HAS_PHI = BIT(1)
};

internal void add_to_worklist(u8* worked, int* worklist_count, IRBasicBlock** worklist, IRBasicBlock* b) {
    if (!(worked[b->id] & BB_WORKED)) {
        worklist[(*worklist_count)++] = b;
        worked[b->id] |= BB_WORKED;
    }
}

internal void mem2reg(IR* ir) {
    Scratch scratch = get_scratch(0, 0);

    #if 0
    BBSetNode* test = 0;
    for (int i = 0; i < 1000; ++i) {
        IRBasicBlock* b = arena_push_type(scratch.arena, IRBasicBlock);
        b->id = i;
        bb_set_insert(scratch.arena, &test, b);
    }

    FOREACH_BB_SET(it, test) {
        printf("%d\n", it.val->id);
    }
    #endif

    int max_id = -1;
    FOREACH_IR_BB(b, ir->first_block)
        max_id = b->id > max_id ? b->id : max_id;

    assert(max_id >= 0);
    int nblock = max_id + 1;

    // Post-order needed for dominance calculation
    PostOrder po = {
        .traversed = arena_push_array(scratch.arena, bool, nblock),
        .indices = arena_push_array(scratch.arena, int, nblock),
        .order = arena_push_array(scratch.arena, IRBasicBlock*, nblock),
    };
    get_post_order(&po, ir->first_block);

    // Gather predecessors
    BBSetNode** pred = arena_push_array(scratch.arena, BBSetNode*, nblock);
    FOREACH_IR_BB(b, ir->first_block) {
        BBList succ = bb_get_succ(b);
        for (int i = 0; i < succ.count; ++i) {
            IRBasicBlock* s = succ.data[i];
            bb_set_insert(scratch.arena, &pred[s->id], b);
        }
    }

    // Compute dominance information
    IRBasicBlock** idom = arena_push_array(scratch.arena, IRBasicBlock*, nblock);
    idom[ir->first_block->id] = ir->first_block;

    for (;;) {
        bool changed = false;

        for (int i = po.count-1; i >= 0; --i)
        {
            IRBasicBlock* b = po.order[i];
            if (b == ir->first_block)
                continue;

            BBSetIter it = bb_set_begin(pred[b->id]);

            IRBasicBlock* new_idom = it.val;
            assert(new_idom);
            bb_set_next(&it);

            for (; it.val; bb_set_next(&it))
            {
                IRBasicBlock* p = it.val;
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
    BBSetNode** df = arena_push_array(scratch.arena, BBSetNode*, nblock);
    FOREACH_IR_BB(n, ir->first_block)
    {
        if (!idom[n->id] || bb_set_count(pred[n->id]) <= 1)
            continue;

        FOREACH_BB_SET(it, pred[n->id])
        {
            IRBasicBlock* runner = it.val;

            while (runner && runner != idom[n->id])
            {
                if (!idom[runner->id])
                    break;

                bb_set_insert(scratch.arena, &df[runner->id], n);
                runner = idom[runner->id];
            }
        }
    }

    #if 0
    FOREACH_IR_BB(b, ir->first_block) {
        printf("DF bb.%d:\n", b->id);
        for (BBIter it = bb_set_begin(df[b->id]); it.val; bb_set_next(&it)) {
            printf("  bb.%d\n", it.val->id);
        }
    }
    printf("\n");
    #endif

    // Count number of allocations
    u32 nallocs = 0;
    for (IRAllocation* a = ir->first_allocation; a; a = a->next)
        ++nallocs;

    // Gather information about allocations in a hash table
    u32 alloc_table_size = nallocs * 2;
    IRAllocation** alloc_keys   = arena_push_array(scratch.arena, IRAllocation*, alloc_table_size);
    BBSetNode** alloc_write_blocks = arena_push_array(scratch.arena, BBSetNode*, alloc_table_size);

    // Gather the blocks that write to allocations
    FOREACH_IR_BB(b, ir->first_block)
    {
        IRInstr* instr = b->start;
        for (int i = 0; i < b->len; ++i)
        {
            if (instr->op == IR_OP_STORE) {
                assert(instr->store.loc.kind == IR_VALUE_ALLOCATION);
                IRAllocation* a = instr->store.loc.allocation;

                u32 index = fnv_1_a_hash(&a, sizeof(a)) % alloc_table_size;

                for (u32 j = 0; j < alloc_table_size; ++j) {
                    if (!alloc_keys[index] || alloc_keys[index] == a) {
                        alloc_keys[index] = a;
                        break;
                    }

                    index = (index + 1) % alloc_table_size;
                }

                assert(alloc_keys[index] == a);
                bb_set_insert(scratch.arena, &alloc_write_blocks[index], b);
            }

            instr = instr->next;
        }
    }

    // Promote allocations to registers, rebuild SSA.
    int worklist_count;
    IRBasicBlock** worklist = arena_push_array(scratch.arena, IRBasicBlock*, nblock);
    u8* worked = arena_push_array(scratch.arena, u8, nblock);

    for (IRAllocation* a = ir->first_allocation; a; a = a->next)
    {
        worklist_count = 0;
        memset(worked, 0, nblock * sizeof(worked[0]));

        u32 index = fnv_1_a_hash(&a, sizeof(a)) % alloc_table_size;

        for (u32 i = 0; i < alloc_table_size; ++i) {
            if (!alloc_keys[index] || alloc_keys[index] == a)
                break;

            index = (index + 1) % alloc_table_size;
        }

        assert(alloc_keys[index] == a);

        #if 0
        printf("Allocation %p blocks:\n", a);
        bb_set_print(alloc_write_blocks[index]);
        printf("\n");
        #endif

        FOREACH_BB_SET(it, alloc_write_blocks[index]) {
            add_to_worklist(worked, &worklist_count, worklist, it.val);
        }

        while (worklist_count > 0) // Blocks that write to this allocation
        {
            IRBasicBlock* write_block = worklist[--worklist_count];

            FOREACH_BB_SET(it, df[write_block->id])
            {
                IRBasicBlock* d = it.val;

                if (!(worked[d->id] & BB_HAS_PHI))
                {
                    printf("bb.%d needs phi for %p\n\n", d->id, a);
                    worked[d->id] |= BB_HAS_PHI;
                }

                add_to_worklist(worked, &worklist_count, worklist, d);
            }
        }
    }

    release_scratch(&scratch);
}

void optimize(IR* ir) {
    immediate_operands(ir);
    mem2reg(ir);
}