#include <stdio.h>

#include "opt.h"
#include "core.h"

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

internal IRReg new_name(IR* ir, IRReg* cur_regs, IRAllocation* a) {
    cur_regs[a->id] = ir->next_reg++;
    return cur_regs[a->id];
} 

internal void promote_allocations(IR* ir, IRReg* cur_regs, int nalloc, BBSetNode** dom_children, IRBasicBlock* b) {
    Scratch scratch = get_scratch(0, 0);

    IRReg* cur_regs_temp = arena_push_array(scratch.arena, IRReg, nalloc);
    memcpy(cur_regs_temp, cur_regs, nalloc * sizeof(IRReg));

    IRInstr* instr = b->start;
    for (int i = 0; i < b->len; ++i)
    {
        switch (instr->op) {
            case IR_OP_PHI: {
                instr->phi.dest = new_name(ir, cur_regs, instr->phi.a);
            } break;

            case IR_OP_STORE: {
                assert(instr->store.loc.kind == IR_VALUE_ALLOCATION);
                IRAllocation* a = instr->store.loc.allocation;

                IRValue src = instr->store.src;
                IRType type = instr->store.type;

                instr->op = IR_OP_COPY;
                instr->copy.type = type;
                instr->copy.dest = new_name(ir, cur_regs, a);
                instr->copy.src = src;
            } break;

            case IR_OP_LOAD: {
                assert(instr->load.loc.kind == IR_VALUE_ALLOCATION);
                IRAllocation* a = instr->load.loc.allocation;

                IRReg dest = instr->load.dest;
                IRType type = instr->load.type;

                instr->op = IR_OP_COPY;
                instr->copy.type = type;
                instr->copy.dest = dest;
                instr->copy.src = ir_reg_value(cur_regs[a->id]);
            } break;
        }

        instr = instr->next;
    }

    BBList succ = bb_get_succ(b);

    for (int i = 0; i < succ.count; ++i)
    {
        IRBasicBlock* s = succ.data[i];

        instr = s->start;
        for (int j = 0; j < s->len; ++j)
        {
            if (instr->op != IR_OP_PHI)
                break;
             
            IRReg cur = cur_regs[instr->phi.a->id];

            IRPhiParam* param = 0;
            for (int k = 0; k < instr->phi.param_count; ++k)
            {
                IRPhiParam* p = &instr->phi.params[k];
                if (p->block == b) {
                    param = p;
                    break;
                }
            }

            assert(param);
            param->reg = cur;

            instr = instr->next;
        }
    }

    FOREACH_BB_SET(it, dom_children[b->id]) {
        promote_allocations(ir, cur_regs, nalloc, dom_children, it.val);
    }

    memcpy(cur_regs, cur_regs_temp, nalloc * sizeof(IRReg));
    release_scratch(&scratch);
}

