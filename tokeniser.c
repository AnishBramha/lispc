#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "./common.h"
#include "./tokeniser.h"


TokenInfo* unsafe_get(FILE* src, size_t line) {

    TokenInfo* tokenInfo = (TokenInfo*)malloc(sizeof(TokenInfo));
    tokenInfo->line = line;

    int c;

    for (;;) {

        switch (c = fgetc(src)) {

            case EOF:
                
                tokenInfo->lexeme[0] = NIL;
                tokenInfo->token = END_OF_FILE;
                return tokenInfo;

            case  '(': tokenInfo->lexeme[0] = c; tokenInfo->lexeme[1] = NIL; tokenInfo->token = LEFT_PAREN;  return tokenInfo;

            case  ')': tokenInfo->lexeme[0] = c; tokenInfo->lexeme[1] = NIL; tokenInfo->token = RIGHT_PAREN; return tokenInfo;

            case  '+': tokenInfo->lexeme[0] = c; tokenInfo->lexeme[1] = NIL; tokenInfo->token = PLUS;        return tokenInfo;

            case  '-': tokenInfo->lexeme[0] = c; tokenInfo->lexeme[1] = NIL; tokenInfo->token = MINUS;       return tokenInfo;

            case  '*': tokenInfo->lexeme[0] = c; tokenInfo->lexeme[1] = NIL; tokenInfo->token = STAR;        return tokenInfo;

            case  '/': tokenInfo->lexeme[0] = c; tokenInfo->lexeme[1] = NIL; tokenInfo->token = SLASH;       return tokenInfo;

            case  '%': tokenInfo->lexeme[0] = c; tokenInfo->lexeme[1] = NIL; tokenInfo->token = PERCENT;     return tokenInfo;

            case  '^': tokenInfo->lexeme[0] = c; tokenInfo->lexeme[1] = NIL; tokenInfo->token = CARET;       return tokenInfo;

            // skip whitespace
            case  ' ':
            case '\t':
            case '\r':
                  break;

            case '\n':
                  tokenInfo->line++;
                  break;

            case '\"': return unsafe_get_string(src, tokenInfo);

            case ';': // skip comments

                while ((c = fgetc(src)) != '\n' && c != EOF);
                ungetc(c, src);
                break;

            default:

                if (isdigit((unsigned char)c))
                    return unsafe_get_number(src, c, tokenInfo);

                if (isalpha((unsigned char)c))
                    return unsafe_get_alpha(src, c, tokenInfo);

                fprintf(stderr, "SYNTAX ERROR: Illegal token `%c` on line %zu\n", c, tokenInfo->line);
                free(tokenInfo);
                exit(EX_DATAERR);
        }
    }
}


TokenInfo* unsafe_get_number(FILE* src, int c, TokenInfo* tokenInfo) {

    tokenInfo->lexeme[0] = c;

    int i = 1;
    while ((c = fgetc(src)) != EOF && isdigit((unsigned char)c))
        tokenInfo->lexeme[i++] = c;

    if (c == '.') {

        tokenInfo->lexeme[i++] = c;

        while ((c = fgetc(src)) != EOF && isdigit((unsigned char)c))
            tokenInfo->lexeme[i++] = c;

        if (tokenInfo->lexeme[i - 1] == '.')
            tokenInfo->lexeme[i++] = '0';

        tokenInfo->token = FLOAT;
    
    } else
        tokenInfo->token = INT;

    if (c != EOF && isalpha((unsigned char)c)) {

        fprintf(stderr, "SYNTAX ERROR: Expected DIGIT but got `%c` on line %zu\n", c, tokenInfo->line);
        free(tokenInfo);
        exit(EX_DATAERR);
    }

    if (c != EOF)
        ungetc(c, src);

    tokenInfo->lexeme[i++] = NIL;
    return tokenInfo;
}


TokenInfo* unsafe_get_alpha(FILE* src, int c, TokenInfo* tokenInfo) {

    tokenInfo->lexeme[0] = c;

    int i = 1;
    while ((c = fgetc(src)) != EOF && (isalnum((unsigned char)c) || c == '_'))
        tokenInfo->lexeme[i++] = c;
    tokenInfo->lexeme[i] = NIL;

    if (!strncmp(tokenInfo->lexeme, "print", MAX))
        tokenInfo->token = PRINT;

    else if (!strncmp(tokenInfo->lexeme, "defvar", MAX))
        tokenInfo->token = DEFVAR;

    else if (!strncmp(tokenInfo->lexeme, "defun", MAX))
        tokenInfo->token = DEFUN;

    else
        tokenInfo->token = IDENTIFIER;

    if (c != EOF)
        ungetc(c, src);

    return tokenInfo;
}


TokenInfo* unsafe_get_string(FILE* src, TokenInfo* tokenInfo) {

    int i = 0;

    int c;
    while ((c = fgetc(src)) != '\"') {

        switch (c) {

            case EOF:

                CLEANUP1:
                    tokenInfo->lexeme[i] = NIL;
                    fprintf(stderr, "SYNTAX ERROR: Unterminated string literal `\"%s` on line %zu\n", tokenInfo->lexeme, tokenInfo->line);

                CLEANUP2:
                    free(tokenInfo);
                    exit(EX_DATAERR);

            case '\n': // multiline strings

                tokenInfo->line++;

                while ((c = fgetc(src)) == ' ' || c == '\t'); // skip whitespaces within string
                
                if (c == EOF)
                    goto CLEANUP1;

                ungetc(c, src);

                break;

            case '\\':

                switch (c = fgetc(src)) {

                    case EOF:
                        goto CLEANUP1;

                    case  'a': tokenInfo->lexeme[i++] = '\a'; break;
                    case  'b': tokenInfo->lexeme[i++] = '\b'; break;
                    case  'f': tokenInfo->lexeme[i++] = '\f'; break;
                    case  'n': tokenInfo->lexeme[i++] = '\n'; break;
                    case  'r': tokenInfo->lexeme[i++] = '\r'; break;
                    case  't': tokenInfo->lexeme[i++] = '\t'; break;

                    case '\?':
                    case '\'':
                    case '\"':
                    case '\\':
                        tokenInfo->lexeme[i++] = c;
                        break;

                    default:
                        fprintf(stderr, "SYNTAX ERROR: Illegal escape sequence `\\%c` on line %zu\n", c, tokenInfo->line);
                        goto CLEANUP2;
                }

                break;

            default:
                tokenInfo->lexeme[i++] = c;
        }
    }

    tokenInfo->lexeme[i] = NIL;
    tokenInfo->token = STRING;

    return tokenInfo;
}




