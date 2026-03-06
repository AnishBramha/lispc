#include "./transpiler.h"
#include "./common.h"
#include <ctype.h>
#include <stdio.h>


static bool free_bitmap[8] = {true, true, true, true, true, true, true, true};

static size_t stack_offset = 0;


static inline int map_reg_arm(void) {

    // scratch registers: x9 - x15
    // indirect result register: x8 -> use if all scratch registers overflow
    // if both overflow, spill on to the stack

    // RETURN VALUES:
    // 8 - 15 -> mapping successful
    // 0 -> spill on to stack

    for (int i = 1; i < 8; i++) {

        if (free_bitmap[i]) {

            free_bitmap[i] = false;
            return i + 8;
        }
    }

    if (free_bitmap[0]) {

        free_bitmap[0] = false;
        return 8;
    }

    stack_offset += 8;
    return 0;
}


static inline void free_reg_arm(int reg_num) {

    if (8 <= reg_num && reg_num <= 15)
        free_bitmap[reg_num - 8] = true;
}


static inline size_t fetch_operand(FILE* s, const char* op, int phys_reg[], char* out_reg) {
    
    if (op[0] == '%') {

        size_t idx;
        sscanf(op, "%%t%zu", &idx);

        int loc = phys_reg[idx];
        
        if (loc > 0) {

            snprintf(out_reg, 16, "x%d", loc);
            return loc; 
        }
        
        size_t reg = map_reg_arm();
        reg = reg ? reg : 16;
        
        fprintf(s, "    ldr x%zu, [x29, #%d]\n", reg, loc);
        snprintf(out_reg, 16, "x%zu", reg);
        return reg;
    }

    size_t reg = map_reg_arm();
    reg = reg ? reg : 16;

    if (isalpha(op[0]) || op[0] == '_') {

        fprintf(s, "    adrp x%zu, _%s@PAGE\n", reg, op);
        fprintf(s, "    ldr x%zu, [x%zu, _%s@PAGEOFF]\n", reg, reg, op);

    } else
        fprintf(s, "    mov x%zu, #%lld\n", reg, atoll(op));

    snprintf(out_reg, 16, "x%zu", reg);
    return reg;
}




static inline bool is_string(char* literal, char variables[MAX][MAX], bool var_types[], int varc, bool reg_types[]) {

    if (literal[0] == '\"')
        return true;

    if (literal[0] == '%') {

        size_t r_idx;
        sscanf(literal, "%%t%zu", &r_idx);

        return reg_types[r_idx];
    }

    if (isalpha(literal[0]) || literal[0] == '_') {

        for (int i = 0; i < varc; i++) {

            if (!strncmp(variables[i], literal, MAX))
                return var_types[i];
        }
    }

    return false;
}


