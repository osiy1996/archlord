#ifndef _AP_EVENT_TELEPORT_H_
#define _AP_EVENT_TELEPORT_H_

#include "public/ap_base.h"
#include "public/ap_define.h"
#include "public/ap_event_manager.h"
#include "public/ap_module_instance.h"

#define AP_EVENT_TELEPORT_MODULE_NAME "AgpmEventTeleport"

#define	AP_EVENT_TELEPORT_MAX_POINT_ID 300
#define	AP_EVENT_TELEPORT_MAX_POINT_LIST 100
#define AP_EVENT_TELEPORT_MAX_POINT_OBJECT 30
#define	AP_EVENT_TELEPORT_MAX_POINT_DESCRIPTION_LENGTH 128
#define	AP_EVENT_TELEPORT_MAX_POINT_NAME_LENGTH 32

#define	AP_EVENT_TELEPORT_MAX_GROUP_COUNT 20
#define	AP_EVENT_TELEPORT_MAX_GROUP_DESCRIPTION_LENGTH 128
#define AP_EVENT_TELEPORT_MAX_GROUP_POINT 30
#define	AP_EVENT_TELEPORT_MAX_GROUP_NAME_LENGTH 32
#define	AP_EVENT_TELEPORT_MAX_GROUP_POINT_COUNT 16

#define	AP_EVENT_TELEPORT_MAX_USE_RANGE 1600

BEGIN_DECLS

struct ap_event_manager_event;

enum ap_event_teleport_target_type {
	AP_EVENT_TELEPORT_TARGET_POS = 1,
	AP_EVENT_TELEPORT_TARGET_BASE,
	AP_EVENT_TELEPORT_TARGET_NONE,
};

enum ap_event_teleport_region_type {
	AP_EVENT_TELEPORT_REGION_NORMAL = 0,
	AP_EVENT_TELEPORT_REGION_PVP,
};

enum ap_event_teleport_special_type {
	AP_EVENT_TELEPORT_SPECIAL_NORMAL = 0,
	AP_EVENT_TELEPORT_SPECIAL_RETURN_ONLY,
	AP_EVENT_TELEPORT_SPECIAL_SIEGEWAR,
	AP_EVENT_TELEPORT_SPECIAL_CASTLE_TO_DUNGEON,
	AP_EVENT_TELEPORT_SPECIAL_DUNGEON_TO_LANSPHERE,
	AP_EVENT_TELEPORT_SPECIAL_LANSPHERE,
};

enum ap_event_teleport_use_type {
	AP_EVENT_TELEPORT_USE_HUMAN = 0x01,
	AP_EVENT_TELEPORT_USE_ORC = 0x02,
	AP_EVENT_TELEPORT_USE_MOONELF = 0x04,
	AP_EVENT_TELEPORT_USE_DRAGONSCION = 0x08,
};

enum ap_event_teleport_packet_type {
	AP_EVENT_TELEPORT_PACKET_TELEPORT_POINT = 0,
	AP_EVENT_TELEPORT_PACKET_TELEPORT_START,
	AP_EVENT_TELEPORT_PACKET_TELEPORT_CANCELED,
	AP_EVENT_TELEPORT_PACKET_TELEPORT_CUSTOM_POS,
	AP_EVENT_TELEPORT_PACKET_REQUEST_TELEPORT,
	AP_EVENT_TELEPORT_PACKET_REQUEST_TELEPORT_GRANTED,
	AP_EVENT_TELEPORT_PACKET_REQUEST_TELEPORT_IGNOIRED,
	AP_EVENT_TELEPORT_PACKET_TELEPORT_RETURN,
	AP_EVENT_TELEPORT_PACKET_TELEPORT_LOADING,
};

enum ap_event_teleport_module_data_index {
	AP_EVENT_TELEPORT_MDI_TELEPORT_POINT,
	AP_EVENT_TELEPORT_MDI_TELEPORT_GROUP,
};

enum ap_event_teleport_callback_id {
	AP_EVENT_TELEPORT_CB_RECEIVE,
	AP_EVENT_TELEPORT_CB_TELEPORT_PREPARE,
	AP_EVENT_TELEPORT_CB_TELEPORT_START,
};

union ap_event_teleport_target {
	struct au_pos pos;
	struct ap_base base;
};

struct ap_event_teleport_group {
	char name[AP_EVENT_TELEPORT_MAX_GROUP_NAME_LENGTH + 1];
	char description[AP_EVENT_TELEPORT_MAX_GROUP_DESCRIPTION_LENGTH + 1];
	struct ap_event_teleport_point * source_points[AP_EVENT_TELEPORT_MAX_GROUP_POINT_COUNT];
	uint32_t source_point_count;
	struct ap_event_teleport_point * destination_points[AP_EVENT_TELEPORT_MAX_GROUP_POINT_COUNT];
	uint32_t destination_point_count;
};

