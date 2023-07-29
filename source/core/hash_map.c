// Copyright 2020 Joshua J Baker. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include "core/hash_map.h"
#include "core/malloc.h"

struct bucket {
    uint64_t hash:48;
    uint64_t dib:16;
};

struct hmap {
    boolean oom;
    size_t elsize;
    size_t cap;
    uint64_t seed0;
    uint64_t seed1;
    hmap_hash_t hash;
    hmap_compare_t compare;
    hmap_free_element_t elfree;
    void * udata;
    size_t bucketsz;
    size_t nbuckets;
    size_t count;
    size_t mask;
    size_t growat;
    size_t shrinkat;
    void * buckets;
    void * spare;
    void * edata;
};

static struct bucket * bucket_at(struct hmap * map, size_t index)
{
    return (struct bucket *)((uintptr_t)map->buckets + 
        map->bucketsz * index);
}

static void * bucket_item(struct bucket * entry)
{
    return (void *)((uintptr_t)entry + sizeof(struct bucket));
}

static uint64_t get_hash(struct hmap * map, const void * key)
{
    return map->hash(key, map->seed0, map->seed1) << 16 >> 16;
}

static struct hmap * hmap_new_with_allocator(
    size_t elsize,
    size_t cap, 
    uint64_t seed0,
    uint64_t seed1,
    hmap_hash_t hash,
    hmap_compare_t compare,
    hmap_free_element_t elfree,
    void * user_data)
{
    size_t ncap = 16;
    if (cap < ncap) {
        cap = ncap;
    }
    else {
        while (ncap < cap)
            ncap *= 2;
        cap = ncap;
    }
    size_t bucketsz = sizeof(struct bucket) + elsize;
    while (bucketsz & (sizeof(uintptr_t)-1)) {
        bucketsz++;
    }
    size_t size = sizeof(struct hmap)+bucketsz*2;
    struct hmap * map = alloc(size);
    if (!map) {
        return NULL;
    }
    memset(map, 0, sizeof(struct hmap));
    map->elsize = elsize;
    map->bucketsz = bucketsz;
    map->seed0 = seed0;
    map->seed1 = seed1;
    map->hash = hash;
    map->compare = compare;
    map->elfree = elfree;
    map->udata = user_data;
    map->spare = ((char*)map)+sizeof(struct hmap);
    map->edata = (char*)map->spare+bucketsz;
    map->cap = cap;
    map->nbuckets = cap;
    map->mask = map->nbuckets-1;
    map->buckets = alloc(map->bucketsz*map->nbuckets);
    if (!map->buckets) {
        dealloc(map);
        return NULL;
    }
    memset(map->buckets, 0, map->bucketsz*map->nbuckets);
    map->growat = (size_t)(map->nbuckets*0.75);
    map->shrinkat = (size_t)(map->nbuckets*0.10);
    return map;  
}


// hmap_new returns a new hash map. 
// Param `elsize` is the size of each element in the tree. Every element that
// is inserted, deleted, or retrieved will be this size.
// Param `cap` is the default lower capacity of the hmap. Setting this to
// zero will default to 16.
// Params `seed0` and `seed1` are optional seed values that are passed to the 
// following `hash` function. These can be any value you wish but it's often 
// best to use randomly generated values.
// Param `hash` is a function that generates a hash value for an item. It's
// important that you provide a good hash function, otherwise it will perform
// poorly or be vulnerable to Denial-of-service attacks. This implementation
// comes with two helper functions `hmap_sip()` and `hmap_murmur()`.
// Param `compare` is a function that compares items in the tree. See the 
// qsort stdlib function for an example of how this function works.
// The hmap must be freed with hmap_free(). 
// Param `elfree` is a function that frees a specific item. This should be NULL
// unless you're storing some kind of reference data in the hash.
hmap_t hmap_new(
    size_t element_size,
    size_t cap, 
    uint64_t seed0,
    uint64_t seed1,
    hmap_hash_t hash,
    hmap_compare_t compare,
    hmap_free_element_t free_element,
    void * user_data)
{
    return hmap_new_with_allocator(element_size, cap,
        seed0, seed1, hash, compare, free_element, user_data);
}

