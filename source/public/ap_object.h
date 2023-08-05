#ifndef _AP_OBJECT_H_
#define _AP_OBJECT_H_

#include "public/ap_base.h"
#include "public/ap_define.h"
#include "public/ap_module_instance.h"

#define AP_OBJECT_MODULE_NAME "ApmObject"

#define AP_OBJECT_STREAM_OBJECT 0
#define AP_OBJECT_STREAM_OBJECT_TEMPLATE 1

#define AP_OBJECT_MAX_OBJECT_NAME 32
#define AP_OBJECT_MAX_BLOCK_INFO 12

BEGIN_DECLS

struct ap_object_module;
struct ap_octree_id_list;

enum ap_object_module_data_index {
	AP_OBJECT_MDI_OBJECT_TEMPLATE,
	AP_OBJECT_MDI_OBJECT,
	AP_OBJECT_MDI_COUNT
};

enum ap_object_callback_id {
	AP_OBJECT_CB_INIT_OBJECT,
	AP_OBJECT_CB_COPY_OBJECT,
	AP_OBJECT_CB_MOVE_OBJECT,
};

struct ap_object_template {
	uint32_t tid;
	char name[AP_OBJECT_MAX_OBJECT_NAME];
	uint32_t type;
};

struct ap_object {
	enum ap_base_type base_type;
	uint32_t object_id;
	uint32_t dimension;
	uint32_t tid;
	struct au_pos scale;
	struct au_pos position;
	struct au_matrix matrix;
	float rotation_x;
	float rotation_y;
	uint16_t block_info_count;
	struct au_blocking block_info[AP_OBJECT_MAX_BLOCK_INFO];
	boolean is_static;
	uint32_t current_status;
	uint32_t object_type;
	uint32_t octree_id;
	struct ap_octree_id_list * octree_id_list;
	uint32_t remove_time_msec;
	boolean added_to_world;
	struct ap_object_template * temp;
};

struct ap_object_cb_copy_object_data {
	struct ap_object * src;
	struct ap_object * dst;
};

struct ap_object_cb_move_object_data {
	struct ap_object * obj;
	struct au_pos prev_pos;
};

struct ap_object_module * ap_object_create_module();

boolean ap_object_load_templates(
	struct ap_object_module * mod,
	const char * file_path, 
	boolean decrypt);

boolean ap_object_write_templates(
	struct ap_object_module * mod,
	const char * file_path, 
	boolean encrypt);

void ap_object_add_callback(
	struct ap_object_module * mod,
	enum ap_object_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

void ap_object_add_stream_callback(
	struct ap_object_module * mod,
	enum ap_object_module_data_index data_index,
	const char * module_name,
	ap_module_t callback_module,
	ap_module_stream_read_t read_cb,
	ap_module_stream_write_t write_cb);

size_t ap_object_attach_data(
	struct ap_object_module * mod,
	enum ap_object_module_data_index data_index,
	size_t size,
	ap_module_t callback_module,
	ap_module_default_t constructor,
	ap_module_default_t destructor);

struct ap_object_template * ap_object_get_template(
	struct ap_object_module * mod,
	uint32_t tid);

struct ap_object_template * ap_object_iterate_templates(
	struct ap_object_module * mod,
	size_t * index);

struct ap_object * ap_object_create(struct ap_object_module * mod);

struct ap_object * ap_object_duplicate(
	struct ap_object_module * mod,
	struct ap_object * obj);

void ap_object_destroy(struct ap_object_module * mod, struct ap_object * obj);

/*
 * If successful, returns a vector of `struct ap_object` 
 * pointers.
 * 
 * Otherwise, returns NULL.
 */
boolean ap_object_load_sector(
	struct ap_object_module * mod,
	const char * object_dir,
	struct ap_object *** list,
	uint32_t x, 
	uint32_t z);

boolean ap_object_write_sector(
	struct ap_object_module * mod,
	const char * object_dir,
	uint32_t x, 
	uint32_t z,
	struct ap_object ** objects);

void ap_object_move_object(
	struct ap_object_module * mod,
	struct ap_object * obj, 
	const struct au_pos * pos);

END_DECLS

#endif /* _AP_OBJECT_H_ */
