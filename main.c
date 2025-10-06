#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h> 
#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <llvm-c/Analysis.h>

typedef enum {
    TOK_FN,
    TOK_PRINT,
    TOK_PRINTLN,
    TOK_INT,
    TOK_ID,
    TOK_OPAREN,
    TOK_CPAREN,
    TOK_SEMICOLON,
    TOK_OCURLY,
    TOK_CCURLY,
    TOK_STRING,
    TOK_EMAIL,
    TOK_RETURN,
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
    char *return_thing;
    Stmt *stmt_list;
} ParseFunc;

typedef struct {
    LLVMModuleRef module;
    LLVMBuilderRef builder;
    LLVMContextRef context;
} CodeGenerator;

Token token = {0};
Lexer lexer = {0};
char *funcs[2048];
int func_index = 0;
int line_lex = 0;
CodeGenerator *codegen = NULL; 

Token* get_next_tok(Lexer *l);
Token* new_token(TokenType type, char *str, SourceLocation loc);
void parse(Token *t);

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
        case '@':
            l->index++;
            l->column++;
            return new_token(TOK_EMAIL, NULL, loc);
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
                } else if (strcmp(str, "return") == 0) {
                    return new_token(TOK_RETURN, NULL, loc);
                } else {
                    return new_token(TOK_ID, strdup(str), loc);
                }
            } else if (isdigit(c)) {
                char str[1024];
                size_t j = 0;
    
                while (isdigit(l->cur_char[l->index]) && j < sizeof(str) - 1) {
                    str[j++] = l->cur_char[l->index++];
                    l->column++;
                }
                str[j] = '\0';
    
                return new_token(TOK_INT, strdup(str), loc);
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
    case TOK_EMAIL:
        return "function return";
    case TOK_INT:
        return "integer";
    case TOK_RETURN:
        return "return";
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
    
    if (func_index < 100) {
        funcs[func_index++] = strdup(func_name->str);
    } else {
        error_tok(func_name, "too many functions (max 100)");
    }
    
    ParseFunc *func = malloc(sizeof(ParseFunc));
    func->name = strdup(func_name->str);
    func->return_type = NULL;
    func->stmt_list = NULL;
    
    expect_token(TOK_OPAREN, "function declaration");
    expect_token(TOK_CPAREN, "function declaration");
    expect_token(TOK_EMAIL, "function declaration");
    
    Token *func_ret = get_next_tok(&lexer);
    if (func_ret->type != TOK_ID) {
        error_at(func_ret->loc, "expected a valid return type");
    }
    
    if (strcmp(func_ret->str, "int") == 0) {
        func->return_type = strdup("int");
    } else if (strcmp(func_ret->str, "void") == 0) {
        func->return_type = strdup("void");
    } else {
        error_at(func_ret->loc, "unknown return type '%s'", func_ret->str);
    }
    
    expect_token(TOK_OCURLY, "function declaration");
    
    Token *peek = get_next_tok(&lexer);
    while (peek->type != TOK_EOF && peek->type != TOK_CCURLY) {
        if (peek->type == TOK_PRINT) {
            Stmt *stmt = parse_print_stmt();
            add_stmt(func, stmt);
        } else if (peek->type == TOK_PRINTLN) {
            Stmt *stmt = parse_println_stmt();
            add_stmt(func, stmt);
        } else if (peek->type == TOK_RETURN) {
            Token *return_val = get_next_tok(&lexer);
    
            if (func->return_type && strcmp(func->return_type, "int") == 0) {
                if (return_val->type != TOK_INT) {
                    error_at(return_val->loc, "expected an integer for a func that returns an int");
                }
                func->return_thing = strdup(return_val->str);
            } else if (func->return_type && strcmp(func->return_type, "void") == 0) {
                if (return_val->type != TOK_ID) {
                    error_at(return_val->loc, "expected to return void a func that returns void");
                } else {
                    if (!(strcmp(return_val->str, "void") == 0)) {
                        error_at(return_val->loc, "expected a to return void for a func that returns void");
                    }
                }
            }
    
            Token *semi = get_next_tok(&lexer);

            if (semi->type != TOK_SEMICOLON) {
                error_tok(semi, "expected semicolon after return statement");
            }
    
            break;
        }
         else {
            error_tok(peek, "unexpected token in function body");
        }
    
        peek = get_next_tok(&lexer);
    }

    peek = get_next_tok(&lexer);
    if (peek->type != TOK_CCURLY) {
        error_tok(peek, "expected a '}' at the end of the func");
    }

    return func;
}

