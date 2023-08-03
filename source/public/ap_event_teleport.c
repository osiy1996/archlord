#include "public/ap_event_teleport.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"
#include "core/vector.h"

#include "public/ap_admin.h"
#include "public/ap_character.h"
#include "public/ap_event_manager.h"
#include "public/ap_object.h"
#include "public/ap_packet.h"

#include  "utility/au_packet.h"

#include <assert.h>
#include <stdlib.h>

#define	STREAM_DESCRIPTION "Description"
#define	STREAM_RADIUS_MIN "RadiusMin"
#define	STREAM_RADIUS_MAX "RadiusMax"
#define	STREAM_POINT_TYPE "PointTargetType"
#define	STREAM_POINT_POS "PointPos"
#define	STREAM_POINT_BASE "PointBase"
#define STREAM_POINT_EVENT "PointEvent"
#define	STREAM_USE_TYPE "UseType"
#define	STREAM_POINT_GROUP_NAME "Group"
#define	STREAM_POINT_TARGET_NAME "TargetGroup"
#define	STREAM_POINT_REGION_TYPE "RegionType"
#define	STREAM_POINT_SPECIAL_TYPE "SpecialType"

/* Event stream. */
#define STREAM_POINT_NAME "PointName"
#define STREAM_TELEPORT_END "TeleportEnd"

struct ap_event_teleport_module {
	struct ap_module_instance instance;
	struct ap_character_module * ap_character;
	struct ap_event_manager_module * ap_event_manager;
	struct ap_packet_module * ap_packet;
	struct au_packet packet;
	struct ap_admin point_admin;
	struct ap_admin group_admin;
	size_t character_attachment_offset;
};

static boolean cbpointread(
	struct ap_event_teleport_module * mod,
	struct ap_event_teleport_point * point,
	struct ap_module_stream * stream)
{
	while (ap_module_stream_read_next_value(stream)) {
		const char * value_name = ap_module_stream_get_value_name(stream);
		const char * value = ap_module_stream_get_value(stream);
		if (strcmp(value_name, STREAM_DESCRIPTION) == 0) {
			strlcpy(point->description, value, sizeof(point->description));
		}
		else if (strcmp(value_name, STREAM_RADIUS_MIN) == 0) {
			point->radius_min = strtof(value, NULL);
		}
		else if (strcmp(value_name, STREAM_RADIUS_MAX) == 0) {
			point->radius_max = strtof(value, NULL);
		}
		else if (strcmp(value_name, STREAM_POINT_TYPE) == 0) {
			point->target_type = strtol(value, NULL, 10);
		}
		else if (strcmp(value_name, STREAM_POINT_POS) == 0) {
			if (point->target_type == AP_EVENT_TELEPORT_TARGET_POS) {
				if (sscanf(value, "%f:%f:%f", &point->target.pos.x,
						&point->target.pos.y, &point->target.pos.z) != 3) {
					ERROR("Failed to read point position (%s).", point->point_name);
					return FALSE;
				}
			}
		}
		else if (strcmp(value_name, STREAM_POINT_BASE) == 0) {
			if (point->target_type == AP_EVENT_TELEPORT_TARGET_BASE) {
				if (sscanf(value, "%d:%d", &point->target.base.type,
						&point->target.base.id) != 2) {
					ERROR("Failed to read point base (%s).", point->point_name);
					return FALSE;
				}
				assert(point->target.base.type == AP_BASE_TYPE_OBJECT);
				assert(point->target.base.id != 0);
			}
		}
		else if (strcmp(value_name, STREAM_POINT_EVENT) == 0) {
			point->attach_event = strtol(value, NULL, 10) != 0;
		}
		else if (strcmp(value_name, STREAM_USE_TYPE) == 0) {
			point->use_type = strtol(value, NULL, 10);
		}
		else if (strcmp(value_name, STREAM_POINT_GROUP_NAME) == 0) {
			struct ap_event_teleport_group ** grouppointer;
			struct ap_event_teleport_group * group;
			if (point->group_count >= COUNT_OF(point->groups)) {
				ERROR("Exceeded group count limit (%s).", point->point_name);
				return FALSE;
			}
			grouppointer = ap_admin_get_object_by_name(&mod->group_admin, value);
			if (!grouppointer) {
				grouppointer = ap_admin_add_object_by_name(&mod->group_admin, value);
				assert(grouppointer != NULL);
				group = ap_module_create_module_data(mod, AP_EVENT_TELEPORT_MDI_TELEPORT_GROUP);
				*grouppointer = group;
				strlcpy(group->name, value, sizeof(group->name));
			}
			else {
				group = *grouppointer;
				if (group->destination_point_count >= AP_EVENT_TELEPORT_MAX_GROUP_POINT_COUNT) {
					ERROR("Teleport group has too many teleport points.");
					return FALSE;
				}
			}
			group->destination_points[group->destination_point_count++] = point;
			point->groups[point->group_count++] = group;
		}
		else if (strcmp(value_name, STREAM_POINT_TARGET_NAME) == 0) {
			struct ap_event_teleport_group ** grouppointer;
			struct ap_event_teleport_group * group;
			if (point->target_group_count >= COUNT_OF(point->target_groups)) {
				ERROR("Exceeded target group count limit (%s).", point->point_name);
				return FALSE;
			}
			grouppointer = ap_admin_get_object_by_name(&mod->group_admin, value);
			if (!grouppointer) {
				grouppointer = ap_admin_add_object_by_name(&mod->group_admin, value);
				assert(grouppointer != NULL);
				group = ap_module_create_module_data(mod, AP_EVENT_TELEPORT_MDI_TELEPORT_GROUP);
				*grouppointer = group;
				strlcpy(group->name, value, sizeof(group->name));
			}
			else {
				group = *grouppointer;
				if (group->source_point_count >= AP_EVENT_TELEPORT_MAX_GROUP_POINT_COUNT) {
					ERROR("Teleport group has too many teleport points.");
					return FALSE;
				}
			}
			group->source_points[group->source_point_count++] = point;
			point->target_groups[point->target_group_count++] = group;
		}
		else if (strcmp(value_name, STREAM_POINT_REGION_TYPE) == 0) {
			point->region_type = strtol(value, NULL, 10);
		}
		else if (strcmp(value_name, STREAM_POINT_SPECIAL_TYPE) == 0) {
			point->special_type = strtol(value, NULL, 10);
		}
		else {
			assert(0);
		}
	}
	return TRUE;
}

