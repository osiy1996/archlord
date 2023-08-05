#include "public/ap_object.h"

#include "core/file_system.h"
#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"
#include "core/vector.h"

#include "public/ap_admin.h"
#include "public/ap_config.h"
#include "public/ap_octree.h"
#include "public/ap_sector.h"

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>

#define INI_NAME_NAME			"Name"
#define	INI_NAME_OBJECT_TYPE	"ObjectType"
#define INI_NAME_TID			"TID"
#define INI_NAME_SCALE			"Scale"
#define INI_NAME_POSITION		"Position"
#define	INI_NAME_DEGREE_X		"DegreeX"
#define	INI_NAME_DEGREE_Y		"DegreeY"
#define INI_NAME_STATIC			"Static"
#define INI_NAME_AXIS			"Axis"
#define INI_NAME_DEGREE			"Degree"
#define INI_NAME_OCTREE_IDNUM	"OcTreeIDNum"
#define INI_NAME_OCTREE_IDDATA	"OcTreeIDData"

struct ap_object_module {
	struct ap_module_instance instance;
	struct ap_config_module * ap_config;
	struct ap_admin template_admin;
};

static boolean template_read(
	struct ap_object_module * mod,
	void * data, 
	struct ap_module_stream * stream)
{
	struct ap_object_template * tmp = data;
	while (ap_module_stream_read_next_value(stream)) {
		const char * value_name = ap_module_stream_get_value_name(stream);
		if (strcmp(value_name, INI_NAME_NAME) == 0) {
			size_t i;
			size_t len;
			ap_module_stream_get_str(stream, tmp->name, sizeof(tmp->name));
			len = strlen(tmp->name);
			for (i = 0; i < len; i++) {
				if (!isascii(tmp->name[i]))
					tmp->name[i] = '\0';
			}
		}
		else if (strcmp(value_name, INI_NAME_OBJECT_TYPE) == 0) {
			ap_module_stream_get_u32(stream, &tmp->type);
		}
		else {
			assert(0);
		}
	}
	return TRUE;
}

static boolean template_write(
	struct ap_object_module * mod,
	struct ap_object_template * temp,
	struct ap_module_stream * stream)
{
	boolean result = TRUE;
	result &= ap_module_stream_write_value(stream, INI_NAME_NAME, temp->name);
	result &= ap_module_stream_write_i32(stream, INI_NAME_OBJECT_TYPE, temp->type);
	return result;
}

boolean ap_object_object_read(
	struct ap_object_module * mod,
	void * data, 
	struct ap_module_stream * stream)
{
	struct ap_object * obj = data;
	while (ap_module_stream_read_next_value(stream)) {
		const char * value_name = ap_module_stream_get_value_name(stream);
		const char * value = ap_module_stream_get_value(stream);
		if (strcmp(value_name, INI_NAME_TID) == 0) {
			obj->tid = strtoul(value, NULL, 10);
			obj->temp = ap_object_get_template(mod, obj->tid);
		}
		else if (strcmp(value_name, INI_NAME_OBJECT_TYPE) == 0) {
			obj->object_type = strtol(value, NULL, 10);
		}
		else if (strcmp(value_name, INI_NAME_SCALE) == 0) {
			if (sscanf(value, "%f,%f,%f", &obj->scale.x,
					&obj->scale.y, &obj->scale.z) != 3) {
				ERROR("Failed to read object scale (%u).",
					obj->object_id);
				return FALSE;
			}
		}
		else if (strcmp(value_name, INI_NAME_POSITION) == 0) {
			if (sscanf(value, "%f,%f,%f", &obj->position.x,
					&obj->position.y, &obj->position.z) != 3) {
				ERROR("Failed to read object position (%u).",
					obj->object_id);
				return FALSE;
			}
		}
		else if (strcmp(value_name, INI_NAME_DEGREE_X) == 0) {
			obj->rotation_x = strtof(value, NULL);
		}
		else if (strcmp(value_name, INI_NAME_DEGREE_Y) == 0) {
			obj->rotation_y = strtof(value, NULL);
		}
		else if (strcmp(value_name, INI_NAME_OCTREE_IDNUM) == 0) {
			obj->octree_id = strtol(value, NULL, 10);
		}
		else if (strncmp(value_name, INI_NAME_OCTREE_IDDATA, 
				strlen(INI_NAME_OCTREE_IDDATA)) == 0) {
			struct ap_octree_id_list * id = alloc(sizeof(*id));
			struct ap_octree_id_list * cur = obj->octree_id_list;
			struct ap_octree_id_list * last = NULL;
			memset(id, 0, sizeof(*id));
			if (sscanf(value, "%hd,%hd,%d", &id->six, &id->siz,
					&id->id) != 3) {
				ERROR("Failed to read octree data (%u).",
					obj->object_id);
				return FALSE;
			}
			while (cur) {
				last = cur;
				cur = cur->next;
			}
			if (last)
				last->next = id;
			else
				obj->octree_id_list = id;
		}
		else {
			assert(0);
		}
	}
	return TRUE;
}

