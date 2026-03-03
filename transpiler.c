#include "./transpiler.h"
#include <ctype.h>
#include <stdio.h>


static inline void map_reg(FILE* s, const char* op, char* out_reg, int scratch_reg) {

    if (op[0] == '%') {

        int reg_idx;
        sscanf(op, "%%t%d", &reg_idx);
        snprintf(out_reg, MAX, "x%d", reg_idx + 9); 

    } else {

        long long val = atoll(op);

        if (val < 0) {

            fprintf(s, "    mov x%d, #%lld\n", scratch_reg, -val);
            fprintf(s, "    neg x%d, x%d\n", scratch_reg, scratch_reg);

        } else
            fprintf(s, "    mov x%d, #%lld\n", scratch_reg, val);

        snprintf(out_reg, MAX, "x%d", scratch_reg);
    }
}

static int is_string(char* token, char variables[][64], int var_types[], int varc, int reg_types[]) {

    if (token[0] == '\"')
        return 1;

    if (token[0] == '%') {

        int r_idx; sscanf(token, "%%t%d", &r_idx);
        return reg_types[r_idx];
    }

    if (isalpha(token[0]) || token[0] == '_') {

        for (int i = 0; i < varc; i++) {
            if (!strncmp(variables[i], token, MAX)) return var_types[i];
        }
    }
    return 0; // default to integer
}


