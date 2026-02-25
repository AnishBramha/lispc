#pragma once

#include <stdio.h>


void compile_variable_declaration(FILE* src, FILE* ir);
void compile_function_call(FILE* src, FILE* ir, const char* name);



