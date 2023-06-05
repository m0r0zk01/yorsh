#include "http1.h"
#include "utils.h"

#include <stdlib.h>

#define vector_def(type)                                                         \
int vector_create_##type(vector_##type *vec) {                                   \
    vec->data = NULL;                                                            \
    vec->size = 0;                                                               \
    vec->capacity = 0;                                                           \
    return 0;                                                                    \
}                                                                                \
                                                                                 \
type *vector_get_##type(vector_##type *vec, size_t ind) {                        \
    return vec->data + ind;                                                      \
}                                                                                \
                                                                                 \
void vector_set_##type(vector_##type *vec, size_t ind, type val) {               \
    vec->data[ind] = val;                                                        \
}                                                                                \
                                                                                 \
int vector_push_back_##type(vector_##type *vec, type val) {                      \
    if (vec->size >= vec->capacity) {                                            \
        size_t new_capacity = vec->capacity ? vec->capacity * 2 : 1;             \
        type *new_data = reallocarray(vec->data, new_capacity, sizeof(type));    \
        if (!new_data) {                                                         \
            return 1;                                                            \
        }                                                                        \
        vec->data = new_data;                                                    \
        vec->capacity = new_capacity;                                            \
                                                                                 \
    }                                                                            \
    vec->data[vec->size++] = val;                                                \
    return 0;                                                                    \
}                                                                                \
                                                                                 \
void vector_pop_back_##type(vector_##type *vec, type el) {                       \
    vec->size--;                                                                 \
}                                                                                \
                                                                                 \
size_t vector_size_##type(vector_##type *vec) {                                  \
    return vec->size;                                                            \
}                                                                                \
                                                                                 \
void vector_free_##type(vector_##type *vec, void(*deleter)(vector_##type *)) {   \
    if (deleter) {                                                               \
        deleter(vec);                                                            \
    }                                                                            \
    free(vec->data);                                                             \
}                                                                                \

vector_def(http1_header)
vector_def(string)
vector_def(http1_cgi)
