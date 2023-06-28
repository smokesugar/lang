#include "sem.h"
#include "core.h"

typedef struct {
    Arena* arena;
    char* src;
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

internal bool sem(A* a, Scope* scope, AST* ast) {
    static_assert(NUM_AST_KINDS == 17, "not all ast kinds handled");
    switch (ast->kind) {
        default:
            assert(false);
            return false;

        case AST_INT:
            return true;

        case AST_VAR: {
            Symbol* sym = find_symbol(scope, ast->var.name);
            if (!sym) {
                error_tok(a->src, ast->var.name, "symbol does not exist");
                return false;
            }
            ast->var.sym = sym;
            return true;
        }

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
            success &= sem(a, scope, ast->bin.l);
            success &= sem(a, scope, ast->bin.r);
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

            return sem(a, scope, ast->var_decl.init);
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

bool sem_ast(Arena* arena, char* src, AST* ast) {
    A a = {
        .arena = arena,
        .src = src
    };

    return sem(&a, 0, ast);
}
