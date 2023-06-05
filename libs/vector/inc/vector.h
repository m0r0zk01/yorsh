#pragma once

#define vector_decl(type)                                                           \
typedef struct vector_##type {                                                      \
    type *data;                                                                     \
    size_t size;                                                                    \
    size_t capacity;                                                                \
} vector_##type;                                                                    \
                                                                                    \
int vector_create_##type(vector_##type *vec);                                       \
type *vector_get_##type(vector_##type *vec, size_t ind);                            \
void vector_set_##type(vector_##type *vec, size_t ind, type val);                   \
int vector_push_back_##type(vector_##type *vec, type val);                          \
void vector_pop_back_##type(vector_##type *vec, type el);                           \
size_t vector_size_##type(vector_##type *vec);                                      \
void vector_free_##type(vector_##type *vec, void(*deleter)(vector_##type *));       \
