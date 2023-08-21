#ifndef _AP_MODULE_H_
#define _AP_MODULE_H_

#include "core/macros.h"
#include "core/types.h"

#include "utility/au_ini_manager.h"

#include <assert.h>

#define AP_MODULE_MAX_MODULE_NAME 40
#define AP_MODULE_MAX_STREAM_DATA_COUNT 8
#define AP_MODULE_MAX_MODULE_DATA_COUNT 8
#define AP_MODULE_MAX_ATTACHED_DATA_COUNT 32
#define AP_MODULE_MAX_CALLBACK_ID 32
#define AP_MODULE_MAX_CALLBACK_COUNT 16

BEGIN_DECLS

typedef void * ap_module_t;

struct ap_module_stream;

typedef boolean(*ap_module_default_t)(
	ap_module_t module_,
	void * data);

typedef boolean(*ap_module_stream_read_t)(
	ap_module_t module_,
	void * data, 
	struct ap_module_stream * stream);

typedef boolean(*ap_module_stream_write_t)(
	ap_module_t module_,
	void * data, 
	struct ap_module_stream * stream);

struct ap_module_stream_data {
	char module_name[64];
	ap_module_t callback_module;
	ap_module_stream_read_t read_cb;
	ap_module_stream_write_t write_cb;
	struct ap_module_stream_data * next;
};

struct ap_module_stream {
	struct au_ini_mgr_ctx * ini_mgr;
	uint32_t section_id;
	uint32_t value_id;
	uint32_t module_data_index;
	char section_name[64];
	const char * value_name;
	char * value;
	const char * module_name;
	const char * enum_end;
	size_t module_name_len;
	size_t enum_end_len;
};

struct ap_module_attached_data {
	size_t offset;
	ap_module_t module_;
	ap_module_default_t ctor;
	ap_module_default_t dtor;
};

struct ap_module_data {
	size_t size;
	ap_module_default_t ctor;
	ap_module_default_t dtor;
	struct ap_module_attached_data attached_data[AP_MODULE_MAX_ATTACHED_DATA_COUNT];
	uint32_t attached_data_count;
	struct ap_module_stream_data * stream_data;
};

struct ap_module {
	char name[AP_MODULE_MAX_MODULE_NAME];
	struct ap_module_data data[AP_MODULE_MAX_MODULE_DATA_COUNT];
	ap_module_t callback_modules[AP_MODULE_MAX_CALLBACK_ID][AP_MODULE_MAX_CALLBACK_COUNT];
	ap_module_default_t callbacks[AP_MODULE_MAX_CALLBACK_ID][AP_MODULE_MAX_CALLBACK_COUNT];
	uint32_t callback_count[AP_MODULE_MAX_CALLBACK_ID];
};

void ap_module_init(
	ap_module_t module_, 
	const char * name);

void ap_module_set_module_data(
	ap_module_t module_,
	uint32_t index, 
	size_t size,
	ap_module_default_t constructor,
	ap_module_default_t destructor);

/*
 * Attempts to add new attached data and returns the data 
 * offset if successful.
 * 
 * If attached data count has exceeded the limit, the 
 * function will fail and SIZE_MAX will be returned.
 */
size_t ap_module_attach_data(
	ap_module_t module_,
	uint32_t data_index,
	size_t size,
	ap_module_t callback_module,
	ap_module_default_t constructor,
	ap_module_default_t destructor);

inline void * ap_module_get_attached_data(
	void * data, 
	size_t offset)
{
	return (void *)((uintptr_t)data + offset);
}

size_t ap_module_get_module_data_size(
	ap_module_t module_, 
	uint32_t data_index);

void * ap_module_create_module_data(
	ap_module_t module_, 
	uint32_t data_index);

/**
 * Construct pre-allocated module data.
 * \param[in]     ctx        Module context pointer.
 * \param[in]     data_index Module data index.
 * \param[in,out] data       Pre-allocated module data.
 */
void ap_module_construct_module_data(
	ap_module_t module_, 
	uint32_t data_index,
	void * data);

void ap_module_destroy_module_data(
	ap_module_t module_, 
	uint32_t data_index,
	void * data);

/**
 * Destruct module data but do not deallocate.
 * \param[in]     ctx        Module context pointer.
 * \param[in]     data_index Module data index.
 * \param[in,out] data       Module data.
 */
