#include "../compiler/compiler.h"
#include "../common.h"
#include "../lexer/lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define STD_ARRAY
#include "../../std/array.h"
#define STD_TABLE
#include "../../std/table.h"


Symbol* var_namespace = NULL;
Symbol* func_namespace = NULL;

static size_t reg_idx = 0;

static size_t scope_depth = 0;
static size_t stack_offset = 0;

static size_t else_idx = 0;


static inline void enter_scope(void) {

    scope_depth++;
}


static inline void exit_scope(size_t sibling_offset) {

    scope_depth--;

    for (int i = arr_len(var_namespace) - 1; i >= 0; i--) {

        if (var_namespace[i].depth <= scope_depth)
            break;

        var_namespace[i].sym_type = DEAD;
    }

    stack_offset = sibling_offset;
}


static inline size_t push_local_var(const char* name, DataType type) {

    stack_offset += 16;

    Symbol var = {

        .name = strdup(name),
        .offset = stack_offset,
        .sym_type = LOCAL_VAR,
        .dat_type = type,
        .depth = scope_depth,
        .arity = 0,
    };

    arr_push(var_namespace, var);
    return stack_offset;
}



void compilef(FILE* src, FILE* ir) {

    size_t line = 1;

    loop {

        TokenInfo* tokenInfo = unsafe_get(src, line);

        if (tokenInfo->token == END_OF_FILE) {

            free(tokenInfo);
            break;
        }

        ASTNode* node = unsafe_build(src, tokenInfo);

        char* _ = unsafe_compile_node(ir, node);
        if (_)
            free(_);
        
        freeAST(false, node);

        line = tokenInfo->line;
        reg_idx = 0;

        fputc('\n', ir);
    }
}


char* unsafe_compile_node(FILE* ir, ASTNode* node) {

    if (!node) // everything in lisp
        return NULL;

    if (node->children) // is either a list
        return unsafe_compile_list(ir, node);

    return unsafe_compile_atom(node); // or an atom
}



static char* unsafe_temp_reg(void) {

    char* reg = (char*)malloc(sizeof(char) * MAX);
    snprintf(reg, MAX, "%%t%zu", reg_idx++);

    return reg;
}



char* unsafe_compile_list(FILE* ir, ASTNode* node) {

    if (!node || !node->current || !node->children)
        return NULL;

    // block (nested list)
    if (node->children[0]->children || node->children[0]->current->token == LEFT_PAREN) {
        
        char* val = NULL;
        
        for (int i = 0; i < arr_len(node->children); i++) {
            
            if (val)
                free(val);
            
            val = unsafe_compile_node(ir, node->children[i]); 
        }
        
        return val; 
    }

    switch (node->children[0]->current->token) {

        case PLUS:    return unsafe_compile_arithmetic(ir, node, "ADD");
        case MINUS:   return unsafe_compile_arithmetic(ir, node, "SUB");
        case STAR:    return unsafe_compile_arithmetic(ir, node, "MUL");
        case SLASH:   return unsafe_compile_arithmetic(ir, node, "DIV");
        case PERCENT: return unsafe_compile_arithmetic(ir, node, "REM");
        case CARET:   return unsafe_compile_arithmetic(ir, node, "POW");

        case LESS:          return unsafe_compile_arithmetic(ir, node, "LESS");
        case LESS_EQUAL:    return unsafe_compile_arithmetic(ir, node, "LESS_EQUAL");
        case GREATER:       return unsafe_compile_arithmetic(ir, node, "GREATER");
        case GREATER_EQUAL: return unsafe_compile_arithmetic(ir, node, "GREATER_EQUAL");
        case EQL:           return unsafe_compile_arithmetic(ir, node, "EQL");
        case NOT_EQUAL:     return unsafe_compile_arithmetic(ir, node, "NOT_EQUAL");

        case AND: return unsafe_compile_arithmetic(ir, node, "AND");
        case OR:  return unsafe_compile_arithmetic(ir, node, "OR");
        case NOT: return unsafe_compile_arithmetic(ir, node, "NOT");

        case IF: return unsafe_compile_if(ir, node);

        case PRINT:   return unsafe_compile_print(ir, node);
        case NEWLINE: return unsafe_compile_newline(ir);
        case CONCAT:  return unsafe_compile_concat(ir, node);
        case PANIC:   return unsafe_compile_panic(ir, node);
        case ERROR:   return unsafe_compile_error(ir, node);

        case DEFVAR: return unsafe_compile_defvar(ir, node);
        case DEFUN:  return unsafe_compile_defun(ir, node);
        case LET:    return unsafe_compile_let(ir, node);

        case IDENTIFIER: return unsafe_compile_call(ir, node);

        default: return NULL;
    }
}


