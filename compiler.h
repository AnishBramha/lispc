#pragma once
#include <stdio.h>

#include "./parser.h"

#define STD_ARRAY
#include "./std/array.h"
#define STD_TABLE
#include "./std/table.h"


typedef enum {

    GLOBAL_VAR,
    LOCAL_VAR,
    FUNC,
    UNRESOLVED_FUNC,
    DEAD,

} SymbolType;


typedef struct {

    char* name;
    size_t offset;
    SymbolType type;
    size_t depth;
    int arity;

} Symbol;


extern Symbol* var_namespace;
extern Symbol* func_namespace;


void compile(FILE* src, FILE* ir);