static void free_elements(struct hmap * map)
{
    if (map->elfree) {
        for (size_t i = 0; i < map->nbuckets; i++) {
            struct bucket * bucket = bucket_at(map, i);
            if (bucket->dib)
                map->elfree(bucket_item(bucket));
        }
    }
}

// hmap_clear quickly clears the map. 
// Every item is called with the element-freeing function given in hmap_new,
// if present, to free any data referenced in the elements of the hmap.
// When the update_cap is provided, the map's capacity will be updated to match
// the currently number of allocated buckets. This is an optimization to ensure
// that this operation does not perform any allocations.
void hmap_clear(hmap_t map, boolean update_cap) {
    struct hmap * m = map;
    m->count = 0;
    free_elements(map);
    if (update_cap) {
        m->cap = m->nbuckets;
    }
    else if (m->nbuckets != m->cap) {
        void * new_buckets = alloc(m->bucketsz*m->cap);
        if (new_buckets) {
            dealloc(m->buckets);
            m->buckets = new_buckets;
        }
        m->nbuckets = m->cap;
    }
    memset(m->buckets, 0, m->bucketsz*m->nbuckets);
    m->mask = m->nbuckets-1;
    m->growat = (size_t)(m->nbuckets*0.75);
    m->shrinkat = (size_t)(m->nbuckets*0.10);
}

static boolean resize(hmap_t map, size_t new_cap)
{
    struct hmap * m = map;
    struct hmap * map2 = hmap_new_with_allocator(
        m->elsize, new_cap, m->seed0, 
        m->seed1, m->hash, m->compare,
        m->elfree, m->udata);
    if (!map2)
        return FALSE;
    for (size_t i = 0; i < m->nbuckets; i++) {
        struct bucket * entry = bucket_at(map, i);
        if (!entry->dib)
            continue;
        entry->dib = 1;
        size_t j = entry->hash & map2->mask;
        for (;;) {
            struct bucket *bucket = bucket_at(map2, j);
            if (bucket->dib == 0) {
                memcpy(bucket, entry, m->bucketsz);
                break;
            }
            if (bucket->dib < entry->dib) {
                memcpy(map2->spare, bucket, m->bucketsz);
                memcpy(bucket, entry, m->bucketsz);
                memcpy(entry, map2->spare, m->bucketsz);
            }
            j = (j + 1) & map2->mask;
            entry->dib += 1;
        }
    }
    dealloc(m->buckets);
    m->buckets = map2->buckets;
    m->nbuckets = map2->nbuckets;
    m->mask = map2->mask;
    m->growat = map2->growat;
    m->shrinkat = map2->shrinkat;
    dealloc(map2);
    return TRUE;
}

// hmap_set inserts or replaces an item in the hash map. If an item is
// replaced then it is returned otherwise NULL is returned. This operation
// may allocate memory. If the system is unable to allocate additional
// memory then NULL is returned and hmap_oom() returns TRUE.
void * hmap_set(hmap_t map, const void * item)
{
    struct hmap * m = map;
    m->oom = FALSE;
    if (m->count == m->growat) {
        if (!resize(map, m->nbuckets*2)) {
            m->oom = TRUE;
            return NULL;
        }
    }
    struct bucket *entry = m->edata;
    entry->hash = get_hash(map, item);
    entry->dib = 1;
    memcpy(bucket_item(entry), item, m->elsize);

    size_t i = entry->hash & m->mask;
    for (;;) {
        struct bucket *bucket = bucket_at(map, i);
        if (bucket->dib == 0) {
            memcpy(bucket, entry, m->bucketsz);
            m->count++;
            return NULL;
        }
        if (entry->hash == bucket->hash && 
            m->compare(bucket_item(entry), bucket_item(bucket), 
                m->udata) == 0)
        {
            memcpy(m->spare, bucket_item(bucket), m->elsize);
            memcpy(bucket_item(bucket), bucket_item(entry), m->elsize);
            return m->spare;
        }
        if (bucket->dib < entry->dib) {
            memcpy(m->spare, bucket, m->bucketsz);
            memcpy(bucket, entry, m->bucketsz);
            memcpy(entry, m->spare, m->bucketsz);
        }
        i = (i + 1) & m->mask;
        entry->dib += 1;
    }
}

