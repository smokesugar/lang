#include <stdio.h>
#include <stdlib.h>

#include "base.h"
#include "parse.h"

#define ARENA_CAP (5 * 1024 * 1024)

internal i64 eval(AST* e) {
    switch (e->kind) {
        default:
            assert(false);
            return -1;
        case AST_INT:
            return e->int_val;
        case AST_ADD:
            return eval(e->bin.l) + eval(e->bin.r);
        case AST_SUB:
            return eval(e->bin.l) - eval(e->bin.r);
        case AST_MUL:
            return eval(e->bin.l) * eval(e->bin.r);
        case AST_DIV:
            return eval(e->bin.l) / eval(e->bin.r);
        case AST_BLOCK: {
            for (AST* c = e->block_first; c; c = c->next) {
                eval(c);
            }
            return -1;
        }
        case AST_RETURN: {
            printf("Returned with %lld\n", eval(e->return_val));
            exit(0);
        }
    }
}

int main() {
    Arena arena = {
        .ptr = malloc(ARENA_CAP),
        .cap = ARENA_CAP,
    };

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

    eval(ast);

    printf("Program did not return.\n");
    return 1;
}