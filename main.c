#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h> 

typedef enum {
    TOK_FN,
    TOK_PRINT,
    TOK_PRINTLN,
    TOK_ID,
    TOK_OPAREN,
    TOK_CPAREN,
    TOK_SEMICOLON,
    TOK_OCURLY,
    TOK_CCURLY,
    TOK_STRING,
    TOK_EOF,
} TokenType;

typedef struct {
    int line;
    int column;
    const char *filename;
} SourceLocation;

typedef struct {
    TokenType type;
    char *str;
    SourceLocation loc;
} Token;

typedef struct {
    char *cur_char;
    size_t index;
    int line;
    int column;
    const char *filename;
} Lexer;

typedef enum {
    STMT_PRINT,
    STMT_PRINTLN,
} StmtType;

typedef struct Stmt {
    StmtType type;
    union {
        struct {
            Token *value;
        } print_stmt;
        struct {
            Token *value;
        } println_stmt;
    } data;
    struct Stmt *next;
} Stmt;

typedef struct {
    char *name;
    char *return_type;
    Stmt *stmt_list;
} ParseFunc;

Token token = {0};
Lexer lexer = {0};
int line_lex = 0;

Token* get_next_tok(Lexer *l);
Token* new_token(TokenType type, char *str, SourceLocation loc);

void lexer_init(Lexer *l, const char *source, const char *filename) {
    l->cur_char = (char*)source;
    l->index = 0;
    l->line = 1;
    l->column = 1;
    l->filename = filename;
}

Token* new_token(TokenType type, char *str, SourceLocation loc) {
    Token *t = malloc(sizeof(Token));
    t->type = type;
    t->str = str;
    t->loc = loc;
    return t;
}

void error_at(SourceLocation loc, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    fprintf(stderr, "\033[1;31mError\033[0m at %s:%d:%d: ", 
            loc.filename ? loc.filename : "<input>", 
            loc.line, 
            loc.column);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    
    va_end(args);
    exit(1);
}

void error_tok(Token *tok, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    fprintf(stderr, "\033[1;31mError\033[0m at %s:%d:%d: ", 
            tok->loc.filename ? tok->loc.filename : "<input>",
            tok->loc.line, 
            tok->loc.column);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    
    if (tok->str) {
        fprintf(stderr, "  near: '%s'\n", tok->str);
    }
    
    va_end(args);
    exit(1);
}

void warn_tok(Token *tok, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    fprintf(stderr, "\033[1;33mWarning\033[0m at %s:%d:%d: ", 
            tok->loc.filename ? tok->loc.filename : "<input>",
            tok->loc.line, 
            tok->loc.column);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    
    va_end(args);
}

Token* get_next_tok(Lexer *l) {
    while (1) {
        char c = l->cur_char[l->index];
        
        SourceLocation loc = {l->line, l->column, l->filename};
        
        switch (c) {
        case '(':
            l->index++;
            l->column++;
            return new_token(TOK_OPAREN, NULL, loc);
        case ')':
            l->index++;
            l->column++;
            return new_token(TOK_CPAREN, NULL, loc);
        case '{':
            l->index++;
            l->column++;
            return new_token(TOK_OCURLY, NULL, loc);
        case '}': 
            l->index++;
            l->column++;
            return new_token(TOK_CCURLY, NULL, loc);
        case ';':
            l->index++;
            l->column++;
            return new_token(TOK_SEMICOLON, NULL, loc);
        case '\n':
            l->index++;
            l->line++;
            l->column = 1;
            break;
        case ' ':
        case '\t':
            l->index++;
            l->column++;
            break;
        case '\0':
            return new_token(TOK_EOF, NULL, loc);
        case '\"':
            {
                l->index++;
                l->column++;
                char str[1024];
                size_t j = 0;
                while (j < sizeof(str) - 1 && 
                       l->cur_char[l->index] != '\0' && 
                       l->cur_char[l->index] != '\"') {
                    if (l->cur_char[l->index] == '\n') {
                        l->line++;
                        l->column = 1;
                    } else {
                        l->column++;
                    }
                    str[j++] = l->cur_char[l->index++];
                }
                str[j] = '\0';
                
                if (l->cur_char[l->index] == '\"') {
                    l->index++;
                    l->column++;
                } else {
                    error_at(loc, "unterminated string literal");
                }
                
                return new_token(TOK_STRING, strdup(str), loc);
            }
        default:
            if (isalpha(c)) {
                char str[256];
                size_t j = 0;
                while (isalpha(l->cur_char[l->index]) && j < sizeof(str) - 1) {
                    str[j++] = l->cur_char[l->index++];
                    l->column++;
                }
                str[j] = '\0';
                if (strcmp(str, "fn") == 0) {
                    return new_token(TOK_FN, NULL, loc);
                } else if (strcmp(str, "print") == 0) {
                    return new_token(TOK_PRINT, NULL, loc);
                } else if(strcmp(str, "println") == 0) {
                    return new_token(TOK_PRINTLN, NULL, loc);
                } else {
                    return new_token(TOK_ID, strdup(str), loc);
                }
            }
            error_at(loc, "unexpected character '%c'", c);
        }
    }
}

