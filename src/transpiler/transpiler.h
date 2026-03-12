#pragma once

#include "../common.h"

#define STACK_MAX 4096


void transpile_darwin_ARM64(FILE* ir, FILE* s);
void transpile_gnu_x86_64(FILE* ir, FILE* s);



