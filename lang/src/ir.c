#include <stdio.h>

#include "ir.h"
#include "core.h"

BBList bb_get_succ(IRBasicBlock* block) {
    BBList succ = { 0 };

    if (block->len > 0) {
        switch (block->end->op) {
            default:
                if (block->end->next)
                    succ.data[succ.count++] = block->end->next->block;
                break;
            case IR_OP_RET:
                break;
            case IR_OP_BRANCH:
                succ.data[succ.count++] = block->end->branch.then_loc;
                succ.data[succ.count++] = block->end->branch.els_loc;
                break;
            case IR_OP_JMP:
                succ.data[succ.count++] = block->end->jmp_loc;
                break;
        }
    }
    else {
        if (block->start) {
            succ.data[succ.count++] = block->start->block;
        }
    }

    return succ;
}

void bb_update_end(IRBasicBlock* block) {
    if (block->len > 0) {
        block->end = block->start;
        for (int i = 1; i < block->len; ++i)
            block->end = block->end->next;
    }
    else {
        block->end = 0;
    }
}

internal void print_reg(IRReg reg) {
    printf("%%%lu", reg);
}

internal void print_value(IRValue value) {
    switch (value.kind) {
        default:
            assert(false);
            break;
        case IR_VALUE_REG:
            print_reg(value.reg);
            break;
        case IR_VALUE_INTEGER:
            printf("%llu", value.integer);
            break;
        case IR_VALUE_ALLOCATION:
            printf("[alloca %d]", value.allocation->id);
            break;
    }
}

internal char* get_type_name(IRType type) {
    static_assert(NUM_IR_TYPES == 5, "not all ir types handled");
    switch (type) {
        default:
            assert(false);
            return 0;
        case IR_TYPE_I8:
            return "i8";
        case IR_TYPE_I16:
            return "i16";
        case IR_TYPE_I32:
            return "i32";
        case IR_TYPE_I64:
            return "i64";
    }
}

void print_ir(IR* ir) {
    for (IRInstr* instr = ir->first_instr; instr; instr = instr->next)
    {
        if (instr->block->start == instr) {
            FOREACH_IR_BB(b, ir->first_block) {
                if (b->start == instr)
                    printf("bb.%d:\n", b->id);
            }
        }

        static_assert(NUM_IR_OPS == 19, "not all ir ops handled");
        switch (instr->op) {
            case IR_OP_PHI:
                printf("  ");
                print_reg(instr->phi.dest);
                printf(" = phi %s", get_type_name(instr->phi.type));
                for (int i = 0; i < instr->phi.param_count; ++i) {
                    IRPhiParam param = instr->phi.params[i];

                    if (i != 0)
                        printf(",");

                    printf(" [%%%d, bb.%d]", param.reg, param.block->id);
                }
                printf("\n");
                break;

            case IR_OP_COPY:
                printf("  ");
                print_reg(instr->copy.dest);
                printf(" = copy %s ", get_type_name(instr->copy.type));
                print_value(instr->copy.src);
                printf("\n");
                break;

            case IR_OP_LOAD:
                printf("  ");
                print_reg(instr->load.dest);
                printf(" = load %s ", get_type_name(instr->load.type));
                print_value(instr->load.loc);
                printf("\n");
                break;
            
            case IR_OP_STORE:
                printf("  store %s ", get_type_name(instr->store.type));
                print_value(instr->store.loc);
                printf(", ");
                print_value(instr->store.src);
                printf("\n");
                break;

            case IR_OP_SEXT:
            case IR_OP_ZEXT:
            case IR_OP_TRUNC:
            {
                char* name = 0;
                switch (instr->op) {
                    case IR_OP_SEXT:
                        name = "sext";
                        break;
                    case IR_OP_ZEXT:
                        name = "zext";
                        break;
                    case IR_OP_TRUNC:
                        name = "trunc";
                        break;
                }

                printf("  ");
                print_reg(instr->cast.dest);
                printf(" = %s %s ", name, get_type_name(instr->cast.type_src));
                print_value(instr->cast.src);
                printf(" to %s\n", get_type_name(instr->cast.type_dest));
            } break;

            case IR_OP_ADD:
            case IR_OP_SUB:
            case IR_OP_MUL:
            case IR_OP_DIV:
            case IR_OP_LESS:
            case IR_OP_LEQUAL:
            case IR_OP_NEQUAL:
            case IR_OP_EQUAL:
            {
                printf("  ");
                print_reg(instr->bin.dest);

                char* op_str = 0;
                switch (instr->op) {
                    case IR_OP_ADD:
                        op_str = "add";
                        break;
                    case IR_OP_SUB:
                        op_str = "sub";
                        break;
                    case IR_OP_MUL:
                        op_str = "mul";
                        break;
                    case IR_OP_DIV:
                        op_str = "div";
                        break;
                    case IR_OP_LESS:
                        op_str = "cmp lt";
                        break;
                    case IR_OP_LEQUAL:
                        op_str = "cmp le";
                        break;
                    case IR_OP_NEQUAL:
                        op_str = "cmp ne";
                        break;
                    case IR_OP_EQUAL:
                        op_str = "cmp eq";
                        break;
                }

                printf(" = %s %s ", op_str, get_type_name(instr->bin.type));
                print_value(instr->bin.l);
                printf(", ");
                print_value(instr->bin.r);
                printf("\n");
            } break;

            case IR_OP_RET:
                printf("  ret %s ", get_type_name(instr->ret.type));
                print_value(instr->ret.val);
                printf("\n");
                break;

            case IR_OP_JMP:
                printf("  jmp bb.%d\n", instr->jmp_loc->id);
                break;

            case IR_OP_BRANCH:
                printf("  branch %s ", get_type_name(instr->branch.type));
                print_value(instr->branch.cond);
                printf(", bb.%d, bb.%d\n", instr->branch.then_loc->id, instr->branch.els_loc->id);
                break;
        }
    }

    FOREACH_IR_BB(b, ir->first_block) {
        if (!b->start)
            printf("bb.%d:\n", b->id);
    }

    printf("\n");
}

