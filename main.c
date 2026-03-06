#include "./compiler.h"
#include "./common.h"
#include "./transpiler.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>


int main(int argc, char** argv) {

    FILE* src = NULL;
    FILE* ir = NULL;
    FILE* s = NULL;

    char arg[MAX];
    char* src_path;
    char ir_path[MAX];
    char asm_path[MAX];

    switch (argc) {
        
        case 0:

            // TODO: print help for now
            // TODO: make REPL later

            break;

        case 2:

            strncpy(arg, argv[1], MAX);
            src_path = strrchr(arg, '.');

            if (!src_path) {

                perror("Unrecognised file type (need .lisp)");
                exit(EX_OSFILE);
            }

            if (!strncmp(src_path, ".lisp", MAX))
                *src_path = NIL;

            snprintf(ir_path, MAX, "%s.lisp.ir", arg);
            snprintf(asm_path, MAX, "%s.s", arg);

            src = fopen(argv[1], "r");
            assert(src);
            ir = fopen(ir_path, "w+");
            assert(ir);
            s = fopen(asm_path, "w");
            assert(s);

            compile(src, ir);

            rewind(ir);
            transpile_darwin_ARM64(ir, s);

            fclose(src);
            fclose(ir);
            fclose(s);
            src = NULL;
            ir = NULL;
            s = NULL;

            if (!fork()) {

                char exec_path[MAX];
                snprintf(exec_path, MAX, "%s.out", arg);
                execlp("clang", "clang", asm_path, "-save-temps", "-o", exec_path, NULL);
            }

            else
                wait(NULL);

            break;


        case 3:

            #if 0

            if (strncmp(argv[2], "-nasm", MAX)) {

                perror("Illegal flag: Use -nasm for NASM x86_64");
            }

            strncpy(arg, argv[1], MAX);
            src_path = strrchr(arg, '.');

            if (!src_path) {

                perror("Unrecognised file type (need .lisp)");
                exit(EX_OSFILE);
            }

            if (!strncmp(src_path, ".lisp", MAX))
                *src_path = NIL;

            snprintf(ir_path, MAX, "%s.lisp.ir", arg);
            snprintf(asm_path, MAX, "%s.asm", arg);

            src = fopen(argv[1], "r");
            assert(src);
            ir = fopen(ir_path, "w+");
            assert(ir);
            s = fopen(asm_path, "w");
            assert(s);

            compile(src, ir);

            rewind(ir);
            transpile_nasm_x86_64(ir, s);

            fclose(src);
            fclose(ir);
            fclose(s);
            src = NULL;
            ir = NULL;
            s = NULL;

            if (!fork())
                execlp("nasm", "nasm", "-f", "elf64", asm_path, NULL);

            else {

                wait(NULL);

                if (!fork()) {

                    char obj_path[MAX];
                    snprintf(obj_path, MAX, "%s.o", arg);
                    char exec_path[MAX];
                    snprintf(exec_path, MAX, "%s.out", arg);
                    execlp("gcc", "gcc", obj_path, "-lm", "-no-pie", "-o", exec_path, NULL);
                }

                wait(NULL);
            } 
            
            #endif

            break;


        default:

            perror("Usage: lispc <src>.lisp [-nasm]");
            break;
    }

    return 0;
}




