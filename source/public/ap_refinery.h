#ifndef _AP_REFINERY_H_
#define _AP_REFINERY_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_define.h"
#include "public/ap_module_instance.h"

#define AP_REFINERY_MODULE_NAME "AgpmRefinery"

#define AP_REFINERY_MAX_PRODUCT_COUNT 32
#define AP_REFINERY_MAX_RESOURCE_COUNT 10

BEGIN_DECLS

enum ap_refinery_packet_type {
	AP_REFINERY_PACKET_REFINE = 0,
	AP_REFINERY_PACKET_RESULT,
	AP_REFINERY_PACKET_REFINE_ITEM,
	AP_REFINERY_PACKET_REFINE_ITEM_RESULT,
};

enum ap_refinery_result_type {
	AP_REFINERY_RESULT_NONE = -1,
	AP_REFINERY_RESULT_SUCCESS = 0,
	AP_REFINERY_RESULT_FAIL,
	AP_REFINERY_RESULT_FAIL_DIFFERENT_ITEM,
	AP_REFINERY_RESULT_FAIL_INSUFFICIENT_ITEM,
	AP_REFINERY_RESULT_FAIL_INSUFFICIENT_MONEY,
	AP_REFINERY_RESULT_FAIL_FULL_INVENTORY,
	AP_REFINERY_RESULT_FAIL_SAME_STONE_ATTRIBUTE,
	AP_REFINERY_RESULT_FAIL_INSUFFICIENT_OPTION_ITEM,
};

enum ap_refinery_callback_id {
	AP_REFINERY_CB_RECEIVE,
};

enum ap_refinery_mode_data_index {
};

struct ap_refinery_product {
	uint32_t item_tid;
	uint32_t stack_count;
	boolean is_failure;
	uint32_t min_option_count;
	uint32_t max_option_count;
	uint32_t min_socket_count;
	uint32_t max_socket_count;
};

struct ap_refinery_resource {
	uint32_t item_tid;
	uint32_t stack_count;
};

struct ap_refinery_recipe {
	uint32_t id;
	uint32_t required_gold;
	struct ap_refinery_product products[AP_REFINERY_MAX_PRODUCT_COUNT];
	uint32_t product_count;
	struct ap_refinery_resource resources[AP_REFINERY_MAX_RESOURCE_COUNT];
	uint32_t resource_count;
};

struct ap_refinery_cb_receive {
	enum ap_refinery_packet_type type;
	uint32_t resource_item_id[AP_REFINERY_MAX_RESOURCE_COUNT];
	void * user_data;
};

struct ap_refinery_module * ap_refinery_create_module();

void ap_refinery_add_callback(
	struct ap_refinery_module * mod,
	enum ap_refinery_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

size_t ap_refinery_attach_data(
	struct ap_refinery_module * mod,
	enum ap_refinery_module_data_index data_index,
	size_t data_size,
	ap_module_t callback_module,
	ap_module_default_t constructor,
	ap_module_default_t destructor);

boolean ap_refinery_read_recipe_table(
	struct ap_refinery_module * mod,
	const char * file_path,
	boolean decrypt);

void ap_refinery_make_resource_string(
	char * dst, 
	size_t max_count, 
	const struct ap_refinery_resource * resources, 
	uint32_t resource_count);

struct ap_refinery_recipe * ap_refinery_find_recipe(
	struct ap_refinery_module * mod,
	const char * resource_string);

boolean ap_refinery_on_receive(
	struct ap_refinery_module * mod,
	const void * data,
	uint16_t length,
	void * user_data);

void ap_refinery_make_result_packet(
	struct ap_refinery_module * mod, 
	uint32_t character_id,
	enum ap_refinery_result_type result_type,
	uint32_t result_item_tid);

static int ap_refinery_sort_resources(
	const struct ap_refinery_resource * resource1,
	const struct ap_refinery_resource * resource2)
{
	return (resource1->item_tid > resource2->item_tid);
}


END_DECLS

#endif /* _AP_REFINERY_H_ */