// hmap_get returns the item based on the provided key. If the item is not
// found then NULL is returned.
void * hmap_get(hmap_t map, const void * key)
{
    struct hmap * m = map;
    uint64_t hash = get_hash(m, key);
    size_t i = hash & m->mask;
    for (;;) {
        struct bucket *bucket = bucket_at(m, i);
        if (!bucket->dib) {
            return NULL;
        }
        if (bucket->hash == hash && 
            m->compare(key, bucket_item(bucket),
                m->udata) == 0) {
            return bucket_item(bucket);
        }
        i = (i + 1) & m->mask;
    }
}

// hmap_probe returns the item in the bucket at position or NULL if an item
// is not set for that bucket. The position is 'moduloed' by the number of 
// buckets in the hmap.
void * hmap_probe(hmap_t map, uint64_t position) {
    struct hmap * m = map;
    size_t i = position & m->mask;
    struct bucket *bucket = bucket_at(m, i);
    if (!bucket->dib)
        return NULL;
    return bucket_item(bucket);
}


// hmap_delete removes an item from the hash map and returns it. If the
// item is not found then NULL is returned.
void * hmap_delete(hmap_t map, void * key) {
    struct hmap * m = map;
    m->oom = FALSE;
    uint64_t hash = get_hash(m, key);
    size_t i = hash & m->mask;
    for (;;) {
        struct bucket *bucket = bucket_at(m, i);
        if (!bucket->dib) {
            return NULL;
        }
        if (bucket->hash == hash && 
            m->compare(key, bucket_item(bucket), m->udata) == 0)
        {
            memcpy(m->spare, bucket_item(bucket), m->elsize);
            bucket->dib = 0;
            for (;;) {
                struct bucket *prev = bucket;
                i = (i + 1) & m->mask;
                bucket = bucket_at(m, i);
                if (bucket->dib <= 1) {
                    prev->dib = 0;
                    break;
                }
                memcpy(prev, bucket, m->bucketsz);
                prev->dib--;
            }
            m->count--;
            if (m->nbuckets > m->cap && m->count <= m->shrinkat) {
                // Ignore the return value. It's ok for the resize operation to
                // fail to allocate enough memory because a shrink operation
                // does not change the integrity of the data.
                resize(m, m->nbuckets/2);
            }
            return m->spare;
        }
        i = (i + 1) & m->mask;
    }
}

// hmap_count returns the number of items in the hash map.
size_t hmap_count(hmap_t map)
{
    return ((struct hmap *)map)->count;
}

// hmap_free frees the hash map
// Every item is called with the element-freeing function given in hmap_new,
// if present, to free any data referenced in the elements of the hmap.
void hmap_free(hmap_t map)
{
    struct hmap * m = map;
    if (!m)
        return;
    free_elements(m);
    dealloc(m->buckets);
    dealloc(m);
}

// hmap_oom returns TRUE if the last hmap_set() call failed due to the 
// system being out of memory.
boolean hmap_oom(hmap_t map)
{
    return ((struct hmap *)map)->oom;
}

// hmap_scan iterates over all items in the hash map
// Param `iter` can return FALSE to stop iteration early.
// Returns FALSE if the iteration has been stopped early.
boolean hmap_scan(
    hmap_t map, 
    hmap_iter_t iter,
    void *udata)
{
    struct hmap * m = map;
    for (size_t i = 0; i < m->nbuckets; i++) {
        struct bucket *bucket = bucket_at(m, i);
        if (bucket->dib) {
            if (!iter(bucket_item(bucket), udata))
                return FALSE;
        }
    }
    return TRUE;
}


// hmap_iter iterates one key at a time yielding a reference to an
// entry at each iteration. Useful to write simple loops and avoid writing
// dedicated callbacks and udata structures, as in hmap_scan.
//
// map is a hash map handle. i is a pointer to a size_t cursor that
// should be initialized to 0 at the beginning of the loop. item is a void
// pointer pointer that is populated with the retrieved item. Note that this
// is NOT a copy of the item stored in the hash map and can be directly
// modified.
//
// Note that if hmap_delete() is called on the hmap being iterated,
// the buckets are rearranged and the iterator must be reset to 0, otherwise
// unexpected results may be returned after deletion.
//
// This function has not been tested for thread safety.
//
// The function returns TRUE if an item was retrieved; FALSE if the end of the
// iteration has been reached.
boolean hmap_iter(hmap_t map, size_t *i, void **item)
{
    struct hmap * m = map;
    struct bucket *bucket;
    do {
        if (*i >= m->nbuckets) 
            return FALSE;
        bucket = bucket_at(m, *i);
        (*i)++;
    } while (!bucket->dib);
    *item = bucket_item(bucket);
    return TRUE;
}


