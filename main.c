#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

typedef enum {
    TOK_FN,
    TOK_ID,
    TOK_OPAREN,
    TOK_CPAREN,
    TOK_EOF,
} TokenType;

typedef struct {
    TokenType type;
    char *str;
} Token;

typedef struct {
    char *cur_char;
    size_t index;
} Lexer;

Token token = {0};
Lexer lexer = {0};
int line_lex = 0;

Token* new_fn() {
    Token *t = malloc(sizeof(Token));
    t->type = TOK_FN;
    return t;
}

Token* new_id(char *c) {
    Token *t = malloc(sizeof(Token));
    t->type = TOK_ID;
    t->str = c;
    return t;
}

Token* new_oparen() {
    Token *t = malloc(sizeof(Token));
    t->type = TOK_OPAREN;
    return t;
}

Token* new_cparen() {
    Token *t = malloc(sizeof(Token));
    t->type = TOK_CPAREN;
    return t;
}

Token* new_eof() {
    Token *t = malloc(sizeof(Token));
    t->type = TOK_EOF;
    return t;
}

Token* get_next_tok(Lexer *l) {
    switch (l->cur_char[l->index]) {
    case '(':
        l->index++;
        return new_oparen();
    case ')':
        l->index++;
        return new_cparen();
    case '\n':
        l->index++;
        line_lex++;
        break;
    case ' ':  
        l->index++;
        break;
    case '\t':
        l->index++;
        break;
    case '\0':
        return new_eof();
    default:
        if (isalpha(l->cur_char[l->index])) {
            char str[256];
            int j = 0;
            while (isalpha(l->cur_char[l->index]) && j < sizeof(str) - 1) {
                str[j++] = l->cur_char[l->index++];
            }
            str[j] = '\0';
            printf("%s\n", str);
            if (strcmp(str, "fn") == 0) {
                return new_fn();
            } else {
                return new_id(strdup(str));
            }
        }
        fprintf(stderr, "Error: unknow char: %c at line %d\n", l->cur_char[l->index], line_lex);
        exit(1);
    }
}

int main() {
    lexer.cur_char = "fn hello()";
    Token *tok = get_next_tok(&lexer);
    while (tok->type != TOK_EOF) {
        tok = get_next_tok(&lexer);
    }
    return 0;
}