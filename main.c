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

    char src_path[MAX] = {NIL};

    bool nasm = false;
    bool clean = false;
    bool run = false;
    bool compile = false;

    if (argc == 1) {

        // TODO: make REPL later

        puts("Usage: lispc.out <path-to-script.lisp> [-nasm|-clean|-run|-compile]");
        puts("Note: Default target is ARM64");
        puts("Note: REPL is under construction");
    }

    for (size_t i = 1; i < argc; i++) {

        if (argv[i][0] == '-') {

            if (!strncmp(argv[i], "-nasm", MAX))
                nasm = true;

            else if (!strncmp(argv[i], "-clean", MAX))
                clean = true;

            else if (!strncmp(argv[i], "-run", MAX))
                run = true;

            else if (!strncmp(argv[i], "-compile", MAX))
                compile = true;

            else {

                fprintf(stderr, "ILLEGAL FLAG `%s`\n", argv[i]);
                exit(EXIT_FAILURE);
            }

        } else {

            if (!strlen(src_path))
                strncpy(src_path, argv[i], MAX);
        }
    }

    if (!strlen(src_path)) {

        fputs("NO SOURCE FILES PROVIDED\n", stderr);
        exit(EXIT_FAILURE);
    }

    char* dot = strrchr(src_path, '.');
    if (dot)
        *dot = NIL;

    char src[MAX];
    src[0] = NIL;
    strncat(src, src_path, MAX);
    strncat(src, ".lisp", MAX);

    char ir[MAX];
    ir[0] = NIL;
    strncat(ir, src_path, MAX);
    strncat(ir, ".lisp.ir", MAX);

    char s[MAX];
    s[0] = NIL;
    strncat(s, src_path, MAX);
    strncat(s, ".s", MAX);

    FILE* _src = fopen(src, "r");
    assert(_src);

    FILE* _ir = fopen(ir, "w+");
    assert(_ir);

    FILE* _s = fopen(s, "w");
    assert(_s);

    compilef(_src, _ir);
    rewind(_ir);

    if (nasm);
        // TODO: transpile_nasm_x86_64

    else
        transpile_darwin_ARM64(_ir, _s);

    fclose(_src); _src = NULL;
    fclose(_ir); _ir = NULL;
    fclose(_s); _s = NULL;

    if (!compile) {

        char exec[MAX];
        exec[0] = NIL;
        strncat(exec, src_path, MAX);
        strncat(exec, ".out", MAX);

        char obj[MAX];
        obj[0] = NIL;
        strncat(obj, src_path, MAX);
        strncat(obj, ".o", MAX);

        if (!fork()) {

            if (nasm) {

                #ifdef __linux__

                execlp("nasm", "nasm", "-f", "elf64", "-o", obj, s, NULL);
                exit(EXIT_FAILURE);

                #endif
            }

            if (!fork()) {

                execlp("cc", "cc", "-save-temps", "-o", exec, s, NULL);
                exit(EXIT_FAILURE);
            }
            wait(NULL);

            exit(EXIT_SUCCESS);
        }
        wait(NULL);

        if (clean) {

            unlink(ir);
            unlink(s);
            unlink(obj);
        }

        if (run) {

            char cmd[MAX];
            snprintf(cmd, MAX, "./%s", exec);
            execlp(cmd, cmd, NULL);
            exit(EXIT_FAILURE);
        }
    }

    return 0;
}