//-----------------------------------------------------------------------------
// SipHash reference C implementation
//
// Copyright (c) 2012-2016 Jean-Philippe Aumasson
// <jeanphilippe.aumasson@gmail.com>
// Copyright (c) 2012-2014 Daniel J. Bernstein <djb@cr.yp.to>
//
// To the extent possible under law, the author(s) have dedicated all copyright
// and related and neighboring rights to this software to the public domain
// worldwide. This software is distributed without any warranty.
//
// You should have received a copy of the CC0 Public Domain Dedication along
// with this software. If not, see
// <http://creativecommons.org/publicdomain/zero/1.0/>.
//
// default: SipHash-2-4
//-----------------------------------------------------------------------------
static uint64_t SIP64(const uint8_t *in, const size_t inlen, 
    uint64_t seed0, uint64_t seed1) 
{
#define U8TO64_LE(p) \
    {  (((uint64_t)((p)[0])) | ((uint64_t)((p)[1]) << 8) | \
        ((uint64_t)((p)[2]) << 16) | ((uint64_t)((p)[3]) << 24) | \
        ((uint64_t)((p)[4]) << 32) | ((uint64_t)((p)[5]) << 40) | \
        ((uint64_t)((p)[6]) << 48) | ((uint64_t)((p)[7]) << 56)) }
#define U64TO8_LE(p, v) \
    { U32TO8_LE((p), (uint32_t)((v))); \
      U32TO8_LE((p) + 4, (uint32_t)((v) >> 32)); }
#define U32TO8_LE(p, v) \
    { (p)[0] = (uint8_t)((v)); \
      (p)[1] = (uint8_t)((v) >> 8); \
      (p)[2] = (uint8_t)((v) >> 16); \
      (p)[3] = (uint8_t)((v) >> 24); }
#define ROTL(x, b) (uint64_t)(((x) << (b)) | ((x) >> (64 - (b))))
#define SIPROUND \
    { v0 += v1; v1 = ROTL(v1, 13); \
      v1 ^= v0; v0 = ROTL(v0, 32); \
      v2 += v3; v3 = ROTL(v3, 16); \
      v3 ^= v2; \
      v0 += v3; v3 = ROTL(v3, 21); \
      v3 ^= v0; \
      v2 += v1; v1 = ROTL(v1, 17); \
      v1 ^= v2; v2 = ROTL(v2, 32); }
    uint64_t k0 = U8TO64_LE((uint8_t*)&seed0);
    uint64_t k1 = U8TO64_LE((uint8_t*)&seed1);
    uint64_t v3 = UINT64_C(0x7465646279746573) ^ k1;
    uint64_t v2 = UINT64_C(0x6c7967656e657261) ^ k0;
    uint64_t v1 = UINT64_C(0x646f72616e646f6d) ^ k1;
    uint64_t v0 = UINT64_C(0x736f6d6570736575) ^ k0;
    const uint8_t *end = in + inlen - (inlen % sizeof(uint64_t));
    for (; in != end; in += 8) {
        uint64_t m = U8TO64_LE(in);
        v3 ^= m;
        SIPROUND; SIPROUND;
        v0 ^= m;
    }
    const int left = inlen & 7;
    uint64_t b = ((uint64_t)inlen) << 56;
    switch (left) {
    case 7: b |= ((uint64_t)in[6]) << 48;
    case 6: b |= ((uint64_t)in[5]) << 40;
    case 5: b |= ((uint64_t)in[4]) << 32;
    case 4: b |= ((uint64_t)in[3]) << 24;
    case 3: b |= ((uint64_t)in[2]) << 16;
    case 2: b |= ((uint64_t)in[1]) << 8;
    case 1: b |= ((uint64_t)in[0]); break;
    case 0: break;
    }
    v3 ^= b;
    SIPROUND; SIPROUND;
    v0 ^= b;
    v2 ^= 0xff;
    SIPROUND; SIPROUND; SIPROUND; SIPROUND;
    b = v0 ^ v1 ^ v2 ^ v3;
    uint64_t out = 0;
    U64TO8_LE((uint8_t*)&out, b);
    return out;
}

