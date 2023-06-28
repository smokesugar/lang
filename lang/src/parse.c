#include "parse.h"
#include "lex.h"

typedef struct {
    Arena* arena;
    char* src;
    Lexer* l;
} P;

internal AST* new_ast(Arena* arena, ASTKind kind, Token tok) {
    assert(kind);
    AST* ast = arena_push_type(arena, AST);
    ast->kind = kind;
    ast->tok = tok;
    return ast;
}

internal bool match(P* p, int kind, char* desc) {
    Token tok = lex_peek(p->l);
    if (tok.kind == kind) {
        lex(p->l);
        return true;
    }

    error_tok(p->src, tok, "expected %s", desc);
    return false;
}

#define REQUIRE(kind, desc) if (!match(p, kind, desc)) { return 0; }

internal AST* parse_primary(P* p) {
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

            AST* expr = new_ast(p->arena, AST_INT, tok);
            expr->int_val = val;

            return expr;
        }

        case TOK_IDENT: {
            lex(p->l);

            AST* expr = new_ast(p->arena, AST_VAR, tok);
            expr->var.name = tok;

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

internal AST* parse_bin(P* p, int caller_prec) {
    AST* l = parse_primary(p);
    if (!l) return 0;

    while (bin_prec(lex_peek(p->l)) > caller_prec) {
        Token op = lex(p->l);

        AST* r = parse_bin(p, bin_prec(op));
        if (!r) return 0;

        ASTKind kind = AST_ILLEGAL;
        switch (op.kind) {
            case '*':
                kind = AST_MUL;
                break;
            case '/':
                kind = AST_DIV;
                break;
            case '+':
                kind =  AST_ADD;
                break;
            case '-':
                kind = AST_SUB;
                break;
        }

        AST* bin = new_ast(p->arena, kind, op);
        bin->bin.l = l;
        bin->bin.r = r;
        
        l = bin;
    }

    return l;
}

internal AST* parse_assign(P* p) {
    AST* l= parse_bin(p, 0);
    if (!l) return 0;

    if (lex_peek(p->l).kind == '=') {
        Token equals = lex(p->l);

        AST* r = parse_assign(p);
        if (!r) return 0;

        AST* bin = new_ast(p->arena, AST_ASSIGN, equals);
        bin->bin.l = l;
        bin->bin.r = r;

        l = bin;
    }

    return l;
}

internal AST* parse_expr(P* p) {
    return parse_assign(p);
}

internal AST* parse_stmt(P* p, AST* parent_block);

internal AST* parse_block(P* p) {
    Token lbrace = lex_peek(p->l);
    REQUIRE('{', "{");

    AST head = { 0 };
    AST* cur = &head;

    AST* block = new_ast(p->arena, AST_BLOCK, lbrace);

    while (lex_peek(p->l).kind != TOK_EOF &&
           lex_peek(p->l).kind != '}')
    {
        AST* stmt = parse_stmt(p, block);
        if (!stmt) return 0;
        cur = cur->next = stmt;
    }

    REQUIRE('}', "}");

    block->block.first_stmt = head.next;

    return block;
}

internal AST* parse_stmt(P* p, AST* parent_block) {
    Token tok = lex_peek(p->l);
    switch (tok.kind) {
        case '{':
            return parse_block(p);

        case TOK_RETURN: {
            lex(p->l);

            AST* val = parse_expr(p);
            if (!val) return 0;
            REQUIRE(';', ";");

            AST* ret = new_ast(p->arena, AST_RETURN, tok);
            ret->return_val = val;

            return ret;
        }

        case TOK_IDENT: {
            lex(p->l);

            if (lex_peek(p->l).kind != ':') {
                lex_jump(p->l, tok);
                break;
            }

            REQUIRE(':', ":");
            REQUIRE('=', "=");

            AST* init = parse_expr(p);
            if (!init) return 0;

            REQUIRE(';', ";");

            AST* node = new_ast(p->arena, AST_VAR_DECL, tok);
            node->var_decl.name = tok;
            node->var_decl.init = init;

            parent_block->block.num_locals++;

            return node;
        }
    }

    AST* expr = parse_expr(p);
    if (!expr) return 0;
    REQUIRE(';', ";");

    return expr;
}

AST* parse(Arena* arena, char* src) {
    Lexer l = lex_init(src);

    P p = {
        .arena = arena,
        .src = src,
        .l = &l
    };

    return parse_block(&p);
}
