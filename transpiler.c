#include "./transpiler.h"
#include <ctype.h>
#include <stdio.h>


void transpile_darwin_ARM64(FILE* ir, FILE* s) {

    char line[MAX];
    
    char variables[MAX][64];
    int varc = 0;
    
    char strings[MAX][256];
    int stringc = 0;

    // prologue

    fputs("; === PROLOGUE ===\n", s);
    fputs(".section __TEXT,__text,regular,pure_instructions\n", s);
    fputs(".global _main\n", s);
    fputs(".align 2\n\n", s);
    fputs("; === GLOBAL ENTRY ===\n", s);
    fputs("_main:\n", s);
    fputs("    stp x29, x30, [sp, #-16]!\n", s);
    fputs("    mov x29, sp\n\n", s);


    while (fgets(line, sizeof(line), ir)) {

        char arg1[64], arg2[64];

        if (sscanf(line, "VARIABLE %s = %s", arg1, arg2)) {
            
            strcpy(variables[varc++], arg1);

            fprintf(s, "    ; %s", line);

            if (isdigit(arg2[0])) { // variable assignment

                fprintf(s, "    adrp x8, _%s@PAGE\n", arg1);
                fprintf(s, "    add x8, x8, _%s@PAGEOFF\n", arg1);
                fprintf(s, "    mov x9, #%s\n", arg2);
                fputs("    str x9, [x8]\n\n", s);

            } else { // literal assignment

                fprintf(s, "    adrp x8, _%s@PAGE\n", arg2);
                fprintf(s, "    add x8, x8, _%s@PAGEOFF\n", arg2);
                fprintf(s, "    ldr x9, [x8]\n");
                
                fprintf(s, "    adrp x10, _%s@PAGE\n", arg1);
                fprintf(s, "    add x10, x10, _%s@PAGEOFF\n", arg1);
                fputs("    str x9, [x10]\n\n", s);
            }

        } else if (!strncmp(line, "FUNCALL print", 13)) { // print function call
            
            char* quote_start = strchr(line, '\"');

            if (quote_start) {

                strcpy(strings[stringc], quote_start);

                strings[stringc][strcspn(strings[stringc], "\n")] = 0; 
                
                fprintf(s, "    ; %s", line);
                fprintf(s, "    adrp x0, msg%d@PAGE\n", stringc);
                fprintf(s, "    add x0, x0, msg%d@PAGEOFF\n", stringc);
                fputs("    bl _printf\n\n", s);
                
                stringc++;
            }
        }
    }


    // epilogue

    fputs("; === EPILOGUE ===\n", s);
    fputs("    mov x0, #0\n", s);
    fputs("    ldp x29, x30, [sp], #16\n", s);
    fputs("    ret\n\n", s);


    // global variables

    if (varc > 0) {

        fputs("; === DATA SECTION ===\n", s);
        fputs(".section __DATA,__data\n", s);
        fputs(".align 3\n", s);

        for (int i = 0; i < varc; i++)
            fprintf(s, "_%s: .quad 0\n", variables[i]);

        fputc('\n', s);
    }


    // c strings

    if (stringc > 0) {
        fputs("; === STRING LITERALS ===\n", s);
        fputs(".section __TEXT,__cstring,cstring_literals\n", s);

        for (int i = 0; i < stringc; i++) {
            fprintf(s, "msg%d: .asciz %s\n", i, strings[i]);
        }
    }
}



void transpile_darwin_x86_64(FILE *ir, FILE *s) {

    char line[MAX];
    
    char variables[MAX][64];
    int varc = 0;
    
    char strings[MAX][256];
    int stringc = 0;


    // prologue

    fputs("# === PROLOGUE ===\n", s);
    fputs(".intel_syntax noprefix\n", s);
    fputs(".section __TEXT,__text,regular,pure_instructions\n", s);
    fputs(".global _main\n", s);
    fputs(".align 4, 0x90\n\n", s);
    
    fputs("# === GLOBAL ENTRY ===\n", s);
    fputs("_main:\n", s);
    fputs("    push rbp\n", s);
    fputs("    mov rbp, rsp\n\n", s);


    while (fgets(line, sizeof(line), ir)) {

        char arg1[64], arg2[64];

        if (sscanf(line, "VARIABLE %s = %s", arg1, arg2) == 2) {
            
            strcpy(variables[varc++], arg1);

            fprintf(s, "    # %s", line);

            if (isdigit(arg2[0])) { // literal assignment
                
                fprintf(s, "    mov qword ptr [rip + _%s], %s\n\n", arg1, arg2);

            } else { // variable assignment
                
                fprintf(s, "    mov rax, qword ptr [rip + _%s]\n", arg2);
                fprintf(s, "    mov qword ptr [rip + _%s], rax\n\n", arg1);
            }

        } else if (!strncmp(line, "FUNCALL print", 13)) { // print function call
            
            char* quote_start = strchr(line, '\"');

            if (quote_start) {

                strcpy(strings[stringc], quote_start);
                strings[stringc][strcspn(strings[stringc], "\n")] = 0; 
                
                fprintf(s, "    # %s", line);
                fprintf(s, "    lea rdi, [rip + msg%d]\n", stringc);
                fputs("    xor eax, eax\n", s); 
                fputs("    call _printf\n\n", s);
                
                stringc++;
            }
        }
    }


    // epilogue

    fputs("# === EPILOGUE ===\n", s);
    fputs("    xor eax, eax\n", s);
    fputs("    pop rbp\n", s);
    fputs("    ret\n\n", s);


    // global variables

    if (varc > 0) {
        fputs("# === DATA SECTION ===\n", s);
        fputs(".section __DATA,__data\n", s);
        fputs(".align 3\n", s);

        for (int i = 0; i < varc; i++)
            fprintf(s, "_%s: .quad 0\n", variables[i]);

        fputc('\n', s);
    }


    // c strings

    if (stringc > 0) {
        fputs("# === STRING LITERALS ===\n", s);
        fputs(".section __TEXT,__cstring,cstring_literals\n", s);

        for (int i = 0; i < stringc; i++) {
            fprintf(s, "msg%d: .asciz %s\n", i, strings[i]);
        }
    }
}






