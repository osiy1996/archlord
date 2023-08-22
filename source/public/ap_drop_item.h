#ifndef _AP_DROP_ITEM_H_
#define _AP_DROP_ITEM_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_character.h"
#include "public/ap_define.h"
#include "public/ap_item.h"
#include "public/ap_module_instance.h"

#define AP_DROP_ITEM_MODULE_NAME "AgpmDropItem"
#define AP_DROP_ITEM_MAX_OPTION_COUNT_IN_POOL 64
#define AP_DROP_ITEM_MAX_DROP_GROUP_NAME_SIZE 32
#define AP_DROP_ITEM_MAX_DROP_GROUP_LEVEL_COUNT ((AP_CHARACTER_MAX_LEVEL / 10) + 1)
#define AP_DROP_ITEM_MAX_DROP_GROUP_ID 29
#define AP_DROP_ITEM_MAX_DROP_RANK 10
#define AP_DROP_ITEM_MAX_DROP_GROUP_ITEM_COUNT 150

BEGIN_DECLS

struct ap_drop_item_module;

enum ap_drop_item_callback_id {
	AP_DROP_ITEM_CB_,
};

struct ap_drop_item_option_type_pool {
	struct ap_item_option_template * temp[AP_DROP_ITEM_MAX_OPTION_COUNT_IN_POOL];
	uint32_t option_count;
	uint32_t total_probability;
	uint32_t option_type;
};

struct ap_drop_item_drop_rank_rate {
	uint32_t rate[AP_DROP_ITEM_MAX_DROP_RANK + 1];
};

struct ap_drop_item {
	uint32_t tid;
	struct ap_item_template * temp;
	uint32_t drop_rate;
	uint32_t min_stack_count;
	uint32_t max_stack_count;
};

struct ap_drop_item_drop_group_rate {
	uint32_t id;
	char name[AP_DROP_ITEM_MAX_DROP_GROUP_NAME_SIZE];
	boolean is_affected_by_drop_med;
	uint32_t rate;
	int level_bonus[AP_DROP_ITEM_MAX_DROP_GROUP_LEVEL_COUNT];
};

struct ap_drop_item_drop_group {
	uint32_t id;
	uint32_t monster_tid;
	struct ap_drop_item * items[AP_DROP_ITEM_MAX_DROP_RANK + 1];
	uint32_t rank_total_rate[AP_DROP_ITEM_MAX_DROP_RANK + 1];
};

struct ap_drop_item_character_template_attachment {
	uint32_t drop_rank;
	float drop_mediation;
	struct ap_drop_item_drop_group drop_groups[AP_DROP_ITEM_MAX_DROP_GROUP_ID + 1];
	uint32_t drop_count;
	struct ap_drop_item * drop_items;
};

struct ap_drop_item_template_attachment {
	uint32_t drop_rank;
	uint32_t drop_rate;
	uint32_t group_id;
	uint32_t suitable_level_min;
	uint32_t suitable_level_max;
	struct ap_drop_item_option_type_pool * type_pools;
	uint32_t type_pool_count;
	boolean is_droppable_unique_accessory;
};

struct ap_drop_item_module * ap_drop_item_create_module();

size_t ap_drop_item_attach_data(
	struct ap_drop_item_module * mod,
	enum ap_drop_item_module_data_index data_index,
	size_t data_size,
	ap_module_t callback_module,
	ap_module_default_t constructor,
	ap_module_default_t destructor);

struct ap_drop_item_template_attachment * ap_drop_item_get_item_template_attachment(
	struct ap_drop_item_module * mod,
	struct ap_item_template * temp);

struct ap_drop_item_character_template_attachment * ap_drop_item_get_character_template_attachment(
	struct ap_drop_item_module * mod,
	struct ap_character_template * temp);

boolean ap_drop_item_read_option_num_drop_rate(
	struct ap_drop_item_module * mod,
	const char * file_path, 
	boolean decrypt);

boolean ap_drop_item_read_socket_num_drop_rate(
	struct ap_drop_item_module * mod,
	const char * file_path, 
	boolean decrypt);

boolean ap_drop_item_read_drop_rank_rate(
	struct ap_drop_item_module * mod,
	const char * file_path, 
	boolean decrypt);

boolean ap_drop_item_read_drop_group_rate(
	struct ap_drop_item_module * mod,
	const char * file_path, 
	boolean decrypt);

boolean ap_drop_item_read_drop_table(
	struct ap_drop_item_module * mod,
	const char * file_path, 
	boolean decrypt);

uint32_t ap_drop_item_generate_option_count(
	struct ap_drop_item_module * mod,
	uint32_t min_option,
	uint32_t max_option);

uint32_t ap_drop_item_generate_socket_count(
	struct ap_drop_item_module * mod,
	uint32_t min_socket,
	uint32_t max_socket);

void ap_drop_item_generate_options(
	struct ap_drop_item_module * mod, 
	struct ap_item * item);

void ap_drop_item_generate_options_for_refined(
	struct ap_drop_item_module * mod, 
	struct ap_item * item,
	uint32_t option_count);

void ap_drop_item_generate_options_for_gacha(
	struct ap_drop_item_module * mod, 
	struct ap_item * item,
	uint32_t option_count);

float ap_drop_item_get_drop_rank_rate(
	struct ap_drop_item_module * mod, 
	uint32_t monster_drop_rank,
	uint32_t item_drop_rank);

uint32_t * ap_drop_item_get_drop_rank_rates(
	struct ap_drop_item_module * mod, 
	uint32_t monster_drop_rank);

boolean ap_drop_item_is_drop_group_affected_by_drop_meditation(
	struct ap_drop_item_module * mod, 
	uint32_t group_id);

uint32_t ap_drop_item_get_drop_group_rate(
	struct ap_drop_item_module * mod, 
	uint32_t group_id);

uint32_t ap_drop_item_get_drop_group_bonus(
	struct ap_drop_item_module * mod, 
	uint32_t group_id,
	uint32_t character_level);

END_DECLS

#endif /* _AP_DROP_ITEM_H_ */