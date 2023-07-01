#include <stdio.h>

#include "ir.h"

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
            printf("[alloca %p]", value.allocation);
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
            for (IRBasicBlock* b = ir->first_block; b; b = b->next) {
                if (b->start == instr)
                    printf("bb.%d:\n", b->id);
            }
        }

        static_assert(NUM_IR_OPS == 18, "not all ir ops handled");
        switch (instr->op) {
            case IR_OP_IMM:
                printf("  ");
                print_reg(instr->imm.dest);
                printf(" = imm %s ", get_type_name(instr->imm.type));
                print_value(instr->imm.val);
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
