#include "compiler/compiler.h"
#include "./common.h"
#include "transpiler/transpiler.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define KB64 65536
#define KB8 8192


void repl(void);


int main(int argc, char** argv) {

    char src_path[MAX] = {NIL};

    bool clean = false;
    bool run = false;
    bool compile = false;
    bool help = false;

    if (argc == 1) {

        repl();
        return 0;
    }

    for (size_t i = 1; i < argc; i++) {

        if (argv[i][0] == '-') {

            if (!strncmp(argv[i], "-clean", MAX))
                clean = true;

            else if (!strncmp(argv[i], "-run", MAX))
                run = true;

            else if (!strncmp(argv[i], "-compile", MAX))
                compile = true;

            else if (!strncmp(argv[i], "-help", MAX))
                help = true;

            else {

                fprintf(stderr, "ILLEGAL FLAG `%s`\n", argv[i]);
                exit(EXIT_FAILURE);
            }

        } else {

            if (!strlen(src_path))
                strncpy(src_path, argv[i], MAX);
        }
    }

    if (help) {

        puts("\nUsage: lispc <path-to-script.lisp> [-clean|-run|-compile|-help]");
        puts("\t-clean   : Produce no intermediate files");
        puts("\t-run     : Run the produced executable");
        puts("\t-compile : Compile to Assembly, but do not link and form executable");
        puts("\t-help    : Show list of options\n");

        exit(EXIT_SUCCESS);
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
    if (!_src) {

        fprintf(stderr, "ERROR: No file or directory `%s` exists\n", src);
        exit(EX_IOERR);
    }

    FILE* _ir = fopen(ir, "w+");
    if (!_ir) {

        fprintf(stderr, "ERROR: No file or directory `%s` exists\n", ir);
        exit(EX_IOERR);
    }

    FILE* _s = fopen(s, "w");
    if (!_s) {

        fprintf(stderr, "ERROR: No file or directory `%s` exists\n", s);
        exit(EX_IOERR);
    }

    compilef(_src, _ir);
    rewind(_ir);

    #ifdef __linux__
        transpile_gnu_x86_64(_ir, _s);

    #else
        transpile_darwin_ARM64(_ir, _s);

    #endif

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

            execlp("cc", "cc", "-save-temps", "-o", exec, s, "-lm", NULL);
            exit(EXIT_FAILURE);
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



void repl(void) {

    puts("\nCommon Lisp subset REPL\n");
    puts("Enter (help) for list of options");
    puts("Enter (exit), (quit), <C-d> to exit\n");

    char history[KB64] = {0};
    char input[KB8] = {0};
    int open_parens = 0;

    loop {

        if (!open_parens)
            printf(">>> ");

        else
            printf("?...\t");

        char line[MAX];
        if (!fgets(line, MAX - 1, stdin) ||
            ((!open_parens && (!strncmp(line, "(exit)", 6) ||
            !strncmp(line, "(quit)", 6)))))
            break;

        if (!strncmp(line, "(help)", 6)) {

            puts("\nUsage: lispc <path-to-script.lisp> [-clean|-run|-compile|-help]\n");
            puts("\t-clean   : Produce no intermediate files");
            puts("\t-run     : Run the produced executable");
            puts("\t-compile : Compile to Assembly, but do not link and form executable");
            puts("\t-help    : Show list of options\n");

            continue;
        }

        strncat(input, line, KB8 - strlen(input) - 1);

        for (size_t i = 0; line[i]; i++) {

            if (line[i] == '(')
                open_parens++;

            else if (line[i] == ')')
                open_parens--;
        }

        if (open_parens > 0)
            continue;

        else if (open_parens < 0) {

            fputs("\nERROR: Too many closing parentheses\n", stderr);
            input[0] = NIL;
            open_parens = 0;

            continue;
        }

        char* p = input;
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
            p++;

        if (*p == NIL) {

            input[0] = NIL;
            continue;
        }

        bool define = !strncmp(p, "(defvar", 7) || !strncmp(p, "(defun", 6);


        system("mkdir -p .tmp/");
        FILE* src = fopen(".tmp/.repl.lisp", "w");
        assert(src);
        fputs(history, src);

        if (define) {

            fputc('\n', src);
            fputs(input, src);
            fputc('\n', src);

        } else {

            fprintf(src, "\n(print %s)\n", input);
            fputs("(newline)\n", src);
        }

        fclose(src);
        src = NULL;

        if (!fork()) { // MEMORY LEAK!!

            src = fopen(".tmp/.repl.lisp", "r");
            assert(src);
            FILE* ir = fopen(".tmp/.repl.ir", "w+");
            assert(ir);

            compilef(src, ir);
            fclose(src);
            src = NULL;
            rewind(ir);

            FILE* s = fopen(".tmp/.repl.s", "w");
            assert(s);

            #ifdef __linux__
                transpile_gnu_x86_64(ir, s);    

            #else
                transpile_darwin_ARM64(ir, s);
            
            #endif

            fclose(ir);
            ir = NULL;
            fclose(s);
            s = NULL;

            system("cc -o .tmp/.repl.out .tmp/.repl.s -lm 2>/dev/null && ./.tmp/.repl.out");
            exit(EXIT_SUCCESS);
        }
        int status;
        wait(&status);

        if (WIFEXITED(status) && !WEXITSTATUS(status)) {

            if (define)
                strncat(history, input, KB64 - strlen(history) - 1);
        }

        input[0] = NIL;

        unlink("./.tmp/.repl.lisp");
        unlink("./.tmp/.repl.ir");
        unlink("./.tmp/.repl.s");
        unlink("./.tmp/.repl.o");
        unlink("./.tmp/.repl.out");
        system("rmdir .tmp");
    }
}




