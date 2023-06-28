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
    for (IRInstr* instr = ir->first_instr; instr; instr = instr->next) {
        static_assert(NUM_IR_KINDS == 9, "not all ir ops handled");
        switch (instr->op) {
            case IR_IMM:
                printf("  ");
                print_reg(instr->imm.dest);
                printf(" = imm ");
                print_operand(instr->imm.val);
                printf("\n");
                break;

            case IR_LOAD:
                printf("  ");
                print_reg(instr->load.dest);
                printf(" = load ");
                print_operand(instr->load.loc);
                printf("\n");
                break;
            
            case IR_STORE:
                printf("  store ");
                print_operand(instr->store.loc);
                printf(", ");
                print_operand(instr->store.src);
                printf("\n");
                break;

            case IR_ADD:
            case IR_SUB:
            case IR_MUL:
            case IR_DIV:
            {
                printf("  ");
                print_reg(instr->bin.dest);

                char* op_str = 0;
                switch (instr->op) {
                    case IR_ADD:
                        op_str = "add";
                        break;
                    case IR_SUB:
                        op_str = "sub";
                        break;
                    case IR_MUL:
                        op_str = "mul";
                        break;
                    case IR_DIV:
                        op_str = "div";
                        break;
                }

                printf(" = %s ", op_str);
                print_operand(instr->bin.l);
                printf(", ");
                print_operand(instr->bin.r);
                printf("\n");
            } break;

            case IR_RET:
                printf("  ret ");
                print_operand(instr->ret_val);
                printf("\n");
                break;
        }
    }

    printf("\n");
}
