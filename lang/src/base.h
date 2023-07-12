#pragma once

#include <stdint.h>
#include <memory.h>
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

#define BIT(x) (1 << (x))
#define internal static
#define LEN(x) (sizeof(x)/sizeof((x)[0]))

typedef struct {
    int len;
    char* ptr;
} String;

typedef struct {
    void* ptr;
    size_t cap;
    size_t used;
} Arena;

#define arena_pad_size(size) (((size) + 7) & ~7);

inline void* arena_push(Arena* arena, size_t size) {
    size = arena_pad_size(size);
    assert(arena->cap - arena->used >= size);
    void* ptr = (u8*)arena->ptr + arena->used;
    arena->used += size;
    return size ? ptr : 0;
}

inline void* arena_push_clear(Arena* arena, size_t size) {
    void* ptr = arena_push(arena, size);
    memset(ptr, 0, size);
    return ptr;
}

inline void arena_realloc(Arena* arena, void* ptr, size_t size) {
    if (ptr) {
        size = arena_pad_size(size);
        size_t base = (u8*)ptr - (u8*)arena->ptr;
        assert(base + size <= arena->used);
        arena->used = base + size;
    }
}

#define arena_push_array(arena, type, count) (type*)arena_push_clear((arena), (count) * sizeof(type))
#define arena_push_type(arena, type) arena_push_array(arena, type, 1)

// From https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
inline u64 fnv_1_a_hash(void* data, int len) {
    u64 hash = 0xcbf29ce484222325;

    for (int i = 0; i < len; ++i) {
        hash ^= ((u8*)data)[i];
        hash *= 0x100000001b3;
    }

    return hash;
}

typedef struct {
    u32 bit_count;
    u32 p[1];
} Bitset;

inline u32 num_u32_for_bits(u32 bit_count) {
    return bit_count / 32 + (bit_count % 32 != 0);
}

inline Bitset* bitset_alloc(Arena* arena, u32 bit_count) {
    u32 num_u32 = num_u32_for_bits(bit_count);
    Bitset* bitset = arena_push_clear(arena, offsetof(Bitset, p) + num_u32 * sizeof(u32));
    bitset->bit_count = bit_count;
    return bitset;
}

inline void bitset_set(Bitset* set, u32 index) {
    assert(index < set->bit_count);
    u32 i = index / 32;
    set->p[i] |= 1 << (index % 32);
}

inline void bitset_unset(Bitset* set, u32 index) {
    assert(index < set->bit_count);
    u32 i = index / 32;
    set->p[i] &= ~(1 << (index % 32));
}

inline bool bitset_get(Bitset* set, u32 index) {
    assert(index < set->bit_count);
    u32 i = index / 32;
    return (set->p[i] >> (index % 32)) & 1;
}
