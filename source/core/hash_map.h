#ifndef _CORE_HASH_MAP_H_
#define _CORE_HASH_MAP_H_

#include "core/macros.h"
#include "core/types.h"

BEGIN_DECLS

typedef void * hmap_t;

typedef uint64_t(*hmap_hash_t)(
    const void * item,
    uint64_t seed0,
    uint64_t seed1);

typedef int(*hmap_compare_t)(
    const void * a,
    const void * b,
    void * user_data);

typedef void(*hmap_free_element_t)(void * item);

typedef boolean(*hmap_iter_t)(const void * item, void * user_data);

hmap_t hmap_new(
    size_t element_size,
    size_t cap, 
    uint64_t seed0,
    uint64_t seed1,
    hmap_hash_t hash,
    hmap_compare_t compare,
    hmap_free_element_t free_element,
    void * user_data);

void hmap_free(hmap_t map);

void hmap_clear(hmap_t map, boolean update_cap);

size_t hmap_count(hmap_t map);

boolean hmap_oom(hmap_t map);

void * hmap_get(hmap_t map, const void * item);

void * hmap_set(hmap_t map, const void * item);

void * hmap_delete(hmap_t map, void * item);

void * hmap_probe(hmap_t map, uint64_t position);

boolean hmap_scan(
    hmap_t map,
    hmap_iter_t iter,
    void * user_data);

boolean hmap_iter(hmap_t map, size_t * i, void ** item);

uint64_t hmap_sip(
    const void * data,
    size_t len, 
    uint64_t seed0,
    uint64_t seed1);

uint64_t hmap_murmur(
    const void * data,
    size_t len, 
    uint64_t seed0,
    uint64_t seed1);

END_DECLS

#endif /* _CORE_HASH_MAP_H_ */