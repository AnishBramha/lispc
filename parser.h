#pragma once

#include <stdbool.h>
#include "./tokeniser.h"
#include "./common.h"

#define STD_ARRAY
#include "./std/array.h"


typedef struct ASTNode {

    TokenInfo* current;
    struct ASTNode** children;

} ASTNode;


static inline bool _expect(bool handle, TokenInfo* tokenInfo, const Token* tokens, size_t len);

#define expect(handle, tokenInfo, ...) \
    _expect(handle, tokenInfo, (const Token[]){__VA_ARGS__}, sizeof((const Token[]){__VA_ARGS__}) / sizeof(Token))



static inline ASTNode* unsafe_init(TokenInfo* token);
ASTNode* unsafe_build(FILE* src, TokenInfo* token);
void freeAST(bool abort, ASTNode*);





