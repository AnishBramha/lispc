#include "./compiler.h"
#include "./common.h"
#include "./tokeniser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define STD_ARRAY
#include "./std/array.h"
#define STD_TABLE
#include "./std/table.h"


Symbol* var_namespace = NULL;
Symbol* func_namespace = NULL;

static size_t reg_idx = 0;
// static size_t global_offset = 0;


void compile(FILE* src, FILE* ir) {

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

        case PRINT:   return unsafe_compile_print(ir, node);
        case NEWLINE: return unsafe_compile_newline(ir);

        case DEFVAR:  return unsafe_compile_defvar(ir, node);
        case DEFUN:   return unsafe_compile_defun(ir, node);

        default:      return NULL;
    }
}




char* unsafe_compile_atom(ASTNode* node) {

    if (!node || !node->current)
        return NULL;

    if (node->current->token == STRING) { // add stripped quotes back on to string literals

        size_t len = strlen(node->current->lexeme);
        char* str = (char*)malloc(MAX);
        
        size_t j = 0;
        str[j++] = '\"'; 
        
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

    return strdup(node->current->lexeme);
}




char* unsafe_compile_arithmetic(FILE* ir, ASTNode* node, const char* op) {

    switch(arr_len(node->children)) {

        case 1: // (op)

            if (!strncmp(op, "ADD", MAX) || !strncmp(op, "SUB", MAX) || !strncmp(op, "REM", MAX))
                return strdup("0");

            if (!strncmp(op, "MUL", MAX) || !strncmp(op, "DIV", MAX) || !strncmp(op, "POW", MAX))
                return strdup("1");

        case 2:

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

            fprintf(ir, "%s = %s\n", node->children[1]->current->lexeme, val);
            free(val);

            return NULL;


        default:

            fprintf(stderr, "COMPILATION ERROR: Too many arguments for definition on line %zu\n", node->children[0]->current->line);
            freeAST(true, node);
            exit(EX_DATAERR);
    }
}


char* unsafe_compile_defun(FILE* ir, ASTNode* node) {

    (void)ir;
    (void)node;

    // TODO
    return NULL;
}





