#pragma once

#include <stdio.h>

#include "./parser.h"

typedef enum {

    GLOBAL_VAR,
    LOCAL_VAR,
    FUNC,
    UNRESOLVED_FUNC,
    DEAD,

} SymbolType;


typedef struct {

    char* name;
    size_t offset; // may be value as well
    SymbolType type;
    size_t depth;
    int arity;

} Symbol;


extern Symbol* var_namespace;
extern Symbol* func_namespace;


void compile(FILE* src, FILE* ir);
char* unsafe_compile_node(FILE* ir, ASTNode*);
char* unsafe_compile_list(FILE* ir, ASTNode*);
char* unsafe_compile_atom(ASTNode*);
char* unsafe_compile_arithmetic(FILE* ir, ASTNode*, const char* op);
char* unsafe_fold_arithmetic(FILE* ir, ASTNode*, const char* op);
char* unsafe_compile_print(FILE* ir, ASTNode*);
char* unsafe_compile_newline(FILE* ir);
char* unsafe_compile_defvar(FILE* ir, ASTNode*);
char* unsafe_compile_defun(FILE* ir, ASTNode*);



