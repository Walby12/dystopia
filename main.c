#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>


typedef enum {
    TOK_FN,
    TOK_ID,
    TOK_OPAREN,
    TOK_CPAREN,
    TOK_SEMICOLON,
    TOK_OCURLY,
    TOK_CCURLY,
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

typedef enum {
    STMT_PRINT,
    // Easy to add more later:
    // STMT_RETURN,
    // STMT_ASSIGN,
    // STMT_IF,
    // STMT_WHILE,
    // etc.
} StmtType;

typedef struct Stmt {
    StmtType type;
    union {
        struct {
            Token *value;  // What to print
        } print_stmt;
        
        // Add new statement types here as you need them:
        // struct {
        //     Token *expr;
        // } return_stmt;
        // 
        // struct {
        //     char *var_name;
        //     Token *value;
        // } assign_stmt;
    } data;
    struct Stmt *next;  // For linking multiple statements
} Stmt;

typedef struct {
    char *name;
    char *return_type;
    Stmt *stmt_list;  // Head of statement linked list
} ParseFunc;


int line_lex = 0;

Token* new_fn() {
    Token *t = malloc(sizeof(Token));
    t->type = TOK_FN;
    t->str = NULL;
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
    t->str = NULL;
    return t;
}

Token* new_cparen() {
    Token *t = malloc(sizeof(Token));
    t->type = TOK_CPAREN;
    t->str = NULL;
    return t;
}

Token* new_semiclon() {
    Token *t = malloc(sizeof(Token));
    t->type = TOK_SEMICOLON;
    t->str = NULL;
    return t;
}

Token* new_ocurly() {
    Token *t = malloc(sizeof(Token));
    t->type = TOK_OCURLY;
    t->str = NULL;
    return t;
}

Token* new_ccurly() {
    Token *t = malloc(sizeof(Token));
    t->type = TOK_CCURLY;
    t->str = NULL;
    return t;
}

Token* new_eof() {
    Token *t = malloc(sizeof(Token));
    t->type = TOK_EOF;
    t->str = NULL;
    return t;
}

Token* get_next_tok(Lexer *l) {
    while (1) {  // Loop to skip whitespace
        switch (l->cur_char[l->index]) {
        case '(':
            l->index++;
            return new_oparen();
        case ')':
            l->index++;
            return new_cparen();
        case ';':
            l->index++;
            return new_semiclon();
        case '{':
            l->index++;
            return new_ocurly();
        case '}':
            l->index++;
            return new_ccurly();
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
                if (strcmp(str, "fn") == 0) {
                    return new_fn();
                } else {
                    return new_id(strdup(str));
                }
            }
            fprintf(stderr, "Error: unknown char: %c at line %d\n", l->cur_char[l->index], line_lex);
            exit(1);
        }
    }
}


// Helper function to create a print statement
Stmt* new_print_stmt(Token *value) {
    Stmt *s = malloc(sizeof(Stmt));
    s->type = STMT_PRINT;
    s->data.print_stmt.value = value;
    s->next = NULL;
    return s;
}

// Add a statement to the end of a function's statement list
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
    case TOK_EOF:
        return "end of file";
    default:
        return "unknown token";
    }
}

// Expect a specific token type, error if not found
Token* expect_token(TokenType expected, const char *context) {
    Token *tok = get_next_tok(&lexer);
    if (tok->type != expected) {
        fprintf(stderr, "Error: expected token type %s in %s, got %s at line %d\n", 
                to_string(expected), context, to_string(tok->type), line_lex);
        exit(1);
    }
    return tok;
}

// Parse a print statement: print(value)
Stmt* parse_print_stmt() {
    // We've already consumed the "print" identifier
    expect_token(TOK_OPAREN, "print statement");
    
    Token *value = get_next_tok(&lexer);
    if (value->type != TOK_ID) {
        fprintf(stderr, "Error: expected identifier in print statement at line %d\n", line_lex);
        exit(1);
    }
    
    expect_token(TOK_CPAREN, "print statement");
    expect_token(TOK_SEMICOLON, "print statement");
    
    return new_print_stmt(value);
}

// Parse a statement (currently only print, but expandable)
Stmt* parse_stmt() {
    Token *tok = get_next_tok(&lexer);
    
    if (tok->type == TOK_ID) {
        if (strcmp(tok->str, "print") == 0) {
            return parse_print_stmt();
        }
        // Add more statement types here:
        // else if (strcmp(tok->str, "return") == 0) {
        //     return parse_return_stmt();
        // }
        // else if (strcmp(tok->str, "if") == 0) {
        //     return parse_if_stmt();
        // }
        else {
            fprintf(stderr, "Error: unknown statement '%s' at line %d\n", tok->str, line_lex);
            exit(1);
        }
    }
    
    fprintf(stderr, "Error: expected statement at line %d\n", line_lex);
    exit(1);
}

// Parse a function: fn name() { statements }
ParseFunc* parse_function() {
    // Expect function name
    Token *func_name = get_next_tok(&lexer);
    if (func_name->type != TOK_ID) {
        fprintf(stderr, "Error: function name expected after fn at line %d\n", line_lex);
        exit(1);
    }

    
    printf("Parsing function: %s\n", func_name->str);
    
    // Create the function structure
    ParseFunc *func = malloc(sizeof(ParseFunc));
    func->name = strdup(func_name->str);
    func->return_type = NULL;  // Can be extended later
    func->stmt_list = NULL;
    
    // Expect opening parenthesis
    expect_token(TOK_OPAREN, "function declaration");
    
    // Expect closing parenthesis (no parameters for now)
    expect_token(TOK_CPAREN, "function declaration");
    expect_token(TOK_OCURLY, "function declaration");
    
    // Parse function body - for now, just parse statements until EOF
    // You can add braces { } support later
    Token *peek = get_next_tok(&lexer);
    while (peek->type != TOK_EOF) {
        // Put the token back conceptually (or refactor to have a peek function)
        // For now, we'll handle this differently
        if (peek->type == TOK_ID && strcmp(peek->str, "print") == 0) {
            Stmt *stmt = parse_print_stmt();
            add_stmt(func, stmt);
        } else if (peek->type == TOK_CCURLY) {
            return func;
        } else {
            fprintf(stderr, "Error: unexpected token in function body at line %d\n", line_lex);
            exit(1);
        }
        
        peek = get_next_tok(&lexer);
    }
    expect_token(TOK_CCURLY, "function declaration");

    return func;
}

// Execute/interpret all statements in a function
void execute_function(ParseFunc *func) {
    printf("Executing function: %s\n", func->name);
    Stmt *curr = func->stmt_list;
    while (curr != NULL) {
        switch (curr->type) {
        case STMT_PRINT:
            if (curr->data.print_stmt.value && curr->data.print_stmt.value->str) {
                printf(">> %s\n", curr->data.print_stmt.value->str);
            }
            break;
        default:
            fprintf(stderr, "Error: unknown statement type\n");
            break;
        }
        curr = curr->next;
    }
}

// Updated main parse function
void parse(Token *t) {
    switch (t->type) {
    case TOK_FN:
        {
            ParseFunc *func = parse_function();
            execute_function(func);
            
            // Clean up (you might want to keep functions around)
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
    lexer.cur_char = "fn hello() { print(foo); print(bar); }";
    Token *tok = get_next_tok(&lexer);
    while (tok->type != TOK_EOF) {
        printf("%d\n", tok->type);
        parse(tok);
        tok = get_next_tok(&lexer);
    }
    return 0;
}