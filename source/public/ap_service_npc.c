#include "public/ap_service_npc.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"
#include "core/vector.h"

#include "public/ap_character.h"
#include "public/ap_item.h"

#include "utility/au_table.h"

#include "vendor/pcg/pcg_basic.h"

#include <assert.h>
#include <stdlib.h>
#include <time.h>

#define MAXLEVELUPREWARDCOUNT 512

struct ap_service_npc_module {
	struct ap_module_instance instance;
	struct ap_character_module * ap_character;
	struct ap_item_module * ap_item;
	struct ap_service_npc_level_up_reward level_up_rewards[MAXLEVELUPREWARDCOUNT];
	uint32_t level_up_reward_count;
};

static boolean onregister(
	struct ap_service_npc_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_character, AP_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_item, AP_ITEM_MODULE_NAME);
	return TRUE;
}

static void onshutdown(struct ap_service_npc_module * mod)
{
}

struct ap_service_npc_module * ap_service_npc_create_module()
{
	struct ap_service_npc_module * mod = ap_module_instance_new(AP_SERVICE_NPC_MODULE_NAME, 
		sizeof(*mod), onregister, NULL, NULL, onshutdown);
	return mod;
}

size_t ap_service_npc_attach_data(
	struct ap_service_npc_module * mod,
	enum ap_service_npc_module_data_index data_index,
	size_t data_size,
	ap_module_t callback_module,
	ap_module_default_t constructor,
	ap_module_default_t destructor)
{
	return ap_module_attach_data(mod, data_index, data_size, 
		callback_module, constructor, destructor);
}

enum level_up_reward_table_column_id {
	LEVEL_UP_TABLE_COLUMN_LEVEL,
	LEVEL_UP_TABLE_COLUMN_RACE,
	LEVEL_UP_TABLE_COLUMN_CLASS,
	LEVEL_UP_TABLE_COLUMN_ITEM_TID,
	LEVEL_UP_TABLE_COLUMN_ITEM_STACK,
	LEVEL_UP_TABLE_COLUMN_ITEM_EXPIRE_SEC,
	LEVEL_UP_TABLE_COLUMN_SOCKET_COUNT,
	LEVEL_UP_TABLE_COLUMN_SOCKET_1,
	LEVEL_UP_TABLE_COLUMN_SOCKET_2,
	LEVEL_UP_TABLE_COLUMN_SOCKET_3,
	LEVEL_UP_TABLE_COLUMN_SOCKET_4,
	LEVEL_UP_TABLE_COLUMN_SOCKET_5,
	LEVEL_UP_TABLE_COLUMN_SOCKET_6,
	LEVEL_UP_TABLE_COLUMN_SOCKET_7,
	LEVEL_UP_TABLE_COLUMN_SOCKET_8,
};

