#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "lex.h"

Lexer lex_init(char* src) {
    return (Lexer) {
        .ptr = src,
        .line = 1,
    };
}

internal bool is_ident(char c) {
    return isalnum(c) || c == '_';
}

internal int check_kw(char* start, Lexer* l, char* kw, int kind) {
    size_t len = l->ptr - start;

    if (len == strlen(kw) && memcmp(start, kw, len) == 0)
        return kind;

    return TOK_IDENT;
}

internal int ident_kind(char* start, Lexer* l) {
    switch (start[0]) {
        case 'r':
            return check_kw(start, l, "return", TOK_RETURN);
    }

    return TOK_IDENT;
}

internal void eat_whitespace(Lexer* l) {
    while (isspace(*l->ptr)) {
        if (*l->ptr == '\n')
            ++l->line;
        ++l->ptr;
    }
}

Token lex(Lexer* l) {
    if (l->next.ptr) {
        Token tok = l->next;
        l->next.ptr = 0;
        return tok;
    }

    eat_whitespace(l);

    while (l->ptr[0] == '/' && l->ptr[1] == '/') {
        while (*l->ptr != '\n' && *l->ptr != '\0')
            ++l->ptr;

        eat_whitespace(l);
    }

    char* start = l->ptr++;
    int kind = *start;
    int line = l->line;

    switch (kind) {
        default:
            if (isdigit(*start)) {
                while (isdigit(*l->ptr))
                    ++l->ptr;
                kind = TOK_INT;
            }
            else if (is_ident(*start)) {
                while (is_ident(*l->ptr))
                    ++l->ptr;
                kind = ident_kind(start, l);
            }
            break;
        case '\0':
            --l->ptr;
            break;
    }

    return (Token) {
        .kind = kind,
        .len = (int)(l->ptr-start),
        .ptr = start,
        .line = line
    };
}

Token lex_peek(Lexer* l) {
    if (!l->next.ptr) {
        l->next = lex(l);
    }

    return l->next;
}

void  lex_jump(Lexer* l, Token tok) {
    l->next.ptr = 0;
    l->ptr = tok.ptr;
    l->line = tok.line;
}

void error_tok(char* src, Token tok, char* fmt, ...) {
    char* l = tok.ptr;
    while (l != src && *l != '\n')
        --l;

    while (isspace(*l))
        ++l;

    int nl = 0;
    while (l[nl] != '\0' && l[nl] != '\n')
        ++nl;

    int offset = printf("Line %d: Error: ", tok.line);
    printf("%.*s\n", nl, l);

    printf("%*s^ ", offset + (int)(tok.ptr - l), "");

    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);

    printf("\n");
}

