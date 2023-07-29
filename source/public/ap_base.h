#ifndef _AP_BASE_H_
#define _AP_BASE_H_

#include "public/ap_module_instance.h"

#define AP_BASE_MODULE_NAME "ApBase"

BEGIN_DECLS

struct ap_base_module;

enum ap_base_type {
	AP_BASE_TYPE_NONE = 0,
	AP_BASE_TYPE_OBJECT,
	AP_BASE_TYPE_OBJECT_TEMPLATE,
	AP_BASE_TYPE_CHARACTER,
	AP_BASE_TYPE_CHARACTER_TEMPLATE,
	AP_BASE_TYPE_ITEM,
	AP_BASE_TYPE_ITEM_TEMPLATE,
	AP_BASE_TYPE_SKILL,
	AP_BASE_TYPE_SKILL_TEMPLATE,
	AP_BASE_TYPE_SHRINE,
	AP_BASE_TYPE_SHRINE_TEMPLATE,
	AP_BASE_TYPE_PARTY,
	AP_BASE_TYPE_SERVER,
	AP_BASE_TYPE_SERVER_TEMPLATE,
	AP_BASE_TYPE_QUEST_TEMPLATE,
	AP_BASE_TYPE_UI,
	AP_BASE_TYPE_UI_CONTROL,
	AP_BASE_TYPE_COUNT
};

struct ap_base {
	enum ap_base_type type;
	uint32_t id;
};

struct ap_base_module * ap_base_create_module();

boolean ap_base_parse_packet(
	struct ap_base_module * mod,
	const void * buffer, 
	enum ap_base_type * type,
	uint32_t * id);

void ap_base_make_packet(
	struct ap_base_module * mod,
	void * buffer,
	enum ap_base_type type, 
	uint32_t id);

inline enum ap_base_type ap_base_get_type(const void * data)
{
	return ((struct ap_base *)data)->type;
}

inline uint32_t ap_base_get_id(const void * data)
{
	return ((struct ap_base *)data)->id;
}

END_DECLS

#endif /* _AP_BASE_H_ */