static boolean cbpointwrite(
	struct ap_event_teleport_module * mod,
	struct ap_event_teleport_point * point,
	struct ap_module_stream * stream)
{
	boolean result = TRUE;
	char buffer[128];
	uint32_t i;
	result &= ap_module_stream_write_value(stream, STREAM_DESCRIPTION, point->description);
	result &= ap_module_stream_write_f32(stream, STREAM_RADIUS_MIN, point->radius_min);
	result &= ap_module_stream_write_f32(stream, STREAM_RADIUS_MAX, point->radius_max);
	result &= ap_module_stream_write_i32(stream, STREAM_POINT_TYPE, point->target_type);
	result &= ap_module_stream_write_i32(stream, STREAM_POINT_REGION_TYPE, point->region_type);
	result &= ap_module_stream_write_i32(stream, STREAM_POINT_SPECIAL_TYPE, point->special_type);
	au_ini_mgr_print_compact(buffer, "%f:%f:%f", point->target.pos.x, 
		point->target.pos.y, point->target.pos.z);
	result &= ap_module_stream_write_value(stream, STREAM_POINT_POS, buffer);
	sprintf(buffer, "%d:%u", point->target.base.type, 
		point->target.base.id);
	result &= ap_module_stream_write_value(stream, STREAM_POINT_BASE, buffer);
	result &= ap_module_stream_write_i32(stream, STREAM_POINT_EVENT, point->attach_event);
	result &= ap_module_stream_write_i32(stream, STREAM_USE_TYPE, point->use_type);
	for (i = 0; i < point->group_count; i++)
		result &= ap_module_stream_write_value(stream, STREAM_POINT_GROUP_NAME, point->groups[i]->name);
	for (i = 0; i < point->target_group_count; i++)
		result &= ap_module_stream_write_value(stream, STREAM_POINT_TARGET_NAME, point->target_groups[i]->name);
	return TRUE;
}