char* unsafe_compile_atom(ASTNode* node) {

    if (!node || !node->current)
        return NULL;

    if (node->current->token == STRING)
        return unsafe_compile_string(node);

    if (node->current->token == IDENTIFIER)
        return unsafe_compile_symbol(node);

    return strdup(node->current->lexeme);
}




char* unsafe_compile_arithmetic(FILE* ir, ASTNode* node, const char* op) {

    if (!strncmp(op, "LESS", MAX) || !strncmp(op, "LESS_EQUAL", MAX) || !strncmp(op, "GREATER", MAX) ||
        !strncmp(op, "GREATER_EQUAL", MAX) || !strncmp(op, "EQL", MAX) || !strncmp(op, "NOT_EQUAL", MAX))
        return unsafe_fold_comparison(ir, node, op);

    switch(arr_len(node->children)) {

        case 1: // (op)

            if (!strncmp(op, "AND", MAX))
                return strdup("#t");

            if (!strncmp(op, "OR", MAX))
                return strdup("#f");

            if (!strncmp(op, "ADD", MAX) || !strncmp(op, "SUB", MAX) || !strncmp(op, "REM", MAX))
                return strdup("0");

            if (!strncmp(op, "MUL", MAX) || !strncmp(op, "DIV", MAX) || !strncmp(op, "POW", MAX))
                return strdup("1");

        case 2:

            if (!strncmp(op, "NOT", MAX)) {

                char* reg = unsafe_temp_reg();
                char* clause = unsafe_compile_node(ir, node->children[1]);
                fprintf(ir, "%s = NOT %s\n", reg, clause);

                return reg;
            }

            if (!strncmp(op, "SUB", MAX)) { // unary minus

                char* reg = unsafe_compile_node(ir, node->children[1]);
                char* res = unsafe_temp_reg();
                fprintf(ir, "%s = SUB 0 %s\n", res, reg);
                return res;

            }

            if (!strncmp(op, "DIV", MAX)) { // inverse

                char* reg = unsafe_compile_node(ir, node->children[1]);
                char* res = unsafe_temp_reg();
                fprintf(ir, "%s = DIV 1 %s\n", res, reg);
                return res;
            }

            return unsafe_compile_node(ir, node->children[1]);


        case 3:
            return unsafe_fold_arithmetic(ir, node, op);


        default:

            if (!strncmp(op, "POW", MAX)) {

                fprintf(stderr, "COMPILATION ERROR: Power operation on line %zu supports only 2 operands. Nest for more\n", node->children[0]->current->line);
                freeAST(true, node);
                exit(EX_DATAERR);
            }

            return unsafe_fold_arithmetic(ir, node, op);
    }
}


char* unsafe_fold_arithmetic(FILE* ir, ASTNode* node, const char* op) {

    char* acc = unsafe_compile_node(ir, node->children[1]);

    for (int i = 2; i < arr_len(node->children); i++) {

        char* next = unsafe_compile_node(ir, node->children[i]);

        if (!next) { // only errors and statements return null

            fprintf(stderr, "COMPILATION ERROR: Expected expression on line %zu\n", node->children[i]->current->line);
            free(acc);
            freeAST(true, node);
            exit(EX_DATAERR);
        }

        char* res = unsafe_temp_reg();

        fprintf(ir, "%s = %s %s %s\n", res, op, acc, next);

        free(acc);
        acc = strdup(res);

        free(next);
        free(res);
    }

    return acc;
}



