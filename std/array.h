#pragma once

#ifdef STD_ARRAY

#include <stddef.h>
#include <stdlib.h>
#include "./header.h"

#define arr_push(arr, x) \
 \
    do { \
 \
        if (!arr) { \
 \
            Header* header = (Header*)malloc((sizeof(*arr) * INIT_CAPACITY) + sizeof(Header)); \
            header->count = 0; \
            header->capacity = INIT_CAPACITY; \
            arr = (void*)(header + 1); \
        } \
 \
        Header* header = (Header*)(arr) - 1; \
 \
        if (header->count >= header->capacity) { \
 \
            header->capacity *= 1.5; \
            header = (Header*)realloc(header, (sizeof(*arr) * header->capacity) + sizeof(Header)); \
            arr = (void*)(header + 1); \
        } \
 \
        (arr)[header->count++] = (x); \
 \
    } while (0)


#define arr_len(arr) (arr) ? ((Header*)(arr) - 1)->count : 0

#define arr_free(arr) \
    do { \
        if (arr) free((Header*)(arr) - 1); \
    } while (0)


#endif // STD_ARRAY