//-----------------------------------------------------------------------------
// MurmurHash3 was written by Austin Appleby, and is placed in the public
// domain. The author hereby disclaims copyright to this source code.
//
// Murmur3_86_128
//-----------------------------------------------------------------------------
static void MM86128(
    const void * key,
    const size_t len,
    uint32_t seed,
    void * out) {
#define	ROTL32(x, r) ((x << r) | (x >> (32 - r)))
#define FMIX32(h) h^=h>>16; h*=0x85ebca6b; h^=h>>13; h*=0xc2b2ae35; h^=h>>16;
    const uint8_t * data = (const uint8_t*)key;
    const size_t nblocks = len / 16;
    uint32_t h1 = seed;
    uint32_t h2 = seed;
    uint32_t h3 = seed;
    uint32_t h4 = seed;
    uint32_t c1 = 0x239b961b; 
    uint32_t c2 = 0xab0e9789;
    uint32_t c3 = 0x38b34ae5; 
    uint32_t c4 = 0xa1e38b93;
    const uint32_t * blocks = (const uint32_t *)data;
    for (size_t i = 0; i < nblocks; i++) {
        uint32_t k1 = blocks[(nblocks - (i + 1))*4+0];
        uint32_t k2 = blocks[(nblocks - (i + 1))*4+1];
        uint32_t k3 = blocks[(nblocks - (i + 1))*4+2];
        uint32_t k4 = blocks[(nblocks - (i + 1))*4+3];
        k1 *= c1; k1  = ROTL32(k1,15); k1 *= c2; h1 ^= k1;
        h1 = ROTL32(h1,19); h1 += h2; h1 = h1*5+0x561ccd1b;
        k2 *= c2; k2  = ROTL32(k2,16); k2 *= c3; h2 ^= k2;
        h2 = ROTL32(h2,17); h2 += h3; h2 = h2*5+0x0bcaa747;
        k3 *= c3; k3  = ROTL32(k3,17); k3 *= c4; h3 ^= k3;
        h3 = ROTL32(h3,15); h3 += h4; h3 = h3*5+0x96cd1c35;
        k4 *= c4; k4  = ROTL32(k4,18); k4 *= c1; h4 ^= k4;
        h4 = ROTL32(h4,13); h4 += h1; h4 = h4*5+0x32ac3b17;
    }
    const uint8_t * tail = (const uint8_t*)(data + nblocks*16);
    uint32_t k1 = 0;
    uint32_t k2 = 0;
    uint32_t k3 = 0;
    uint32_t k4 = 0;
    switch(len & 15) {
    case 15: k4 ^= tail[14] << 16;
    case 14: k4 ^= tail[13] << 8;
    case 13: k4 ^= tail[12] << 0;
        k4 *= c4; k4  = ROTL32(k4,18); k4 *= c1; h4 ^= k4;
    case 12: k3 ^= tail[11] << 24;
    case 11: k3 ^= tail[10] << 16;
    case 10: k3 ^= tail[ 9] << 8;
    case  9: k3 ^= tail[ 8] << 0;
        k3 *= c3; k3  = ROTL32(k3,17); k3 *= c4; h3 ^= k3;
    case  8: k2 ^= tail[ 7] << 24;
    case  7: k2 ^= tail[ 6] << 16;
    case  6: k2 ^= tail[ 5] << 8;
    case  5: k2 ^= tail[ 4] << 0;
        k2 *= c2; k2  = ROTL32(k2,16); k2 *= c3; h2 ^= k2;
    case  4: k1 ^= tail[ 3] << 24;
    case  3: k1 ^= tail[ 2] << 16;
    case  2: k1 ^= tail[ 1] << 8;
    case  1: k1 ^= tail[ 0] << 0;
        k1 *= c1; k1  = ROTL32(k1,15); k1 *= c2; h1 ^= k1;
    };
    h1 ^= len; h2 ^= len; h3 ^= len; h4 ^= len;
    h1 += h2; h1 += h3; h1 += h4;
    h2 += h1; h3 += h1; h4 += h1;
    FMIX32(h1); FMIX32(h2); FMIX32(h3); FMIX32(h4);
    h1 += h2; h1 += h3; h1 += h4;
    h2 += h1; h3 += h1; h4 += h1;
    ((uint32_t*)out)[0] = h1;
    ((uint32_t*)out)[1] = h2;
    ((uint32_t*)out)[2] = h3;
    ((uint32_t*)out)[3] = h4;
}

