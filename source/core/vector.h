#ifndef _UTIL_VECTOR_H_
#define _UTIL_VECTOR_H_

#include "core/macros.h"
#include "core/types.h"

BEGIN_DECLS

/*
 * Allocates an empty vector.
 */
void * vec_new(size_t element_size);

/*
 * Allocates a vector with given capacity.
 */
void * vec_new_reserved(size_t element_size, uint32_t size);

/*
 * Ensures that vector has has a capacity of at least `size`.
 * If `v` is NULL, a new vector is created.
 */
void * vec_reserve(void * v, size_t element_size, uint32_t size);

/*
 * Copies contents of `src` to `dst`.
 */
void vec_copy(void ** dst, const void * src);

boolean vec_is_empty(const void * v);

/*
 * Returns the number of elements in the vector.
 */
uint32_t vec_count(const void * v);

/*
 * Returns vector size.
 */
uint32_t vec_size(const void * v);

/*
 * Inserts an element to the back of the vector.
 * If vector isn't large enough, vector is resized and 
 * its value may change, so the user is expected to use the 
 * function as such;
 * vec_push_back(&vec, &e);
 */
void vec_push_back(void ** v, const void * e);

/*
 * Inserts an empty element to the back of the vector.
 * If vector isn't large enough, vector is resized and 
 * its value may change, so the user is expected to use the 
 * function as such;
 * vec_push_back(&vec, &e);
 *
 * Returns a pointer to the newly added element.
 */
void * vec_add_empty(void ** v);

/*
 * Returns pointer to the last element in vector.
 * Returns NULL if vector is empty.
 */
void * vec_back(void * v);

/*
 * Returns element pointer at `v[index]`.
 */
void * vec_at(void * v, uint32_t index);

/*
 * Returns element index.
 */
uint32_t vec_index(void * v, const void * e);

/*
 * Attempts to erase element.
 */
void vec_erase(void * v, const void * e);

/**
 * Erase while iterating over the vector.
 */
uint32_t vec_erase_iterator(void * v, uint32_t index);

/*
 * Erase indices [begin, .., end).
 */
void vec_erase_chunk(void * v, uint32_t begin, uint32_t end);

/*
 * Resets element count.
 */
void vec_clear(void * v);

/*
 * Frees allocated memory.
 */
void vec_free(void * v);

/*
 * Attempts to zero-initialize `count` number of elements.
 */
void vec_fillz(void * v, uint32_t count);

/* 
 * Attempts to set the number of elements in vector to `count`.
 * Returns FALSE if vector's capacity is less than `count`.
 */
boolean vec_set_count(void * v, uint32_t count);

END_DECLS

#endif /* _UTIL_VECTOR_H_ */