boolean ap_service_npc_read_level_up_reward_table(
	struct ap_service_npc_module * mod,
	const char * file_path, 
	boolean decrypt)
{
	struct au_table * table = au_table_open(file_path, decrypt);
	boolean r = TRUE;
	if (!table) {
		ERROR("Failed to open file (%s).", file_path);
		return FALSE;
	}
	r &= au_table_set_column(table, "Level", LEVEL_UP_TABLE_COLUMN_LEVEL);
	r &= au_table_set_column(table, "Race", LEVEL_UP_TABLE_COLUMN_RACE);
	r &= au_table_set_column(table, "Class", LEVEL_UP_TABLE_COLUMN_CLASS);
	r &= au_table_set_column(table, "Item_TID", LEVEL_UP_TABLE_COLUMN_ITEM_TID);
	r &= au_table_set_column(table, "Item_Stack", LEVEL_UP_TABLE_COLUMN_ITEM_STACK);
	r &= au_table_set_column(table, "Item_Expire (sec)", LEVEL_UP_TABLE_COLUMN_ITEM_EXPIRE_SEC);
	r &= au_table_set_column(table, "Socket_Count", LEVEL_UP_TABLE_COLUMN_SOCKET_COUNT);
	r &= au_table_set_column(table, "Socket (1/8)", LEVEL_UP_TABLE_COLUMN_SOCKET_1);
	r &= au_table_set_column(table, "Socket (2/8)", LEVEL_UP_TABLE_COLUMN_SOCKET_2);
	r &= au_table_set_column(table, "Socket (3/8)", LEVEL_UP_TABLE_COLUMN_SOCKET_3);
	r &= au_table_set_column(table, "Socket (4/8)", LEVEL_UP_TABLE_COLUMN_SOCKET_4);
	r &= au_table_set_column(table, "Socket (5/8)", LEVEL_UP_TABLE_COLUMN_SOCKET_5);
	r &= au_table_set_column(table, "Socket (6/8)", LEVEL_UP_TABLE_COLUMN_SOCKET_6);
	r &= au_table_set_column(table, "Socket (7/8)", LEVEL_UP_TABLE_COLUMN_SOCKET_7);
	r &= au_table_set_column(table, "Socket (8/8)", LEVEL_UP_TABLE_COLUMN_SOCKET_8);
	if (!r) {
		ERROR("Failed to setup columns.");
		au_table_destroy(table);
		return FALSE;
	}
	if (!au_table_read_next_line(table)) {
		ERROR("Invalid file format.");
		au_table_destroy(table);
		return FALSE;
	}
	while (au_table_read_next_line(table)) {
		struct ap_service_npc_level_up_reward * reward = 
			&mod->level_up_rewards[mod->level_up_reward_count++];
		assert(mod->level_up_reward_count <= MAXLEVELUPREWARDCOUNT);
		while (au_table_read_next_column(table)) {
			enum level_up_reward_table_column_id id = au_table_get_column(table);
			char * value = au_table_get_value(table);
			switch (id) {
			case LEVEL_UP_TABLE_COLUMN_LEVEL:
				reward->level = strtoul(value, NULL, 10);
				break;
			case LEVEL_UP_TABLE_COLUMN_RACE:
				reward->race = strtol(value, NULL, 10);
				break;
			case LEVEL_UP_TABLE_COLUMN_CLASS:
				reward->cs = strtol(value, NULL, 10);
				break;
			case LEVEL_UP_TABLE_COLUMN_ITEM_TID:
				reward->item_tid = strtoul(value, NULL, 10);
				reward->item_template = ap_item_get_template(mod->ap_item, 
					reward->item_tid);
				if (!reward->item_template) {
					ERROR("Invalid item template id (%u).", reward->item_tid);
					au_table_destroy(table);
					return FALSE;
				}
				break;
			case LEVEL_UP_TABLE_COLUMN_ITEM_STACK:
				reward->stack_count = strtoul(value, NULL, 10);
				break;
			case LEVEL_UP_TABLE_COLUMN_ITEM_EXPIRE_SEC:
				reward->expire_sec = strtoul(value, NULL, 10);
				break;
			case LEVEL_UP_TABLE_COLUMN_SOCKET_COUNT:
				reward->socket_count = strtoul(value, NULL, 10);
				break;
			case LEVEL_UP_TABLE_COLUMN_SOCKET_1:
				assert(AP_ITEM_CONVERT_MAX_SOCKET > 0);
				reward->convert_tid[0] = strtoul(value, NULL, 10);
				break;
			case LEVEL_UP_TABLE_COLUMN_SOCKET_2:
				assert(AP_ITEM_CONVERT_MAX_SOCKET > 1);
				reward->convert_tid[1] = strtoul(value, NULL, 10);
				break;
			case LEVEL_UP_TABLE_COLUMN_SOCKET_3:
				assert(AP_ITEM_CONVERT_MAX_SOCKET > 2);
				reward->convert_tid[2] = strtoul(value, NULL, 10);
				break;
			case LEVEL_UP_TABLE_COLUMN_SOCKET_4:
				assert(AP_ITEM_CONVERT_MAX_SOCKET > 3);
				reward->convert_tid[3] = strtoul(value, NULL, 10);
				break;
			case LEVEL_UP_TABLE_COLUMN_SOCKET_5:
				assert(AP_ITEM_CONVERT_MAX_SOCKET > 4);
				reward->convert_tid[4] = strtoul(value, NULL, 10);
				break;
			case LEVEL_UP_TABLE_COLUMN_SOCKET_6:
				assert(AP_ITEM_CONVERT_MAX_SOCKET > 5);
				reward->convert_tid[5] = strtoul(value, NULL, 10);
				break;
			case LEVEL_UP_TABLE_COLUMN_SOCKET_7:
				assert(AP_ITEM_CONVERT_MAX_SOCKET > 6);
				reward->convert_tid[6] = strtoul(value, NULL, 10);
				break;
			case LEVEL_UP_TABLE_COLUMN_SOCKET_8:
				assert(AP_ITEM_CONVERT_MAX_SOCKET > 7);
				reward->convert_tid[7] = strtoul(value, NULL, 10);
				break;
			default:
				ERROR("Invalid column id (%u).", id);
				au_table_destroy(table);
				return FALSE;
			}
		}
		assert(reward->level != 0);
		assert(reward->item_tid != 0);
		assert(reward->stack_count != 0);
	}
	au_table_destroy(table);
	qsort(&mod->level_up_rewards, mod->level_up_reward_count, 
		sizeof(*mod->level_up_rewards), ap_service_npc_sort_level_up_rewards);
	return TRUE;
}

struct ap_service_npc_level_up_reward * ap_service_npc_get_level_up_rewards(
	struct ap_service_npc_module * mod,
	uint32_t * count)
{
	*count = mod->level_up_reward_count;
	return mod->level_up_rewards;
}
