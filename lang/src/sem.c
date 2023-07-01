#include "sem.h"
#include "core.h"

Type ty_u8  = { .name = {.ptr = "u8",  .len = 2}, .size = 1, .flags = TYPE_IS_FIRST_CLASS };
Type ty_u16 = { .name = {.ptr = "u16", .len = 3}, .size = 2, .flags = TYPE_IS_FIRST_CLASS }; 
Type ty_u32 = { .name = {.ptr = "u32", .len = 3}, .size = 4, .flags = TYPE_IS_FIRST_CLASS };
Type ty_u64 = { .name = {.ptr = "u64", .len = 3}, .size = 8, .flags = TYPE_IS_FIRST_CLASS };

Type ty_i8  = { .name = {.ptr = "i8",  .len = 2}, .size = 1, .flags = TYPE_IS_FIRST_CLASS | TYPE_IS_SIGNED };
Type ty_i16 = { .name = {.ptr = "i16", .len = 3}, .size = 2, .flags = TYPE_IS_FIRST_CLASS | TYPE_IS_SIGNED }; 
Type ty_i32 = { .name = {.ptr = "i32", .len = 3}, .size = 4, .flags = TYPE_IS_FIRST_CLASS | TYPE_IS_SIGNED };
Type ty_i64 = { .name = {.ptr = "i64", .len = 3}, .size = 8, .flags = TYPE_IS_FIRST_CLASS | TYPE_IS_SIGNED };

Type ty_void = { .name = {.ptr = "void", .len = 4}, .size = 0, .flags = TYPE_IS_FIRST_CLASS };

typedef struct {
    Arena* arena;
    char* src;
    Program* prog;
} A;

typedef struct Scope Scope;
struct Scope {
    Scope* parent;
    u32 local_table_size;
    Symbol** local_table;
};

internal bool token_match_string(Token tok, String str) {
    return tok.len == str.len && memcmp(tok.ptr, str.ptr, tok.len) == 0;
}

internal void insert_type(Program* program, Type* type) {
    u32 index = fnv_1_a_hash(type->name.ptr, type->name.len) % program->type_table_size;

    for (int i = 0; i < program->type_table_size; ++i)
    {
        if (!program->type_table[index]) {
            program->type_table[index] = type;
            return;
        }

        assert(program->type_table[index] != type);
        index = (index + 1) % program->type_table_size;
    }

    assert(false && "type table overflow");
}

Program program_init(Arena* arena) {
    Program prog = { 0 };

    prog.type_table_size = 16;
    prog.type_table = arena_push_array(arena, Type*, prog.type_table_size);

    insert_type(&prog, &ty_u8);
    insert_type(&prog, &ty_u16);
    insert_type(&prog, &ty_u32);
    insert_type(&prog, &ty_u64);

    insert_type(&prog, &ty_i8);
    insert_type(&prog, &ty_i16);
    insert_type(&prog, &ty_i32);
    insert_type(&prog, &ty_i64);

    return prog;
}
internal Symbol* find_symbol(Scope* scope, Token name) {
    if (scope->local_table_size > 0) {
        u32 index = fnv_1_a_hash(name.ptr, name.len) % scope->local_table_size;

        for (u32 i = 0; i < scope->local_table_size; ++i)
        {
            if (!scope->local_table[index]) {
                break;
            }
            else if (token_match_string(name, scope->local_table[index]->name)) {
                return scope->local_table[index];
            }

            index = (index + 1) % scope->local_table_size;
        }
    }

    if (scope->parent)
        return find_symbol(scope->parent, name); 

    return 0;
}

internal Type* find_type(Program* prog, Token type_name) {
    u32 index = fnv_1_a_hash(type_name.ptr, type_name.len) % prog->type_table_size;

    for (int i = 0; i < prog->type_table_size; ++i) {
        if (!prog->type_table[index])
            break;
        else if (token_match_string(type_name, prog->type_table[index]->name))
            return prog->type_table[index];

        index = (index + 1) % prog->type_table_size;
    }

    return 0;
}

internal u64 get_primitive_priority(Type* type) {
    return type->size * 2 + ((type->flags & TYPE_IS_SIGNED) == 0);
}

internal AST* cast(Arena* arena, AST* expr, Type* type) {
    AST* c = new_ast(arena, AST_CAST, expr->tok);
    c->type = type;
    c->cast.expr = expr;
    return c;
}

internal bool check_assign_types(A* a, Token tok, Type* left_type, AST** right) {
    bool success = true;

    if ((*right)->kind == AST_INT) {
        (*right)->type = left_type;
    }
    else if ((*right)->type != left_type) {
        if (get_primitive_priority((*right)->type) <
            get_primitive_priority(left_type))
        {
            *right = cast(a->arena, *right, left_type);
        }
        else {
            error_tok(a->src, tok, "lvalue cannot store this value because of its type");
            success = false;
        }
    }

    return success;
}

