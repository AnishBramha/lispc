#include "./compiler.h"
#include "./lexer.h"
#include <stdio.h>
#include <string.h>


void compile(FILE* src, FILE* ir) {

    char c;
    while ((c = fgetc(src)) != EOF) {

        char token[MAX];

        switch (c) {

            case '(':

                fscanf(src, "%s", token);
    
                if (!strncmp(token, "defvar", MAX))
                    compile_variable_declaration(src, ir);
                    
                else if (!strncmp(token, "defun", MAX));
                    // TODO: dispatch to handle function declaration

                else
                    compile_function_call(src, ir, token);
                    // TODO: handle user-defined function call or illegal token

                break;

            
            case ' ':
            case '\t':
            case '\n':
            case '\r':
                break;

            case ';':

                while ((c = fgetc(src)) != '\n') {

                    if (c == EOF)
                        return;
                }
        }
    }
}




