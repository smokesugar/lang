#include <stdio.h>

#include "ir_gen.h"

typedef struct {
    Arena* arena;
    IRInstr* cur_instr;
    IRBasicBlock* cur_block;
    IRBasicBlock* first_block_to_be_placed;
    IRAllocation* cur_allocation;
    IRReg next_reg;
    int next_block_id;
} G;

internal IRBasicBlock* new_ir_basic_block(Arena* arena) {
    return arena_push_type(arena, IRBasicBlock);
}

internal void emit(G* g, IRInstr* instr) {
    g->cur_instr = g->cur_instr->next = instr;

    FOREACH_IR_BB(b, g->first_block_to_be_placed)
        b->start = instr;

    g->cur_block->len++;
    g->first_block_to_be_placed = 0;

    instr->block = g->cur_block;
}

internal void place_block(G* g, IRBasicBlock* block) {
    block->id = g->next_block_id++;
    g->cur_block = g->cur_block->next = block;

    if (!g->first_block_to_be_placed)
        g->first_block_to_be_placed = block;
}

internal IRReg new_reg(G* g) {
    return g->next_reg++;
}

IRType get_first_class_type(Type* type) {
    assert(type);
    assert(type->flags & TYPE_IS_FIRST_CLASS);
    switch (type->size) {
        default:
            assert(false);
            return IR_TYPE_ILLEGAL;
        case 1:
            return IR_TYPE_I8;
        case 2:
            return IR_TYPE_I16;
        case 4:
            return IR_TYPE_I32;
        case 8:
            return IR_TYPE_I64;
    }
}

