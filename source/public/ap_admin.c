#include <assert.h>

#include "core/malloc.h"
#include "core/string.h"
#include "core/vector.h"

#include "public/ap_admin.h"

struct uint_map_item {
	uint64_t key;
	uint32_t index;
};

struct str_map_item {
	char key[256];
	uint32_t index;
};

static const struct uint_map_item * uint_item_const(const void * i)
{
	return i;
}

static const struct str_map_item * str_item_const(const void * i)
{
	return i;
}

static uint64_t hash_uint(
	const void * item, 
	uint64_t seed0,
	uint64_t seed1)
{
	return hmap_murmur(&uint_item_const(item)->key, 
		sizeof(uint64_t), seed0, seed1);
}

static int compare_uint(
	const void * a, 
	const void * b, 
	void * user_data)
{
	return (uint_item_const(a)->key < uint_item_const(b)->key);
}

static uint64_t hash_str(
	const void * item, 
	uint64_t seed0,
	uint64_t seed1)
{
	const struct str_map_item * i = item;
	return hmap_murmur(i->key, strlen(i->key), seed0, seed1);
}

static int compare_str(
	const void * a, 
	const void * b, 
	void * user_data)
{
	return strcmp(str_item_const(a)->key, str_item_const(b)->key);
}

static boolean make_str_map_item(
	struct str_map_item * item, 
	const char * key)
{
	return (strlcpy(item->key, key, 
		sizeof(item->key)) < sizeof(item->key));
}

static void * add_object(
	struct ap_admin * admin, 
	uint32_t * index)
{
	void * obj;
	if (!vec_is_empty(admin->free_objects)) {
		obj = admin->free_objects[0];
		vec_erase(admin->free_objects, &admin->free_objects[0]);
	}
	else {
		obj = vec_add_empty(&admin->objects);
	}
	memset(obj, 0, admin->object_size);
	if (index)
		*index = vec_index(admin->objects, obj);
	return obj;
}

void ap_admin_init(
	struct ap_admin * admin,
	size_t object_size,
	uint32_t object_count)
{
	uint32_t i;
	memset(admin, 0, sizeof(*admin));
	admin->uint_map = hmap_new(sizeof(struct uint_map_item), 16, 
		(uint64_t)strcpy, (uint64_t)memset, 
		hash_uint, compare_uint, NULL, NULL);
	admin->str_map = hmap_new(sizeof(struct str_map_item), 16, 
		(uint64_t)strcpy, (uint64_t)memset, 
		hash_str, compare_str, NULL, NULL);
	admin->objects = vec_new_reserved(object_size, object_count);
	vec_set_count(admin->objects, object_count);
	admin->free_objects = (void **)vec_new_reserved(
		sizeof(void *), object_count);
	for (i = 0; i < object_count; i++) {
		void * obj = vec_at(admin->objects, i);
		vec_push_back((void **)&admin->free_objects, &obj);
	}
	admin->object_size = object_size;
}

void ap_admin_destroy(struct ap_admin * admin)
{
	hmap_free(admin->uint_map);
	hmap_free(admin->str_map);
	vec_free(admin->objects);
	vec_free(admin->free_objects);
}

void * ap_admin_add_object_by_id(
	struct ap_admin * admin, 
	uint64_t id)
{
	struct uint_map_item i = { id, 0 };
	struct uint_map_item * r = hmap_get(admin->uint_map, &i);
	void * obj;
	if (r)
		return NULL;
	obj = add_object(admin, &i.index);
	hmap_set(admin->uint_map, &i);
	return obj;
}

void * ap_admin_add_object_by_name(
	struct ap_admin * admin, 
	const char * name)
{
	struct str_map_item i = { 0 };
	struct str_map_item * r;
	void * obj;
	if (!make_str_map_item(&i, name))
		return NULL;
	r = hmap_get(admin->str_map, &i);
	if (r)
		return NULL;
	obj = add_object(admin, &i.index);
	hmap_set(admin->str_map, &i);
	return obj;
}