static boolean cbgroupread(
	struct ap_event_teleport_module * mod,
	struct ap_event_teleport_group * group,
	struct ap_module_stream * stream)
{
	while (ap_module_stream_read_next_value(stream)) {
		const char * value_name = ap_module_stream_get_value_name(stream);
		const char * value = ap_module_stream_get_value(stream);
		if (strcmp(value_name, STREAM_DESCRIPTION) == 0) {
			strlcpy(group->description, value, sizeof(group->description));
		}
		else {
			assert(0);
		}
	}
	return TRUE;
}

static boolean cbgroupwrite(
	struct ap_event_teleport_module * mod,
	struct ap_event_teleport_group * group,
	struct ap_module_stream * stream)
{
	return ap_module_stream_write_value(stream, STREAM_DESCRIPTION, group->description);
}

static boolean event_ctor(struct ap_event_teleport_module * mod, void * data)
{
	struct ap_event_manager_event * e = data;
	e->data = alloc(sizeof(struct ap_event_teleport_event));
	memset(e->data, 0, sizeof(struct ap_event_teleport_event));
	return TRUE;
}

static boolean event_dtor(
	struct ap_event_teleport_module * mod, 
	struct ap_event_manager_event * e)
{
	struct ap_event_teleport_event * teleportevent = ap_event_teleport_get_event(e);
	dealloc(teleportevent);
	e->data = NULL;
	return TRUE;
}

static inline uint64_t makeid(uint32_t base_type, uint32_t base_id)
{
	return ((uint64_t)base_type << 32) | base_id;
}

static boolean event_read(
	struct ap_event_teleport_module * mod, 
	struct ap_event_manager_event * event,
	struct ap_module_stream * stream)
{
	struct ap_event_teleport_event * e = ap_event_teleport_get_event(event);
	while (ap_module_stream_read_next_value(stream)) {
		const char * value_name = 
			ap_module_stream_get_value_name(stream);
		if (strcmp(value_name, STREAM_POINT_NAME) == 0) {
			struct ap_event_teleport_point ** pointentry;
			if (!ap_module_stream_get_str(stream, 
					e->point_name, sizeof(e->point_name))) {
				ERROR("Failed to read event teleport point name.");
				return FALSE;
			}
			pointentry = ap_admin_get_object_by_name(&mod->point_admin, e->point_name);
			if (!pointentry) {
				assert(event->source != NULL);
				WARN("Failed to retrieve event teleport point: type (%d), id (%u).", 
					ap_base_get_type(event->source), ap_base_get_id(event->source));
			}
			else {
				e->point = *pointentry;
				if (e->point->target_type != AP_EVENT_TELEPORT_TARGET_BASE) {
					WARN("Event teleport point target type does not match (%s).",
						e->point_name);
					e->point->target_type = AP_EVENT_TELEPORT_TARGET_BASE;
				}
				if (e->point->target.base.type != ap_base_get_type(event->source)) {
					WARN("Event teleport point target base type does not match (%s).",
						e->point_name);
					e->point->target.base.type = ap_base_get_type(event->source);
				}
				if (e->point->target.base.id != ap_base_get_id(event->source)) {
					WARN("Event teleport point target base id does not match (%s).",
						e->point_name);
					e->point->target.base.id = ap_base_get_id(event->source);
				}
				e->point->target_base = event->source;
			}
		}
		else if (strcmp(value_name, STREAM_TELEPORT_END) == 0) {
			break;
		}
		else {
			assert(0);
		}
	}
	return TRUE;
}

static boolean event_write(
	struct ap_event_teleport_module * mod, 
	void * data, 
	struct ap_module_stream * stream)
{
	struct ap_event_teleport_event * e = ap_event_teleport_get_event(data);
	if (!ap_module_stream_write_value(stream, STREAM_POINT_NAME, e->point_name) ||
		!ap_module_stream_write_i32(stream, STREAM_TELEPORT_END, 0)) {
		ERROR("Failed to write event teleport stream.");
		return FALSE;
	}
	return TRUE;
}

static boolean onregister(
	struct ap_event_teleport_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_character, AP_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_manager, AP_EVENT_MANAGER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_packet, AP_PACKET_MODULE_NAME);
	mod->character_attachment_offset = ap_character_attach_data(mod->ap_character, 
		AP_CHARACTER_MDI_CHAR, sizeof(struct ap_event_teleport_character_attachment), 
		mod, NULL, NULL);
	if (!ap_event_manager_register_event(mod->ap_event_manager,
			AP_EVENT_MANAGER_FUNCTION_TELEPORT, mod,
			event_ctor, event_dtor, 
			event_read, event_write, NULL)) {
		ERROR("Failed to register event.");
		return FALSE;
	}
	return TRUE;
}

