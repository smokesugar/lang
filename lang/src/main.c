#include <stdio.h>
#include <stdlib.h>

#include "base.h"
#include "ir_gen.h"
#include "parse.h"
#include "core.h"
#include "opt.h"
#include "sem.h"

#define ARENA_CAP (5 * 1024 * 1024)

static Arena scratch_arenas[3];

Scratch get_scratch(Arena** conflicts, int conflict_count) {
    for (int i = 0; i < LEN(scratch_arenas); ++i)
    {
        bool does_conflict = false;

        for (int j = 0; j < conflict_count; ++j)
        {
            if (&scratch_arenas[i] == conflicts[j]) {
                does_conflict = true;
                break;
            }
        }
    
        if (!does_conflict) {
            return (Scratch) {
                .arena = scratch_arenas + i,
                .used = scratch_arenas[i].used
            };
        }
    }

    assert(false && "all scratch arenas conflict!");
    return (Scratch) { 0 };
}

void release_scratch(Scratch* scratch) {
    assert(scratch->arena->used >= scratch->used);
    scratch->arena->used = scratch->used;
}

internal i64 value_val(i64* regs, IRValue value) {
    switch (value.kind) {
        default:
            assert(false);
            return 0;

        case IR_VALUE_REG:
            return regs[value.reg];

        case IR_VALUE_INTEGER:
            return value.integer;
    }
}

internal i64* value_addr(IRValue value) {
    switch (value.kind) {
        default:
            assert(false);
            return 0;

        case IR_VALUE_ALLOCATION:
            return &value.allocation->_val;
    }
}

int main() {
    Arena arena = {
        .ptr = malloc(ARENA_CAP),
        .cap = ARENA_CAP,
    };

    for (int i = 0; i < LEN(scratch_arenas); ++i) {
        scratch_arenas[i] = (Arena){
            .ptr = malloc(ARENA_CAP),
            .cap = ARENA_CAP
        };
    }

    char* src_path = "examples/test.lang";
    FILE* file;
    if (fopen_s(&file, src_path, "r")) {
        printf("Failed to load '%s'\n", src_path);
        return 1;
    }

    fseek(file, 0, SEEK_END);
    size_t file_len = ftell(file);
    rewind(file);

    char* src = arena_push(&arena, file_len + 1);
    size_t src_len = fread(src, 1, file_len, file);
    src[src_len] = '\0';

    AST* ast = parse(&arena, src);
    if (!ast) return 1;

    Program prog = program_init(&arena);

    if (!sem_ast(&arena, src, &prog, ast)) return 1;

    IR ir = ir_gen(&arena, ast);
    optimize(&ir);

    print_ir(&ir);

    i64 regs[512];

    for (IRInstr* instr = ir.first_instr; instr;) {
        static_assert(NUM_IR_OPS == 18, "not all ir ops handled");
        switch (instr->op) {
            default:
                assert(false);
                break;

            case IR_OP_IMM:
                regs[instr->imm.dest] = value_val(regs, instr->imm.val);
                break;

            case IR_OP_LOAD:
                regs[instr->load.dest] = *value_addr(instr->load.loc);
                break;
            case IR_OP_STORE:
                *value_addr(instr->store.loc) = value_val(regs, instr->store.src);
                break;

            case IR_OP_SEXT:
            case IR_OP_ZEXT:
            case IR_OP_TRUNC:
                regs[instr->cast.dest] = value_val(regs, instr->cast.src);
                break;
                
            case IR_OP_ADD:
                regs[instr->bin.dest] = value_val(regs, instr->bin.l) + value_val(regs, instr->bin.r);
                break;
            case IR_OP_SUB:
                regs[instr->bin.dest] = value_val(regs, instr->bin.l) - value_val(regs, instr->bin.r);
                break;
            case IR_OP_MUL:
                regs[instr->bin.dest] = value_val(regs, instr->bin.l) * value_val(regs, instr->bin.r);
                break;
            case IR_OP_DIV:
                regs[instr->bin.dest] = value_val(regs, instr->bin.l) / value_val(regs, instr->bin.r);
                break;
            case IR_OP_LESS:
                regs[instr->bin.dest] = value_val(regs, instr->bin.l) < value_val(regs, instr->bin.r);
                break;
            case IR_OP_LEQUAL:
                regs[instr->bin.dest] = value_val(regs, instr->bin.l) <= value_val(regs, instr->bin.r);
                break;
            case IR_OP_NEQUAL:
                regs[instr->bin.dest] = value_val(regs, instr->bin.l) != value_val(regs, instr->bin.r);
                break;
            case IR_OP_EQUAL:
                regs[instr->bin.dest] = value_val(regs, instr->bin.l) == value_val(regs, instr->bin.r);
                break;

            case IR_OP_RET:
                printf("Result: %lld\n", value_val(regs, instr->ret.val));
                return 0;

            case IR_OP_JMP:
                instr = instr->jmp_loc->start;
                continue;

            case IR_OP_BRANCH: {
                IRBasicBlock* block = value_val(regs, instr->branch.cond) ? instr->branch.then_loc : instr->branch.els_loc;
                instr = block->start;
            } continue;
        }

        instr = instr->next;
    }

    printf("Program did not return.\n");
    return 1;
}