internal bool sem(A* a, Scope* scope, AST* ast) {
    static_assert(NUM_AST_KINDS == 18, "not all ast kinds handled");
    switch (ast->kind) {
        default:
            assert(false);
            return false;

        case AST_INT:
            ast->type = &ty_u64;
            return true;

        case AST_VAR: {
            Symbol* sym = find_symbol(scope, ast->var.name);
            if (!sym) {
                error_tok(a->src, ast->var.name, "symbol does not exist");
                return false;
            }
            ast->var.sym = sym;
            ast->type = sym->type;
            return true;
        }

        case AST_CAST:
            return true;

        case AST_ADD:
        case AST_SUB:
        case AST_MUL:
        case AST_DIV:
        case AST_LESS:
        case AST_LEQUAL:
        case AST_NEQUAL:
        case AST_EQUAL:
        {
            bool success = true;

            AST* left = ast->bin.l;
            AST* right = ast->bin.r;

            success &= sem(a, scope, left);
            success &= sem(a, scope, right);

            if (left->kind == AST_INT &&
                right->kind == AST_INT)
            {
                u64 result = 0;

                switch (ast->kind) {
                    default:
                        assert(false);
                        break;
                    case AST_ADD:
                        result = left->int_val + right->int_val;
                        break;
                    case AST_SUB:
                        result = left->int_val - right->int_val;
                        break;
                    case AST_MUL:
                        result = left->int_val * right->int_val;
                        break;
                    case AST_DIV:
                        result = left->int_val / right->int_val;
                        break;
                    case AST_LESS:
                        result = left->int_val < right->int_val;
                        break;
                    case AST_LEQUAL:
                        result = left->int_val <= right->int_val;
                        break;
                    case AST_NEQUAL:
                        result = left->int_val != right->int_val;
                        break;
                    case AST_EQUAL:
                        result = left->int_val == right->int_val;
                        break;
                }

                ast->kind = AST_INT;
                ast->int_val = result;
                ast->type = &ty_u64;
            }
            else if (left->kind == AST_INT &&
                     right->kind != AST_INT)
            {
                left->type = right->type;
                ast->type = right->type;
            }
            else if (left->kind != AST_INT &&
                     right->kind == AST_INT)
            {
                right->type = left->type;
                ast->type = left->type;
            }
            else if (left->type != right->type)
            {
                u64 left_priority  = get_primitive_priority(left->type);
                u64 right_priority = get_primitive_priority(right->type);
                assert(left_priority != right_priority);

                if (left_priority > right_priority) {
                    ast->bin.r = right = cast(a->arena, right, left->type);
                    ast->type = left->type;
                }
                else {
                    ast->bin.l = left = cast(a->arena, left, right->type);
                    ast->type = right->type;
                }
            }
            else {
                ast->type = left->type;
            }

            assert(left->type == right->type);
            
            return success;
        }

        case AST_ASSIGN: {
            bool success = true;

            success &= sem(a, scope, ast->bin.l);
            success &= sem(a, scope, ast->bin.r);

            if (ast->bin.l->kind != AST_VAR) {
                error_tok(a->src, ast->tok, "left operand is not assignable");
                success = false;
            }

            success &= check_assign_types(a, ast->tok, ast->bin.l->type, &ast->bin.r);
            ast->type = ast->bin.l->type;

            return success;
        }

        case AST_BLOCK: {
            Scratch scratch = get_scratch(&a->arena, 1);

            bool success = true;

            u32 local_table_size = ast->block.num_locals * 2;

            Scope inner_scope = {
                .parent = scope,
                .local_table_size = local_table_size,
                .local_table = arena_push_array(scratch.arena, Symbol*, local_table_size)
            };

            for (AST* c = ast->block.first_stmt; c; c = c->next) {
                success &= sem(a, &inner_scope, c);
            }

            release_scratch(&scratch);
            return success;
        }

        case AST_RETURN: {
            return sem(a, scope, ast->return_val);
        } 

        case AST_VAR_DECL: {
            bool success = true;

            if (find_symbol(scope, ast->var_decl.name)) {
                error_tok(a->src, ast->var_decl.name, "symbol redefinition");
                return false;
            }

            Symbol* sym = arena_push_type(a->arena, Symbol);
            sym->name = (String){
                .len = ast->var_decl.name.len,
                .ptr = arena_push(a->arena, ast->var_decl.name.len)
            };
            memcpy(sym->name.ptr, ast->var_decl.name.ptr, ast->var_decl.name.len);

            Type* type = find_type(a->prog, ast->var_decl.type_name);
            if (!type) {
                error_tok(a->src, ast->var_decl.type_name, "not a valid type");
                success = false;
                type = &ty_void;
            }

            sym->type = type;

            u32 table_index = fnv_1_a_hash(sym->name.ptr, sym->name.len) % scope->local_table_size;
            bool inserted = false;

            for (u32 i = 0; i < scope->local_table_size; ++i) {
                if (!scope->local_table[table_index]) {
                    scope->local_table[table_index] = sym;
                    inserted = true;
                    break;
                }
                table_index = (table_index + 1) % scope->local_table_size;
            }

            assert(inserted);

            ast->var_decl.sym = sym;

            success &= sem(a, scope, ast->var_decl.init);
            success &= check_assign_types(a, ast->var_decl.assign_tok, type, &ast->var_decl.init);

            return success;
        }

        case AST_IF:
        case AST_WHILE:
        {
            bool success = true;

            success &= sem(a, scope, ast->conditional.cond);
            success &= sem(a, scope, ast->conditional.then);

            if (ast->conditional.els) {
                success &= sem(a, scope, ast->conditional.els);
            }

            return success;
        }
    }
}

bool sem_ast(Arena* arena, char* src, Program* prog, AST* ast) {
    A a = {
        .arena = arena,
        .src = src,
        .prog = prog
    };

    return sem(&a, 0, ast);
}
