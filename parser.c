#include "./common.h"
#include "./parser.h"
#include "./tokeniser.h"
#include <stdio.h>
#include <stdlib.h>


#define expect(handle, tokenInfo, ...) \
    _expect(handle, tokenInfo, (const Token[]){__VA_ARGS__}, sizeof((const Token[]){__VA_ARGS__}) / sizeof(Token))


static inline bool _expect(bool handle, TokenInfo* tokenInfo, const Token* tokens, size_t len) {

    for (int i = 0; i < len; i++) {

        if (tokens[i] == tokenInfo->token)
            return true;
    }

    if (handle) {

        fputs("SYNTAX ERROR: Expected one of [ ", stderr);

        for (int i = 0; i < len; i++) {

            char* token = unsafe_token_to_string(tokens[i]);
            fprintf(stderr, "`%s`", token);
            arr_free(token);

            if (i < len - 1)
                fputs(" , ", stderr);
        }

        char* token = unsafe_token_to_string(tokenInfo->token);
        fprintf(stderr, " ] but got `%s` on line %zu\n", token, tokenInfo->line);
        arr_free(token);
    }

    return false;
}



static inline ASTNode* unsafe_init(TokenInfo* token) {

    ASTNode* node = (ASTNode*)malloc(sizeof(ASTNode));
    node->current = token;
    node->children = NULL;

    return node;
}


ASTNode* unsafe_build(FILE* src, TokenInfo* tokenInfo) { // recursive descent

    if (expect(false, tokenInfo, LEFT_PAREN)) { // list

        ASTNode* node = unsafe_init(tokenInfo);

        loop {

            TokenInfo* next = unsafe_get(src, tokenInfo->line);

            if (next->token == RIGHT_PAREN) { // close list

                free(next);
                return node;
            }

            if (next->token == END_OF_FILE) {

                fprintf(stderr, "SYNTAX ERROR: Unterminated list [missing `)`] due to unexpected `EOF` on line %zu\n", next->line);
                free(next);
                freeAST(true, node);
                exit(EX_DATAERR);
            }

            arr_push(node->children, unsafe_build(src, next));
        }

    } else if (expect(true, tokenInfo, 

                PLUS, MINUS, STAR, SLASH, PERCENT, CARET, SEMICOLON, TRUE, FALSE, AND, OR, NOT,
                PRINT, IDENTIFIER, INT, FLOAT, STRING, DEFVAR, DEFUN, LET, NEWLINE,
                LESS, LESS_EQUAL, GREATER, GREATER_EQUAL, EQL, NOT_EQUAL,

                )) { // atom

        return unsafe_init(tokenInfo); // no children

    } else { // illegal

        free(tokenInfo);
        exit(EX_DATAERR);
    }
}


void freeAST(bool abort, ASTNode* node) {

    if (node) {

        size_t len = arr_len(node->children);

        for (int i = 0; i < len; i++) {

            if (node->children[i])
                freeAST(abort, node->children[i]);
        }

        arr_free(node->children);

        if (abort && node->current)
            free(node->current);

        free(node);
    }

    return;
}




