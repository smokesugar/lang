#pragma once

#include "base.h"

typedef enum {
    EXPR_ILLEGAL,

    EXPR_INT,

    EXPR_ADD,
    EXPR_SUB,
    EXPR_MUL,
    EXPR_DIV,

    NUM_EXPR_KINDS,
} ExprKind;

typedef struct Expr Expr;
struct Expr {
    ExprKind kind;
    union {
        u64 int_val;
        struct {
            Expr* l;
            Expr* r;
        } bin;
    };
};
