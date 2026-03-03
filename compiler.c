#include "./compiler.h"
#include "./common.h"
#include "./tokeniser.h"
#include <stdio.h>
#include <string.h>


#define STD_ARRAY
#include "./std/array.h"
#define STD_TABLE
#include "./std/table.h"


Symbol* var_namespace = NULL;
Symbol* func_namespace = NULL;

static size_t reg_idx = 0;
static size_t global_offset = 0;


void compile(FILE* src, FILE* ir) {

    size_t line = 1;

    loop {

        TokenInfo* tokenInfo = unsafe_get(src, line);

        if (tokenInfo->token == END_OF_FILE) {

            free(tokenInfo);
            break;
        }

        ASTNode* node = unsafe_build(src, tokenInfo);

        char* reg = unsafe_compile_list(ir, node);
        
        if (reg)
            free(reg);

        freeAST(false, node);

        line = tokenInfo->line;
        reg_idx = 0;

        fputc('\n', ir);
    }
}


char* unsafe_compile_list(FILE* ir, ASTNode* node) {

    /* TODO:
    * if atom, compile atom
    * if list, compile each element as a list
    * if each element is an atom, compile as atom, else recurse */

    if (!node->children) // atom
        return unsafe_compile_atom(ir, node);

    char* reg = NULL;
    size_t mem_offset;
    // list
    switch (node->children[0]->current->token) { // first element of a list denotes op

        case PLUS:    return unsafe_compile_arithmetic(ir, node, "ADD");
        case MINUS:   return unsafe_compile_arithmetic(ir, node, "SUB");
        case STAR:    return unsafe_compile_arithmetic(ir, node, "MUL");
        case SLASH:   return unsafe_compile_arithmetic(ir, node, "DIV");
        case PERCENT: return unsafe_compile_arithmetic(ir, node, "REM");
        case CARET:   return unsafe_compile_arithmetic(ir, node, "POW");

        case PRINT:   return unsafe_compile_print(ir, node);
        case NEWLINE: return unsafe_compile_newline(ir);

        case DEFVAR: {

            char* name = node->children[1]->current->lexeme;
            char* reg = unsafe_compile_list(ir, node->children[2]);

            if (!reg) {

                fprintf(stderr, "COMPILATION ERROR: Illegal r-value is assigned to l-value `%s` on line %zu\n", name, node->children[1]->current->line);
                exit(EX_DATAERR); // MEMORY LEAK!!
            }

            idx_t idx = table_get(var_namespace, name);

            if (idx == -1) { // new var 

                mem_offset = global_offset++;
                table_put(var_namespace, name, mem_offset);

            } else
                mem_offset = var_namespace[idx].offset;

            fprintf(ir, "%s = %s\n", name, reg);
            free(reg);
            return strdup(name);
         }

        case DEFUN: // TODO: need scopes
        
        case IDENTIFIER: // TODO: handle function calls or vars

        default:
            fprintf(stderr, "COMPILATION ERROR: Illegal token `%s` found on line `%zu`\n", node->children[0]->current->lexeme, node->children[0]->current->line);
            exit(EX_DATAERR); // MEMORY LEAK!!
    }

    return NULL;
}


static char* unsafe_temp_reg(void) {

    char* reg = (char*)malloc(sizeof(char) * MAX);
    snprintf(reg, MAX, "%%t%zu", reg_idx++);

    return reg;
}


char* unsafe_compile_arithmetic(FILE* ir, ASTNode* node, const char* op) {

    char* acc = NULL;
    size_t len = arr_len(node->children);
    switch (len - 1) {

        case 0:
            
            if (!strncmp(op, "ADD", MAX) || !strncmp(op, "SUB", MAX) || !strncmp(op, "REM", MAX))
                return strdup("0");

            else if (!strncmp(op, "MUL", MAX) || !strncmp(op, "DIV", MAX) || !strncmp(op, "POW", MAX))
                return strdup("1");

        case 1:

            if (!strncmp(op, "SUB", MAX)) {

                char* arg = unsafe_compile_list(ir, node->children[1]);
                char* reg = unsafe_temp_reg();
                fprintf(ir, "%s = SUB 0 %s\n", reg, arg);

                free(arg);
                return reg;
            }

            if (!strncmp(op, "DIV", MAX)) {

                char* arg = unsafe_compile_list(ir, node->children[1]);
                char* reg = unsafe_temp_reg();
                fprintf(ir, "%s = DIV 1 %s\n", reg, arg);

                free(arg);
                return reg;
            }

            return unsafe_compile_list(ir, node->children[1]); // return expression itself

        default:

            acc = unsafe_compile_list(ir, node->children[1]);

            size_t len = arr_len(node->children);
            for (int i = 2; i <= len - 1; i++) {

                char* arg = unsafe_compile_list(ir, node->children[i]);
                char* reg = unsafe_temp_reg();

                fprintf(ir, "%s = %s %s %s\n", reg, op, acc, arg);

                free(arg);
                free(acc);
                acc = reg;
            }

            return acc;
    }

    return NULL;
}


char* unsafe_compile_print(FILE* ir, ASTNode* node) {

    size_t len = arr_len(node->children);
    for (int i = 1; i <= len - 1; i++) {

        char* arg = unsafe_compile_list(ir, node->children[i]);

        if (!arg)
            return NULL;

        fprintf(ir, "PRINT %s\n", arg);
        free(arg);
    }

    return NULL;
}

char* unsafe_compile_newline(FILE* ir) {

    fputs("NEWLINE\n", ir);
    
    return NULL;
}


char* unsafe_compile_atom(FILE* ir, ASTNode* node) {

    if (node->current->token == INT || node->current->token == FLOAT)
        return strdup(node->current->lexeme);

    if (node->current->token == STRING) {

        char* str = (char*)malloc(sizeof(char) * MAX);
        size_t j = 0;
        
        str[j++] = '\"';

        char* lex = node->current->lexeme;
        size_t len = strlen(lex);

        for (size_t i = 0; i < len && j < MAX - 3; i++) {
            
            if ((i == 0 || i == len - 1) && lex[i] == '\"') {
                continue;
            }

            switch (lex[i]) {
                case '\n': str[j++] = '\\'; str[j++] = 'n'; break;
                case '\r': str[j++] = '\\'; str[j++] = 'r'; break;
                case '\t': str[j++] = '\\'; str[j++] = 't'; break;
                case '\\': str[j++] = '\\'; str[j++] = '\\'; break;
                case '\"': str[j++] = '\\'; str[j++] = '\"'; break;

                default:
                   str[j++] = lex[i];
                   break;
            }
        }

        str[j++] = '\"';
        str[j] = NIL;
        
        return str;
    }

    if (node->current->token == IDENTIFIER) {

        char* reg = unsafe_temp_reg();
        idx_t idx = table_get(var_namespace, node->current->lexeme);

        if (idx == -1) {

            fprintf(stderr, "COMPILATION ERROR: Undefined variable `%s` on line `%zu`\n", node->current->lexeme, node->current->line);
            exit(EX_DATAERR); // MEMORY LEAK!!
        }

        fprintf(ir, "%s = %s\n", reg, node->current->lexeme);

        return reg;
    }

    return NULL;
}





