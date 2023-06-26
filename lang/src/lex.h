#pragma once

#include "base.h"

enum {
    TOK_EOF,

    TOK_INT = 256,
    TOK_IDENT,

    TOK_RETURN,
};

typedef struct {
    int kind;
    int len;
    char* ptr;
    int line;
} Token;

typedef struct {
    char* ptr;
    int line;
    Token next;
} Lexer;

Lexer lex_init(char* src);
Token lex(Lexer* l);
Token lex_peek(Lexer* l);

void error_tok(char* src, Token tok, char* fmt, ...);
