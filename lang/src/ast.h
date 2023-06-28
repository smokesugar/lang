#pragma once

#include "base.h"
#include "lex.h"
#include "ir.h"

typedef struct {
    String name;
    IRAllocation* allocation;
} Symbol;

typedef enum {
    AST_ILLEGAL,

    AST_INT,
    AST_VAR,

    AST_ADD,
    AST_SUB,
    AST_MUL,
    AST_DIV,

    AST_LESS,
    AST_LEQUAL,
    AST_NEQUAL,
    AST_EQUAL,

    AST_ASSIGN,

    AST_BLOCK,
    AST_RETURN,
    AST_VAR_DECL,

    NUM_AST_KINDS,
} ASTKind;

typedef struct AST AST;
struct AST {
    AST* next;
    ASTKind kind;
    Token tok;
    union {
        u64 int_val;
        struct {
            Token name;
            Symbol* sym;
        } var;
        struct {
            AST* l;
            AST* r;
        } bin;
        struct {
            AST* first_stmt;
            int num_locals;
        } block;
        AST* return_val;
        struct {
            Token name;
            AST* init;
            Symbol* sym;
        } var_decl;
    };
};