// hmap_sip returns a hash value for `data` using SipHash-2-4.
uint64_t hmap_sip(const void *data, size_t len, 
    uint64_t seed0, uint64_t seed1)
{
    return SIP64((uint8_t*)data, len, seed0, seed1);
}

// hmap_murmur returns a hash value for `data` using Murmur3_86_128.
uint64_t hmap_murmur(
    const void * data,
    size_t len, 
    uint64_t seed0,
    uint64_t seed1)
{
    char out[16];
    MM86128(data, len, (uint32_t)seed0, &out);
    return *(uint64_t*)out;
}

//==============================================================================
// TESTS AND BENCHMARKS
// $ cc -Dhmap_TEST hmap.c && ./a.out              # run tests
// $ cc -Dhmap_TEST -O3 hmap.c && BENCH=1 ./a.out  # run benchmarks
//==============================================================================
#ifdef hmap_TEST

static size_t deepcount(struct hmap * map) {
    size_t count = 0;
    for (size_t i = 0; i < map->nbuckets; i++) {
        if (bucket_at(map, i)->dib) {
            count++;
        }
    }
    return count;
}


#pragma GCC diagnostic ignored "-Wextra"


#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <stdio.h>
#include "hmap.h"

static boolean rand_alloc_fail = FALSE;
static int rand_alloc_fail_odds = 3; // 1 in 3 chance alloc will fail.
static uintptr_t total_allocs = 0;
static uintptr_t total_mem = 0;

static void *xalloc(size_t size) {
    if (rand_alloc_fail && rand()%rand_alloc_fail_odds == 0) {
        return NULL;
    }
    void *mem = alloc(sizeof(uintptr_t)+size);
    assert(mem);
    *(uintptr_t*)mem = size;
    total_allocs++;
    total_mem += size;
    return (char*)mem+sizeof(uintptr_t);
}

static void xfree(void *ptr) {
    if (ptr) {
        total_mem -= *(uintptr_t*)((char*)ptr-sizeof(uintptr_t));
        free((char*)ptr-sizeof(uintptr_t));
        total_allocs--;
    }
}

static void shuffle(void *array, size_t numels, size_t elsize) {
    char tmp[elsize];
    char *arr = array;
    for (size_t i = 0; i < numels - 1; i++) {
        int j = i + rand() / (RAND_MAX / (numels - i) + 1);
        memcpy(tmp, arr + j * elsize, elsize);
        memcpy(arr + j * elsize, arr + i * elsize, elsize);
        memcpy(arr + i * elsize, tmp, elsize);
    }
}

static boolean iter_ints(const void *item, void *udata) {
    int *vals = *(int**)udata;
    vals[*(int*)item] = 1;
    return TRUE;
}

static int compare_ints(const void *a, const void *b) {
    return *(int*)a - *(int*)b;
}

static int compare_ints_udata(const void *a, const void *b, void *udata) {
    return *(int*)a - *(int*)b;
}

static int compare_strs(const void *a, const void *b, void *udata) {
    return strcmp(*(char**)a, *(char**)b);
}

static uint64_t hash_int(const void *item, uint64_t seed0, uint64_t seed1) {
    return hmap_murmur(item, sizeof(int), seed0, seed1);
}

static uint64_t hash_str(const void *item, uint64_t seed0, uint64_t seed1) {
    return hmap_murmur(*(char**)item, strlen(*(char**)item), seed0, seed1);
}

static void free_str(void *item) {
    xfree(*(char**)item);
}

