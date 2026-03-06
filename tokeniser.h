#pragma once

#include <stdio.h>

#include "./common.h"

typedef enum {

    // symbol delimiters
    LEFT_PAREN, RIGHT_PAREN,

    // arithmetic
    PLUS, MINUS, STAR, SLASH, PERCENT, CARET,

    // comments
    SEMICOLON,

    // builtin functions
    PRINT, NEWLINE,

    // miscellaneous
    IDENTIFIER, INT, FLOAT, STRING, END_OF_FILE,

    // keywords
    DEFVAR, DEFUN, LET,

} Token;


typedef struct {

    Token token;
    char lexeme[MAX];
    size_t line;

} TokenInfo;

extern Token* tokens;


TokenInfo* unsafe_get(FILE* src, size_t line);
TokenInfo* unsafe_get_number(FILE* src, int c, TokenInfo* tokenInfo);
TokenInfo* unsafe_get_alpha(FILE* src, int c, TokenInfo* tokenInfo);
TokenInfo* unsafe_get_string(FILE* src, TokenInfo* tokenInfo);
char* unsafe_token_to_string(Token token);