char* unsafe_compile_print(FILE* ir, ASTNode* node) {

    for (int i = 1; i < arr_len(node->children); i++) {

        char* arg = unsafe_compile_node(ir, node->children[i]);

        if (arg) {

            fprintf(ir, "PRINT %s\n", arg);
            free(arg);
        }
    }

    return NULL;
}


char* unsafe_compile_newline(FILE* ir) {

    fputs("NEWLINE\n", ir);

    return NULL;
}


char* unsafe_compile_defvar(FILE* ir, ASTNode* node) {

    switch (arr_len(node->children)) {

        case 1:

            fprintf(stderr, "COMPILATION ERROR: Missing l-value in definition on line %zu\n", node->children[0]->current->line);
            freeAST(true, node);
            exit(EX_DATAERR);


        case 2:

            fprintf(stderr, "COMPILATION ERROR: Missing r-value in definition on line %zu\n", node->children[0]->current->line);
            freeAST(true, node);
            exit(EX_DATAERR);


        case 3:

            if (node->children[1]->current->token != IDENTIFIER) {

                fprintf(stderr, "COMPILATION ERROR: Illegal l-value in definition on line %zu\n", node->children[0]->current->line);
                freeAST(true, node);
                exit(EX_DATAERR);
            }

            char* val = unsafe_compile_node(ir, node->children[2]);

            if (!val) {

                fprintf(stderr, "COMPILATION ERROR: Illegal r-value in definition on line %zu\n", node->children[0]->current->line);
                freeAST(true, node);
                exit(EX_DATAERR);
            }

            Symbol var = {

                .name = strdup(node->children[1]->current->lexeme),
                .offset = 0,
                .sym_type = GLOBAL_VAR,
                .dat_type = (val[0] == '\"') ? DAT_STRING : DAT_INT,
                .depth = 0,
                .arity = 0,
            };

            arr_push(var_namespace, var);

            fprintf(ir, "%s = %s\n", node->children[1]->current->lexeme, val);
            free(val);

            return NULL;


        default:

            fprintf(stderr, "COMPILATION ERROR: Too many arguments for definition on line %zu\n", node->children[0]->current->line);
            freeAST(true, node);
            exit(EX_DATAERR);
    }
}


char* unsafe_compile_let(FILE* ir, ASTNode* node) {

    if (arr_len(node->children) < 2) {

        fprintf(stderr, "COMPILATION ERROR: Bindings list missing for `let` on line %zu\n", node->children[0]->current->line);
        freeAST(true, node);
        exit(EX_DATAERR);
    }

    enter_scope();
    size_t frame = stack_offset;

    char* name = NULL;
    char* val = NULL;

    // bindings
    for (size_t i = 0; i < arr_len(node->children[1]->children); i++) {

        // (let ((name val)) ...)
        if (node->children[1]->children[i]->children) {

            if (arr_len(node->children[1]->children[i]->children) != 2 || 
                node->children[1]->children[i]->children[0]->current->token != IDENTIFIER) {

                fprintf(stderr, "COMPILATION ERROR: Illegal `let` binding on line %zu\n", node->children[1]->children[i]->current->line);
                freeAST(true, node);
                exit(EX_DATAERR);
            }

            name = node->children[1]->children[i]->children[0]->current->lexeme;
            val = unsafe_compile_node(ir, node->children[1]->children[i]->children[1]);

        } else { // (let (identifier) ...)

            if (node->children[1]->children[i]->current->token != IDENTIFIER) {

                fprintf(stderr, "COMPILATION ERROR: Illegal l-value in `let` binding on line %zu\n", node->children[1]->current->line);
                freeAST(true, node);
                exit(EX_DATAERR);
            }

            name = node->children[1]->children[i]->current->lexeme;
            val = strdup("0");
        }

        size_t offset = push_local_var(name, (val[0] == '\"') ? DAT_STRING : DAT_INT);
        fprintf(ir, "@%zu = %s\n", offset, val);
        free(val);
    }


    // body

    char* reg = NULL;
    for (size_t i = 2; i < arr_len(node->children); i++) {

        if (reg)
            free(reg);

        reg = unsafe_compile_node(ir, node->children[i]);
    }

    exit_scope(frame);
    return reg;
}