boolean ap_object_object_write(
	struct ap_object_module * mod,
	void * data, 
	struct ap_module_stream * stream)
{
	struct ap_object * obj = data;
	char tmp[128];
	struct ap_octree_id_list * octlist;
	uint32_t index;
	if (!ap_module_stream_write_i32(stream, INI_NAME_TID, obj->tid)) {
		ERROR("Failed to write object TID (oid = %u).",
			obj->object_id);
		return FALSE;
	}
	if (!ap_module_stream_write_i32(stream, INI_NAME_OBJECT_TYPE, 
			obj->object_type)) {
		ERROR("Failed to write object type (oid = %u).",
			obj->object_id);
		return FALSE;
	}
	snprintf(tmp, sizeof(tmp), "%f,%f,%f", 
		obj->scale.x, obj->scale.y, obj->scale.z);
	if (!ap_module_stream_write_value(stream, INI_NAME_SCALE, tmp)) {
		ERROR("Failed to write object scale (oid = %u).",
			obj->object_id);
		return FALSE;
	}
	snprintf(tmp, sizeof(tmp), "%f,%f,%f", 
		obj->position.x, obj->position.y, obj->position.z);
	if (!ap_module_stream_write_value(stream, INI_NAME_POSITION, tmp)) {
		ERROR("Failed to write object position (oid = %u).",
			obj->object_id);
		return FALSE;
	}
	if (!ap_module_stream_write_f32(stream, INI_NAME_DEGREE_X, 
			obj->rotation_x) ||
		!ap_module_stream_write_f32(stream, INI_NAME_DEGREE_Y, 
			obj->rotation_y)) {
		ERROR("Failed to write object rotation (oid = %u).",
			obj->object_id);
		return FALSE;
	}
	if (!ap_module_stream_write_i32(stream, INI_NAME_OCTREE_IDNUM, 
			obj->octree_id)) {
		ERROR("Failed to write object type (oid = %u).",
			obj->object_id);
		return FALSE;
	}
	index = 0;
	octlist = obj->octree_id_list;
	while (octlist) {
		char value[128];
		snprintf(tmp, sizeof(tmp), "%s%u", 
			INI_NAME_OCTREE_IDDATA, index);
		snprintf(value, sizeof(value), "%d,%d,%d", 
			octlist->six, octlist->siz, octlist->id);
		if (!ap_module_stream_write_value(stream, tmp, value)) {
			ERROR("Failed to write object octree data (oid = %u).",
				obj->object_id);
			return FALSE;
		}
		index++;
		octlist = octlist->next;
	}
	return TRUE;
}

static boolean ctor(struct ap_object_module * mod, struct ap_object * obj)
{
	obj->base_type = AP_BASE_TYPE_OBJECT;
	return TRUE;
}

static boolean onregister(
	struct ap_object_module * mod,
	struct ap_module_registry * registry)
{
	mod->ap_config = ap_module_registry_get_module(registry, AP_CONFIG_MODULE_NAME);
	return (mod->ap_config != NULL);
}

static void onclose(struct ap_object_module * mod)
{
	size_t index = 0;
	void * object;
	while (ap_admin_iterate_id(&mod->template_admin, &index, &object)) {
		ap_module_destruct_module_data(mod, AP_OBJECT_MDI_OBJECT_TEMPLATE, 
			*(struct ap_object_template **)object);
	}
	ap_admin_clear_objects(&mod->template_admin);
}

static void onshutdown(struct ap_object_module * mod)
{
	ap_admin_destroy(&mod->template_admin);
}

struct ap_object_module * ap_object_create_module()
{
	struct ap_object_module * mod = ap_module_instance_new(AP_OBJECT_MODULE_NAME,
		sizeof(*mod), onregister, NULL, NULL, onshutdown);
	ap_admin_init(&mod->template_admin, sizeof(struct ap_object_template *), 128);
	ap_object_add_stream_callback(mod, AP_OBJECT_MDI_OBJECT_TEMPLATE,
		AP_OBJECT_MODULE_NAME, mod, template_read, template_write);
	ap_object_add_stream_callback(mod, AP_OBJECT_MDI_OBJECT,
		AP_OBJECT_MODULE_NAME, mod, ap_object_object_read, ap_object_object_write);
	ap_module_set_module_data(mod, AP_OBJECT_MDI_OBJECT_TEMPLATE, 
		sizeof(struct ap_object_template),
		NULL, NULL);
	ap_module_set_module_data(mod, AP_OBJECT_MDI_OBJECT, sizeof(struct ap_object),
		ctor, NULL);
	return mod;
}