void transpile_darwin_ARM64(FILE* ir, FILE* s) {

    // Shuttle registers for backup in load operations - x16, x17

    char line[MAX];

    char variables[MAX][MAX];
    bool var_types[MAX] = {false};
    size_t varc = 0;

    bool reg_types[MAX] = {false};

    char strings[MAX][MAX];
    size_t stringc = 0;

    int phys_reg[MAX] = {0};


    // prologue
    fputs("; === PROLOGUE ===\n", s);
    fputs(".section __TEXT,__text,regular,pure_instructions\n", s);
    fputs(".global _main\n", s);
    fputs(".align 2\n\n", s);
    fputs("; === GLOBAL ENTRY ===\n", s);
    fputs("_main:\n", s);
    fputs("    stp x29, x30, [sp, #-16]!\n", s);
    fputs("    mov x29, sp\n", s);
    fputs("    ; STACK SPILL BUFFER\n", s);
    fputs("    sub sp, sp, #4096\n\n", s);


    // read IR

    while (fgets(line, MAX, ir)) {

        if (line[0] == '\n' || line[0] == '\r')
            continue;


        char* quote_start;
        if ((quote_start = strchr(line, '\"'))) { // if found string literal

            char* quote_end = strrchr(line, '\"');

            if (quote_end && quote_end > quote_start) {

                for (char* p = quote_start; p < quote_end; p++)
                    *p = (*p == ' ') ? '\x01' : *p; // unprintable character - start of heading
            }
        }

        // IR line args
        char t1[MAX];
        char t2[MAX];
        char t3[MAX];
        char t4[MAX];
        char t5[MAX];

        int n = sscanf(line, "%s %s %s %s %s\n", t1, t2, t3, t4, t5);

        for (int i = 0; i < strlen(line); i++) // restore spaces
            line[i] = (line[i] == '\x01') ? ' ' : line[i];

        fprintf(s, "    ; %s", line); // IR instruction as comment


        if (n == 1 && !strncmp(t1, "NEWLINE", MAX)) { // newline

            fputs("    adrp x0, l_empty@PAGE\n", s);
            fputs("    add x0, x0, l_empty@PAGEOFF\n", s);
            fputs("    bl _puts\n\n", s);

        } else if (n == 2 && !strncmp(t1, "PRINT", MAX)) { // print

            if (is_string(t2, variables, var_types, varc, reg_types)) { // string

                if (t2[0] == '\"') { // string literal

                    t2[strlen(t2) - 1] = NIL;
                    strncpy(strings[stringc], t2 + 1, MAX);

                    fprintf(s, "    adrp x0, msg%zu@PAGE\n", stringc);
                    fprintf(s, "    add x0, x0, msg%zu@PAGEOFF\n", stringc++);

                } else { // string variable

                    char arm_reg[MAX];
                    int reg = fetch_operand(s, t2, phys_reg, arm_reg);

                    if (strncmp(arm_reg, "x0", MAX))
                        fprintf(s, "    mov x0, %s\n", arm_reg);

                    free_reg_arm(reg);
                }

                fputs("    bl _printf\n\n", s);

            } else { // number

                char arm_reg[16];
                int reg = fetch_operand(s, t2, phys_reg, arm_reg);

                // Apple's ABI requires variadic args to be loaded onto stack
                fprintf(s, "    str %s, [sp, #-16]!\n", arm_reg);
                
                free_reg_arm(reg);

                // format string
                fputs("    adrp x0, l_msg_int@PAGE\n", s);
                fputs("    add x0, x0, l_msg_int@PAGEOFF\n", s);
                
                fputs("    bl _printf\n", s);
                
                // free the stack
                fputs("    add sp, sp, #16\n\n", s);
            }

        } else if (n >= 3 && !strncmp(t2, "=", MAX)) {

            int is_rhs_string = is_string(t3, variables, var_types, varc, reg_types);

            // lhs
            if (t1[0] != '%') { // global variable assignment

                char* name = t1;
                int v_idx = -1;

                // check if global variable exists
                for (int i = 0; i < varc; i++) {

                    if (!strncmp(variables[i], name, MAX)) {

                        v_idx = i;
                        break;
                    }
                }

                if (v_idx == -1) { // new

                    v_idx = varc++;
                    strncpy(variables[v_idx], name, MAX);
                }

                var_types[v_idx] = is_rhs_string;


                // rhs
                char load_reg[MAX]; // loaded
                size_t reg = 0;

                if (t3[0] == '\"') { // string literal

                    t3[strlen(t3) - 1] = NIL;
                    strncpy(strings[stringc], t3 + 1, MAX);
                    strings[stringc][MAX - 1] = NIL;

                    reg = map_reg_arm();
                    reg = reg ? reg : 16;

                    fprintf(s, "    adrp x%zu, msg%zu@PAGE\n", reg, stringc);
                    fprintf(s, "    add x%zu, x%zu, msg%zu@PAGEOFF\n", reg, reg, stringc++);
                    snprintf(load_reg, MAX, "x%zu", reg);

                } else
                    reg = fetch_operand(s, t3, phys_reg, load_reg);

                size_t dest = map_reg_arm();
                dest = dest ? dest : 17;

                fprintf(s, "    adrp x%zu, _%s@PAGE\n", dest, name);
                fprintf(s, "    str %s, [x%zu, _%s@PAGEOFF]\n", load_reg, dest, name);

                free_reg_arm(dest);
                free_reg_arm(reg);

            } else { // arithmetic or temporary assignment

                size_t dest;
                sscanf(t1, "%%t%zu", &dest);

                size_t reg = map_reg_arm();
                char arm_reg[MAX];

                if (!reg) {

                    phys_reg[dest] = -stack_offset;
                    snprintf(arm_reg, MAX, "x16");

                } else {

                    if (dest >= MAX) {

                        fprintf(stderr, "TRANSPILER ERROR: Register index %zu exceeds MAX = %d\n", dest, MAX - 1);
                        exit(EX_DATAERR);
                    }

                    phys_reg[dest] = reg;
                    snprintf(arm_reg, MAX, "x%zu", reg);
                }

                // arithmetic
                if (!strncmp(t3, "ADD", MAX) || !strncmp(t3, "SUB", MAX) || !strncmp(t3, "MUL", MAX) ||
                    !strncmp(t3, "DIV", MAX) || !strncmp(t3, "REM", MAX) || !strncmp(t3, "POW", MAX)) {

                    char r1[16];
                    int p1 = fetch_operand(s, t4, phys_reg, r1);

                    char r2[16];
                    int p2 = fetch_operand(s, t5, phys_reg, r2);

                    if (!strncmp(t3, "ADD", MAX))
                        fprintf(s, "    add %s, %s, %s\n", arm_reg, r1, r2);

                    else if (!strncmp(t3, "SUB", MAX))
                        fprintf(s, "    sub %s, %s, %s\n", arm_reg, r1, r2);

                    else if (!strncmp(t3, "MUL", MAX))
                        fprintf(s, "    mul %s, %s, %s\n", arm_reg, r1, r2);

                    else if (!strncmp(t3, "DIV", MAX))
                        fprintf(s, "    sdiv %s, %s, %s\n", arm_reg, r1, r2);

                    else if (!strncmp(t3, "REM", MAX)) {

                        size_t p = map_reg_arm();
                        p = p ? p : 17;

                        fprintf(s, "    sdiv x%zu, %s, %s\n", p, r1, r2);
                        fprintf(s, "    msub %s, x%zu, %s, %s\n", arm_reg, p, r2, r1);
                        free_reg_arm(p);

                    } else if (!strncmp(t3, "POW", MAX)) {

                        fprintf(s, "    scvtf d0, %s\n", r1);
                        fprintf(s, "    scvtf d1, %s\n", r2);

                        // libc-pow clobbers scratch registers x8-x15
                        // save on stack first
                        fputs("    sub sp, sp, #64\n", s);
                        fputs("    stp x8, x9, [sp, #0]\n", s);
                        fputs("    stp x10, x11, [sp, #16]\n", s);
                        fputs("    stp x12, x13, [sp, #32]\n", s);
                        fputs("    stp x14, x15, [sp, #48]\n", s);

                        fputs("    bl _pow\n", s);

                        // retrieve
                        fputs("    ldp x8, x9, [sp, #0]\n", s);
                        fputs("    ldp x10, x11, [sp, #16]\n", s);
                        fputs("    ldp x12, x13, [sp, #32]\n", s);
                        fputs("    ldp x14, x15, [sp, #48]\n", s);
                        fputs("    add sp, sp, #64\n", s);

                        fprintf(s, "    fcvtzs %s, d0\n", arm_reg);
                    }

                    free_reg_arm(p1);
                    free_reg_arm(p2);

                } else if (t3[0] == '\"') { // assign string literal to temporary register

                    t3[strlen(t3) - 1] = NIL;
                    strncpy(strings[stringc], t3, MAX);
                    strings[stringc][MAX - 1] = NIL;

                    size_t dest;
                    sscanf(arm_reg, "x%zu", &dest);

                    fprintf(s, "    adrp x%zu, msg%zu@PAGE\n", dest, stringc);
                    fprintf(s, "    add x%zu, x%zu, msg%zu@PAGEOFF\n", dest, dest, stringc++);

                } else { // assign variable to temporary register

                    char load_reg[MAX];
                    size_t reg = fetch_operand(s, t3, phys_reg, load_reg);

                    if (strncmp(arm_reg, load_reg, MAX))
                        fprintf(s, "    mov %s, %s\n", arm_reg, load_reg);

                    free_reg_arm(reg);
                }

                // store spillover onto stack

                if (!reg)
                    fprintf(s, "    str x16, [x29, #-%zu]\n", stack_offset);
                fputc('\n', s);
            }
        }
    }



    // epilogue
    fputs("; === EPILOGUE ===\n", s);
    fputs("mov x0, #0\n", s);
    fputs("mov sp, x29\n", s);
    fputs("ldp x29, x30, [sp], #16\n", s);
    fputs("ret\n\n", s);



    // data section

    if (varc > 0) {

        fputs("\n; === GLOBAL VARIABLES ===\n", s);
        fputs(".section __DATA,__data\n", s);
        fputs(".align 3\n", s);

        for (size_t i = 0; i < varc; i++) 
            fprintf(s, "_%s: .quad 0\n", variables[i]);

        fputc('\n', s);
    }

    // c string literals

    fputs("\n; === C STRINGS ===\n", s);
    fputs(".section __TEXT,__cstring,cstring_literals\n", s);
    fputs("l_msg_int: .asciz \"%lld\"\n", s);
    fputs("l_empty: .asciz \"\"\n", s);

    for (size_t i = 0; i < stringc; i++) {

        size_t k = 0;

        for (size_t j = 0; strings[i][j]; j++) {

            if (strings[i][j] == '\x01')
                strings[i][k++] = ' ';

            // Apple's ABI requires non-escaped ' and ?
            else if (strings[i][j] == '\\' && (strings[i][j+1] == '\?' || strings[i][j+1] == '\''))
                strings[i][k++] = strings[i][++j];

            else
                strings[i][k++] = strings[i][j];
        }
        strings[i][k] = NIL;

        fprintf(s, "msg%zu: .asciz \"%s\"\n", i, strings[i]);
    }
}




