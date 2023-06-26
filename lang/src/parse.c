#include "parse.h"
#include "lex.h"

typedef struct {
    Arena* arena;
    char* src;
    Lexer* l;
} P;

internal Expr* new_expr(Arena* arena, ExprKind kind) {
    assert(kind);
    Expr* expr = arena_push_type(arena, Expr);
    expr->kind = kind;
    return expr;
}

internal Expr* parse_primary(P* p) {
    Token tok = lex_peek(p->l);
    switch (tok.kind)
    {
        case TOK_INT: {
            lex(p->l);

            u64 val = 0;
            for (int i = 0; i < tok.len; ++i) {
                val *= 10;
                val += tok.ptr[i] - '0';
            }

            Expr* expr = new_expr(p->arena, EXPR_INT);
            expr->int_val = val;

            return expr;
        }
    }

    error_tok(p->src, tok, "expected an expression");
    return 0;
}

internal int bin_prec(Token op) {
    switch (op.kind) {
        default:
            return 0;

        case '*':
        case '/':
            return 20;
        case '+':
        case '-':
            return 10;
    }
}

internal Expr* parse_bin(P* p, int caller_prec) {
    Expr* l = parse_primary(p);
    if (!l) return 0;

    while (bin_prec(lex_peek(p->l)) > caller_prec) {
        Token op = lex(p->l);

        Expr* r = parse_bin(p, bin_prec(op));
        if (!r) return 0;

        ExprKind kind = EXPR_ILLEGAL;
        switch (op.kind) {
            case '*':
                kind = EXPR_MUL;
                break;
            case '/':
                kind = EXPR_DIV;
                break;
            case '+':
                kind =  EXPR_ADD;
                break;
            case '-':
                kind = EXPR_SUB;
                break;
        }

        Expr* bin = new_expr(p->arena, kind);
        bin->bin.l = l;
        bin->bin.r = r;
        
        l = bin;
    }

    return l;
}

internal Expr* parse_expr(P* p) {
    return parse_bin(p, 0);
}

Expr* parse(Arena* arena, char* src) {
    Lexer l = lex_init(src);

    P p = {
        .arena = arena,
        .src = src,
        .l = &l
    };

    return parse_expr(&p);
}
