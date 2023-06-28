#include "ir_gen.h"

typedef struct {
    Arena* arena;
    IRInstr* cur_instr;
    IRBasicBlock* cur_block;
    IRBasicBlock* first_block_to_be_placed;
    IRReg next_reg;
    int next_block_id;
} G;

internal IRInstr* new_ir_instr(Arena* arena, IROpCode op) {
    assert(op);
    IRInstr* instr = arena_push_type(arena, IRInstr);
    instr->op = op;
    return instr;
}

internal IRBasicBlock* new_ir_basic_block(Arena* arena) {
    return arena_push_type(arena, IRBasicBlock);
}

internal void emit(G* g, IRInstr* instr) {
    g->cur_instr = g->cur_instr->next = instr;

    for (IRBasicBlock* b = g->first_block_to_be_placed; b; b = b->next)
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

internal IROperand integer_operand(u64 val) {
    return (IROperand) {
        .kind = IR_OPERAND_INTEGER,
        .integer = val
    };
}

internal IROperand reg_operand(IRReg reg) {
    return (IROperand) {
        .kind = IR_OPERAND_REG,
        .reg = reg
    };
}

internal IROperand allocation_operand(IRAllocation* allocation) {
    return (IROperand) {
        .kind = IR_OPERAND_ALLOCATION,
        .allocation = allocation
    };
}

internal IRReg gen(G* g, AST* ast) {
    static_assert(NUM_AST_KINDS == 17, "not all ast kinds handled");
    switch (ast->kind) {
        default:
            assert(false);
            return 0;

        case AST_INT: {
            IRInstr* instr = new_ir_instr(g->arena, IR_IMM);
            instr->imm.dest = new_reg(g);
            instr->imm.val = integer_operand(ast->int_val);
            emit(g, instr);
            return instr->imm.dest;
        }

        case AST_VAR: {
            IRInstr* instr = new_ir_instr(g->arena, IR_LOAD);
            instr->load.loc = allocation_operand(ast->var.sym->allocation);
            instr->load.dest = new_reg(g);
            emit(g, instr);
            return instr->load.dest;
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
            IROpCode op = IR_ILLEGAL;
            switch (ast->kind) {
                case AST_ADD:
                    op = IR_ADD;
                    break;
                case AST_SUB:
                    op = IR_SUB;
                    break;
                case AST_MUL:
                    op = IR_MUL;
                    break;
                case AST_DIV:
                    op = IR_DIV;
                    break;
                case AST_LESS:
                    op = IR_LESS;
                    break;
                case AST_LEQUAL:
                    op = IR_LEQUAL;
                    break;
                case AST_NEQUAL:
                    op = IR_NEQUAL;
                    break;
                case AST_EQUAL:
                    op = IR_EQUAL;
                    break;
            }

            IRInstr* instr = new_ir_instr(g->arena, op);
            instr->bin.l = reg_operand(gen(g, ast->bin.l));
            instr->bin.r = reg_operand(gen(g, ast->bin.r));
            instr->bin.dest = new_reg(g);
            emit(g, instr);

            return instr->bin.dest;
        }

        case AST_ASSIGN: {
            assert(ast->bin.l->kind == AST_VAR);

            IRReg result = gen(g, ast->bin.r);

            IRInstr* instr = new_ir_instr(g->arena, IR_STORE);
            instr->store.src = reg_operand(result);
            instr->store.loc = allocation_operand(ast->bin.l->var.sym->allocation);
            emit(g, instr);

            return result;
        }

        case AST_BLOCK:
            for (AST* c = ast->block.first_stmt; c; c = c->next)
                gen(g, c);
            return 0;

        case AST_RETURN: {
            IRInstr* instr = new_ir_instr(g->arena, IR_RET);
            instr->ret_val = reg_operand(gen(g, ast->return_val));
            emit(g, instr);

            place_block(g, new_ir_basic_block(g->arena));

            return 0;
        }

        case AST_VAR_DECL: {
            IRAllocation* allocation = arena_push_type(g->arena, IRAllocation);
            ast->var_decl.sym->allocation = allocation;

            IRInstr* instr = new_ir_instr(g->arena, IR_STORE);
            instr->store.src = reg_operand(gen(g, ast->var_decl.init));
            instr->store.loc = allocation_operand(allocation);
            emit(g, instr);

            return 0;
        }

        case AST_IF: {
            IRBasicBlock* then = new_ir_basic_block(g->arena);
            IRBasicBlock* els  = new_ir_basic_block(g->arena);
            IRBasicBlock* end  = 0;

            IRInstr* br = new_ir_instr(g->arena, IR_BRANCH);
            br->branch.cond = reg_operand(gen(g, ast->conditional.cond));
            br->branch.then_loc = then;
            br->branch.els_loc  = els;
            emit(g, br);

            place_block(g, then);
            gen(g, ast->conditional.then);

            if (ast->conditional.els) {
                end = new_ir_basic_block(g->arena);
                IRInstr* jmp = new_ir_instr(g->arena, IR_JMP);
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
            IRInstr* br = new_ir_instr(g->arena, IR_BRANCH);
            br->branch.cond = reg_operand(gen(g, ast->conditional.cond));
            br->branch.then_loc = body;
            br->branch.els_loc  = end;
            emit(g, br);

            place_block(g, body);
            gen(g, ast->conditional.then);

            IRInstr* jmp = new_ir_instr(g->arena, IR_JMP);
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

    G g = {
        .arena = arena,
        .cur_instr = &instr_head,
        .cur_block = &block_head,
        .next_reg = 1
    };

    place_block(&g, new_ir_basic_block(g.arena));
    gen(&g, ast);

    IRInstr* prev = 0;
    for (IRInstr* instr = instr_head.next; instr; instr = instr->next) {
        instr->prev = prev;
        prev = instr;
    }

    return (IR) {
        .first_instr = instr_head.next,
        .first_block = block_head.next,
        .next_reg = g.next_reg,
    };
}
