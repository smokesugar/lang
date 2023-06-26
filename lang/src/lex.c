#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>

#include "lex.h"

Lexer lex_init(char* src) {
    return (Lexer) {
        .ptr = src,
        .line = 1,
    };
}

Token lex(Lexer* l) {
    if (l->next.ptr) {
        Token tok = l->next;
        l->next.ptr = 0;
        return tok;
    }

    while (isspace(*l->ptr)) {
        if (*l->ptr == '\n')
            ++l->line;
        ++l->ptr;
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

