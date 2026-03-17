#pragma once

#include <stdbool.h>
#include "lexer.h"
#include "common.h"

#define STD_ARRAY
#include "std/array.h"


typedef struct ASTNode {

    TokenInfo* current;
    struct ASTNode** children;

} ASTNode;


ASTNode* unsafe_build(FILE* src, TokenInfo* token);
void freeAST(bool abort, ASTNode*);