// Initialize code generator
CodeGenerator* codegen_init(const char *module_name) {
    CodeGenerator *gen = malloc(sizeof(CodeGenerator));
    
    gen->context = LLVMGetGlobalContext();
    gen->module = LLVMModuleCreateWithName(module_name);
    gen->builder = LLVMCreateBuilder();
    
    return gen;
}

// Generate LLVM IR for a print statement
void codegen_print_stmt(CodeGenerator *gen, Token *value) {
    LLVMValueRef printf_func = LLVMGetNamedFunction(gen->module, "printf");
    if (!printf_func) {
        LLVMTypeRef printf_type = LLVMFunctionType(
            LLVMInt32Type(),
            (LLVMTypeRef[]){LLVMPointerType(LLVMInt8Type(), 0)},
            1,
            1
        );
        printf_func = LLVMAddFunction(gen->module, "printf", printf_type);
    }
    
    LLVMValueRef str_val = LLVMBuildGlobalStringPtr(gen->builder, value->str, ".str");
    
    LLVMValueRef args[] = {str_val};
    LLVMBuildCall2(
        gen->builder,
        LLVMFunctionType(LLVMInt32Type(), (LLVMTypeRef[]){LLVMPointerType(LLVMInt8Type(), 0)}, 1, 1),
        printf_func,
        args,
        1,
        ""
    );
}

// Generate LLVM IR for a println statement
void codegen_println_stmt(CodeGenerator *gen, Token *value) {
    LLVMValueRef puts_func = LLVMGetNamedFunction(gen->module, "puts");
    if (!puts_func) {
        LLVMTypeRef puts_type = LLVMFunctionType(
            LLVMInt32Type(),
            (LLVMTypeRef[]){LLVMPointerType(LLVMInt8Type(), 0)},
            1,
            0
        );
        puts_func = LLVMAddFunction(gen->module, "puts", puts_type);
    }
    
    LLVMValueRef str_val = LLVMBuildGlobalStringPtr(gen->builder, value->str, ".str");
    
    LLVMValueRef args[] = {str_val};
    LLVMBuildCall2(
        gen->builder,
        LLVMFunctionType(LLVMInt32Type(), (LLVMTypeRef[]){LLVMPointerType(LLVMInt8Type(), 0)}, 1, 0),
        puts_func,
        args,
        1,
        ""
    );
}

// Generate LLVM IR for a statement
void codegen_statement(CodeGenerator *gen, Stmt *stmt) {
    switch (stmt->type) {
    case STMT_PRINT:
        codegen_print_stmt(gen, stmt->data.print_stmt.value);
        break;
    case STMT_PRINTLN:
        codegen_println_stmt(gen, stmt->data.println_stmt.value);
        break;
    default:
        fprintf(stderr, "Unknown statement type in codegen\n");
        break;
    }
}