char* unsafe_compile_defun(FILE* ir, ASTNode* node) {

    // (defun name (args) body)
    if (arr_len(node->children) < 4) {

        fprintf(stderr, "COMPILATION ERROR: Illegal function definition on line %zu\n", node->children[0]->current->line);
        freeAST(true, node);
        exit(EX_DATAERR);
    }

    if (node->children[1]->children || node->children[1]->current->token != IDENTIFIER) {

        fprintf(stderr, "COMPILATION ERROR: Illegal function name on line %zu\n", node->children[1]->current->line);
        freeAST(true, node);
        exit(EX_DATAERR);
    }

    // name

    u32 idx;

    if (table_get(func_namespace, node->children[1]->current->lexeme) != -1) {

        fprintf(stderr, "COMPILATION ERROR: Shadowing global functions is not yet supported on line %zu\n", node->children[1]->current->line);
        freeAST(true, node);
        exit(EX_DATAERR);
    }

    table_put(func_namespace, node->children[1]->current->lexeme, 0, idx);
    func_namespace[idx].sym_type = FUNC;
    func_namespace[idx].arity = node->children[2]->children ? arr_len(node->children[2]->children) : 0;

    fprintf(ir, "FUNC _%s\n", node->children[1]->current->lexeme);

    enter_scope();
    size_t frame = stack_offset;
    stack_offset = 0;

    // args

    if (node->children[2]->children) {

        for (size_t i = 0; i < arr_len(node->children[2]->children); i++) {

            if (node->children[2]->children[i]->children || node->children[2]->children[i]->current->token != IDENTIFIER) {

                fprintf(stderr, "COMPILATION ERROR: Illegal argument to funcion `%s` on line %zu\n", node->children[1]->current->lexeme, node->children[2]->children[i]->current->line);
                freeAST(true, node);
                exit(EX_DATAERR);
            }

            fprintf(ir, "PARAM @%zu %zu\n", push_local_var(node->children[2]->children[i]->current->lexeme, DAT_INT), i);
        }
    }

    // body

    char* reg = NULL;
    for (size_t i = 3; i < arr_len(node->children); i++) {

        if (reg)
            free(reg);

        reg = unsafe_compile_node(ir, node->children[i]);
    }
    reg = reg ? reg : strdup("0");

    // epilogue

    fprintf(ir, "RET %s\n", reg);
    fprintf(ir, "END _%s\n", node->children[1]->current->lexeme);

    free(reg);
    exit_scope(frame);

    return NULL;
}


char* unsafe_compile_string(ASTNode* node) {

        size_t len = strlen(node->current->lexeme);
        char* str = (char*)malloc(MAX);
        
        size_t j = 0;
        str[j++] = '\"';  // add stripped quotes back on to string literals
        
        for (size_t i = 0; i < len && j < MAX - 3; i++) { 

            switch (node->current->lexeme[i]) {

                case '\n':

                    str[j++] = '\\';
                    str[j++] = 'n';
                    break;
                    
                case '\t':

                    str[j++] = '\\';
                    str[j++] = 't';
                    break;
                    
                case '\r':

                    str[j++] = '\\';
                    str[j++] = 'r';
                    break;


                case '\'':
                case '\"':
                case '\?':
                case '\\':

                    str[j++] = '\\';
                    str[j++] = node->current->lexeme[i];
                    break;

                default:
                    str[j++] = node->current->lexeme[i];
            }
        }

        str[j++] = '\"'; 
        str[j] = NIL;

        return str;
}



