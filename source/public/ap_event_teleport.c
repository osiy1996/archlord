#include "public/ap_event_teleport.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"

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
	size_t character_attachment_offset;
};

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
	if (teleportevent->point && teleportevent->point->target_base)
		teleportevent->point->target_base = NULL;
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
			if (!ap_module_stream_get_str(stream, 
					e->point_name, sizeof(e->point_name))) {
				ERROR("Failed to read event teleport point name.");
				return FALSE;
			}
			e->point = ap_admin_get_object_by_name(&mod->point_admin, e->point_name);
			if (!e->point) {
				assert(event->source != NULL);
				WARN("Failed to retrieve event teleport point: type (%d), id (%u).", 
					ap_base_get_type(event->source), ap_base_get_id(event->source));
			}
			else {
				uint64_t id = makeid(ap_base_get_type(event->source),
					ap_base_get_id(event->source));
				if (!ap_admin_add_id(&mod->point_admin, e->point_name, id)) {
					ERROR("Failed to bind event teleport point id.");
					return FALSE;
				}
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

static void onshutdown(struct ap_event_teleport_module * mod)
{
	ap_admin_destroy(&mod->point_admin);
}

struct ap_event_teleport_module * ap_event_teleport_create_module()
{
	struct ap_event_teleport_module * mod = ap_module_instance_new(AP_EVENT_TELEPORT_MODULE_NAME, 
		sizeof(*mod), onregister, NULL, NULL, onshutdown);
	au_packet_init(&mod->packet, sizeof(uint8_t),
		AU_PACKET_TYPE_UINT8, 1, /* Packet Type */ 
		AU_PACKET_TYPE_PACKET, 1, /* Event Base */
		AU_PACKET_TYPE_INT32, 1, /* Character CID */
		AU_PACKET_TYPE_CHAR, AP_EVENT_TELEPORT_MAX_POINT_NAME_LENGTH + 1, /* Target TeleportPoint Name */
		AU_PACKET_TYPE_POS, 1, /* Target Custom Position */
		AU_PACKET_TYPE_MEMORY_BLOCK, 1, /* disable target point id (array) */
		AU_PACKET_TYPE_END);
	ap_admin_init(&mod->point_admin, sizeof(struct ap_event_teleport_point), 128);
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
	struct au_ini_mgr_ctx * ini = au_ini_mgr_create();
	uint32_t sectioncount;
	uint32_t i;
	au_ini_mgr_set_mode(ini, AU_INI_MGR_MODE_NAME_OVERWRITE);
	au_ini_mgr_set_path(ini, file_path);
	if (!au_ini_mgr_read_file(ini, 0, decrypt)) {
		ERROR("Failed to read file (%s).", file_path);
		au_ini_mgr_destroy(ini);
		return FALSE;
	}
	sectioncount = au_ini_mgr_get_section_count(ini);
	for (i = 0; i < sectioncount; i++) {
		const char * pointname = au_ini_mgr_get_section_name(ini, i);
		struct ap_event_teleport_point * point = 
			ap_admin_add_object_by_name(&mod->point_admin, pointname);
		uint32_t keycount = au_ini_mgr_get_key_count(ini, i);
		uint32_t j;
		if (!point) {
			ERROR("Failed to add teleport point (%s).", pointname);
			au_ini_mgr_destroy(ini);
			return FALSE;
		}
		for (j = 0; j < keycount; j++) {
			const char * key = au_ini_mgr_get_key_name(ini, i, j);
			const char * value = au_ini_mgr_get_value(ini, i, j);
			if (strcmp(key, STREAM_DESCRIPTION) == 0) {
				strlcpy(point->description, value, sizeof(point->description));
			}
			else if (strcmp(key, STREAM_RADIUS_MIN) == 0) {
				point->radius_min = strtof(value, NULL);
			}
			else if (strcmp(key, STREAM_RADIUS_MAX) == 0) {
				point->radius_max = strtof(value, NULL);
			}
			else if (strcmp(key, STREAM_POINT_TYPE) == 0) {
				point->target_type = strtol(value, NULL, 10);
			}
			else if (strcmp(key, STREAM_POINT_POS) == 0) {
				if (point->target_type == AP_EVENT_TELEPORT_TARGET_POS) {
					if (sscanf(value, "%f:%f:%f", &point->target.pos.x,
							&point->target.pos.y, &point->target.pos.z) != 3) {
						ERROR("Failed to read point position (%s).", pointname);
						au_ini_mgr_destroy(ini);
						return FALSE;
					}
				}
			}
			else if (strcmp(key, STREAM_POINT_BASE) == 0) {
				if (point->target_type == AP_EVENT_TELEPORT_TARGET_BASE) {
					if (sscanf(value, "%d:%d", &point->target.base.type,
							&point->target.base.id) != 2) {
						ERROR("Failed to read point base (%s).", pointname);
						au_ini_mgr_destroy(ini);
						return FALSE;
					}
					assert(point->target.base.type == AP_BASE_TYPE_OBJECT);
					assert(point->target.base.id != 0);
				}
			}
			else if (strcmp(key, STREAM_POINT_EVENT) == 0) {
				point->attach_event = strtol(value, NULL, 10) != 0;
			}
			else if (strcmp(key, STREAM_USE_TYPE) == 0) {
				point->use_type = strtol(value, NULL, 10);
			}
			else if (strcmp(key, STREAM_POINT_GROUP_NAME) == 0) {
				if (point->group_count >= COUNT_OF(point->groups)) {
					ERROR("Exceeded group count limit (%s).", pointname);
					au_ini_mgr_destroy(ini);
					return FALSE;
				}
				strlcpy(point->groups[point->group_count++].name, value, 
					sizeof(point->groups[0]));
			}
			else if (strcmp(key, STREAM_POINT_TARGET_NAME) == 0) {
				if (point->target_group_count >= COUNT_OF(point->target_groups)) {
					ERROR("Exceeded target group count limit (%s).", pointname);
					au_ini_mgr_destroy(ini);
					return FALSE;
				}
				strlcpy(point->target_groups[point->target_group_count++].name, value, 
					sizeof(point->target_groups[0]));
			}
			else if (strcmp(key, STREAM_POINT_REGION_TYPE) == 0) {
				point->region_type = strtol(value, NULL, 10);
			}
			else if (strcmp(key, STREAM_POINT_SPECIAL_TYPE) == 0) {
				point->special_type = strtol(value, NULL, 10);
			}
		}
	}
	au_ini_mgr_destroy(ini);
	return TRUE;
}

struct ap_event_teleport_point * ap_event_teleport_find_point_by_id(
	struct ap_event_teleport_module * mod,
	enum ap_base_type type,
	uint32_t id)
{
	return ap_admin_get_object_by_id(&mod->point_admin, makeid(type, id));
}

struct ap_event_teleport_point * ap_event_teleport_find_point_by_name(
	struct ap_event_teleport_module * mod,
	const char * point_name)
{
	return ap_admin_get_object_by_name(&mod->point_admin, point_name);
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