Stmt* new_print_stmt(Token *value) {
    Stmt *s = malloc(sizeof(Stmt));
    s->type = STMT_PRINT;
    s->data.print_stmt.value = value;
    s->next = NULL;
    return s;
}

Stmt* new_println_stmt(Token *value) {
    Stmt *s = malloc(sizeof(Stmt));
    s->type = STMT_PRINTLN;
    s->data.println_stmt.value = value;
    s->next = NULL;
    return s;
}

void add_stmt(ParseFunc *func, Stmt *new_stmt) {
    if (func->stmt_list == NULL) {
        func->stmt_list = new_stmt;
    } else {
        Stmt *curr = func->stmt_list;
        while (curr->next != NULL) {
            curr = curr->next;
        }
        curr->next = new_stmt;
    }
}

const char* to_string(TokenType t) {
    switch (t) {
    case TOK_SEMICOLON:
        return "semicolon";
    case TOK_PRINT:
        return "print";
    case TOK_PRINTLN:
        return "println";
    case TOK_ID:
        return "identifier";
    case TOK_FN:
        return "fn";
    case TOK_OPAREN:
        return "open parenthesis";
    case TOK_CPAREN:
        return "close parenthesis";
    case TOK_OCURLY:
        return "open curly";
    case TOK_CCURLY:
        return "close curly";
    case TOK_STRING:
        return "string";
    case TOK_EOF:
        return "end of file";
    default:
        return "unknown token";
    }
}

Token* expect_token(TokenType expected, const char *context) {
    Token *tok = get_next_tok(&lexer);
    if (tok->type != expected) {
        error_tok(tok, "expected %s in %s, got %s",
                  to_string(expected), context, to_string(tok->type));
    }
    return tok;
}

Stmt* parse_print_stmt() {
    expect_token(TOK_OPAREN, "print statement");
    
    Token *value = get_next_tok(&lexer);
    if (value->type != TOK_STRING) {
        error_tok(value, "expected string in print statement");
    }
    
    expect_token(TOK_CPAREN, "print statement");
    expect_token(TOK_SEMICOLON, "print statement");
    
    return new_print_stmt(value);
}

Stmt* parse_println_stmt() {
    expect_token(TOK_OPAREN, "println statement");
    
    Token *value = get_next_tok(&lexer);
    if (value->type != TOK_STRING) {
        error_tok(value, "expected string in println statement");
    }
    
    expect_token(TOK_CPAREN, "println statement");
    expect_token(TOK_SEMICOLON, "println statement");
    
    return new_println_stmt(value);
}

ParseFunc* parse_function() {
    Token *func_name = get_next_tok(&lexer);
    if (func_name->type != TOK_ID) {
        error_tok(func_name, "function name expected after fn");
    }
    
    printf("Parsing function: %s\n", func_name->str);
    
    ParseFunc *func = malloc(sizeof(ParseFunc));
    func->name = strdup(func_name->str);
    func->return_type = NULL;
    func->stmt_list = NULL;
    
    expect_token(TOK_OPAREN, "function declaration");
    expect_token(TOK_CPAREN, "function declaration");
    expect_token(TOK_OCURLY, "function declaration");
    
    Token *peek = get_next_tok(&lexer);
    while (peek->type != TOK_EOF && peek->type != TOK_CCURLY) {
        if (peek->type == TOK_PRINT) {
            Stmt *stmt = parse_print_stmt();
            add_stmt(func, stmt);
        } else if (peek->type == TOK_PRINTLN) {
            Stmt *stmt = parse_println_stmt();
            add_stmt(func, stmt);
        } else {
            error_tok(peek, "unexpected token in function body");
        }
        
        peek = get_next_tok(&lexer);
    }
    
    if (peek->type != TOK_CCURLY) {
        error_tok(peek, "expected '}' at end of function");
    }

    return func;
}

void execute_function(ParseFunc *func) {
    printf("Executing function: %s\n", func->name);
    Stmt *curr = func->stmt_list;
    while (curr != NULL) {
        switch (curr->type) {
        case STMT_PRINT:
            if (curr->data.print_stmt.value && curr->data.print_stmt.value->str) {
                printf("%s", curr->data.print_stmt.value->str);
            }
            break;
        case STMT_PRINTLN:
            if (curr->data.println_stmt.value && curr->data.println_stmt.value->str) {
                printf("%s\n", curr->data.println_stmt.value->str);
            }
            break;
        default:
            fprintf(stderr, "Error: unknown statement type\n");
            break;
        }
        curr = curr->next;
    }
}

void parse(Token *t) {
    switch (t->type) {
    case TOK_FN:
        {
            ParseFunc *func = parse_function();
            execute_function(func);
            
            // Clean up
            free(func->name);
            // TODO: free statement list
            free(func);
        }
        break;
    default:
        break;
    }
}

int main() {
    const char *source = "fn hello() { println(\"foo\"); println(\"bar\"); }";
    lexer_init(&lexer, source, "example.lang");
    
    Token *tok = get_next_tok(&lexer);
    while (tok->type != TOK_EOF) {
        parse(tok);
        tok = get_next_tok(&lexer);
    }
    return 0;
}