static void onclose(struct ap_event_teleport_module * mod)
{
	size_t index = 0;
	void * object;
	while (ap_admin_iterate_name(&mod->point_admin, &index, &object)) {
		struct ap_event_teleport_point * point = 
			*(struct ap_event_teleport_point **)object;
		ap_module_destroy_module_data(mod, AP_EVENT_TELEPORT_MDI_TELEPORT_POINT, point);
	}
	ap_admin_clear_objects(&mod->point_admin);
	while (ap_admin_iterate_name(&mod->group_admin, &index, &object)) {
		struct ap_event_teleport_group * group = 
			*(struct ap_event_teleport_group **)object;
		ap_module_destroy_module_data(mod, AP_EVENT_TELEPORT_MDI_TELEPORT_GROUP, group);
	}
	ap_admin_clear_objects(&mod->group_admin);
}

static void onshutdown(struct ap_event_teleport_module * mod)
{
	ap_admin_destroy(&mod->point_admin);
	ap_admin_destroy(&mod->group_admin);
}

struct ap_event_teleport_module * ap_event_teleport_create_module()
{
	struct ap_event_teleport_module * mod = ap_module_instance_new(AP_EVENT_TELEPORT_MODULE_NAME, 
		sizeof(*mod), onregister, NULL, onclose, onshutdown);
	au_packet_init(&mod->packet, sizeof(uint8_t),
		AU_PACKET_TYPE_UINT8, 1, /* Packet Type */ 
		AU_PACKET_TYPE_PACKET, 1, /* Event Base */
		AU_PACKET_TYPE_INT32, 1, /* Character CID */
		AU_PACKET_TYPE_CHAR, AP_EVENT_TELEPORT_MAX_POINT_NAME_LENGTH + 1, /* Target TeleportPoint Name */
		AU_PACKET_TYPE_POS, 1, /* Target Custom Position */
		AU_PACKET_TYPE_MEMORY_BLOCK, 1, /* disable target point id (array) */
		AU_PACKET_TYPE_END);
	ap_module_set_module_data(mod, AP_EVENT_TELEPORT_MDI_TELEPORT_POINT, 
		sizeof(struct ap_event_teleport_point), NULL, NULL);
	ap_module_set_module_data(mod, AP_EVENT_TELEPORT_MDI_TELEPORT_GROUP, 
		sizeof(struct ap_event_teleport_group), NULL, NULL);
	ap_admin_init(&mod->point_admin, sizeof(struct ap_event_teleport_point *), 128);
	ap_admin_init(&mod->group_admin, sizeof(struct ap_event_teleport_group *), 128);
	ap_module_stream_add_callback(mod, AP_EVENT_TELEPORT_MDI_TELEPORT_POINT,
		AP_EVENT_TELEPORT_MODULE_NAME, mod, cbpointread, cbpointwrite);
	ap_module_stream_add_callback(mod, AP_EVENT_TELEPORT_MDI_TELEPORT_GROUP,
		AP_EVENT_TELEPORT_MODULE_NAME, mod, cbgroupread, cbgroupwrite);
	return mod;
}

void ap_event_teleport_add_callback(
	struct ap_event_teleport_module * mod,
	enum ap_event_teleport_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	ap_module_add_callback(mod, id, callback_module, callback);
}

boolean ap_event_teleport_read_teleport_points(
	struct ap_event_teleport_module * mod,
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
		const char * pointname = ap_module_stream_read_section_name(stream, i);
		struct ap_event_teleport_point ** pointpointer = 
			ap_admin_add_object_by_name(&mod->point_admin, pointname);
		struct ap_event_teleport_point * point;
		if (!pointpointer) {
			ERROR("Failed to add teleport point (%s).", pointname);
			ap_module_stream_destroy(stream);
			return FALSE;
		}
		point = ap_module_create_module_data(mod, AP_EVENT_TELEPORT_MDI_TELEPORT_POINT);
		*pointpointer = point;
		strlcpy(point->point_name, pointname, sizeof(point->point_name));
		if (!ap_module_stream_enum_read(mod, stream, 
				AP_EVENT_TELEPORT_MDI_TELEPORT_POINT, point)) {
			ERROR("Failed to read teleport point (%s).", pointname);
			ap_module_stream_destroy(stream);
			ap_admin_remove_object_by_name(&mod->point_admin, pointname);
			return FALSE;
		}
	}
	ap_module_stream_destroy(stream);
	return TRUE;
}