void ap_module_destruct_module_data(
	ap_module_t module_, 
	uint32_t data_index,
	void * data);

void ap_module_add_callback(
	ap_module_t module_,
	uint32_t id,
	ap_module_t callback_module,
	ap_module_default_t callback);

boolean ap_module_enum_callback(
	ap_module_t module_,
	uint32_t id,
	void * data);

boolean ap_module_enum_callback_indexed(
	ap_module_t module_,
	uint32_t id,
	void * data,
	uint32_t callback_index);

struct ap_module_stream * ap_module_stream_create();

void ap_module_stream_destroy(struct ap_module_stream * stream);

boolean ap_module_stream_open(
	struct ap_module_stream * stream,
	const char * file,
	uint32_t part_index,
	boolean decrypt);

boolean ap_module_stream_parse(
	struct ap_module_stream * stream,
	void * data,
	uint32_t data_size,
	boolean decrypt);

boolean ap_module_stream_write(
	struct ap_module_stream * stream,
	const char * file,
	uint32_t part_index,
	boolean encrypt);

boolean ap_module_stream_write_to_buffer(
	struct ap_module_stream * stream,
	void * buffer,
	size_t buffer_size,
	uint32_t * written_count,
	boolean encrypt);

boolean ap_module_stream_set_section_name(
	struct ap_module_stream * stream,
	const char * section_name);

boolean ap_module_stream_write_f32(
	struct ap_module_stream * stream,
	const char * value_name,
	float value);

boolean ap_module_stream_write_i32(
	struct ap_module_stream * stream,
	const char * value_name,
	int32_t value);

boolean ap_module_stream_write_i64(
	struct ap_module_stream * stream,
	const char * value_name, 
	int64_t value);

boolean ap_module_stream_write_value(
	struct ap_module_stream * stream,
	const char * value_name, 
	const char * value);

boolean ap_module_stream_read_next_value(struct ap_module_stream * stream);

boolean ap_module_stream_read_prev_value(struct ap_module_stream * stream);

const char * ap_module_stream_get_value(struct ap_module_stream * stream);

boolean ap_module_stream_get_f32(
	struct ap_module_stream * stream,
	float * value);

boolean ap_module_stream_get_i32(
	struct ap_module_stream * stream,
	int32_t * value);

boolean ap_module_stream_get_i64(
	struct ap_module_stream * stream,
	int64_t * value);

boolean ap_module_stream_get_u32(
	struct ap_module_stream * stream,
	uint32_t * value);

boolean ap_module_stream_get_u64(
	struct ap_module_stream * stream,
	uint64_t * value);

boolean ap_module_stream_get_str(
	struct ap_module_stream * stream,
	char * value,
	size_t maxcount);

const char * ap_module_stream_get_value_name(struct ap_module_stream * stream);

boolean ap_module_stream_enum_write(
	ap_module_t module_,
	struct ap_module_stream * stream,
	uint16_t data_index,
	void * data);

boolean ap_module_stream_enum_read(
	ap_module_t module_,
	struct ap_module_stream * stream,
	uint16_t data_index, 
	void * data);

const char * ap_module_stream_read_section_name(
	struct ap_module_stream * stream,
	uint32_t section_id);

uint32_t ap_module_stream_get_section_count(struct ap_module_stream * stream);

boolean ap_module_stream_set_mode(
	struct ap_module_stream * stream,
	enum au_ini_mgr_mode mode);

boolean ap_module_stream_set_type(
	struct ap_module_stream * stream,
	enum au_ini_mgr_type type);

struct ap_module_stream_data * ap_module_stream_alloc_data();

void ap_module_stream_destroy_data(struct ap_module_stream_data * data);

void ap_module_stream_add_callback(
	ap_module_t module_,
	uint32_t data_index,
	const char * module_name,
	ap_module_t callback_module,
	ap_module_stream_read_t read_cb,
	ap_module_stream_write_t write_cb);

static inline uint32_t ap_module_get_callback_count(
	ap_module_t module_, 
	uint32_t id)
{
	assert(id < AP_MODULE_MAX_CALLBACK_ID);
	return ((struct ap_module *)module_)->callback_count[id];
}

END_DECLS

#endif /* _AP_MODULE_H_ */