/*
static void load_all()
{
	uint32_t x = 0;
	for (x = 0; x < 50; x++) {
		uint32_t z = 0;
		for (z = 0; z < 50; z++) {
			char path[512];
			uint32_t index;
			if (!make_path(path, sizeof(path), "%s/ini/obj%05u.ini",
					ap_config_get(mod->ap_config, "ClientDir"), 
					x * 100 + z)) {
				ERROR("Failed to create path.");
				return;
			}
			for (index = 0; index < 256; index++) {
			struct ap_module_stream * stream;
				uint32_t count;
				uint32_t i;
				stream = ap_module_stream_create();
				ap_module_stream_set_mode(stream, 
					AU_INI_MGR_MODE_NAME_OVERWRITE);
				ap_module_stream_set_type(stream,
					AU_INI_MGR_TYPE_PART_INDEX);
				if (!ap_module_stream_open(stream, path, index, FALSE)) {
					ap_module_stream_destroy(stream);
					continue;
				}
				count = ap_module_stream_get_section_count(stream);
				for (i = 0; i < count; i++) {
					uint32_t object_id = strtoul(
						ap_module_stream_read_section_name(stream, i), 
						NULL, 10);
					struct ap_object * obj = 
						ap_module_create_module_data( mod, AP_OBJECT_MDI_OBJECT);
					obj->object_id = object_id;
					if (!ap_module_stream_enum_read(mod, stream, 
							AP_OBJECT_MDI_OBJECT, obj)) {
						ERROR("Failed to read object (oid = %u).",
							object_id);
						ap_module_stream_destroy(stream);
						continue;
					}
					ap_object_destroy(mod, obj);
				}
				ap_module_stream_destroy(stream);
			}
		}
	}
}
*/

boolean ap_object_load_templates(
	struct ap_object_module * mod,
	const char * file_path, 
	boolean decrypt)
{
	struct ap_module_stream * stream;
	uint32_t count;
	uint32_t i;
	stream = ap_module_stream_create();
	if (!ap_module_stream_open(stream, file_path, 0, decrypt)) {
		ERROR("Failed to open stream (%s).", file_path);
		ap_module_stream_destroy(stream);
		return FALSE;
	}
	count = ap_module_stream_get_section_count(stream);
	for (i = 0; i < count; i++) {
		uint32_t template_id = strtoul(
			ap_module_stream_read_section_name(stream, i), NULL, 10);
		struct ap_object_template ** object = ap_admin_add_object_by_id(
			&mod->template_admin, template_id);
		struct ap_object_template * temp;
		if (!object) {
			ERROR("Invalid object template id (%u).", template_id);
			ap_module_stream_destroy(stream);
			return FALSE;
		}
		temp = ap_module_create_module_data(mod, AP_OBJECT_MDI_OBJECT_TEMPLATE);
		temp->tid = template_id;
		*object = temp;
		if (!ap_module_stream_enum_read(mod, stream, 
				AP_OBJECT_MDI_OBJECT_TEMPLATE, temp)) {
			ERROR("Failed to read object template (tid = %u).", template_id);
			ap_module_stream_destroy(stream);
			return FALSE;
		}
	}
	ap_module_stream_destroy(stream);
	return TRUE;
}

static int sorttemplates(
	const struct ap_object_template * const * t1,
	const struct ap_object_template * const * t2)
{
	return ((*t1)->tid - (*t2)->tid);
}