static int sortpoints(
	const struct ap_event_teleport_point * const * p1,
	const struct ap_event_teleport_point * const * p2)
{
	return strcmp((*p1)->point_name, (*p2)->point_name);
}

boolean ap_event_teleport_write_teleport_points(
	struct ap_event_teleport_module * mod,
	const char * file_path, 
	boolean encrypt)
{
	struct ap_module_stream * stream;
	size_t index = 0;
	void * object;
	struct ap_event_teleport_point * point;
	struct ap_event_teleport_point ** list = vec_new_reserved(sizeof(*list), 
		ap_admin_get_object_count(&mod->point_admin));
	while (ap_admin_iterate_name(&mod->point_admin, &index, &object)) {
		point = *(struct ap_event_teleport_point **)object;
		vec_push_back((void **)&list, &point);
	}
	qsort(list, vec_count(list), sizeof(*list), sortpoints);
	stream = ap_module_stream_create();
	ap_module_stream_set_mode(stream, AU_INI_MGR_MODE_NAME_OVERWRITE);
	ap_module_stream_set_type(stream, AU_INI_MGR_TYPE_NORMAL);
	for (index = 0; index < vec_count(list); index++) {
		point = list[index];
		if (!ap_module_stream_set_section_name(stream, point->point_name)) {
			ERROR("Failed to set section name.");
			ap_module_stream_destroy(stream);
			vec_free(list);
			return FALSE;
		}
		if (!ap_module_stream_enum_write(mod, stream, 
				AP_EVENT_TELEPORT_MDI_TELEPORT_POINT, point)) {
			ERROR("Failed to write teleport point (%s).", point->point_name);
			ap_module_stream_destroy(stream);
			vec_free(list);
			return FALSE;
		}
	}
	vec_free(list);
	if (!ap_module_stream_write(stream, file_path, 0, encrypt)) {
		ERROR("Failed to write teleport points..");
		ap_module_stream_destroy(stream);
		return FALSE;
	}
	ap_module_stream_destroy(stream);
	return TRUE;
}

boolean ap_event_teleport_write_teleport_groups(
	struct ap_event_teleport_module * mod,
	const char * file_path, 
	boolean encrypt)
{
	struct ap_module_stream * stream;
	size_t index = 0;
	void * object;
	struct ap_event_teleport_group * group;
	stream = ap_module_stream_create();
	ap_module_stream_set_mode(stream, AU_INI_MGR_MODE_NAME_OVERWRITE);
	ap_module_stream_set_type(stream, AU_INI_MGR_TYPE_NORMAL);
	while (ap_admin_iterate_name(&mod->group_admin, &index, &object)) {
		group = *(struct ap_event_teleport_group **)object;
		if (!ap_module_stream_set_section_name(stream, group->name)) {
			ERROR("Failed to set section name.");
			ap_module_stream_destroy(stream);
			return FALSE;
		}
		if (!ap_module_stream_enum_write(mod, stream, 
				AP_EVENT_TELEPORT_MDI_TELEPORT_GROUP, group)) {
			ERROR("Failed to write teleport group (%s).", group->name);
			ap_module_stream_destroy(stream);
			return FALSE;
		}
	}
	if (!ap_module_stream_write(stream, file_path, 0, encrypt)) {
		ERROR("Failed to write teleport groups..");
		ap_module_stream_destroy(stream);
		return FALSE;
	}
	ap_module_stream_destroy(stream);
	return TRUE;
}

struct ap_event_teleport_point * ap_event_teleport_add_point(
	struct ap_event_teleport_module * mod,
	const char * point_name,
	void * target_base)
{
	struct ap_event_teleport_point ** pointpointer = 
		ap_admin_add_object_by_name(&mod->point_admin, point_name);
	struct ap_event_teleport_point * point;
	if (!pointpointer)
		return NULL;
	point = ap_module_create_module_data(mod, AP_EVENT_TELEPORT_MDI_TELEPORT_POINT);
	*pointpointer = point;
	strlcpy(point->point_name, point_name, sizeof(point->point_name));
	point->target_type = AP_EVENT_TELEPORT_TARGET_BASE;
	point->target.base.type = ap_base_get_type(target_base);
	point->target.base.id = ap_base_get_id(target_base);
	point->target_base = target_base;
	point->radius_min = 400.0f;
	point->radius_max = 600.0f;
	point->region_type = AP_EVENT_TELEPORT_REGION_NORMAL;
	point->special_type = AP_EVENT_TELEPORT_SPECIAL_NORMAL;
	return point;
}

