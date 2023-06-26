#include <stdio.h>
#include <stdlib.h>

#include "base.h"
#include "parse.h"

#define ARENA_CAP (5 * 1024 * 1024)

internal i64 eval(Expr* e) {
    switch (e->kind) {
        case EXPR_INT:
            return e->int_val;
        case EXPR_ADD:
            return eval(e->bin.l) + eval(e->bin.r);
        case EXPR_SUB:
            return eval(e->bin.l) - eval(e->bin.r);
        case EXPR_MUL:
            return eval(e->bin.l) * eval(e->bin.r);
        case EXPR_DIV:
            return eval(e->bin.l) / eval(e->bin.r);
    }

    return -1;
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

    Expr* expr = parse(&arena, src);
    if (!expr) return 1;

    printf("Result: %lld\n", eval(expr));

    return 0;
}