struct ap_event_teleport_group_member {
	char name[AP_EVENT_TELEPORT_MAX_GROUP_NAME_LENGTH + 1];
};

struct ap_event_teleport_point {
	char point_name[AP_EVENT_TELEPORT_MAX_POINT_NAME_LENGTH + 1];
	enum ap_event_teleport_target_type target_type;
	union ap_event_teleport_target target;
	void * target_base;
	char description[AP_EVENT_TELEPORT_MAX_POINT_DESCRIPTION_LENGTH + 1];
	float radius_min;
	float radius_max;
	boolean attach_event;
	struct ap_event_teleport_group * groups[AP_EVENT_TELEPORT_MAX_GROUP_COUNT];
	uint32_t group_count;
	struct ap_event_teleport_group * target_groups[AP_EVENT_TELEPORT_MAX_GROUP_COUNT];
	uint32_t target_group_count;
	enum ap_event_teleport_region_type region_type;
	enum ap_event_teleport_special_type special_type;
	enum ap_event_teleport_use_type use_type;
};

struct ap_event_teleport_attach {
	uint32_t point_id;
	struct ap_event_teleport_point * point;
};

struct ap_event_teleport_event {
	char point_name[64];
	struct ap_event_teleport_point * point;
};

struct ap_event_teleport_character_attachment {
	boolean teleporting;
};

struct ap_event_teleport_cb_receive {
	enum ap_event_teleport_packet_type type;
	struct ap_event_manager_base_packet * event;
	char target_point_name[AP_EVENT_TELEPORT_MAX_POINT_NAME_LENGTH + 1];
	void * user_data;
};

struct ap_event_teleport_cb_teleport_prepare {
	struct ap_character * character;
	struct ap_event_teleport_character_attachment * attachment;
	const struct au_pos * target_position;
};

struct ap_event_teleport_cb_teleport_start {
	struct ap_character * character;
	struct ap_event_teleport_character_attachment * attachment;
	const struct au_pos * target_position;
};

struct ap_event_teleport_module * ap_event_teleport_create_module();

void ap_event_teleport_add_callback(
	struct ap_event_teleport_module * mod,
	enum ap_event_teleport_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

boolean ap_event_teleport_read_teleport_points(
	struct ap_event_teleport_module * mod,
	const char * file_path, 
	boolean decrypt);

boolean ap_event_teleport_write_teleport_points(
	struct ap_event_teleport_module * mod,
	const char * file_path, 
	boolean encrypt);

boolean ap_event_teleport_write_teleport_groups(
	struct ap_event_teleport_module * mod,
	const char * file_path, 
	boolean encrypt);

struct ap_event_teleport_point * ap_event_teleport_add_point(
	struct ap_event_teleport_module * mod,
	const char * point_name,
	void * target_base);

void ap_event_teleport_remove_point(
	struct ap_event_teleport_module * mod,
	struct ap_event_teleport_point * point);

struct ap_event_teleport_point * ap_event_teleport_find_point_by_name(
	struct ap_event_teleport_module * mod,
	const char * point_name);

struct ap_event_teleport_point * ap_event_teleport_iterate(
	struct ap_event_teleport_module * mod,
	size_t * index);

struct ap_event_teleport_group * ap_event_teleport_add_group(
	struct ap_event_teleport_module * mod,
	const char * group_name);

struct ap_event_teleport_group * ap_event_teleport_iterate_groups(
	struct ap_event_teleport_module * mod,
	size_t * index);

struct ap_event_teleport_character_attachment * ap_event_teleport_get_character_attachment(
	struct ap_event_teleport_module * mod,
	struct ap_character * character);

void ap_event_teleport_start(
	struct ap_event_teleport_module * mod,
	struct ap_character * character,
	const struct au_pos * position);

boolean ap_event_teleport_on_receive(
	struct ap_event_teleport_module * mod,
	const void * data,
	uint16_t length,
	void * user_data);

void ap_event_teleport_make_packet(
	struct ap_event_teleport_module * mod,
	enum ap_event_teleport_packet_type type,
	const struct ap_event_manager_event * event,
	uint32_t * character_id);

void ap_event_teleport_make_start_packet(
	struct ap_event_teleport_module * mod,
	uint32_t character_id,
	const struct au_pos * target_position);

void ap_event_teleport_make_loading_packet(
	struct ap_event_teleport_module * mod,
	uint32_t character_id);

static inline struct ap_event_teleport_event * ap_event_teleport_get_event(
	struct ap_event_manager_event * e)
{
	return (struct ap_event_teleport_event *)e->data;
}

END_DECLS

#endif /* _AP_EVENT_TELEPORT_H_ */
