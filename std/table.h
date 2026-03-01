// ONLY WORKS for symbol tables with the structure { char* name, size_t offset, ... }
// first 2 members are fixed


#pragma once

#ifdef STD_TABLE

#include <stddef.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "./header.h"

typedef uint32_t u32;
typedef ptrdiff_t idx_t;


// FNV-1a hash function
static inline u32 _table_hash(const char* name) {

    u32 hash = 2166136261u;

    for (int i = 0; name[i]; i++) {

        hash ^= (unsigned char)name[i];
        hash *= 16777619;
    }

    return hash;
}


static inline void* _table_grow(void* m, size_t elem_size) {

    size_t old_cap = m ? ((Header*)m - 1)->capacity : 0;
    size_t new_cap = old_cap ? old_cap * 2 : 16;
    
    Header* new_hdr = (Header*)calloc(1, sizeof(Header) + (new_cap * elem_size));
    new_hdr->capacity = new_cap;
    new_hdr->count = m ? ((Header*)m - 1)->count : 0;
    void* new_m = new_hdr + 1;
    
    if (m) {

        for (size_t i = 0; i < old_cap; i++) {

            char* name = *(char**)((char*)m + (i * elem_size));

            if (name) {

                u32 idx = _table_hash(name) & (new_cap - 1);

                while (*(char**)((char*)new_m + (idx * elem_size)) != NULL)
                    idx = (idx + 1) & (new_cap - 1);

                memcpy((char*)new_m + (idx * elem_size), (char*)m + (i * elem_size), elem_size);
            }
        }

        free((Header*)m - 1);
    }

    return new_m;
}


static inline idx_t _table_find(void* m, size_t cap, size_t elem_size, const char* k) {

    if (!m) return -1;

    u32 idx = _table_hash(k) & (cap - 1);

    for (;;) {

        char* name = *(char**)((char*)m + (idx * elem_size));

        if (!name)
            return -1;

        if (!strcmp(name, k))
            return idx;

        idx = (idx + 1) & (cap - 1);
    }
}


#define table_header(m) ((Header*)(m) - 1)

#define table_len(m) ((m) ? table_header(m)->count : 0)

#define table_free(m) \
    do { \
 \
        if (m) \
            free(table_header(m)); \
\
    } while(0)


#define table_get(m, k) \
    ((m) ? _table_find((m), table_header(m)->capacity, sizeof(*(m)), (k)) : -1)


#define table_put(m, k, v) \
    do { \
        if (!(m) || table_header(m)->count * 2 >= table_header(m)->capacity) \
            (m) = _table_grow((m), sizeof(*(m))); \
\
        u32 _i = _table_hash(k) & (table_header(m)->capacity - 1); \
\
        while ((m)[_i].name && strncmp((m)[_i].name, k, MAX)) \
            _i = (_i + 1) & (table_header(m)->capacity - 1); \
\
        if (!(m)[_i].name) table_header(m)->count++; \
\
        (m)[_i].name = (char*)(k); \
        (m)[_i].offset = (v); \
\
    } while(0)


#endif // STD_TABLE





