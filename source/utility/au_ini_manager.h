#ifndef _AU_INI_MANAGER_H_
#define _AU_INI_MANAGER_H_

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "core/macros.h"
#include "core/types.h"

#define	AU_INI_MGR_MAX_SECTIONCOUNT	20000
#define	AU_INI_MGR_MAX_KEYCOUNT		20000
#define AU_INI_MGR_MAX_NAME			1024
#define AU_INI_MGR_MAX_KEYVALUE		1024
#define AU_INI_MGR_MAX_VALUEDATA	64

BEGIN_DECLS

enum au_ini_mgr_mode {
	AU_INI_MGR_MODE_NORMAL = 0,
	AU_INI_MGR_MODE_NAME_OVERWRITE,
	AU_INI_MGR_MODE_COUNT
};

enum au_ini_mgr_type {
	AU_INI_MGR_TYPE_NORMAL = 0X00,
	AU_INI_MGR_TYPE_PART_INDEX = 0X01,
	AU_INI_MGR_TYPE_KEY_INDEX = 0X02,
	AU_INI_MGR_TYPE_COUNT
};

enum au_ini_mgr_key_type {
	AU_INI_MGR_KEY_TYPE_NONE,
	AU_INI_MGR_KEY_TYPE_INT,
	AU_INI_MGR_KEY_TYPE_INT64,
	AU_INI_MGR_KEY_TYPE_FLOAT,
	AU_INI_MGR_KEY_TYPE_STRING
};

enum au_ini_mgr_process_mode {
	AU_INI_MGR_PROCESS_MODE_NONE = 0,
	AU_INI_MGR_PROCESS_MODE_TXT = 1,
	AU_INI_MGR_PROCESS_MODE_BIN = 2,
};

struct au_ini_mgr_key {
	uint32_t key_index;
	char key_value[AU_INI_MGR_MAX_KEYVALUE];
	enum au_ini_mgr_key_type type;
};

struct au_ini_mgr_section {
	/* '[' Section Name ']' */
	char section_name[AU_INI_MGR_MAX_NAME];
	uint32_t key_count;
	struct au_ini_mgr_key * keys;
};

union au_ini_mgr_key_bin_data {
	char str[AU_INI_MGR_MAX_KEYVALUE];
	int32_t i;
	int64_t i64;
	float f;
};

struct au_ini_mgr_key_bin {
	int key_index;
	enum au_ini_mgr_key_type type;
	union au_ini_mgr_key_bin_data data;
};

struct au_ini_mgr_section_bin {
	char section_name[AU_INI_MGR_MAX_NAME];
	uint32_t key_count;
	struct au_ini_mgr_key_bin * keys;
};

struct au_ini_mgr_ctx {
	enum au_ini_mgr_process_mode process_mode;
	char * path_name;
	uint32_t section_count;
	uint32_t half_section_count;
	int is_section_count_odd;
	struct au_ini_mgr_section * sections;
	uint32_t binary_section_count;
	struct au_ini_mgr_section_bin * binary_sections;
	enum au_ini_mgr_mode mode;
	enum au_ini_mgr_type type;
	uint32_t part_count;
	uint32_t * part_indices;
	uint32_t key_table_count;
	char ** key_table;
};

struct au_ini_mgr_ctx * au_ini_mgr_create();

void au_ini_mgr_destroy(struct au_ini_mgr_ctx * ctx);

const char * au_ini_mgr_get_key_name_table(
	struct au_ini_mgr_ctx * ctx,
	uint32_t index);

uint32_t au_ini_mgr_get_key_index(
	struct au_ini_mgr_ctx * ctx,
	const char * str);

void au_ini_mgr_set_path(
	struct au_ini_mgr_ctx * ctx,
	const char * path);

void au_ini_mgr_set_process_mode(
	struct au_ini_mgr_ctx * ctx,
	enum au_ini_mgr_process_mode mode);

boolean au_ini_mgr_read_file(
	struct au_ini_mgr_ctx * ctx,
	uint32_t index,
	boolean decrypt);

boolean au_ini_mgr_write_file(
	struct au_ini_mgr_ctx * ctx,
	uint32_t index,
	boolean encrypt);

boolean au_ini_mgr_from_memory(
	struct au_ini_mgr_ctx * ctx,
	void * data,
	uint32_t data_size,
	boolean decrypt);

boolean au_ini_mgr_from_file(
	struct au_ini_mgr_ctx * ctx,
	uint32_t index,
	boolean decrypt);

boolean au_ini_mgr_write_to_memory(
	struct au_ini_mgr_ctx * ctx,
	void * buffer, 
	size_t buffer_size, 
	uint32_t * written_count,
	boolean encrypt);

void au_ini_mgr_clear_all_section_keys(
	struct au_ini_mgr_ctx * ctx);

const char * au_ini_mgr_get_section_name(
	struct au_ini_mgr_ctx * ctx,
	uint32_t section_id);

