#pragma once

#include "base.h"

typedef enum {
    AST_ILLEGAL,

    AST_INT,

    AST_ADD,
    AST_SUB,
    AST_MUL,
    AST_DIV,

    AST_BLOCK,
    AST_RETURN,

    NUM_AST_KINDS,
} ASTKind;

typedef struct AST AST;
struct AST {
    AST* next;
    ASTKind kind;
    union {
        u64 int_val;
        struct {
            AST* l;
            AST* r;
        } bin;
        AST* block_first;
        AST* return_val;
    };
};