internal void mem2reg(Arena* arena, IR* ir) {
    Scratch scratch = get_scratch(0, 0);

    int max_bb_id = -1;
    FOREACH_IR_BB(b, ir->first_block)
        max_bb_id = b->id > max_bb_id ? b->id : max_bb_id;

    assert(max_bb_id >= 0);
    int nblock = max_bb_id + 1;

    int max_alloc_id = -1;
    for (IRAllocation* a = ir->first_allocation; a; a = a->next)
        max_alloc_id = a->id > max_alloc_id ? a->id : max_alloc_id;

    int nalloc = max_alloc_id + 1;

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

    BBSetNode** dom_children = arena_push_array(scratch.arena, BBSetNode*, nblock);
    FOREACH_IR_BB(n, ir->first_block) {
        IRBasicBlock* dom = idom[n->id];
        if (dom)
            bb_set_insert(scratch.arena, &dom_children[dom->id], n);
    }

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

    // Get liveness information

    Bitset** var_kill = arena_push_array(scratch.arena, Bitset*, nblock);
    Bitset** ue_var   = arena_push_array(scratch.arena, Bitset*, nblock);
    Bitset** live_out = arena_push_array(scratch.arena, Bitset*, nblock);

    FOREACH_IR_BB(b, ir->first_block) {
        var_kill[b->id] = bitset_alloc(scratch.arena, nalloc);
        ue_var[b->id]   = bitset_alloc(scratch.arena, nalloc);
        live_out[b->id] = bitset_alloc(scratch.arena, nalloc);

        IRInstr* instr = b->start;
        for (int i = 0; i < b->len; ++i)
        {
            switch (instr->op)
            {
                case IR_OP_STORE: {
                    assert(instr->store.loc.kind == IR_VALUE_ALLOCATION);
                    IRAllocation* a = instr->store.loc.allocation;
                    bitset_set(var_kill[b->id], a->id);
                } break;

                case IR_OP_LOAD: {
                    assert(instr->load.loc.kind == IR_VALUE_ALLOCATION);
                    IRAllocation* a = instr->load.loc.allocation;
                    if (!bitset_get(var_kill[b->id], a->id))
                        bitset_set(ue_var[b->id], a->id);
                } break;
            }

            instr = instr->next;
        }
    }

    for (;;) {
        bool changed = false;

        u32 num_u32 = num_u32_for_bits(nalloc);

        FOREACH_IR_BB(n, ir->first_block) {
            BBList succ = bb_get_succ(n);
            for (int s = 0; s < succ.count; ++s)
            {
                IRBasicBlock* m = succ.data[s];
                for (u32 j = 0; j < num_u32; ++j)
                {
                    u32* dw = &live_out[n->id]->p[j];

                    u32 ue_var_m   = ue_var  [m->id]->p[j];
                    u32 live_out_m = live_out[m->id]->p[j];
                    u32 var_kill_m = var_kill[m->id]->p[j];

                    u32 add = ue_var_m | (live_out_m & ~var_kill_m);
                    if ((add | *dw) != *dw) {
                        changed = true;
                        *dw |= add;
                    }
                }
            }
        }

        if (!changed)
            break;
    }

    // Gather information about allocations
    BBSetNode** alloc_write_blocks = arena_push_array(scratch.arena, BBSetNode*, nalloc);
    IRReg* alloc_cur_regs          = arena_push_array(scratch.arena, IRReg, nalloc);

    for (int i = 0; i < nalloc; ++i) {
        alloc_cur_regs[i] = IR_EMPTY_REG;
    }

    // Gather the blocks that write to allocations
    FOREACH_IR_BB(b, ir->first_block)
    {
        IRInstr* instr = b->start;
        for (int i = 0; i < b->len; ++i)
        {
            if (instr->op == IR_OP_STORE) {
                assert(instr->store.loc.kind == IR_VALUE_ALLOCATION);
                IRAllocation* a = instr->store.loc.allocation;
                bb_set_insert(scratch.arena, &alloc_write_blocks[a->id], b);
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

        FOREACH_BB_SET(it, alloc_write_blocks[a->id]) {
            add_to_worklist(worked, &worklist_count, worklist, it.val);
        }

        while (worklist_count > 0) // Blocks that write to this allocation
        {
            IRBasicBlock* write_block = worklist[--worklist_count];

            FOREACH_BB_SET(it, df[write_block->id])
            {
                IRBasicBlock* d = it.val;

                bool phi_needed = bitset_get(live_out[d->id], a->id) || bitset_get(ue_var[d->id], a->id);

                if (phi_needed && !(worked[d->id] & BB_HAS_PHI))
                {
                    int param_count = bb_set_count(pred[d->id]);
                    assert(param_count > 0);

                    IRInstr* instr = new_ir_instr(arena, IR_OP_PHI);
                    instr->phi.type = a->type;
                    instr->phi.param_count = param_count;
                    instr->phi.params = arena_push_array(arena, IRPhiParam, param_count);
                    instr->phi.a = a;

                    int counter = 0;
                    FOREACH_BB_SET(it2, pred[d->id]) {
                        int i = counter++;
                        instr->phi.params[i].block = it2.val;
                        instr->phi.params[i].reg = IR_EMPTY_REG;
                    }

                    insert_ir_instr_at_block_start(ir, d, instr);
                    
                    worked[d->id] |= BB_HAS_PHI;

                    add_to_worklist(worked, &worklist_count, worklist, d);
                }
            }
        }
    }

    promote_allocations(ir, alloc_cur_regs, nalloc, dom_children, ir->first_block);

    release_scratch(&scratch);
}

void optimize(Arena* arena, IR* ir) {
    mem2reg(arena, ir);
}