void ap_event_teleport_remove_point(
	struct ap_event_teleport_module * mod,
	struct ap_event_teleport_point * point)
{
	uint32_t i;
	for (i = 0; i < point->group_count; i++) {
		struct ap_event_teleport_group * group = point->groups[i];
		uint32_t j;
		for (j = 0; j < group->destination_point_count; j++) {
			if (group->destination_points[j] == point) {
				group->destination_points[j] = 
					group->destination_points[--group->destination_point_count];
				break;
			}
		}
	}
	for (i = 0; i < point->target_group_count; i++) {
		struct ap_event_teleport_group * group = point->target_groups[i];
		uint32_t j;
		for (j = 0; j < group->source_point_count; j++) {
			if (group->source_points[j] == point) {
				group->source_points[j] = 
					group->source_points[--group->source_point_count];
				break;
			}
		}
	}
	if (point->target_base) {
		struct ap_event_manager_attachment * attachment = 
			ap_event_manager_get_attachment(mod->ap_event_manager, point->target_base);
		for (i = 0; i < attachment->event_count; i++) {
			if (attachment->events[i].function == AP_EVENT_MANAGER_FUNCTION_TELEPORT) {
				ap_event_manager_remove_function(mod->ap_event_manager, attachment, i);
				break;
			}
		}
	}
	ap_admin_remove_object_by_name(&mod->point_admin, point->point_name);
	ap_module_destroy_module_data(mod, AP_EVENT_TELEPORT_MDI_TELEPORT_POINT, point);
}

struct ap_event_teleport_point * ap_event_teleport_find_point_by_name(
	struct ap_event_teleport_module * mod,
	const char * point_name)
{
	struct ap_event_teleport_point ** point = 
		ap_admin_get_object_by_name(&mod->point_admin, point_name);
	return point ? *point : NULL;
}

struct ap_event_teleport_point * ap_event_teleport_iterate(
	struct ap_event_teleport_module * mod,
	size_t * index)
{
	struct ap_event_teleport_point ** point;
	if (ap_admin_iterate_name(&mod->point_admin, index, (void **)&point))
		return *point;
	else
		return NULL;
}

struct ap_event_teleport_group * ap_event_teleport_add_group(
	struct ap_event_teleport_module * mod,
	const char * group_name)
{
	struct ap_event_teleport_group ** grouppointer = 
		ap_admin_add_object_by_name(&mod->group_admin, group_name);
	struct ap_event_teleport_group * group;
	if (!grouppointer)
		return NULL;
	group = ap_module_create_module_data(mod, AP_EVENT_TELEPORT_MDI_TELEPORT_GROUP);
	*grouppointer = group;
	strlcpy(group->name, group_name, sizeof(group->name));
	return group;
}

struct ap_event_teleport_group * ap_event_teleport_iterate_groups(
	struct ap_event_teleport_module * mod,
	size_t * index)
{
	struct ap_event_teleport_group ** group;
	if (ap_admin_iterate_name(&mod->group_admin, index, (void **)&group))
		return *group;
	else
		return NULL;
}

struct ap_event_teleport_character_attachment * ap_event_teleport_get_character_attachment(
	struct ap_event_teleport_module * mod,
	struct ap_character * character)
{
	return ap_module_get_attached_data(character, mod->character_attachment_offset);
}

static void prepareteleport(
	struct ap_event_teleport_module * mod,
	struct ap_character * character,
	struct ap_event_teleport_character_attachment * attachment,
	const struct au_pos * position)
{
	struct ap_event_teleport_cb_teleport_prepare cb = { 0 };
	cb.character = character;
	cb.attachment = attachment;
	cb.target_position = position;
	ap_module_enum_callback(mod, AP_EVENT_TELEPORT_CB_TELEPORT_PREPARE, &cb);
}