boolean ap_object_write_templates(
	struct ap_object_module * mod,
	const char * file_path, 
	boolean encrypt)
{
	struct ap_module_stream * stream;
	size_t index = 0;
	size_t count = 0;
	void * object;
	struct ap_object_template * temp;
	struct ap_object_template ** list = vec_new_reserved(sizeof(*list), 
		ap_admin_get_object_count(&mod->template_admin));
	stream = ap_module_stream_create();
	ap_module_stream_set_mode(stream, AU_INI_MGR_MODE_NAME_OVERWRITE);
	ap_module_stream_set_type(stream, AU_INI_MGR_TYPE_KEY_INDEX);
	while (ap_admin_iterate_id(&mod->template_admin, &index, &object))
		vec_push_back((void **)&list, object);
	count = vec_count(list);
	qsort(list, count, sizeof(*list), sorttemplates);
	for (index = 0; index < count; index++) {
		char name[32];
		temp = list[index];
		snprintf(name, sizeof(name), "%u", temp->tid);
		if (!ap_module_stream_set_section_name(stream, name)) {
			ERROR("Failed to set section name.");
			ap_module_stream_destroy(stream);
			vec_free(list);
			return FALSE;
		}
		if (!ap_module_stream_enum_write(mod, stream, 
				AP_OBJECT_MDI_OBJECT_TEMPLATE, temp)) {
			ERROR("Failed to write object template (%s).", name);
			ap_module_stream_destroy(stream);
			vec_free(list);
			return FALSE;
		}
	}
	vec_free(list);
	if (!ap_module_stream_write(stream, file_path, 0, encrypt)) {
		ERROR("Failed to write object templates..");
		ap_module_stream_destroy(stream);
		return FALSE;
	}
	ap_module_stream_destroy(stream);
	return TRUE;
}

void ap_object_add_callback(
	struct ap_object_module * mod,
	enum ap_object_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	ap_module_add_callback(mod, id, callback_module, callback);
}

void ap_object_add_stream_callback(
	struct ap_object_module * mod,
	enum ap_object_module_data_index data_index,
	const char * module_name,
	ap_module_t callback_module,
	ap_module_stream_read_t read_cb,
	ap_module_stream_write_t write_cb)
{
	if (data_index < 0 || data_index >= AP_OBJECT_MDI_COUNT) {
		ERROR("Invalid stream data index: %u.");
		return;
	}
	ap_module_stream_add_callback(mod, data_index,
		module_name, callback_module, read_cb, write_cb);
}

size_t ap_object_attach_data(
	struct ap_object_module * mod,
	enum ap_object_module_data_index data_index,
	size_t size,
	ap_module_t callback_module,
	ap_module_default_t constructor,
	ap_module_default_t destructor)
{
	return ap_module_attach_data(mod, data_index, size, 
		callback_module, constructor, destructor);
}

struct ap_object_template * ap_object_get_template(
	struct ap_object_module * mod,
	uint32_t tid)
{
	struct ap_object_template ** object = 
		ap_admin_get_object_by_id(&mod->template_admin, tid);
	if (object)
		return *object;
	else
		return NULL;
}

struct ap_object_template * ap_object_iterate_templates(
	struct ap_object_module * mod,
	size_t * index)
{
	struct ap_object_template ** object;
	if (!ap_admin_iterate_id(&mod->template_admin, index, (void **)&object))
		return NULL;
	else
		return  *object;
}

struct ap_object * ap_object_create(struct ap_object_module * mod)
{
	struct ap_object * obj = ap_module_create_module_data(
		mod, AP_OBJECT_MDI_OBJECT);
	return obj;
}

struct ap_object * ap_object_duplicate(
	struct ap_object_module * mod,
	struct ap_object * obj)
{
	struct ap_object * dobj = ap_object_create(mod);
	struct ap_object_cb_copy_object_data data = { 0 };
	dobj->tid = obj->tid;
	dobj->scale = obj->scale;
	dobj->position = obj->position;
	dobj->matrix = obj->matrix;
	dobj->rotation_x = obj->rotation_x;
	dobj->rotation_y = obj->rotation_y;
	dobj->is_static = obj->is_static;
	dobj->current_status = obj->current_status;
	dobj->object_type = obj->object_type;
	dobj->temp = obj->temp;
	data.src = obj;
	data.dst = dobj;
	if (!ap_module_enum_callback(mod, AP_OBJECT_CB_COPY_OBJECT, &data)) {
		WARN("Failed to copy duplicated object.");
		ap_object_destroy(mod, obj);
		return NULL;
	}
	return dobj;
}

void ap_object_destroy(struct ap_object_module * mod, struct ap_object * obj)
{
	ap_module_destroy_module_data(mod, 
		AP_OBJECT_MDI_OBJECT, obj);
	/*
	struct ap_octree_id_list * cur = obj->octree_id_list;
	while (cur) {
		struct ap_octree_id_list * next = cur->next;
		dealloc(cur);
		cur = next;
	}
	*/
}

