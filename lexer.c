#include "./lexer.h"
#include "./common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void compile_variable_declaration(FILE* src, FILE* ir) {

    char scratch;
    int i = 1;

    char name[MAX];
    fscanf(src, "%s", name);

    if (name[0] >= '0' && name[0] <= '9') {

        perror("SYNTAX ERROR: Cannot declare numeric literal as variable");
        exit(EX_DATAERR);

    } else {

        fprintf(ir, "VARIABLE %s = ", name);

        char value[MAX];
        loop {

            char c = fgetc(src);

            switch (c) {

                case ' ':
                case '\n':
                case '\r':
                case '\t':
                    break;

                default:
                    value[0] = c;
                    goto VAL;
                    break;
            }
        }

        VAL:

            while ((scratch = fgetc(src)) != ')') {

                switch (scratch) {

                    case ' ':
                    case '\t':
                    case '\r':
                    case '\n':
                        break;

                    default:
                        value[i++] = scratch;
                }

                if (scratch == EOF) {

                    perror("SYNTAX ERROR: Expected `)`");
                    exit(EX_DATAERR);
                }
            }

            if (scratch != ')') {

                perror("SYNTAX ERROR: Expected `)`");
                exit(EX_DATAERR);
            }

            value[i] = NIL;
            fprintf(ir, "%s\n", value);
    }
}



void compile_function_call(FILE* src, FILE* ir, const char* name) {

    char scratch;

    // built-in functions
    if (!strncmp(name, "print", MAX)) {

        while ((scratch = fgetc(src)) != EOF) {

            if (scratch == '\"' || scratch == '\'')
                break;
        }

        if (scratch == EOF) {

            perror("SYNTAX ERROR: Expected `)`");
            exit(EX_DATAERR);
        }

        if (scratch == ')') {

            fputs("FUNCALL print argc=0", ir);
            return;
        }


        if (scratch == '\"' || scratch == '\'') {

            char str[MAX];
            char opening = scratch;

            int i = 0;
            while ((scratch = fgetc(src)) != EOF && scratch != opening) {

                if (scratch == opening)
                    break;

                str[i++] = scratch;
            }

            if (scratch == EOF) {

                fprintf(stderr, "SYNTAX ERROR: Expected `%c`", opening);
                exit(EX_DATAERR);
            }

            if (scratch == opening)
                str[i] = NIL;

            while ((scratch = fgetc(src)) != EOF && scratch != ')');

            if (scratch == EOF) {

                perror("SYNTAX ERROR: Expected `)`");
                exit(EX_DATAERR);
            }

            fprintf(ir, "FUNCALL print argc=1 args=\"%s\"\n", str);
        }
    }
}









