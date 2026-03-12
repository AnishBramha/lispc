#pragma once

#include <stdio.h>

#include "./parser.h"

typedef enum {

    GLOBAL_VAR,
    LOCAL_VAR,
    FUNC,
    DEAD,

} SymbolType;


typedef enum {

    DAT_INT,
    DAT_STRING,
    DAT_BOOL,

} DataType;


typedef struct {

    char* name;
    size_t offset; // may be value as well
    SymbolType sym_type;
    DataType dat_type;
    size_t depth;
    size_t arity;

} Symbol;


extern Symbol* var_namespace;
extern Symbol* func_namespace;


void compilef(FILE* src, FILE* ir);
char* unsafe_compile_node(FILE* ir, ASTNode*);
char* unsafe_compile_list(FILE* ir, ASTNode*);
char* unsafe_compile_atom(ASTNode*);
char* unsafe_compile_arithmetic(FILE* ir, ASTNode*, const char* op);
char* unsafe_fold_arithmetic(FILE* ir, ASTNode*, const char* op);
char* unsafe_compile_print(FILE* ir, ASTNode*);
char* unsafe_compile_newline(FILE* ir);
char* unsafe_compile_defvar(FILE* ir, ASTNode*);
char* unsafe_compile_let(FILE* ir, ASTNode*);
char* unsafe_compile_defun(FILE* ir, ASTNode*);
char* unsafe_compile_string(ASTNode*);
char* unsafe_compile_symbol(ASTNode*);
char* unsafe_compile_call(FILE* ir, ASTNode*);
char* unsafe_fold_comparison(FILE* ir, ASTNode*, const char* op);
char* unsafe_compile_if(FILE* ir, ASTNode*);
char* unsafe_compile_concat(FILE* ir, ASTNode*);
char* unsafe_compile_panic(FILE* ir, ASTNode*);
char* unsafe_compile_error(FILE* ir, ASTNode*);