void transpile_darwin_ARM64(FILE* ir, FILE* s) {

    char line[MAX];
    
    char variables[MAX][64];
    int var_types[MAX] = {0};
    int varc = 0;
    
    int reg_types[1024] = {0};
    
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

        if (line[0] == '\n' || line[0] == ';' || line[0] == '\r')
            continue;

        char* quote_start = strchr(line, '\"');

        if (quote_start) {

            char* quote_end = strrchr(line, '\"');

            if (quote_end && quote_end > quote_start) {

                for (char* p = quote_start; p < quote_end; p++)
                    if (*p == ' ') *p = '\x01';
            }
        }

        char t1[MAX] = {0}, t2[MAX] = {0}, t3[MAX] = {0};
        char t4[MAX] = {0}, t5[MAX] = {0}, t6[MAX] = {0};

        int n = sscanf(line, "%s %s %s %s %s %s", t1, t2, t3, t4, t5, t6);

        for (int i = 0; i < strlen(t2); i++) {

            if (t2[i] == '\x01')
                t2[i] = ' ';
        }

        for (int i = 0; i < strlen(t3); i++) {

            if (t3[i] == '\x01')
                t3[i] = ' ';
        }

        fputs("    ; ", s);
        for (int i = 0; i < strlen(line); i++) {

            if (line[i] == '\x01')
                line[i] = ' ';
        }

        fputs(line, s);

        if (n == 1 && !strncmp(t1, "NEWLINE", MAX)) {

            fputs("    adrp x0, empty@PAGE\n", s);
            fputs("    add x0, x0, empty@PAGEOFF\n", s);
            fputs("    bl _puts\n\n", s);

        } else if (n >= 2 && !strncmp(t1, "PRINT", MAX)) {

            if (is_string(t2, variables, var_types, varc, reg_types)) {

                if (t2[0] == '\"') {

                    strncpy(strings[stringc], t2, MAX);

                    fprintf(s, "    adrp x0, msg%d@PAGE\n", stringc);
                    fprintf(s, "    add x0, x0, msg%d@PAGEOFF\n", stringc);
                    fputs("    bl _printf\n\n", s);

                    stringc++;

                } else {

                    char hw_arg[8];

                    if (t2[0] == '%') {

                        map_reg(s, t2, hw_arg, 8);
                        fprintf(s, "    mov x0, %s\n", hw_arg);

                    } else {

                        fprintf(s, "    adrp x6, _%s@PAGE\n", t2);
                        fprintf(s, "    add x6, x6, _%s@PAGEOFF\n", t2);
                        fprintf(s, "    ldr x0, [x6]\n"); // Load pointer directly to x0

                    }

                    fputs("    bl _printf\n\n", s);
                }

            } else {

                char hw_arg[8];

                if (t2[0] == '%')
                    map_reg(s, t2, hw_arg, 8);

                else if (isalpha(t2[0]) || t2[0] == '_') {

                    fprintf(s, "    adrp x6, _%s@PAGE\n", t2);
                    fprintf(s, "    add x6, x6, _%s@PAGEOFF\n", t2);
                    fprintf(s, "    ldr x8, [x6]\n");

                    snprintf(hw_arg, 8, "x8");

                } else
                    map_reg(s, t2, hw_arg, 8);


                fputs("    adrp x0, l_msg_int@PAGE\n", s);
                fputs("    add x0, x0, l_msg_int@PAGEOFF\n", s);

                // fprintf(s, "    mov x1, %s\n", hw_arg);
                // fputs("    bl _printf\n\n", s);

                fputs("    sub sp, sp, #16\n", s);
                fprintf(s, "    str %s, [sp]\n", hw_arg);
                fputs("    bl _printf\n", s);
                fputs("    add sp, sp, #16\n\n", s);
            }
        } 
        
        else if (n >= 3 && !strncmp(t2, "=", MAX)) {
            
            int is_rhs_string = is_string(t3, variables, var_types, varc, reg_types);

            if (t1[0] != '%') {

                char* name = t1;
                char* val_str = t3;

                int v_idx = -1;
                for (int i = 0; i < varc; i++)
                    if (!strncmp(variables[i], name, MAX)) { v_idx = i; break; }


                if (v_idx == -1) {
                    v_idx = varc++;
                    strncpy(variables[v_idx], name, MAX);
                }

                var_types[v_idx] = is_rhs_string;

                char hw_arg[8];

                if (val_str[0] == '\"') {

                    strncpy(strings[stringc], val_str, MAX);

                    fprintf(s, "    adrp x8, msg%d@PAGE\n", stringc);
                    fprintf(s, "    add x8, x8, msg%d@PAGEOFF\n", stringc++);

                    snprintf(hw_arg, 8, "x8");

                } else if (isalpha(val_str[0]) || val_str[0] == '_') {

                    fprintf(s, "    adrp x6, _%s@PAGE\n", val_str);
                    fprintf(s, "    add x6, x6, _%s@PAGEOFF\n", val_str);
                    fprintf(s, "    ldr x8, [x6]\n");

                    snprintf(hw_arg, 8, "x8");

                } else
                    map_reg(s, val_str, hw_arg, 8);

                
                fprintf(s, "    adrp x6, _%s@PAGE\n", name);
                fprintf(s, "    add x6, x6, _%s@PAGEOFF\n", name);
                fprintf(s, "    str %s, [x6]\n\n", hw_arg);

            } else {

                char hw_dest[8];
                int d_idx; 

                sscanf(t1, "%%t%d", &d_idx);
                reg_types[d_idx] = is_rhs_string;
                snprintf(hw_dest, 8, "x%d", d_idx + 9);

                if (!strncmp(t3, "ADD", MAX) || !strncmp(t3, "SUB", MAX) || 
                    !strncmp(t3, "MUL", MAX) || !strncmp(t3, "DIV", MAX) || 
                    !strncmp(t3, "REM", MAX)) {
                    
                    char hw_arg1[8], hw_arg2[8];
                    map_reg(s, t4, hw_arg1, 8);
                    map_reg(s, t5, hw_arg2, 7);

                    if (!strncmp(t3, "ADD", MAX))
                        fprintf(s, "    add %s, %s, %s\n\n", hw_dest, hw_arg1, hw_arg2);

                    else if (!strncmp(t3, "SUB", MAX))
                        fprintf(s, "    sub %s, %s, %s\n\n", hw_dest, hw_arg1, hw_arg2);

                    else if (!strncmp(t3, "MUL", MAX))
                        fprintf(s, "    mul %s, %s, %s\n\n", hw_dest, hw_arg1, hw_arg2);

                    else if (!strncmp(t3, "DIV", MAX))
                        fprintf(s, "    sdiv %s, %s, %s\n\n", hw_dest, hw_arg1, hw_arg2);

                    else if (!strncmp(t3, "REM", MAX)) {

                        fprintf(s, "    sdiv %s, %s, %s\n", hw_dest, hw_arg1, hw_arg2);
                        fprintf(s, "    msub %s, %s, %s, %s\n\n", hw_dest, hw_dest, hw_arg2, hw_arg1);
                    }

                } else if (isalpha(t3[0]) || t3[0] == '_') {

                    char* var_name = t3;

                    fprintf(s, "    adrp x6, _%s@PAGE\n", var_name);
                    fprintf(s, "    add x6, x6, _%s@PAGEOFF\n", var_name);
                    fprintf(s, "    ldr %s, [x6]\n\n", hw_dest);

                } else if (t3[0] == '\"') {

                    strncpy(strings[stringc], t3, MAX);

                    fprintf(s, "    adrp %s, msg%d@PAGE\n", hw_dest, stringc);
                    fprintf(s, "    add %s, %s, msg%d@PAGEOFF\n\n", hw_dest, hw_dest, stringc++);

                } else
                    map_reg(s, t3, hw_dest, d_idx + 9);
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
    fputs("; === STRING LITERALS ===\n", s);
    fputs(".section __TEXT,__cstring,cstring_literals\n", s);
    fputs("l_msg_int: .asciz \"%lld\"\n", s); 
    fputs("empty: .asciz \"\"\n", s);
    
    if (stringc > 0) {

        for (int i = 0; i < stringc; i++)
            fprintf(s, "msg%d: .asciz %s\n", i, strings[i]);
    }
}