boolean ap_object_load_sector(
	struct ap_object_module * mod,
	const char * object_dir,
	struct ap_object *** list,
	uint32_t x, 
	uint32_t z)
{
	char path[512];
	uint32_t map_x = x / AP_SECTOR_DEFAULT_DEPTH;
	uint32_t map_z = z / AP_SECTOR_DEFAULT_DEPTH;
	uint32_t sector_x = x - map_x * AP_SECTOR_DEFAULT_DEPTH;
	uint32_t sector_z = z - map_z * AP_SECTOR_DEFAULT_DEPTH;
	uint32_t part_index =
		sector_z * AP_SECTOR_DEFAULT_DEPTH + sector_x;
	struct ap_module_stream * stream;
	uint32_t c;
	uint32_t i;
	if (!make_path(path, sizeof(path), "%s/obj%05u.ini",
			object_dir, map_x * 100 + map_z)) {
		ERROR("Failed to create path.");
		return FALSE;
	}
	stream = ap_module_stream_create();
	ap_module_stream_set_mode(stream, 
		AU_INI_MGR_MODE_NAME_OVERWRITE);
	ap_module_stream_set_type(stream,
		AU_INI_MGR_TYPE_PART_INDEX);
	if (!ap_module_stream_open(stream, path, part_index, FALSE)) {
		ap_module_stream_destroy(stream);
		return FALSE;
	}
	c = ap_module_stream_get_section_count(stream);
	for (i = 0; i < c; i++) {
		uint32_t object_id = strtoul(
			ap_module_stream_read_section_name(stream, i), 
			NULL, 10);
		struct ap_object * obj = ap_object_create(mod);
		obj->object_id = object_id;
		if (!ap_module_stream_enum_read(mod, stream, 
				AP_OBJECT_MDI_OBJECT, obj)) {
			ERROR("Failed to read object (oid = %u).",
				object_id);
			c--;
			i--;
			ap_object_destroy(mod, obj);
			continue;
		}
		if (!ap_module_enum_callback(mod, AP_OBJECT_CB_INIT_OBJECT, obj)) {
			WARN("Failed to initialize object (%u).", 
				obj->object_id);
			ap_object_destroy(mod, obj);
			continue;
		}
		vec_push_back((void **)list, &obj);
	}
	ap_module_stream_destroy(stream);
	return TRUE;
}

boolean ap_object_write_sector(
	struct ap_object_module * mod,
	const char * object_dir,
	uint32_t x, 
	uint32_t z,
	struct ap_object ** objects)
{
	char path[512];
	uint32_t map_x = x / AP_SECTOR_DEFAULT_DEPTH;
	uint32_t map_z = z / AP_SECTOR_DEFAULT_DEPTH;
	uint32_t sector_x = x - map_x * AP_SECTOR_DEFAULT_DEPTH;
	uint32_t sector_z = z - map_z * AP_SECTOR_DEFAULT_DEPTH;
	uint32_t part_index =
		sector_z * AP_SECTOR_DEFAULT_DEPTH + sector_x;
	struct ap_module_stream * stream;
	uint32_t c = vec_count(objects);
	uint32_t i;
	if (!make_path(path, sizeof(path), "%s/obj%05u.ini",
			object_dir, map_x * 100 + map_z)) {
		ERROR("Failed to create path.");
		return FALSE;
	}
	stream = ap_module_stream_create();
	ap_module_stream_set_mode(stream, 
		AU_INI_MGR_MODE_NAME_OVERWRITE);
	ap_module_stream_set_type(stream,
		AU_INI_MGR_TYPE_PART_INDEX);
	for (i = 0; i < c; i++) {
		struct ap_object * obj = objects[i];
		char section_name[128];
		snprintf(section_name, sizeof(section_name), "%u",
			obj->object_id);
		if (!ap_module_stream_set_section_name(stream, section_name)) {
			ERROR("Failed to set section name (oid = %u).",
				obj->object_id);
			ap_module_stream_destroy(stream);
			return FALSE;
		}
		if (!ap_module_stream_enum_write(mod, stream, 
				AP_OBJECT_MDI_OBJECT, obj)) {
			ERROR("Failed to write object (oid = %u).",
				obj->object_id);
			ap_module_stream_destroy(stream);
			return FALSE;
		}
	}
	if (!ap_module_stream_write(stream, path, part_index, FALSE)) {
		ERROR("Failed to write object sector (x = %u, z = %u).",
			x, z);
		ap_module_stream_destroy(stream);
		return FALSE;
	}
	ap_module_stream_destroy(stream);
	return TRUE;
}

void ap_object_move_object(
	struct ap_object_module * mod,
	struct ap_object * obj, 
	const struct au_pos * pos)
{
	struct ap_object_cb_move_object_data data = { 0 };
	data.obj = obj;
	data.prev_pos = obj->position;
	obj->position = *pos;
	ap_module_enum_callback(mod, AP_OBJECT_CB_MOVE_OBJECT, &data);
}