// Generate LLVM IR for a function
LLVMValueRef codegen_function(CodeGenerator *gen, ParseFunc *func) {
    LLVMTypeRef func_type = NULL;
    if (strcmp(func->return_type, "int") == 0) {
        func_type = LLVMFunctionType(LLVMInt32Type(), NULL, 0, 0);
    } else if (strcmp(func->return_type, "void") == 0) {
        func_type = LLVMFunctionType(LLVMVoidType(), NULL, 0, 0);
    }
    LLVMValueRef llvm_func = LLVMAddFunction(gen->module, func->name, func_type);
    
    LLVMBasicBlockRef entry = LLVMAppendBasicBlock(llvm_func, "entry");
    LLVMPositionBuilderAtEnd(gen->builder, entry);
    
    Stmt *stmt = func->stmt_list;
    while (stmt) {
        codegen_statement(gen, stmt);
        stmt = stmt->next;
    }
    if (strcmp(func->return_type, "int") == 0) {
        LLVMValueRef z = LLVMConstInt(LLVMInt32Type(), atoi(func->return_thing), 0);
        LLVMBuildRet(gen->builder, z);
    } else if (strcmp(func->return_type, "void") == 0) {
        LLVMBuildRetVoid(gen->builder);
    }
    
    if (LLVMVerifyFunction(llvm_func, LLVMPrintMessageAction)) {
        fprintf(stderr, "Function verification failed\n");
    }
    
    return llvm_func;
}

// Dump LLVM IR to stdout
void codegen_dump(CodeGenerator *gen) {
    printf("=== LLVM DUMP ===");
    char *ir = LLVMPrintModuleToString(gen->module);
    printf("%s\n", ir);
    LLVMDisposeMessage(ir);
}

int check_if_main() {
    for (int i = 0; i < func_index; i++) {
        if (strcmp(funcs[i], "main") == 0) {
            return 1;
        }
    }
    return 0;
}

// Write LLVM IR to file
void codegen_write_to_file(CodeGenerator *gen, const char *filename) {
    if (!check_if_main()) {
        fprintf(stderr, "There is no main function in the source code");
        exit(1);
    }
    char *error = NULL;
    if (LLVMPrintModuleToFile(gen->module, filename, &error)) {
        fprintf(stderr, "Error writing to file: %s\n", error);
        LLVMDisposeMessage(error);
    } 
}

// Cleanup
void codegen_cleanup(CodeGenerator *gen) {
    LLVMDisposeBuilder(gen->builder); 
    LLVMDisposeModule(gen->module);
    free(gen);
}

void parse(Token *t) {
    switch (t->type) {
    case TOK_FN:
        {
            ParseFunc *func = parse_function();
            
            codegen_function(codegen, func);
            
            free(func->name);
            free(func);
        }
        break;
    default:
        break;
    }
}

void usage() {
    printf("USAGE: [file_name]");
    exit(1);
}

void strip_extension(char *filename) {
    char *dot = strrchr(filename, '.');
    if (dot != NULL) {
        *dot = '\0'; 
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Not enough args\n");
        usage();
    } else if (argc > 2) {
        fprintf(stderr, "Passed too many args\n");
        usage();
    } 
    
    char *filename = argv[1];
    FILE *f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "Error: could not open file '%s'\n", filename);
        exit(1);
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    char *source = malloc(size + 1);
    if (!source) {
        fprintf(stderr, "Memory allocation failed\n");
        fclose(f);
        exit(1);
    }
    
    size_t read_bytes = fread(source, 1, size, f);
    source[read_bytes] = '\0';
    fclose(f);
    
    char *ll_file_name = malloc(strlen(filename) + 4);
    if (!ll_file_name) {
        fprintf(stderr, "Memory allocation failed\n");
        free(source);
        exit(1);
    }
    strcpy(ll_file_name, filename);
    
    char *dot = strrchr(ll_file_name, '.');
    if (dot != NULL) {
        *dot = '\0';
    }
    strcat(ll_file_name, ".ll");

    codegen = codegen_init("dystopia_module");
    lexer_init(&lexer, source, filename);
    
    Token *tok = get_next_tok(&lexer);
    while (tok->type != TOK_EOF) {
        parse(tok);
        tok = get_next_tok(&lexer);
    }
    
    codegen_write_to_file(codegen, ll_file_name);
    
    codegen_cleanup(codegen);
    free(source);
    free(ll_file_name);
    
    return 0;
}