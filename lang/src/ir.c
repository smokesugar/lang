#include <stdio.h>

#include "ir.h"

internal void print_reg(IRReg reg) {
    printf("%%%lu", reg);
}

internal void print_operand(IROperand operand) {
    switch (operand.kind) {
        default:
            assert(false);
            break;
        case IR_OPERAND_REG:
            print_reg(operand.reg);
            break;
        case IR_OPERAND_INTEGER:
            printf("%llu", operand.integer);
            break;
        case IR_OPERAND_ALLOCATION:
            printf("[alloca %p]", operand.allocation);
            break;
    }
}

void print_ir(IR* ir) {
    for (IRInstr* instr = ir->first_instr; instr; instr = instr->next)
    {
        if (instr->block->start == instr) {
            for (IRBasicBlock* b = ir->first_block; b; b = b->next) {
                if (b->start == instr)
                    printf("bb.%d:\n", b->id);
            }
        }

        static_assert(NUM_IR_OPS == 15, "not all ir ops handled");
        switch (instr->op) {
            case IR_OP_IMM:
                printf("  ");
                print_reg(instr->imm.dest);
                printf(" = imm ");
                print_operand(instr->imm.val);
                printf("\n");
                break;

            case IR_OP_LOAD:
                printf("  ");
                print_reg(instr->load.dest);
                printf(" = load ");
                print_operand(instr->load.loc);
                printf("\n");
                break;
            
            case IR_OP_STORE:
                printf("  store ");
                print_operand(instr->store.loc);
                printf(", ");
                print_operand(instr->store.src);
                printf("\n");
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

                printf(" = %s ", op_str);
                print_operand(instr->bin.l);
                printf(", ");
                print_operand(instr->bin.r);
                printf("\n");
            } break;

            case IR_OP_RET:
                printf("  ret ");
                print_operand(instr->ret_val);
                printf("\n");
                break;

            case IR_OP_JMP:
                printf("  jmp bb.%d\n", instr->jmp_loc->id);
                break;

            case IR_OP_BRANCH:
                printf("  branch ");
                print_operand(instr->branch.cond);
                printf(", bb.%d, bb.%d\n", instr->branch.then_loc->id, instr->branch.els_loc->id);
                break;
        }
    }

    for (IRBasicBlock* b = ir->first_block; b; b = b->next) {
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
        for (IRBasicBlock* b = ir->first_block; b; b = b->next) {
            if (b->start == instr) {
                b->start = instr->next;
            }
        }
    }

    instr->block->len--;
}
