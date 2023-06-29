#pragma once

#include "ast.h"

Program program_init(Arena* arena);

bool sem_ast(Arena* arena, char* src, Program* prog, AST* ast);