uint32_t au_ini_mgr_get_key_count(
	struct au_ini_mgr_ctx * ctx,
	uint32_t section_id);

const char * au_ini_mgr_get_key_name(
	struct au_ini_mgr_ctx * ctx,
	uint32_t section_id, 
	uint32_t key_id);

uint32_t au_ini_mgr_find_section(
	struct au_ini_mgr_ctx * ctx,
	const char * section_name);

uint32_t au_ini_mgr_find_key_by_name(
	struct au_ini_mgr_ctx * ctx,
	const char * section_name, 
	const char * key_name);

uint32_t au_ini_mgr_find_key(
	struct au_ini_mgr_ctx * ctx,
	uint32_t section_id, 
	const char * key_name);

uint32_t au_ini_mgr_add_section(
	struct au_ini_mgr_ctx * ctx,
	const char * section_name);

const char * au_ini_mgr_get_value(
	struct au_ini_mgr_ctx * ctx,
	uint32_t section_id,
	uint32_t key_id);

const char * au_ini_mgr_get_value_by_name(
	struct au_ini_mgr_ctx * ctx,
	const char * section_name,
	const char * key_name);

int32_t au_ini_mgr_get_i32(
	struct au_ini_mgr_ctx * ctx,
	uint32_t section_id,
	uint32_t key_id);

int64_t au_ini_mgr_get_i64(
	struct au_ini_mgr_ctx * ctx,
	uint32_t section_id,
	uint32_t key_id);

double au_ini_mgr_get_f64(
	struct au_ini_mgr_ctx * ctx,
	uint32_t section_id,
	uint32_t key_id);

boolean au_ini_mgr_get_str_by_name(
	struct au_ini_mgr_ctx * ctx,
	const char * section_name,
	const char * key_name,
	char * dst,
	uint32_t dst_length);

boolean au_ini_mgr_get_str(
	struct au_ini_mgr_ctx * ctx,
	uint32_t section_id,
	uint32_t key_id,
	char * dst,
	uint32_t dst_length);

boolean au_ini_mgr_set_value(
	struct au_ini_mgr_ctx * ctx,
	const char * section_name,
	const char * key_name,
	const char * value);

boolean au_ini_mgr_set_i32(
	struct au_ini_mgr_ctx * ctx,
	const char * section_name,
	const char * key_name,
	int32_t value);

boolean au_ini_mgr_set_i64(
	struct au_ini_mgr_ctx * ctx,
	const char * section_name,
	const char * key_name,
	int64_t value);

boolean au_ini_mgr_set_f32(
	struct au_ini_mgr_ctx * ctx,
	const char * section_name,
	const char * key_name,
	float value);

boolean au_ini_mgr_set_mode(
	struct au_ini_mgr_ctx * ctx,
	enum au_ini_mgr_mode mode);

boolean au_ini_mgr_set_type(
	struct au_ini_mgr_ctx * ctx,
	enum au_ini_mgr_type type);

boolean au_ini_mgr_read_part_indices_buffer(
	struct au_ini_mgr_ctx * ctx,
	char * buffer);

boolean au_ini_mgr_read_part_indices(
	struct au_ini_mgr_ctx * ctx,
	FILE * file);

boolean au_ini_mgr_write_part_indices(
	struct au_ini_mgr_ctx * ctx,
	FILE * file);

boolean au_ini_mgr_encrypt_save(struct au_ini_mgr_ctx * ctx);

boolean au_ini_mgr_set_value_by_id(
	struct au_ini_mgr_ctx * ctx,
	uint32_t section_id,
	uint32_t key_id,
	enum au_ini_mgr_key_type type,
	union au_ini_mgr_key_bin_data data);

boolean au_ini_mgr_set_value_by_name(
	struct au_ini_mgr_ctx * ctx,
	const char * section_name,
	const char * key_name,
	enum au_ini_mgr_key_type type,
	union au_ini_mgr_key_bin_data data);

uint32_t au_ini_mgr_add_key(
	struct au_ini_mgr_ctx * ctx,
	uint32_t section_id,
	const char * key_name,
	enum au_ini_mgr_key_type type,
	union au_ini_mgr_key_bin_data data);

inline boolean au_ini_mgr_is_key_available(int nKey)
{
	return (nKey != -1) ? TRUE : FALSE;
}

inline boolean au_ini_mgr_is_process_mode(
	struct au_ini_mgr_ctx * ctx,
	enum au_ini_mgr_process_mode mode)
{
	return (ctx->process_mode & mode) ? TRUE : FALSE; 
}

inline uint32_t au_ini_mgr_get_section_count(
	struct au_ini_mgr_ctx * ctx)
{
	return ctx->section_count;
}

/*
 * Fills [dst] with string determined by [fmt].
 *
 * Function assumes that [dst] is large enough to 
 * hold the entire string.
 */
size_t au_ini_mgr_print_compact(char * dst, const char * fmt, ...);

END_DECLS

#endif /* _AU_INI_MANAGER_H_ */