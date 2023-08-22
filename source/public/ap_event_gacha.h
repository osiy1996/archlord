#ifndef _AP_EVENT_GACHA_H_
#define _AP_EVENT_GACHA_H_

#include "public/ap_character.h"

#define AP_EVENT_GACHA_MODULE_NAME "AgpmEventGacha"

#define AP_EVENT_GACHA_TYPE_STRING_SIZE 64
#define AP_EVENT_GACHA_MAX_RANK 4

BEGIN_DECLS

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
	uint32_t gacha_type;
};

struct ap_event_gacha_module * ap_event_gacha_create_module();

struct ap_event_gacha_event * ap_event_gacha_get_event(
	struct ap_event_manager_event * e);

boolean ap_event_gacha_read_types(
	struct ap_event_gacha_module * mod,
	const char * file_path);

END_DECLS

#endif /* _AP_EVENT_GACHA_H_ */