void remove_ir_instr(IR* ir, IRInstr* instr) {
    if (instr->prev)
        instr->prev->next = instr->next;
    else
        ir->first_instr = instr->next;

    if (instr->next)
        instr->next->prev = instr->prev;
    
    if (instr->block->start == instr) {
        FOREACH_IR_BB(b, ir->first_block) {
            if (b->start == instr) {
                assert(b == instr->block || b->len == 0);
                b->start = instr->next;
            }
        }
    }

    instr->block->len--;
    bb_update_end(instr->block);
}

void insert_ir_instr_before(IR* ir, IRInstr* after, IRInstr* instr) {
    instr->block = after->block;

    if (after->block->start == after) {
        for (IRBasicBlock* b = ir->first_block; b; b = b->next)
        {
            if (b->start == after) {
                assert(b == instr->block || b->len == 0);
                b->start = instr;
            }
        }
    }

    instr->next = after;
    instr->prev = after->prev;

    if (after->prev)
        after->prev->next = instr;
    else
        ir->first_instr = instr;

    after->prev = instr;

    instr->block->len++;
    bb_update_end(instr->block);
}

void insert_ir_instr_at_block_start(IR* ir, IRBasicBlock* b, IRInstr* instr) {
    instr->block = b;
    instr->next = b->start;

    if (b->start) {
        instr->prev = b->start->prev;
        b->start->prev = instr;
    }
    else {
        if (ir->first_instr) {
            IRInstr* last = ir->first_instr;

            while (last->next)
                last = last->next;

            instr->prev = last;
        }
        else {
            instr->prev = 0;
        }
    }

    if (instr->prev)
        instr->prev->next = instr;
    else
        ir->first_instr = instr;

    for (IRBasicBlock* a = ir->first_block; a; a = a->next) {
        if (a->start == b->start) {
            assert(a == b || a->len == 0);
            a->start = instr;
        }
    }

    b->len++;
    bb_update_end(instr->block);
}

void output_cfg_graphviz(IR* ir, char* path) {
    (void)ir;
    
    FILE* file;
    if (fopen_s(&file, path, "w")) {
        assert(false);
    }

    fprintf(file, "digraph G {\n");

    FOREACH_IR_BB(b, ir->first_block) {
        BBList succ = bb_get_succ(b);
        for (int i = 0; i < succ.count; ++i) {
            fprintf(file, "  bb%d -> bb%d\n", b->id, succ.data[i]->id);
        }
    }

    fprintf(file, "}\n");

    fclose(file);
}

IRInstr* new_ir_instr(Arena* arena, IROpCode op) {
    assert(op);
    IRInstr* instr = arena_push_type(arena, IRInstr);
    instr->op = op;
    return instr;
}

IRValue ir_integer_value(u64 val) {
    return (IRValue) {
        .kind = IR_VALUE_INTEGER,
        .integer = val
    };
}

IRValue ir_reg_value(IRReg reg) {
    return (IRValue) {
        .kind = IR_VALUE_REG,
        .reg = reg
    };
}

IRValue ir_allocation_value(IRAllocation* allocation) {
    return (IRValue) {
        .kind = IR_VALUE_ALLOCATION,
        .allocation = allocation
    };
}

