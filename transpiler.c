#include "./transpiler.h"
#include "./common.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>


static bool free_bitmap[8] = {true, true, true, true, true, true, true, true};

static size_t stack_offset = 512;
static size_t print_idx = 0;

char strings[MAX][MAX];
size_t stringc = 0;



static inline int map_reg_arm(void) {

    // scratch registers: x9 - x15 -> CLOBBERED!!
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


static inline size_t fetch_operand_arm(FILE* s, const char* op, int phys_reg[], char* out_reg) {
    
    if (op[0] == '%') { // temporary register

        size_t idx;
        sscanf(op, "%%t%zu", &idx);

        int loc = phys_reg[idx];
        size_t offset = -loc;
        
        size_t reg = map_reg_arm();
        reg = reg ? reg : 16;

        if (offset <= 504)
            fprintf(s, "    ldp x%zu, x1, [x29, #%d]\n", reg, loc);

        else {

            fprintf(s, "    mov x17, #%zu\n", offset);
            fputs("    sub x17, x29, x17\n", s);
            fprintf(s, "    ldp x%zu, x1, [x17]\n", reg);
        }
        
        snprintf(out_reg, 16, "x%zu", reg);
        return reg;
    }

    
    if (op[0] == '@') { // local variable

        size_t offset;
        sscanf(op, "@%zu", &offset);

        size_t reg = map_reg_arm();
        reg = reg ? reg : 16;

        if (offset <= 504)
            fprintf(s, "    ldp x%zu, x1, [x29, #-%zu]\n", reg, offset);

        else {

            fprintf(s, "    mov x17, #%zu\n", offset);
            fputs("    sub x17, x29, x17\n", s);
            fprintf(s, "    ldp x%zu, x1, [x17]\n", reg);
        }

        snprintf(out_reg, 16, "x%zu", reg);
        return reg;
    }


    // global variable

    size_t reg = map_reg_arm();
    reg = reg ? reg : 16;

    if (isalpha(op[0]) || op[0] == '_') {

        fprintf(s, "    adrp x%zu, _%s@PAGE\n", reg, op);
        fprintf(s, "    add x%zu, x%zu, _%s@PAGEOFF\n", reg, reg, op);
        fprintf(s, "    ldp x%zu, x1, [x%zu]\n", reg, reg);

    // boolean

    } else if (op[0] == '#') {
        
        bool val = op[1] == 't';
        fprintf(s, "    mov x%zu, #%d\n", reg, val);
        fputs("    mov x1, #2\n", s);

    } else if (op[0] == '\"') {
        
        char temp[MAX];
        strncpy(temp, op, MAX);
        temp[strlen(temp) - 1] = NIL;
        strncpy(strings[stringc], temp + 1, MAX);
        strings[stringc][MAX - 1] = NIL;
        
        fprintf(s, "    adrp x%zu, msg%zu@PAGE\n", reg, stringc);
        fprintf(s, "    add x%zu, x%zu, msg%zu@PAGEOFF\n", reg, reg, stringc++);
        fputs("    mov x1, #1\n", s);

    } else { // number

        fprintf(s, "    mov x%zu, #%lld\n", reg, atoll(op));
        fputs("    mov x1, #0\n", s);
    }

    snprintf(out_reg, 16, "x%zu", reg);
    return reg;
}




static inline bool is_string(char* literal, char variables[MAX][MAX], bool var_types[], int varc, bool reg_types[], bool local_types[]) {

    if (literal[0] == '\"')
        return true;

    if (literal[0] == '%') {

        size_t r_idx;
        sscanf(literal, "%%t%zu", &r_idx);

        return reg_types[r_idx];
    }

    if (literal[0] == '@') {

        size_t offset;
        sscanf(literal, "@%zu", &offset);

        return local_types[offset];
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
    bool local_types[MAX] = {false};

    int phys_reg[MAX] = {0};

    FILE* out = s;


    // prologue
    fputs("; === PROLOGUE ===\n", out);
    fputs(".section __TEXT,__text,regular,pure_instructions\n", out);
    fputs(".global _main\n", out);
    fputs(".align 2\n\n", out);
    fputs("; === GLOBAL ENTRY ===\n", out);
    fputs("_main:\n", out);
    fputs("    stp x29, x30, [sp, #-16]!\n", out);
    fputs("    mov x29, sp\n", out);
    fputs("    ; STACK SPILL BUFFER\n", out);
    fputs("    sub sp, sp, #1024\n\n", out);


    // function buffer
    FILE* buf = tmpfile();
    size_t frame;
    bool frame_reg[8];
    int frame_phys_reg[MAX] = {0};


    // read IR

    while (fgets(line, MAX, ir)) {

        if (line[0] == '\n' || line[0] == '\r')
            continue;


        bool in_quote = false;
        for (char* p = line; *p; p++) {

            if (*p == '\"')
                in_quote = !in_quote;

            else if (in_quote && *p == ' ')
                *p = '\x01';
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

        if (n == 2 && !strncmp(t1, "FUNC", MAX)) {

            out = buf;
            fprintf(out, "; %s", line); // IR instruction as comment
        
        } else if (n == 2 && !strncmp(t1, "END", MAX)) {

            fprintf(out, "; %s\n", line); // IR instruction as comment
            out = s;

        } else
            fprintf(out, "    ; %s", line); // IR instruction as comment

        if (n == 1 && !strncmp(t1, "PANIC", MAX)) // panic
            fputs("    bl _abort\n", out);

        else if (n == 1 && !strncmp(t1, "ABORT", MAX)) { // error

            fprintf(out, "    mov x0, %d\n", EX_PROTOCOL);
            fputs("    bl _exit\n", out);

        } else if (n == 1 && !strncmp(t1, "NEWLINE", MAX)) { // newline

            // fputs("    adrp x0, l_empty@PAGE\n", out);
            // fputs("    add x0, x0, l_empty@PAGEOFF\n", out);
            // fputs("    bl _puts\n\n", out);

            fputs("    mov x0, 10\n", out);
            fputs("    bl _putchar\n", out);

        } else if (n == 2 && !strncmp(t1, "FUNC", MAX)) { // function

            // PROLOGUE

            fprintf(out, "F%s:\n", t2);
            fputs("; === PROLOGUE ===\n", out);
            fputs("    stp x29, x30, [sp, #-16]!\n", out);
            fputs("    mov x29, sp\n", out);
            fputs("    sub sp, sp, #1024\n", out);

            frame = stack_offset;
            memcpy(frame_reg, free_bitmap, sizeof(free_bitmap));

            memcpy(frame_phys_reg, phys_reg, sizeof(phys_reg));
            memset(phys_reg, 0, sizeof(phys_reg));

            stack_offset = 512;

            for (size_t i = 0; i < 8; i++)
                free_bitmap[i] = true;

        } else if (n == 2 && !strncmp(t1, "END", MAX)) {

            stack_offset = frame;
            memcpy(free_bitmap, frame_reg, sizeof(frame_reg));
            memcpy(phys_reg, frame_phys_reg, sizeof(frame_phys_reg));

        } else if (n == 2 && !strncmp(t1, "RET", MAX)) {

            fputs("; === EPILOGUE ===\n", out);

            if (t2[0] == '\"') {

                t2[strlen(t2) - 1] = NIL;
                strncpy(strings[stringc], t2 + 1, MAX);
                strings[stringc][MAX - 1] = NIL;

                fprintf(out, "    adrp x0, msg%zu@PAGE\n", stringc);
                fprintf(out, "    add x0, x0, msg%zu@PAGEOFF\n", stringc++);

                fputs("    mov x1, #1\n", out);

            } else if (t2[0] == '#') {

                fprintf(out, "    mov x0, #%d\n", t2[1] == 't');
                fputs("    mov x1, #2\n", out);

            } else {

                char arm_reg[MAX];
                int reg = fetch_operand_arm(out, t2, phys_reg, arm_reg);

                if (strncmp(arm_reg, "x0", MAX))
                    fprintf(out, "    mov x0, %s\n", arm_reg);

                free_reg_arm(reg);
            }

            fputs("    mov sp, x29\n", out);
            fputs("    ldp x29, x30, [sp], #16\n", out);
            fputs("    ret\n", out);

        } else if (n == 2 && !strncmp(t1, "PRINT", MAX)) { // print

            if (t2[0] == '\"') { 

                t2[strlen(t2) - 1] = NIL;
                strncpy(strings[stringc], t2 + 1, MAX);
                strings[stringc][MAX - 1] = NIL;
                
                fprintf(out, "    adrp x0, msg%zu@PAGE\n", stringc);
                fprintf(out, "    add x0, x0, msg%zu@PAGEOFF\n", stringc++);
                fputs("    mov x1, #1\n", out);

            } else if (t2[0] == '#') {

                fprintf(out, "    mov x0, #%d\n", t2[1] == 't');
                fputs("    mov x1, #2\n", out);

            } else if (isdigit(t2[0]) || t2[0] == '-') {

                fprintf(out, "    mov x0, #%lld\n", atoll(t2));
                fputs("    mov x1, #0\n", out);

            } else {

                char arm_reg[MAX];
                int reg = fetch_operand_arm(out, t2, phys_reg, arm_reg);
                fprintf(out, "    mov x0, %s\n", arm_reg);
                free_reg_arm(reg);
            }

            fputs("    cmp x1, #1\n", out);
            fprintf(out, "    beq L_print%zu\n", print_idx);

            fputs("    cmp x1, #2\n", out);
            fprintf(out, "    beq L_printb%zu\n", print_idx);

            fputs("    mov x1, x0\n", out);
            fputs("    adrp x0, l_msg_int@PAGE\n", out);
            fputs("    add x0, x0, l_msg_int@PAGEOFF\n", out);
            fputs("    str x1, [sp, #-16]!\n", out);
            fputs("    bl _printf\n", out);
            fputs("    add sp, sp, #16\n", out);
            fprintf(out, "    b L_print_end%zu\n", print_idx);

            fprintf(out, "L_print%zu:\n", print_idx);
            fputs("    bl _printf\n", out);
            fprintf(out, "    b L_print_end%zu\n", print_idx);

            fprintf(out, "L_printb%zu:\n", print_idx);
            fputs("    adrp x9, l_msg_true@PAGE\n", out);
            fputs("    add x9, x9, l_msg_true@PAGEOFF\n", out);
            fputs("    adrp x10, l_msg_false@PAGE\n", out);
            fputs("    add x10, x10, l_msg_false@PAGEOFF\n", out);
            fputs("    cmp x0, #1\n", out);
            fputs("    csel x0, x9, x10, eq\n", out);
            fputs("    bl _printf\n", out);

            fprintf(out, "L_print_end%zu:\n\n", print_idx++);

        } else if (n == 2 && !strncmp(t1, "JMP", MAX))
            fprintf(out, "    b %s\n", t2);

        else if (n == 3 && !strncmp(t1, "JMPF", MAX)) {

            char arm_reg[MAX];
            size_t reg = fetch_operand_arm(out, t2, phys_reg, arm_reg);
            (void)reg;

            fputs("    cmp x1, #2\n", out);
            fprintf(out, "    ccmp %s, #0, #0, eq\n", arm_reg);
            fprintf(out, "    beq %s\n", t3);

        } else if (n == 2 && !strncmp(t1, "LABEL", MAX))
            fprintf(out, "%s:\n", t2);

        else if (n == 3 && !strncmp(t1, "PARAM", MAX)) {

            size_t offset;
            sscanf(t2, "@%zu", &offset);

            size_t arg_idx;
            sscanf(t3, "%zu", &arg_idx);

            if (arg_idx < 4)
                fprintf(out, "    stp x%zu, x%zu, [x29, #-%zu]\n\n", arg_idx * 2, (arg_idx * 2) + 1, offset);

            else { 

                size_t stack_arg_offset = 16 + ((arg_idx - 4) * 16) + 64;

                fprintf(out, "    ldp x16, x17, [x29, #%zu]\n", stack_arg_offset);
                fprintf(out, "    stp x16, x17, [x29, #-%zu]\n\n", offset);
            }

        } else if (n == 3 && !strncmp(t1, "ARG", MAX)) {

            size_t arg_idx;
            sscanf(t2, "%zu", &arg_idx);

            char arm_reg[MAX];

            // fetch argument

            if (t3[0] == '\"') { 
                
                t3[strlen(t3) - 1] = NIL;
                strncpy(strings[stringc], t3 + 1, MAX);
                strings[stringc][MAX - 1] = NIL;

                size_t reg = map_reg_arm();
                reg = reg ? reg : 16;

                fprintf(out, "    adrp x%zu, msg%zu@PAGE\n", reg, stringc);
                fprintf(out, "    add x%zu, x%zu, msg%zu@PAGEOFF\n", reg, reg, stringc++);
                snprintf(arm_reg, MAX, "x%zu", reg);
                
                fputs("    mov x1, #1\n", out);
                free_reg_arm(reg);

            } else if (t3[0] == '#') {

                size_t reg = map_reg_arm();
                reg = reg ? reg : 16;

                fprintf(out, "    mov x%zu, #%d\n", reg, t3[1] == 't');
                fputs("    mov x1, #2\n", out);
                snprintf(arm_reg, MAX, "x%zu", reg);
                free_reg_arm(reg);

            } else {
                
                size_t reg = fetch_operand_arm(out, t3, phys_reg, arm_reg);
                free_reg_arm(reg);
            }

            // Apple's ABI requires first 8 arguments to a function call
            // to be loaded onto registers x0-x7
            // and extra arguments to be loaded onto stack

            // EDIT: using fat pointers halves the number of registers
            // used for passing arguments


            if (arg_idx < 4) { // was 8 before fat pointers

                fprintf(out, "    mov x%zu, %s\n", arg_idx * 2, arm_reg);
                fprintf(out, "    mov x%zu, x1\n", (arg_idx * 2) + 1);
            }

            else // Apple's ABI requires 16-bit alignment, so 8 bytes added padding
                fprintf(out, "    stp %s, x1, [sp, #-16]!\n", arm_reg);

        } else if (n == 5 && !strncmp(t3, "CALL", MAX)) {

            size_t args;
            sscanf(t5, "%zu", &args);

            // save clobbered scratch registers onto stack
            fputs("    sub sp, sp, #64\n", out);
            fputs("    stp x8, x9, [sp, #0]\n", out);
            fputs("    stp x10, x11, [sp, #16]\n", out);
            fputs("    stp x12, x13, [sp, #32]\n", out);
            fputs("    stp x14, x15, [sp, #48]\n", out);

            // call and jump
            fprintf(out, "    bl F%s\n", t4);

            // retrieve clobbered registers from stack
            fputs("    ldp x8, x9, [sp, #0]\n", out);
            fputs("    ldp x10, x11, [sp, #16]\n", out);
            fputs("    ldp x12, x13, [sp, #32]\n", out);
            fputs("    ldp x14, x15, [sp, #48]\n", out);
            fputs("    add sp, sp, #64\n\n", out);

            // free spillover stack space
            if (args > 4)
                fprintf(out, "    add sp, sp, #%zu\n", (args - 4) * 16);

            // return value
            size_t res;
            sscanf(t1, "%%t%zu", &res);

            stack_offset += 16;
            phys_reg[res] = -stack_offset;

                if (stack_offset <= 504)
                    fprintf(out, "    stp x0, x1, [x29, #-%zu]\n\n", stack_offset);

                else {

                    fprintf(out, "    mov x17, #%zu\n", stack_offset);
                    fputs("    sub x17, x29, x17\n", out);
                    fprintf(out, "    stp x0, x1, [x17]\n\n");
                }

        } else if (n >= 3 && !strncmp(t2, "=", MAX)) { // assignment

            int is_rhs_string = is_string(t3, variables, var_types, varc, reg_types, local_types);

            // lhs

            if (t1[0] == '@') { // local variable

                size_t offset;
                sscanf(t1, "@%zu", &offset);

                local_types[offset] = is_rhs_string;

                char load_reg[MAX]; 
                size_t reg = 0;

                if (t3[0] == '\"') { // string literal
                    
                    t3[strlen(t3) - 1] = NIL;
                    strncpy(strings[stringc], t3 + 1, MAX);
                    strings[stringc][MAX - 1] = NIL;

                    reg = map_reg_arm();
                    reg = reg ? reg : 16;

                    fprintf(out, "    adrp x%zu, msg%zu@PAGE\n", reg, stringc);
                    fprintf(out, "    add x%zu, x%zu, msg%zu@PAGEOFF\n", reg, reg, stringc++);
                    snprintf(load_reg, MAX, "x%zu", reg);
                    fputs("    mov x1, #1\n", out);

                } else if (t3[0] == '#') {

                    size_t reg = map_reg_arm();
                    reg = reg ? reg : 16;

                    fprintf(out, "    mov x%zu, #%d\n", reg, t3[1] == 't');
                    fputs("    mov x1, #2\n", out);
                    snprintf(load_reg, MAX, "x%zu", reg);
                    free_reg_arm(reg);

                } else
                    reg = fetch_operand_arm(out, t3, phys_reg, load_reg);


                if (offset <= 504)
                    fprintf(out, "    stp %s, x1, [x29, #-%zu]\n\n", load_reg, offset);

                else {

                    fprintf(out, "    mov x17, #%zu\n", offset);
                    fputs("    sub x17, x29, x17\n", out);
                    fprintf(out, "    stp %s, x1, [x17]\n\n", load_reg);
                }

                free_reg_arm(reg);

            } else if (t1[0] != '%') { // global variable assignment

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

                    fprintf(out, "    adrp x%zu, msg%zu@PAGE\n", reg, stringc);
                    fprintf(out, "    add x%zu, x%zu, msg%zu@PAGEOFF\n", reg, reg, stringc++);
                    snprintf(load_reg, MAX, "x%zu", reg);
                    fputs("    mov x1, #1\n", out);

                } else
                    reg = fetch_operand_arm(out, t3, phys_reg, load_reg);

                size_t dest = map_reg_arm();
                dest = dest ? dest : 17;

                fprintf(out, "    adrp x%zu, _%s@PAGE\n", dest, name);
                fprintf(out, "    add x%zu, x%zu, _%s@PAGEOFF\n", dest, dest, name);
                fprintf(out, "    stp %s, x1, [x%zu]\n", load_reg, dest);

                free_reg_arm(dest);
                free_reg_arm(reg);

            } else { // arithmetic or temporary assignment

                size_t dest;
                sscanf(t1, "%%t%zu", &dest);

                char arm_reg[MAX];

                if (!phys_reg[dest]) {

                    stack_offset += 16;
                    phys_reg[dest] = -stack_offset;
                }

                snprintf(arm_reg, MAX, "x16");

                if (!strncmp(t3, "NOT", MAX)) {
                        
                    char r1[16];
                    int p1 = fetch_operand_arm(out, t4, phys_reg, r1);

                    fputs("    cmp x1, #2\n", out); 
                    fprintf(out, "    ccmp %s, #0, #0, eq\n", r1); 
                    fprintf(out, "    cset %s, eq\n", arm_reg); 

                    free_reg_arm(p1);
                }

                // arithmetic
                else if (!strncmp(t3, "ADD", MAX) || !strncmp(t3, "SUB", MAX) || !strncmp(t3, "MUL", MAX) ||
                    !strncmp(t3, "DIV", MAX) || !strncmp(t3, "REM", MAX) || !strncmp(t3, "POW", MAX) ||
                    !strncmp(t3, "AND", MAX) || !strncmp(t3, "OR", MAX) ||
                    !strncmp(t3, "LESS", MAX) || !strncmp(t3, "LESS_EQUAL", MAX) ||
                    !strncmp(t3, "GREATER", MAX) || !strncmp(t3, "GREATER_EQUAL", MAX) ||
                    !strncmp(t3, "EQL", MAX) || !strncmp(t3, "NOT_EQUAL", MAX) ||
                    !strncmp(t3, "CONCAT", MAX)) {

                    char r1[16];
                    int p1 = fetch_operand_arm(out, t4, phys_reg, r1);

                    fputs("    mov x2, x1\n", out); // save type tag from clobbering

                    char r2[16];
                    int p2 = fetch_operand_arm(out, t5, phys_reg, r2);

                    if (!strncmp(t3, "ADD", MAX))
                        fprintf(out, "    add %s, %s, %s\n", arm_reg, r1, r2);

                    else if (!strncmp(t3, "SUB", MAX))
                        fprintf(out, "    sub %s, %s, %s\n", arm_reg, r1, r2);

                    else if (!strncmp(t3, "MUL", MAX))
                        fprintf(out, "    mul %s, %s, %s\n", arm_reg, r1, r2);

                    else if (!strncmp(t3, "DIV", MAX))
                        fprintf(out, "    sdiv %s, %s, %s\n", arm_reg, r1, r2);

                    else if (!strncmp(t3, "REM", MAX)) {

                        size_t p = map_reg_arm();
                        p = p ? p : 17;

                        fprintf(out, "    sdiv x%zu, %s, %s\n", p, r1, r2);
                        fprintf(out, "    msub %s, x%zu, %s, %s\n", arm_reg, p, r2, r1);
                        free_reg_arm(p);

                    } else if (!strncmp(t3, "POW", MAX)) {

                        fprintf(out, "    scvtf d0, %s\n", r1);
                        fprintf(out, "    scvtf d1, %s\n", r2);

                        // libc-pow clobbers scratch registers x8-x15
                        // save on stack first
                        fputs("    sub sp, sp, #64\n", out);
                        fputs("    stp x8, x9, [sp, #0]\n", out);
                        fputs("    stp x10, x11, [sp, #16]\n", out);
                        fputs("    stp x12, x13, [sp, #32]\n", out);
                        fputs("    stp x14, x15, [sp, #48]\n", out);

                        fputs("    bl _pow\n", out);

                        // retrieve
                        fputs("    ldp x8, x9, [sp, #0]\n", out);
                        fputs("    ldp x10, x11, [sp, #16]\n", out);
                        fputs("    ldp x12, x13, [sp, #32]\n", out);
                        fputs("    ldp x14, x15, [sp, #48]\n", out);
                        fputs("    add sp, sp, #64\n", out);

                        fprintf(out, "    fcvtzs %s, d0\n", arm_reg);

                    } else if (!strncmp(t3, "LESS", MAX)) {

                        fprintf(out, "    cmp %s, %s\n", r1, r2);
                        fprintf(out, "    cset %s, lt\n", arm_reg);

                    } else if (!strncmp(t3, "LESS_EQUAL", MAX)) {

                        fprintf(out, "    cmp %s, %s\n", r1, r2);
                        fprintf(out, "    cset %s, le\n", arm_reg);

                    } else if (!strncmp(t3, "GREATER", MAX)) {

                        fprintf(out, "    cmp %s, %s\n", r1, r2);
                        fprintf(out, "    cset %s, gt\n", arm_reg);

                    } else if (!strncmp(t3, "GREATER_EQUAL", MAX)) {

                        fprintf(out, "    cmp %s, %s\n", r1, r2);
                        fprintf(out, "    cset %s, ge\n", arm_reg);

                    } else if (!strncmp(t3, "EQL", MAX)) {

                        fprintf(out, "    cmp %s, %s\n", r1, r2);
                        fprintf(out, "    cset %s, eq\n", arm_reg);
                        fputs("    cmp x2, x1\n", out);
                        fputs("    cset x17, eq\n", out);
                        fprintf(out, "    and %s, %s, x17\n", arm_reg, arm_reg);

                    } else if (!strncmp(t3, "NOT_EQUAL", MAX)) {

                        fprintf(out, "    cmp %s, %s\n", r1, r2);
                        fprintf(out, "    cset %s, ne\n", arm_reg);
                        fputs("    cmp x2, x1\n", out);
                        fputs("    cset x17, ne\n", out);
                        fprintf(out, "    orr %s, %s, x17\n", arm_reg, arm_reg);

                    } else if (!strncmp(t3, "AND", MAX) || !strncmp(t3, "OR", MAX)) {
                        
                        fputs("    cmp x2, #2\n", out); 
                        fprintf(out, "    ccmp %s, #0, #0, eq\n", r1); 
                        fputs("    cset x16, ne\n", out);
                        fputs("    cmp x1, #2\n", out); 
                        fprintf(out, "    ccmp %s, #0, #0, eq\n", r2); 
                        fputs("    cset x17, ne\n", out);

                        if (!strncmp(t3, "AND", MAX))
                            fprintf(out, "    and %s, x16, x17\n", arm_reg);

                        else
                            fprintf(out, "    orr %s, x16, x17\n", arm_reg);

                    } else if (!strncmp(t3, "CONCAT", MAX)) {

                        fputs("    sub sp, sp, #64\n", out);
                        fputs("    stp x8, x9, [sp, #0]\n", out);
                        fputs("    stp x10, x11, [sp, #16]\n", out);
                        fputs("    stp x12, x13, [sp, #32]\n", out);
                        fputs("    stp x14, x15, [sp, #48]\n", out);
                        
                        fprintf(out, "    mov x0, %s\n", r1);
                        fputs("    mov x3, x1\n", out);
                        fputs("    mov x1, x2\n", out);
                        fprintf(out, "    mov x2, %s\n", r2);

                        fputs("    bl _builtin_concat\n", out);
                        fprintf(out, "    mov %s, x0\n", arm_reg);

                        fputs("    ldp x8, x9, [sp, #0]\n", out);
                        fputs("    ldp x10, x11, [sp, #16]\n", out);
                        fputs("    ldp x12, x13, [sp, #32]\n", out);
                        fputs("    ldp x14, x15, [sp, #48]\n", out);
                        fputs("    add sp, sp, #64\n", out);
                    }

                    free_reg_arm(p1);
                    free_reg_arm(p2);

            } else if (t3[0] == '\"') { // assign string literal to temporary register

                t3[strlen(t3) - 1] = NIL;
                strncpy(strings[stringc], t3 + 1, MAX);
                strings[stringc][MAX - 1] = NIL;

                size_t dest;
                sscanf(arm_reg, "x%zu", &dest);

                fprintf(out, "    adrp x%zu, msg%zu@PAGE\n", dest, stringc);
                fprintf(out, "    add x%zu, x%zu, msg%zu@PAGEOFF\n", dest, dest, stringc++);

            } else { // assign variable to temporary register

                char load_reg[MAX];
                size_t reg = fetch_operand_arm(out, t3, phys_reg, load_reg);

                if (strncmp(arm_reg, load_reg, MAX))
                    fprintf(out, "    mov %s, %s\n", arm_reg, load_reg);

                free_reg_arm(reg);
            }

            if (!strncmp(t3, "ADD", MAX) || !strncmp(t3, "SUB", MAX) || !strncmp(t3, "MUL", MAX) ||
                !strncmp(t3, "DIV", MAX) || !strncmp(t3, "REM", MAX) || !strncmp(t3, "POW", MAX))
                fputs("    mov x1, #0\n", out);

            else if (t3[0] == '\"' || !strncmp(t3, "CONCAT", MAX))
                fputs("    mov x1, #1\n", out);

            else if (!strncmp(t3, "AND", MAX) || !strncmp(t3, "OR", MAX) ||
                     !strncmp(t3, "LESS",  MAX) || !strncmp(t3, "LESS_EQUAL",  MAX) ||
                     !strncmp(t3, "GREATER",  MAX) || !strncmp(t3, "GREATER_EQUAL",  MAX) ||
                     !strncmp(t3, "EQL",  MAX) || !strncmp(t3, "NOT_EQUAL", MAX) ||
                     !strncmp(t3, "NOT", MAX))
                fputs("    mov x1, #2\n", out);

            size_t offset = -phys_reg[dest]; 
            if (offset <= 504)
                fprintf(out, "    stp x16, x1, [x29, #-%zu]\n\n", offset);

            else {

                fprintf(out, "    mov x17, #%zu\n", offset);
                fputs("    sub x17, x29, x17\n", out);
                fprintf(out, "    stp x16, x1, [x17]\n\n");
            }
        }
    }
}



    // epilogue

    fputs("    ; === EPILOGUE ===\n", out);
    fputs("    mov x0, #0\n", out);
    fputs("    mov sp, x29\n", out);
    fputs("    ldp x29, x30, [sp], #16\n", out);
    fputs("    ret\n\n", out);


    // functions

    fputs("; === FUNCTIONS ===\n\n", out);

    fputs("_builtin_to_string:\n", out);
    fputs("    stp x29, x30, [sp, #-16]!\n", out);
    fputs("    mov x29, sp\n", out);
    fputs("    sub sp, sp, #32\n", out);
    fputs("    cmp x1, #1\n", out);
    fputs("    beq 1f\n", out);
    fputs("    cmp x1, #2\n", out);
    fputs("    beq 2f\n", out);
    fputs("    str x0, [sp, #16]\n", out);
    fputs("    mov x0, #32\n", out);
    fputs("    bl _malloc\n", out);
    fputs("    mov x9, x0\n", out);
    fputs("    ldr x10, [sp, #16]\n", out);
    fputs("    mov x0, x9\n", out);
    fputs("    adrp x1, l_msg_int@PAGE\n", out);
    fputs("    add x1, x1, l_msg_int@PAGEOFF\n", out);
    fputs("    str x9, [sp, #24]\n", out);
    fputs("    str x10, [sp, #-16]!\n", out);
    fputs("    bl _sprintf\n", out);
    fputs("    add sp, sp, #16\n", out);
    fputs("    ldr x0, [sp, #24]\n", out);
    fputs("    ldr x0, [sp, #24]\n", out);
    fputs("    b 1f\n", out);
    fputs("2:\n", out);
    fputs("    adrp x9, l_msg_true@PAGE\n", out);
    fputs("    add x9, x9, l_msg_true@PAGEOFF\n", out);
    fputs("    adrp x10, l_msg_false@PAGE\n", out);
    fputs("    add x10, x10, l_msg_false@PAGEOFF\n", out);
    fputs("    cmp x0, #1\n", out);
    fputs("    csel x0, x9, x10, eq\n", out);
    fputs("1:\n", out);
    fputs("    mov sp, x29\n", out);
    fputs("    ldp x29, x30, [sp], #16\n", out);
    fputs("    ret\n\n", out);

    fputs("_builtin_concat:\n", out);
    fputs("    stp x29, x30, [sp, #-16]!\n", out);
    fputs("    mov x29, sp\n", out);
    fputs("    sub sp, sp, #64\n", out);
    fputs("    stp x2, x3, [sp, #16]\n", out);
    fputs("    bl _builtin_to_string\n", out);
    fputs("    str x0, [sp, #32]\n", out);
    fputs("    ldp x0, x1, [sp, #16]\n", out);
    fputs("    bl _builtin_to_string\n", out);
    fputs("    str x0, [sp, #40]\n", out);
    fputs("    mov x0, #512\n", out);
    fputs("    bl _malloc\n", out);
    fputs("    str x0, [sp, #48]\n", out);
    fputs("    ldr x1, [sp, #32]\n", out);
    fputs("    bl _strcpy\n", out);
    fputs("    ldr x0, [sp, #48]\n", out);
    fputs("    ldr x1, [sp, #40]\n", out);
    fputs("    bl _strcat\n", out);
    fputs("    ldr x0, [sp, #48]\n", out);
    fputs("    mov x1, #1\n", out);
    fputs("    mov sp, x29\n", out);
    fputs("    ldp x29, x30, [sp], #16\n", out);
    fputs("    ret\n\n", out);

    char func[MAX];

    rewind(buf);
    while (fgets(func, MAX, buf))
        fputs(func, out);

    fclose(buf);


    // data section

    if (varc > 0) {

        fputs("\n; === GLOBAL VARIABLES ===\n", out);
        fputs(".section __DATA,__data\n", out);
        fputs(".align 3\n", out);

        for (size_t i = 0; i < varc; i++) 
            fprintf(out, "_%s: .quad 0, 0\n", variables[i]);

        fputc('\n', out);
    }

    // c string literals

    fputs("\n; === C STRINGS ===\n", out);
    fputs(".section __TEXT,__cstring,cstring_literals\n", out);
    fputs("l_msg_int: .asciz \"%lld\"\n", out);
    fputs("l_msg_true: .asciz \"#t\"\n", out);
    fputs("l_msg_false: .asciz \"#f\"\n", out);

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

        fprintf(out, "msg%zu: .asciz \"%s\"\n", i, strings[i]);
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

            if (is_string(t2, variables, var_types, varc, reg_types, local_types)) {

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

            int is_rhs_string = is_string(t3, variables, var_types, varc, reg_types, local_types);
            
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





