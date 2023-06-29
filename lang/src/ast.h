#pragma once

#include "base.h"
#include "lex.h"
#include "ir.h"

typedef enum {
    TYPE_DEFAULT,
    TYPE_IS_SIGNED = BIT(1)
} TypeFlags;

typedef struct {
    String name;
    u32 size;
    u32 flags;
} Type;

extern Type ty_u8;
extern Type ty_u16;
extern Type ty_u32;
extern Type ty_u64;

extern Type ty_i8;
extern Type ty_i16;
extern Type ty_i32;
extern Type ty_i64;

extern Type ty_void;

typedef struct {
    String name;
    IRAllocation* allocation;
    Type* type;
} Symbol;

typedef enum {
    AST_ILLEGAL,

    AST_INT,
    AST_VAR,

    AST_CAST,

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
    AST_IF,
    AST_WHILE,

    NUM_AST_KINDS,
} ASTKind;

typedef struct AST AST;
struct AST {
    AST* next;
    ASTKind kind;
    Token tok;
    Type* type;
    union {
        u64 int_val;
        struct {
            Token name;
            Symbol* sym;
        } var;
        struct {
            AST* expr;
        } cast;
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
            Token type_name;
            Token assign_tok;
            AST* init;
            Symbol* sym;
        } var_decl;
        struct {
            AST* cond;
            AST* then;
            AST* els;
        } conditional;
    };
};

typedef struct {
    int    type_table_size;
    Type** type_table;
} Program;

AST* new_ast(Arena* arena, ASTKind kind, Token tok);
