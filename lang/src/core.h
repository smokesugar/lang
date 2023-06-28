#pragma once

#include "base.h"

typedef struct {
    Arena* arena;
    size_t used;
} Scratch ;

Scratch get_scratch(Arena** conflicts, int conflict_count);
void release_scratch(Scratch* scratch);