void * ap_admin_add_object(
	struct ap_admin * admin, 
	uint64_t id, 
	const char * name)
{
	struct uint_map_item iuint = { id, 0 };
	struct uint_map_item * ruint;
	struct str_map_item istr = { 0 };
	struct str_map_item * rstr;
	void * obj;
	if (!make_str_map_item(&istr, name))
		return NULL;
	ruint = hmap_get(admin->uint_map, &iuint);
	rstr = hmap_get(admin->str_map, &istr);
	if (ruint || rstr)
		return NULL;
	obj = add_object(admin, &iuint.index);
	istr.index = iuint.index;
	hmap_set(admin->uint_map, &iuint);
	hmap_set(admin->str_map, &istr);
	return obj;
}

boolean ap_admin_add_id(
	struct ap_admin * admin, 
	const char * name,
	uint64_t id)
{
	struct uint_map_item iuint = { id, 0 };
	struct uint_map_item * ruint;
	struct str_map_item istr = { 0 };
	struct str_map_item * rstr;
	if (!make_str_map_item(&istr, name))
		return FALSE;
	rstr = hmap_get(admin->str_map, &istr);
	if (!rstr)
		return FALSE;
	ruint = hmap_get(admin->uint_map, &iuint);
	if (ruint)
		return FALSE;
	iuint.key = id;
	iuint.index = rstr->index;
	hmap_set(admin->uint_map, &iuint);
	return TRUE;
}

boolean ap_admin_add_name(
	struct ap_admin * admin, 
	uint64_t id, 
	const char * name)
{
	struct uint_map_item i = { id, 0 };
	struct uint_map_item * r = hmap_get(admin->uint_map, &i);
	struct str_map_item istr = { 0 };
	if (!r)
		return FALSE;
	if (!make_str_map_item(&istr, name))
		return FALSE;
	if (hmap_get(admin->str_map, &istr))
		return FALSE;
	istr.index = r->index;
	hmap_set(admin->str_map, &istr);
	return TRUE;
}

boolean ap_admin_update_name(
	struct ap_admin * admin,
	const char * original_name,
	const char * new_name)
{
	struct str_map_item i = { 0 };
	struct str_map_item n = { 0 };
	struct str_map_item * r;
	if (!make_str_map_item(&i, original_name))
		return FALSE;
	r = hmap_get(admin->str_map, &i);
	if (!r)
		return FALSE;
	if (!make_str_map_item(&n, new_name))
		return FALSE;
	n.index = r->index;
	if (!hmap_delete(admin->str_map, r))
		return FALSE;
	hmap_set(admin->str_map, &n);
	return TRUE;
}

boolean ap_admin_update_name_by_id(
	struct ap_admin * admin,
	uint64_t id,
	const char * new_name)
{
	struct uint_map_item i = { id, 0 };
	struct uint_map_item * r = hmap_get(admin->uint_map, &i);
	struct str_map_item n;
	if (!r)
		return FALSE;
	if (!make_str_map_item(&n, new_name))
		return FALSE;
	n.index = r->index;
	hmap_set(admin->str_map, &n);
	return TRUE;
}

boolean ap_admin_remove_object_by_id(
	struct ap_admin * admin, 
	uint64_t id)
{
	struct uint_map_item i = { id, 0 };
	struct uint_map_item * r = hmap_get(admin->uint_map, &i);
	uint32_t index;
	void * obj;
	if (!r)
		return FALSE;
	/* If string hash map is in use, removing an object 'just' 
	 * from uint hash map will cause the object index in string 
	 * hash map to become invalid.
	 * 
	 * If both uint and string hash maps are in use,
	 * use ap_admin_remove_object function instead. */
	assert(hmap_count(admin->str_map) == 0);
	index = r->index;
	if (!hmap_delete(admin->uint_map, &i))
		return FALSE;
	obj = vec_at(admin->objects, index);
	vec_push_back((void **)&admin->free_objects, (void *)&obj);
	return TRUE;
}