#if 0

static inline void map_reg_x86(FILE* s, const char* op, char* out_reg, const char* scratch_reg) {
    if (op[0] == '%') {
        int reg_idx;
        sscanf(op, "%%t%d", &reg_idx);
        snprintf(out_reg, 16, "r%d", (reg_idx % 6) + 10); 
    } else {
        long long val = atoll(op);
        fprintf(s, "    mov %s, %lld\n", scratch_reg, val);
        snprintf(out_reg, 16, "%s", scratch_reg);
    }
}

// TODO: fix multiline strings and strings with quotes inside them
void transpile_nasm_x86_64(FILE* ir, FILE* s) {

    char line[MAX];
    char variables[MAX][64];
    int var_types[MAX] = {0};
    int varc = 0;
    int reg_types[1024] = {0};
    char strings[MAX][256];
    int stringc = 0;

    // Prologue (System V ABI)
    fputs("; === PROLOGUE ===\n", s);
    fputs("section .text\n", s);
    fputs("global main\n", s);
    fputs("extern printf\n", s);
    fputs("extern puts\n", s);
    fputs("extern pow\n", s);
    fputs("main:\n", s);
    fputs("    push rbp\n", s);
    fputs("    mov rbp, rsp\n", s);
    fputs("    sub rsp, 32\n\n", s);

    while (fgets(line, sizeof(line), ir)) {
        if (line[0] == '\n' || line[0] == '\r') continue;

        char* quote_start = strchr(line, '\"');
        if (quote_start) {
            char* quote_end = strrchr(line, '\"');
            if (quote_end && quote_end > quote_start) {
                for (char* p = quote_start; p < quote_end; p++) {
                    if (*p == ' ') *p = '\x01';
                }
            }
        }

        char t1[MAX], t2[MAX], t3[MAX], t4[MAX], t5[MAX], t6[MAX];
        int n = sscanf(line, "%s %s %s %s %s %s", t1, t2, t3, t4, t5, t6);

        for (int i = 0; i < strlen(line); i++) {

            if (line[i] == '\x01')
                line[i] = ' ';
        }

        fprintf(s, "    ; %s", line);

        if (n == 1 && !strncmp(t1, "NEWLINE", MAX)) {

            fputs("    lea rdi, [rel empty]\n", s);
            fputs("    call puts\n\n", s);
        } 
        
        else if (n >= 2 && !strncmp(t1, "PRINT", MAX)) {

            if (is_string(t2, variables, var_types, varc, reg_types)) {

                if (t2[0] == '\"') {

                    for (int i = 0; t2[i]; i++) {

                        if (t2[i] == '\x01')
                            t2[i] = ' ';
                    }

                    size_t len = strlen(t2);
                    if (len >= 2 && t2[0] == '\"' && t2[len - 1] == '\"') {

                        t2[len - 1] = NIL;
                        strncpy(strings[stringc], t2 + 1, MAX); // skip NASM quotes

                    } else
                        strncpy(strings[stringc], t2, MAX);

                    fprintf(s, "    lea rdi, [rel msg%d]\n", stringc++);

                } else if (t3[0] == '%') {

                    char x86_reg[16];
                    map_reg_x86(s, t2, x86_reg, "rdi");

                    if (strcmp(x86_reg, "rdi") != 0)
                        fprintf(s, "    mov rdi, %s\n", x86_reg);

                } else
                    fprintf(s, "    mov rdi, [rel _%s]\n", t2);

                fputs("    xor eax, eax ; printf is variadic\n", s);
                fputs("    call printf\n\n", s);

            } else {

                char x86_reg[16];

                if (t2[0] == '%')
                    map_reg_x86(s, t2, x86_reg, "rsi");

                else if (isalpha(t2[0]) || t2[0] == '_') {

                    fprintf(s, "    mov rsi, [rel _%s]\n", t2);
                    strcpy(x86_reg, "rsi");

                } else
                    map_reg_x86(s, t2, x86_reg, "rsi");

                fputs("    lea rdi, [rel l_msg_int]\n", s);

                if (strcmp(x86_reg, "rsi") != 0)
                    fprintf(s, "    mov rsi, %s\n", x86_reg);

                fputs("    xor eax, eax\n", s);
                fputs("    call printf\n\n", s);
            }
        }

        else if (n >= 3 && !strncmp(t2, "=", MAX)) {

            int is_rhs_string = is_string(t3, variables, var_types, varc, reg_types);
            
            if (t1[0] != '%') { // Assignment to variable

                char* name = t1;
                int v_idx = -1;

                for (int i = 0; i < varc; i++) {
                    if (!strncmp(variables[i], name, MAX))
                        v_idx = i; break;
                }

                if (v_idx == -1) {

                    v_idx = varc++;
                    strncpy(variables[v_idx], name, MAX);
                }

                var_types[v_idx] = is_rhs_string;


                char src_reg[16];

                if (t3[0] == '\"') {

                    for (int i = 0; t3[i]; i++) {

                        if (t3[i] == '\x01')
                            t3[i] = ' ';
                    }

                    strncpy(strings[stringc], t3, 255);
                    strings[stringc][255] = '\0';

                    fprintf(s, "    lea rax, [rel msg%d]\n", stringc++);
                    strcpy(src_reg, "rax");

                } else if (isalpha(t3[0]) || t3[0] == '_') {

                    fprintf(s, "    mov rax, [rel _%s]\n", t3);
                    strcpy(src_reg, "rax");

                } else
                    map_reg_x86(s, t3, src_reg, "rax");

                fprintf(s, "    mov [rel _%s], %s\n\n", name, src_reg);
            } 
            else { // Arithmetic or Temporary Assignment

                int d_idx; sscanf(t1, "%%t%d", &d_idx);
                char hw_dest[16]; snprintf(hw_dest, 16, "r%d", (d_idx % 6) + 10);
                reg_types[d_idx] = is_rhs_string;

                if (!strncmp(t3, "ADD", MAX) || !strncmp(t3, "SUB", MAX) || !strncmp(t3, "MUL", MAX) || !strncmp(t3, "DIV", MAX) || !strncmp(t3, "REM", MAX) || !strncmp(t3, "POW", MAX)) {

                    char reg1[16], reg2[16];
                    map_reg_x86(s, t4, reg1, "rax");
                    map_reg_x86(s, t5, reg2, "rcx");

                    if (!strncmp(t3, "ADD", MAX))
                        fprintf(s, "    mov %s, %s\n    add %s, %s\n", hw_dest, reg1, hw_dest, reg2);
                    
                    else if (!strncmp(t3, "SUB", MAX))
                        fprintf(s, "    mov %s, %s\n    sub %s, %s\n", hw_dest, reg1, hw_dest, reg2);

                    else if (!strncmp(t3, "MUL", MAX))
                        fprintf(s, "    mov rax, %s\n    imul rax, %s\n    mov %s, rax\n", reg1, reg2, hw_dest);

                    else if (!strncmp(t3, "DIV", MAX) || !strncmp(t3, "REM", MAX)) {
                        fprintf(s, "    mov rax, %s\n    cqo\n    idiv %s\n", reg1, reg2);
                        fprintf(s, "    mov %s, %s\n", hw_dest, !strncmp(t3, "REM", MAX) ? "rdx" : "rax");

                    } else if (!strncmp(t3, "REM", MAX)) {
                        fprintf(s, "    mov rax, %s\n    cqo\n    idiv %s\n", reg1, reg2);
                        fprintf(s, "    mov %s, rdx\n", hw_dest);


                    } else if (!strncmp(t3, "POW", MAX)) {

                        char base_reg[16], exp_reg[16];

                        map_reg_x86(s, t4, base_reg, "rax");
                        map_reg_x86(s, t5, exp_reg, "rcx");

                        fprintf(s, "    cvtsi2sd xmm0, %s\n", base_reg);
                        fprintf(s, "    cvtsi2sd xmm1, %s\n", exp_reg);
                        fputs("    call pow\n", s);
                        fprintf(s, "    cvttsd2si %s, xmm0\n\n", hw_dest);
}
                } else if (isalpha(t3[0]) || t3[0] == '_') // variable
                                                             //
                    fprintf(s, "    mov %s, [rel _%s]\n", hw_dest, t3);

                else if (t3[0] == '\"') { // string literal

                    for (int i = 0; t3[i]; i++) {

                        if (t3[i] == '\x01')
                            t3[i] = ' ';
                    }

                    strncpy(strings[stringc], t3, MAX);
                    fprintf(s, "    lea %s, [rel msg%d]\n", hw_dest, stringc++);

                } else // literal
                    map_reg_x86(s, t3, hw_dest, hw_dest);

            }
        }
    }

    // Epilogue
    fputs("; === EPILOGUE ===\n", s);
    fputs("    mov eax, 0\n", s);
    fputs("    leave\n", s);
    fputs("    ret\n\n", s);

    // Data Sections
    if (varc > 0) {
        fputs("section .data\n", s);
        for (int i = 0; i < varc; i++) fprintf(s, "_%s: dq 0\n", variables[i]);
    }

    fputs("\nsection .rodata\n", s);
    fputs("l_msg_int: db \"%lld\", 0\n", s);
    fputs("empty: db 0\n", s);

    for (int i = 0; i < stringc; i++) {

        fprintf(s, "msg%d: db ", i);

        for (int j = 0; strings[i][j]; j++) {

            if (strings[i][j] == '\\') {

                j++;

                switch (strings[i][j]) {

                    case 'n':  fputs("10, ", s); break;
                    case 'r':  fputs("13, ", s); break;
                    case 't':  fputs("9, ", s);  break;
                    case 'a':  fputs("7, ", s);  break;
                    case '?':  fputs("63, ", s); break;
                    case '\'': fputs("39, ", s); break;
                    case '\"': fputs("34, ", s); break;
                    case '\\': fputs("92, ", s); break;

                    case NIL:
                        j--;
                        fputs("92, ", s);
                        break;

                    default:
                        fprintf(s, "%d, ", (unsigned char)strings[i][j]);
                        break;
                }

            } else
                fprintf(s, "%d, ", (unsigned char)strings[i][j]);
        }

        fputs("0\n", s);
    }
}

#endif





