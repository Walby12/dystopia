#include <stdio.h>
#include <stdlib.h>

typedef enum {
    TOK_FN,
    TOK_ID,
    TOK_OPAREN,
    TOK_CPAREN,
    TOK_EOF,
} TokenType;

typedef struct {
    TokenType **toks;
    size_t index;
} Tokens;

typedef struct {
    char *cur_char;
    size_t index;
} Lexer;

Tokens tokens = {0};
Lexer lexer = {0};

TokenType get_next_tok(Lexer *l) {
    if (sizeof(l->cur_char) < l->index) {
        return TOK_EOF;
    }
    switch (l->cur_char[l->index++]) {
    case '(':
        return TOK_OPAREN;
        break;
    case ')':
        return TOK_CPAREN;
        break;
    default:
        fprintf(stderr, "Error: unknow char: %c", l->cur_char[l->index++]);
    }
}

int main() {
    lexer.cur_char = ")";
    TokenType tok = get_next_tok(&lexer);  
    if (tok == TOK_OPAREN) {
        printf("gay\n");
    } else {
        printf("g\n");
    }
    return 0;
}