boolean ap_admin_remove_object_by_name(
	struct ap_admin * admin, 
	const char * name)
{
	struct str_map_item i = { 0 };
	struct str_map_item * r;
	uint32_t index;
	void * obj;
	if (!make_str_map_item(&i, name))
		return FALSE;
	r = hmap_get(admin->str_map, &i);
	if (!r)
		return FALSE;
	/* Same principle as explained in 
	 * ap_admin_remove_object_by_id. */
	assert(hmap_count(admin->uint_map) == 0);
	index = r->index;
	if (!hmap_delete(admin->str_map, &i))
		return FALSE;
	obj = vec_at(admin->objects, index);
	vec_push_back((void **)&admin->free_objects, (void *)&obj);
	return TRUE;
}

boolean ap_admin_remove_object(
	struct ap_admin * admin, 
	uint64_t id,
	const char * name)
{
	struct str_map_item items = { 0 };
	struct str_map_item * rs;
	struct uint_map_item itemu = { id, 0 };
	struct uint_map_item * ru;
	void * obj;
	void * r[2] = { 0 };
	if (!make_str_map_item(&items, name))
		return FALSE;
	ru = hmap_get(admin->uint_map, &itemu);
	if (!ru)
		return FALSE;
	rs = hmap_get(admin->str_map, &items);
	if (!rs)
		return FALSE;
	assert(ru->index == rs->index);
	obj = vec_at(admin->objects, ru->index);
	vec_push_back((void **)&admin->free_objects, (void *)&obj);
	r[0] = hmap_delete(admin->uint_map, &itemu);
	r[1] = hmap_delete(admin->str_map, &items);
	assert(r[0] != NULL && r[1] != NULL);
	return (r[0] && r[1]);
}

void ap_admin_clear_objects(struct ap_admin * admin)
{
	uint32_t i;
	uint32_t count = vec_count(admin->objects);
	vec_clear(admin->free_objects);
	for (i = 0; i < count; i++) {
		void * obj = vec_at(admin->objects, i);
		vec_push_back((void **)&admin->free_objects, (void *)&obj);
	}
	hmap_clear(admin->uint_map, TRUE);
	hmap_clear(admin->str_map, TRUE);
}

uint32_t ap_admin_get_object_count(const struct ap_admin * admin)
{
	return (vec_count(admin->objects) - 
		vec_count(admin->free_objects));
}

void * ap_admin_get_object_by_id(
	struct ap_admin * admin,
	uint64_t id)
{
	struct uint_map_item i = { id, 0 };
	struct uint_map_item * r = hmap_get(admin->uint_map, &i);
	if (!r)
		return FALSE;
	return vec_at(admin->objects, r->index);
}

void * ap_admin_get_object_by_name(
	struct ap_admin * admin,
	const char * name)
{
	struct str_map_item i = { 0 };
	struct str_map_item * r;
	if (!make_str_map_item(&i, name))
		return FALSE;
	r = hmap_get(admin->str_map, &i);
	if (!r)
		return FALSE;
	return vec_at(admin->objects, r->index);
}

void * ap_admin_get_object_by_index(
	struct ap_admin * admin,
	uint32_t index)
{
	assert(index < vec_count(admin->objects));
	if (index >= vec_count(admin->objects))
		return NULL;
	return vec_at(admin->objects, index);
}

uint64_t * ap_admin_iterate_id(
	struct ap_admin * admin,
	size_t * index,
	void ** object)
{
	struct uint_map_item * r = NULL;
	if (!hmap_iter(admin->uint_map, index, &r))
		return NULL;
	if (object)
		*object = vec_at(admin->objects, r->index);
	return &r->key;
}

const uint64_t * ap_admin_const_iterate_id(
	const struct ap_admin * admin,
	size_t * index,
	const void ** object)
{
	struct uint_map_item * r = NULL;
	if (!hmap_iter(admin->uint_map, index, &r))
		return NULL;
	if (object)
		*object = vec_at(admin->objects, r->index);
	return &r->key;
}

const char * ap_admin_iterate_name(
	struct ap_admin * admin,
	size_t * index,
	void ** object)
{
	struct str_map_item * r = NULL;
	if (!hmap_iter(admin->str_map, index, &r))
		return NULL;
	if (object)
		*object = vec_at(admin->objects, r->index);
	return r->key;
}
