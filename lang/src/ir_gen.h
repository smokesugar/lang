#pragma once

#include "ast.h"
#include "ir.h"

IR ir_gen(Arena* arena, AST* ast);
