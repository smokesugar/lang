#include "ir_gen.h"

typedef struct {
    Arena* arena;
    IRInstr* cur_instr;
    IRReg next_reg;
} G;

internal IRInstr* new_ir_instr(Arena* arena, IROpCode op) {
    assert(op);
    IRInstr* instr = arena_push_type(arena, IRInstr);
    instr->op = op;
    return instr;
}

internal void emit(G* g, IRInstr* instr) {
    g->cur_instr = g->cur_instr->next = instr;
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
    static_assert(NUM_AST_KINDS == 11, "not all ast kinds handled");
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
    }
}

IR ir_gen(Arena* arena, AST* ast) {
    IRInstr instr_head = { 0 };

    G g = {
        .arena = arena,
        .cur_instr = &instr_head,
        .next_reg = 1
    };

    gen(&g, ast);

    return (IR) {
        .first_instr = instr_head.next,
        .next_reg = g.next_reg
    };
}