internal IRReg gen(G* g, AST* ast) {
    static_assert(NUM_AST_KINDS == 18, "not all ast kinds handled");
    switch (ast->kind) {
        default:
            assert(false);
            return 0;

        case AST_INT: {
            IRInstr* instr = new_ir_instr(g->arena, IR_OP_IMM);
            instr->imm.type = get_first_class_type(ast->type);
            instr->imm.dest = new_reg(g);
            instr->imm.val = ir_integer_value(ast->int_val);
            emit(g, instr);
            return instr->imm.dest;
        }

        case AST_VAR: {
            IRInstr* instr = new_ir_instr(g->arena, IR_OP_LOAD);
            instr->load.type = get_first_class_type(ast->type);
            instr->load.loc = ir_allocation_value(ast->var.sym->allocation);
            instr->load.dest = new_reg(g);
            emit(g, instr);
            return instr->load.dest;
        }

        case AST_CAST: {
            Type* a = ast->cast.expr->type;
            Type* b = ast->type;

            IRReg src = gen(g, ast->cast.expr);
            IROpCode op = 0;

            if (a->size > b->size) {
                op = IR_OP_TRUNC;
            }
            else if (a->size == b->size) {
                return src;
            }
            else if (b->flags & TYPE_IS_SIGNED) {
                op = IR_OP_SEXT;
            }
            else {
                op = IR_OP_ZEXT;
            }

            IRInstr* instr = new_ir_instr(g->arena, op);
            instr->cast.type_src = get_first_class_type(a);
            instr->cast.type_dest = get_first_class_type(b);
            instr->cast.src = ir_reg_value(src);
            instr->cast.dest = new_reg(g);
            emit(g, instr);

            return instr->cast.dest;
        }

        case AST_ADD:
        case AST_SUB:
        case AST_MUL:
        case AST_DIV:
        case AST_LESS:
        case AST_LEQUAL:
        case AST_NEQUAL:
        case AST_EQUAL:
        {
            IROpCode op = IR_OP_ILLEGAL;
            switch (ast->kind) {
                case AST_ADD:
                    op = IR_OP_ADD;
                    break;
                case AST_SUB:
                    op = IR_OP_SUB;
                    break;
                case AST_MUL:
                    op = IR_OP_MUL;
                    break;
                case AST_DIV:
                    op = IR_OP_DIV;
                    break;
                case AST_LESS:
                    op = IR_OP_LESS;
                    break;
                case AST_LEQUAL:
                    op = IR_OP_LEQUAL;
                    break;
                case AST_NEQUAL:
                    op = IR_OP_NEQUAL;
                    break;
                case AST_EQUAL:
                    op = IR_OP_EQUAL;
                    break;
            }

            IRInstr* instr = new_ir_instr(g->arena, op);
            instr->bin.type = get_first_class_type(ast->type);
            instr->bin.l = ir_reg_value(gen(g, ast->bin.l));
            instr->bin.r = ir_reg_value(gen(g, ast->bin.r));
            instr->bin.dest = new_reg(g);
            emit(g, instr);

            return instr->bin.dest;
        }

        case AST_ASSIGN: {
            assert(ast->bin.l->kind == AST_VAR);

            IRReg result = gen(g, ast->bin.r);

            IRInstr* instr = new_ir_instr(g->arena, IR_OP_STORE);
            instr->store.type = get_first_class_type(ast->type);
            instr->store.src = ir_reg_value(result);
            instr->store.loc = ir_allocation_value(ast->bin.l->var.sym->allocation);
            emit(g, instr);

            return result;
        }

        case AST_BLOCK:
            for (AST* c = ast->block.first_stmt; c; c = c->next)
                gen(g, c);
            return 0;

        case AST_RETURN: {
            IRInstr* instr = new_ir_instr(g->arena, IR_OP_RET);
            instr->ret.type = get_first_class_type(ast->return_val->type);
            instr->ret.val = ir_reg_value(gen(g, ast->return_val));
            emit(g, instr);

            place_block(g, new_ir_basic_block(g->arena));

            return 0;
        }

        case AST_VAR_DECL: {
            IRAllocation* allocation = arena_push_type(g->arena, IRAllocation);
            ast->var_decl.sym->allocation = allocation;

            g->cur_allocation = g->cur_allocation->next = allocation;

            IRInstr* instr = new_ir_instr(g->arena, IR_OP_STORE);
            instr->store.type = get_first_class_type(ast->var_decl.init->type);
            instr->store.src = ir_reg_value(gen(g, ast->var_decl.init));
            instr->store.loc = ir_allocation_value(allocation);
            emit(g, instr);

            allocation->type = instr->store.type;

            return 0;
        }

        case AST_IF: {
            IRBasicBlock* then = new_ir_basic_block(g->arena);
            IRBasicBlock* els  = new_ir_basic_block(g->arena);
            IRBasicBlock* end  = 0;

            IRInstr* br = new_ir_instr(g->arena, IR_OP_BRANCH);
            br->branch.type = get_first_class_type(ast->conditional.cond->type);
            br->branch.cond = ir_reg_value(gen(g, ast->conditional.cond));
            br->branch.then_loc = then;
            br->branch.els_loc  = els;
            emit(g, br);

            place_block(g, then);
            gen(g, ast->conditional.then);

            if (ast->conditional.els) {
                end = new_ir_basic_block(g->arena);
                IRInstr* jmp = new_ir_instr(g->arena, IR_OP_JMP);
                jmp->jmp_loc = end;
                emit(g, jmp);
            }

            place_block(g, els);

            if (ast->conditional.els) {
                gen(g, ast->conditional.els);
                place_block(g, end);
            }

            return 0;
        }

        case AST_WHILE: {
            IRBasicBlock* start = new_ir_basic_block(g->arena);
            IRBasicBlock* body  = new_ir_basic_block(g->arena);
            IRBasicBlock* end   = new_ir_basic_block(g->arena);

            place_block(g, start);
            IRInstr* br = new_ir_instr(g->arena, IR_OP_BRANCH);
            br->branch.type = get_first_class_type(ast->conditional.cond->type);
            br->branch.cond = ir_reg_value(gen(g, ast->conditional.cond));
            br->branch.then_loc = body;
            br->branch.els_loc  = end;
            emit(g, br);

            place_block(g, body);
            gen(g, ast->conditional.then);

            IRInstr* jmp = new_ir_instr(g->arena, IR_OP_JMP);
            jmp->jmp_loc = start;
            emit(g, jmp);

            place_block(g, end);

            return 0;
        }
    }
}

IR ir_gen(Arena* arena, AST* ast) {
    IRInstr instr_head = { 0 };
    IRBasicBlock block_head = { 0 };
    IRAllocation allocation_head = { 0 };

    G g = {
        .arena = arena,
        .cur_instr = &instr_head,
        .cur_block = &block_head,
        .cur_allocation = &allocation_head,
        .next_reg = 1
    };

    place_block(&g, new_ir_basic_block(g.arena));
    gen(&g, ast);

    IRInstr* prev = 0;
    for (IRInstr* instr = instr_head.next; instr; instr = instr->next) {
        instr->prev = prev;
        prev = instr;
    }

    FOREACH_IR_BB(b, block_head.next)
        bb_update_end(b);

    return (IR) {
        .first_instr = instr_head.next,
        .first_block = block_head.next,
        .first_allocation = allocation_head.next,
        .next_reg = g.next_reg,
        .num_regs = g.next_reg-1
    };
}