static void all() {
    int seed = getenv("SEED")?atoi(getenv("SEED")):time(NULL);
    int N = getenv("N")?atoi(getenv("N")):2000;
    printf("seed=%d, count=%d, item_size=%zu\n", seed, N, sizeof(int));
    srand(seed);

    rand_alloc_fail = TRUE;

    // test sip and murmur hashes
    assert(hmap_sip("hello", 5, 1, 2) == 2957200328589801622);
    assert(hmap_murmur("hello", 5, 1, 2) == 1682575153221130884);

    int *vals;
    while (!(vals = xalloc(N * sizeof(int)))) {}
    for (int i = 0; i < N; i++) {
        vals[i] = i;
    }

    struct hmap * map;

    while (!(map = hmap_new(sizeof(int), 0, seed, seed, 
        hash_int, compare_ints_udata, NULL, NULL))) {}
    shuffle(vals, N, sizeof(int));
    for (int i = 0; i < N; i++) {
        // // printf("== %d ==\n", vals[i]);
        assert(map->count == i);
        assert(map->count == hmap_count(map));
        assert(map->count == deepcount(map));
        int *v;
        assert(!hmap_get(map, &vals[i]));
        assert(!hmap_delete(map, &vals[i]));
        while (TRUE) {
            assert(!hmap_set(map, &vals[i]));
            if (!hmap_oom(map)) {
                break;
            }
        }

        for (int j = 0; j < i; j++) {
            v = hmap_get(map, &vals[j]);
            assert(v && *v == vals[j]);
        }
        while (TRUE) {
            v = hmap_set(map, &vals[i]);
            if (!v) {
                assert(hmap_oom(map));
                continue;
            } else {
                assert(!hmap_oom(map));
                assert(v && *v == vals[i]);
                break;
            }
        }
        v = hmap_get(map, &vals[i]);
        assert(v && *v == vals[i]);
        v = hmap_delete(map, &vals[i]);
        assert(v && *v == vals[i]);
        assert(!hmap_get(map, &vals[i]));
        assert(!hmap_delete(map, &vals[i]));
        assert(!hmap_set(map, &vals[i]));
        assert(map->count == i+1);
        assert(map->count == hmap_count(map));
        assert(map->count == deepcount(map));
    }

    int *vals2;
    while (!(vals2 = xalloc(N * sizeof(int)))) {}
    memset(vals2, 0, N * sizeof(int));
    assert(hmap_scan(map, iter_ints, &vals2));

    // Test hmap_iter. This does the same as hmap_scan above.
    size_t iter = 0;
    void *iter_val;
    while (hmap_iter (map, &iter, &iter_val)) {
        assert (iter_ints(iter_val, &vals2));
    }
    for (int i = 0; i < N; i++) {
        assert(vals2[i] == 1);
    }
    xfree(vals2);

    shuffle(vals, N, sizeof(int));
    for (int i = 0; i < N; i++) {
        int *v;
        v = hmap_delete(map, &vals[i]);
        assert(v && *v == vals[i]);
        assert(!hmap_get(map, &vals[i]));
        assert(map->count == N-i-1);
        assert(map->count == hmap_count(map));
        assert(map->count == deepcount(map));
        for (int j = N-1; j > i; j--) {
            v = hmap_get(map, &vals[j]);
            assert(v && *v == vals[j]);
        }
    }

    for (int i = 0; i < N; i++) {
        while (TRUE) {
            assert(!hmap_set(map, &vals[i]));
            if (!hmap_oom(map)) {
                break;
            }
        }
    }

    assert(map->count != 0);
    size_t prev_cap = map->cap;
    hmap_clear(map, TRUE);
    assert(prev_cap < map->cap);
    assert(map->count == 0);


    for (int i = 0; i < N; i++) {
        while (TRUE) {
            assert(!hmap_set(map, &vals[i]));
            if (!hmap_oom(map)) {
                break;
            }
        }
    }

    prev_cap = map->cap;
    hmap_clear(map, FALSE);
    assert(prev_cap == map->cap);

    hmap_free(map);

    xfree(vals);


    while (!(map = hmap_new(sizeof(char*), 0, seed, seed,
        hash_str, compare_strs, free_str, NULL)));

    for (int i = 0; i < N; i++) {
        char *str;
        while (!(str = xalloc(16)));
        sprintf(str, "s%i", i);
        while(!hmap_set(map, &str));
    }

    hmap_clear(map, FALSE);
    assert(hmap_count(map) == 0);

    for (int i = 0; i < N; i++) {
        char *str;
        while (!(str = xalloc(16)));
        sprintf(str, "s%i", i);
        while(!hmap_set(map, &str));
    }

    hmap_free(map);

    if (total_allocs != 0) {
        fprintf(stderr, "total_allocs: expected 0, got %lu\n", total_allocs);
        exit(1);
    }
}

