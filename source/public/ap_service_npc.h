#ifndef _AP_SERVICE_NPC_H_
#define _AP_SERVICE_NPC_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_item_convert.h"
#include "public/ap_module_instance.h"

#define AP_SERVICE_NPC_MODULE_NAME "AgpmServiceNPC"

BEGIN_DECLS

enum ap_service_npc_callback_id {
	AP_SERVICE_NPC_CB_,
};

struct ap_service_npc_level_up_reward {
	uint32_t level;
	enum au_race_type race;
	enum au_char_class_type cs;
	uint32_t item_tid;
	const struct ap_item_template * item_template;
	uint32_t stack_count;
	uint32_t expire_sec;
	uint32_t socket_count;
	uint32_t convert_tid[AP_ITEM_CONVERT_MAX_SOCKET];
};

struct ap_service_npc_module * ap_service_npc_create_module();

size_t ap_service_npc_attach_data(
	struct ap_service_npc_module * mod,
	enum ap_service_npc_module_data_index data_index,
	size_t data_size,
	ap_module_t callback_module,
	ap_module_default_t constructor,
	ap_module_default_t destructor);

boolean ap_service_npc_read_level_up_reward_table(
	struct ap_service_npc_module * mod,
	const char * file_path, 
	boolean decrypt);

struct ap_service_npc_level_up_reward * ap_service_npc_get_level_up_rewards(
	struct ap_service_npc_module * mod,
	uint32_t * count);

static int ap_service_npc_sort_level_up_rewards(
	const struct ap_service_npc_level_up_reward * reward1,
	const struct ap_service_npc_level_up_reward * reward2)
{
	return (reward1->level > reward2->level);
}

END_DECLS

#endif /* _AP_SERVICE_NPC_H_ */