char* unsafe_compile_symbol(ASTNode* node) {

    // boolean
    
    if (!strncmp(node->current->lexeme, "#t", MAX) || !strncmp(node->current->lexeme, "#f", MAX))
        return strdup(node->current->lexeme);

    for (int i = arr_len(var_namespace) - 1; i >= 0; i--) {

        if (var_namespace[i].sym_type != DEAD && !strncmp(var_namespace[i].name, node->current->lexeme, MAX)) {

            char* retrieve_sym = (char*)malloc(MAX);

            if (var_namespace[i].sym_type == LOCAL_VAR)
                snprintf(retrieve_sym, MAX, "@%zu", var_namespace[i].offset);

            else // GLOBAL_VAR
                snprintf(retrieve_sym, MAX, "%s", var_namespace[i].name);

            return retrieve_sym;
        }
    }

    fprintf(stderr, "COMPILATION ERROR: Undefined symbol `%s` on line %zu\n", node->current->lexeme, node->current->line);
    freeAST(true, node);
    exit(EX_DATAERR);
}


char* unsafe_compile_call(FILE* ir, ASTNode* node) {

    u32 idx = table_get(func_namespace, node->children[0]->current->lexeme);
    if (idx == -1) {

        fprintf(stderr, "COMPILATION ERROR: Undefined reference to function `%s` on line %zu\n", node->children[0]->current->lexeme, node->children[0]->current->line);
        freeAST(true, node);
        exit(EX_DATAERR);
    }


    // arity check

    if (arr_len(node->children) - 1 != func_namespace[idx].arity) {

        fprintf(stderr, "COMPILATION ERROR: Function `%s` expects %zu arguments, but got %zu arguments on line %zu\n", node->children[0]->current->lexeme, func_namespace[idx].arity, arr_len(node->children) - 1, node->children[0]->current->line);
        freeAST(true, node);
        exit(EX_DATAERR);
    }

    if (func_namespace[idx].arity > MAX) {

        fprintf(stderr, "COMPILATION ERROR: Function `%s` can accept at most %d arguments, but was defined with %zu arguments on line %zu\n", node->children[0]->current->lexeme, MAX + 8, func_namespace[idx].arity, node->children[0]->current->line);
        freeAST(true, node);
        exit(EX_DATAERR);
    }

    char* args[MAX]; // 1KB argument stack - 1024 arguments
    for (size_t i = 0; i < func_namespace[idx].arity; i++)
        args[i] = unsafe_compile_node(ir, node->children[i + 1]);

    // reverse stack push arguments
    for (int i = func_namespace[idx].arity - 1; i >= 0; i--) {

        fprintf(ir, "ARG %d %s\n", i, args[i]);
        free(args[i]);
    }

    // return value
    char* ret = unsafe_temp_reg();
    fprintf(ir, "%s = CALL _%s %zu\n", ret, node->children[0]->current->lexeme, func_namespace[idx].arity);

    return ret;
}


char* unsafe_fold_comparison(FILE* ir, ASTNode* node, const char* op) {

    if (!strncmp(op, "NOT", MAX)) {

        if (arr_len(node->children) != 2) {

            fprintf(stderr, "COMPILATION ERROR: Expected one operand for `NOT` operation on line %zu\n", node->children[0]->current->line);
            freeAST(true, node);
            exit(EX_DATAERR);
        }

        char* reg = unsafe_temp_reg();
        char* clause = unsafe_compile_node(ir, node->children[1]);
        fprintf(ir, "%s = NOT %s\n", reg, clause);

        return reg;
    }

    if (arr_len(node->children) < 3) {

        fprintf(stderr, "COMPILATION ERROR: Expected at least two operands for `%s` operation on line %zu\n", node->children[0]->current->lexeme, node->children[0]->current->line);
        freeAST(true, node);
        exit(EX_DATAERR);
    }

    char* acc = NULL;
    char* clause1 = unsafe_compile_node(ir, node->children[1]);

    for (size_t i = 2; i < arr_len(node->children); i++) {

        char* clause2 = unsafe_compile_node(ir, node->children[i]);

        char* boolean = unsafe_temp_reg();
        fprintf(ir, "%s = %s %s %s\n", boolean, op, clause1, clause2);

        if (!acc)
            acc = strdup(boolean);

        else {

            char* inter = unsafe_temp_reg();
            fprintf(ir, "%s = AND %s %s\n", inter, acc, boolean);
            free(acc);
            acc = inter;
        }

        free(clause1);
        clause1 = clause2;
        free(boolean);
    }

    free(clause1);
    return acc;
}