#define bench(name, N, code) {{ \
    if (strlen(name) > 0) { \
        printf("%-14s ", name); \
    } \
    size_t tmem = total_mem; \
    size_t tallocs = total_allocs; \
    uint64_t bytes = 0; \
    clock_t begin = clock(); \
    for (int i = 0; i < N; i++) { \
        (code); \
    } \
    clock_t end = clock(); \
    double elapsed_secs = (double)(end - begin) / CLOCKS_PER_SEC; \
    double bytes_sec = (double)bytes/elapsed_secs; \
    printf("%d ops in %.3f secs, %.0f ns/op, %.0f op/sec", \
        N, elapsed_secs, \
        elapsed_secs/(double)N*1e9, \
        (double)N/elapsed_secs \
    ); \
    if (bytes > 0) { \
        printf(", %.1f GB/sec", bytes_sec/1024/1024/1024); \
    } \
    if (total_mem > tmem) { \
        size_t used_mem = total_mem-tmem; \
        printf(", %.2f bytes/op", (double)used_mem/N); \
    } \
    if (total_allocs > tallocs) { \
        size_t used_allocs = total_allocs-tallocs; \
        printf(", %.2f allocs/op", (double)used_allocs/N); \
    } \
    printf("\n"); \
}}

static void benchmarks() {
    int seed = getenv("SEED")?atoi(getenv("SEED")):time(NULL);
    int N = getenv("N")?atoi(getenv("N")):5000000;
    printf("seed=%d, count=%d, item_size=%zu\n", seed, N, sizeof(int));
    srand(seed);


    int *vals = xalloc(N * sizeof(int));
    for (int i = 0; i < N; i++) {
        vals[i] = i;
    }

    shuffle(vals, N, sizeof(int));

    struct hmap * map;
    shuffle(vals, N, sizeof(int));

    map = hmap_new(sizeof(int), 0, seed, seed, hash_int, compare_ints_udata, 
        NULL, NULL);
    bench("set", N, {
        int *v = hmap_set(map, &vals[i]);
    assert(!v);
        })
        shuffle(vals, N, sizeof(int));
        bench("get", N, {
            int *v = hmap_get(map, &vals[i]);
        assert(v && *v == vals[i]);
            })
            shuffle(vals, N, sizeof(int));
            bench("delete", N, {
                int *v = hmap_delete(map, &vals[i]);
            assert(v && *v == vals[i]);
                })
                hmap_free(map);

                map = hmap_new(sizeof(int), N, seed, seed, hash_int, compare_ints_udata, 
                    NULL, NULL);
                bench("set (cap)", N, {
                    int *v = hmap_set(map, &vals[i]);
                assert(!v);
                    })
                    shuffle(vals, N, sizeof(int));
                    bench("get (cap)", N, {
                        int *v = hmap_get(map, &vals[i]);
                    assert(v && *v == vals[i]);
                        })
                        shuffle(vals, N, sizeof(int));
                        bench("delete (cap)" , N, {
                            int *v = hmap_delete(map, &vals[i]);
                        assert(v && *v == vals[i]);
                            })

                            hmap_free(map);


                            xfree(vals);

                            if (total_allocs != 0) {
                                fprintf(stderr, "total_allocs: expected 0, got %lu\n", total_allocs);
                                exit(1);
                            }
}

int main() {
    hmap_set_allocator(xalloc, xfree);

    if (getenv("BENCH")) {
        printf("Running hmap.c benchmarks...\n");
        benchmarks();
    } else {
        printf("Running hmap.c tests...\n");
        all();
        printf("PASSED\n");
    }
}


#endif