void ap_event_teleport_start(
	struct ap_event_teleport_module * mod,
	struct ap_character * character,
	const struct au_pos * position)
{
	struct ap_event_teleport_cb_teleport_start cb = { 0 };
	struct ap_event_teleport_character_attachment * attachment = 
		ap_event_teleport_get_character_attachment(mod, character);
	prepareteleport(mod, character, attachment, position);
	attachment->teleporting = TRUE;
	ap_character_stop_movement(mod->ap_character, character);
	ap_character_stop_action(mod->ap_character, character);
	ap_character_move(mod->ap_character, character, position);
	cb.character = character;
	cb.attachment = attachment;
	cb.target_position = position;
	ap_module_enum_callback(mod, AP_EVENT_TELEPORT_CB_TELEPORT_START, &cb);
}

boolean ap_event_teleport_on_receive(
	struct ap_event_teleport_module * mod,
	const void * data,
	uint16_t length,
	void * user_data)
{
	struct ap_event_teleport_cb_receive cb = { 0 };
	const void * basepacket = NULL;
	struct ap_event_manager_base_packet event = { 0 };
	const char * targetpointname = NULL;
	if (!au_packet_get_field(&mod->packet, TRUE, data, length,
			&cb.type, /* Packet Type */ 
			&basepacket, /* Event Base */
			NULL, /* Character CID */
			&targetpointname, /* Target TeleportPoint Name */
			NULL, /* Target Custom Position */
			NULL, NULL)) { /* disable target point id (array) */
		return FALSE;
	}
	if (basepacket) {
		if (!ap_event_manager_get_base_packet(mod->ap_event_manager, basepacket, &event))
			return FALSE;
		cb.event = &event;
	}
	AU_PACKET_GET_STRING(cb.target_point_name, targetpointname);
	cb.user_data = user_data;
	return ap_module_enum_callback(mod, AP_EVENT_TELEPORT_CB_RECEIVE, &cb);
}

void ap_event_teleport_make_packet(
	struct ap_event_teleport_module * mod,
	enum ap_event_teleport_packet_type type,
	const struct ap_event_manager_event * event,
	uint32_t * character_id)
{
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	void * base = NULL;
	uint16_t length = 0;
	if (event) {
		base = ap_packet_get_temp_buffer(mod->ap_packet);
		ap_event_manager_make_base_packet(mod->ap_event_manager, event, base);
	}
	au_packet_make_packet(&mod->packet, buffer, 
		TRUE, &length, AP_EVENT_TELEPORT_PACKET_TYPE,
		&type, /* Packet Type */ 
		base, /* Event Base */
		character_id, /* Character CID */
		NULL, /* Target TeleportPoint Name */
		NULL, /* Target Custom Position */
		NULL); /* disable target point id (array) */
	ap_packet_set_length(mod->ap_packet, length);
	ap_packet_reset_temp_buffers(mod->ap_packet);
}

void ap_event_teleport_make_start_packet(
	struct ap_event_teleport_module * mod,
	uint32_t character_id,
	const struct au_pos * target_position)
{
	uint8_t type = AP_EVENT_TELEPORT_PACKET_TELEPORT_START;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	uint16_t length = 0;
	au_packet_make_packet(&mod->packet, buffer, 
		TRUE, &length, AP_EVENT_TELEPORT_PACKET_TYPE,
		&type, /* Packet Type */ 
		NULL, /* Event Base */
		&character_id, /* Character CID */
		NULL, /* Target TeleportPoint Name */
		target_position, /* Target Custom Position */
		NULL); /* disable target point id (array) */
	ap_packet_set_length(mod->ap_packet, length);
}

void ap_event_teleport_make_loading_packet(
	struct ap_event_teleport_module * mod,
	uint32_t character_id)
{
	uint8_t type = AP_EVENT_TELEPORT_PACKET_TELEPORT_LOADING;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	uint16_t length = 0;
	au_packet_make_packet(&mod->packet, buffer, 
		TRUE, &length, AP_EVENT_TELEPORT_PACKET_TYPE,
		&type, /* Packet Type */ 
		NULL, /* Event Base */
		&character_id, /* Character CID */
		NULL, /* Target TeleportPoint Name */
		NULL, /* Target Custom Position */
		NULL); /* disable target point id (array) */
	ap_packet_set_length(mod->ap_packet, length);
}