char* unsafe_compile_if(FILE* ir, ASTNode* node) {

    if (arr_len(node->children) != 4) {

        fprintf(stderr, "COMPILATION ERROR: Expected `then` and `else` clauses in `if` expression on line %zu\n", node->children[0]->current->line);
        freeAST(true, node);
        exit(EX_DATAERR);
    }

    size_t curr_else_idx = else_idx++;

    char* res = unsafe_temp_reg();

    char* cond = unsafe_compile_node(ir, node->children[1]);
    fprintf(ir, "JMPF %s L_else_%zu\n", cond, curr_else_idx);
    free(cond);

    char* then_clause = unsafe_compile_node(ir, node->children[2]);

    if (then_clause) {

        fprintf(ir, "%s = %s\n", res, then_clause);
        free(then_clause);

    } else
        fprintf(ir, "%s = 0\n", res);

    fprintf(ir, "JMP L_endif%zu\n", curr_else_idx);

    fprintf(ir, "LABEL L_else_%zu\n", curr_else_idx);

    char* else_clause = unsafe_compile_node(ir, node->children[3]);

    if (else_clause) {

        fprintf(ir, "%s = %s\n", res, else_clause);
        free(else_clause);

    } else
        fprintf(ir, "%s = 0\n", res);

    fprintf(ir, "LABEL L_endif%zu\n", curr_else_idx);

    return res;
}


char* unsafe_compile_concat(FILE* ir, ASTNode* node) {

    if (arr_len(node->children) < 2) {

        fprintf(stderr, "COMPILATION ERROR: Not enough arguments for `concat` function on line %zu\n", node->children[0]->current->line);
        freeAST(true, node);
        exit(EX_DATAERR);
    }

    if (arr_len(node->children) == 2) {

        char* str = unsafe_compile_node(ir, node->children[1]);

        if (!str) { // only errors and statements return null

            fprintf(stderr, "COMPILATION ERROR: Expected expression on line %zu\n", node->children[1]->current->line);
            freeAST(true, node);
            exit(EX_DATAERR);
        }

        char* res = unsafe_temp_reg();
        fprintf(ir, "%s = CONCAT %s \"\"\n", res, str);

        return res;
    }

    char* acc = unsafe_compile_node(ir, node->children[1]);

    for (size_t i = 2; i < arr_len(node->children); i++) {

        char* next = unsafe_compile_node(ir, node->children[i]);

        if (!next) { // only errors and statements return null

            fprintf(stderr, "COMPILATION ERROR: Expected expression on line %zu\n", node->children[i]->current->line);
            free(acc);
            freeAST(true, node);
            exit(EX_DATAERR);
        }

        char* res = unsafe_temp_reg();
        fprintf(ir, "%s = CONCAT %s %s\n", res, acc, next);

        free(acc);
        acc = strdup(res);

        free(next);
        free(res);
    }

    return acc;
}


char* unsafe_compile_panic(FILE* ir, ASTNode* node) {

    fprintf(ir, "PRINT \"Panicked on line %zu\\n\"\n", node->children[0]->current->line);
    fputs("PRINT \"Trigger: \"\n", ir);
    free(unsafe_compile_print(ir, node));
    fputs("PANIC\n", ir);

    return NULL;
}


char* unsafe_compile_error(FILE* ir, ASTNode* node) {

    fprintf(ir, "PRINT \"Error on line %zu\\n\"\n", node->children[0]->current->line);
    fputs("PRINT \"Diagnosis: \"\n", ir);
    free(unsafe_compile_print(ir, node));
    fputs("ABORT\n", ir);

    return NULL;
}




  
