#ifndef _AP_EVENT_GACHA_H_
#define _AP_EVENT_GACHA_H_

#include "public/ap_character.h"

#define AP_EVENT_GACHA_MODULE_NAME "AgpmEventGacha"

#define AP_EVENT_GACHA_TYPE_STRING_SIZE 64
#define AP_EVENT_GACHA_MAX_TYPE 5
#define AP_EVENT_GACHA_MAX_RANK 4

#define AP_EVENT_GACHA_MAX_USE_RANGE 600.0f

BEGIN_DECLS

extern size_t AP_EVENT_GACHA_ITEM_TEMPLATE_ATTACHMENT_OFFSET;

enum ap_event_gacha_packet_type {
	AP_EVENT_GACHA_PACKET_REQUEST,
	AP_EVENT_GACHA_PACKET_REQUESTGRANTED,
	AP_EVENT_GACHA_PACKET_GACHA,
	AP_EVENT_GACHA_PACKET_RESULT,
};

enum ap_event_gacha_callback_id {
	AP_EVENT_GACHA_CB_RECEIVE,
};

struct ap_event_gacha_drop {
	uint32_t require_item_tid;
	uint32_t require_stack_count;
	uint32_t require_gold;
	uint32_t require_charisma;
	uint32_t rate[AP_EVENT_GACHA_MAX_RANK];
};

struct ap_event_gacha_type {
	uint32_t id;
	char name[AP_EVENT_GACHA_TYPE_STRING_SIZE];
	char comment[AP_EVENT_GACHA_TYPE_STRING_SIZE];
	boolean check_race;
	boolean check_class;
	boolean check_level;
	uint32_t level_limit;
	struct ap_event_gacha_drop drop_table[AP_CHARACTER_MAX_LEVEL];
};

struct ap_event_gacha_event {
	struct ap_event_gacha_type * type;
};

struct ap_event_gacha_item_template_attachment {
	uint32_t gacha_level_min;
	uint32_t gacha_level_max;
};

struct ap_event_gacha_cb_receive {
	enum ap_event_gacha_packet_type type;
	struct ap_event_manager_base_packet * event;
	void * user_data;
};

struct ap_event_gacha_module * ap_event_gacha_create_module();

void ap_event_gacha_add_callback(
	struct ap_event_gacha_module * mod,
	enum ap_event_gacha_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

struct ap_event_gacha_event * ap_event_gacha_get_event(
	struct ap_event_manager_event * e);

boolean ap_event_gacha_read_types(
	struct ap_event_gacha_module * mod,
	const char * file_path);

boolean ap_event_gacha_on_receive(
	struct ap_event_gacha_module * mod,
	const void * data,
	uint16_t length,
	void * user_data);

void ap_event_gacha_make_grant_packet(
	struct ap_event_gacha_module * mod,
	struct ap_event_manager_event * event,
	uint32_t character_id,
	uint32_t character_level,
	const uint32_t * item_list,
	uint32_t item_count);

static inline struct ap_event_gacha_item_template_attachment * ap_event_gacha_get_item_template_attachment(
	struct ap_item_template * temp)
{
	return (struct ap_event_gacha_item_template_attachment *)ap_module_get_attached_data(temp, 
		AP_EVENT_GACHA_ITEM_TEMPLATE_ATTACHMENT_OFFSET);
}

END_DECLS

#endif /* _AP_EVENT_GACHA_H_ */
