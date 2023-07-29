#ifndef _AP_ADMIN_H_
#define _AP_ADMIN_H_

#include "core/hash_map.h"
#include "core/macros.h"
#include "core/types.h"

BEGIN_DECLS

struct ap_admin {
	hmap_t uint_map;
	hmap_t str_map;
	/* A vector of objects. */
	void * objects;
	void ** free_objects;
	size_t object_size;
};

void ap_admin_init(
	struct ap_admin * admin,
	size_t object_size,
	uint32_t object_count);

void ap_admin_destroy(struct ap_admin * admin);

/*
 * Add object by its ID.
 */
void * ap_admin_add_object_by_id(
	struct ap_admin * admin, 
	uint64_t id);

/*
 * Add object by its name.
 */
void * ap_admin_add_object_by_name(
	struct ap_admin * admin, 
	const char * name);

/*
 * Add object by both ID and name.
 */
void * ap_admin_add_object(
	struct ap_admin * admin, 
	uint64_t id, 
	const char * name);

/*
 * Suplement id for an object that is already 
 * in hash map.
 * 
 * This function will fail if object's id was 
 * already included in the hash map.
 */
boolean ap_admin_add_id(
	struct ap_admin * admin, 
	const char * name,
	uint64_t id);

/*
 * Suplement name for an object that is already 
 * in hash map.
 * 
 * This function will fail if object's name was 
 * already included in the hash map.
 */
boolean ap_admin_add_name(
	struct ap_admin * admin, 
	uint64_t id, 
	const char * name);

/*
 * Update an object's name.
 */
boolean ap_admin_update_name(
	struct ap_admin * admin,
	const char * original_name,
	const char * new_name);

/*
 * Update an object's name by using its ID.
 *
 * This function differs from ap_admin_add_name in that 
 * it succeeds whether the object's hash map previously 
 * included a name or not.
 */
boolean ap_admin_update_name_by_id(
	struct ap_admin * admin,
	uint64_t id,
	const char * new_name);

boolean ap_admin_remove_object_by_id(
	struct ap_admin * admin, 
	uint64_t id);

boolean ap_admin_remove_object_by_name(
	struct ap_admin * admin, 
	const char * name);

boolean ap_admin_remove_object(
	struct ap_admin * admin, 
	uint64_t id,
	const char * name);

void ap_admin_clear_objects(struct ap_admin * admin);

uint32_t ap_admin_get_object_count(const struct ap_admin * admin);

void * ap_admin_get_object_by_id(
	struct ap_admin * admin,
	uint64_t id);

void * ap_admin_get_object_by_name(
	struct ap_admin * admin,
	const char * name);

void * ap_admin_get_object_by_index(
	struct ap_admin * admin,
	uint32_t index);

uint64_t * ap_admin_iterate_id(
	struct ap_admin * admin,
	size_t * index,
	void ** object);

const uint64_t * ap_admin_const_iterate_id(
	const struct ap_admin * admin,
	size_t * index,
	const void ** object);

const char * ap_admin_iterate_name(
	struct ap_admin * admin,
	size_t * index,
	void ** object);

END_DECLS

#endif /* _AP_ADMIN_H_ */
