#include "public/ap_refinery.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"
#include "core/vector.h"

#include "public/ap_admin.h"
#include "public/ap_item.h"
#include "public/ap_item_convert.h"
#include "public/ap_packet.h"
#include "public/ap_tick.h"

#include "utility/au_packet.h"
#include "utility/au_table.h"

#include <assert.h>
#include <stdlib.h>

#define OPTION_POOL_MAX_COUNT 64

struct option_pool {
	struct ap_item_option_template * options[OPTION_POOL_MAX_COUNT];
	uint32_t count;
	uint32_t total_probability;
};

struct ap_refinery_module {
	struct ap_module_instance instance;
	struct ap_item_module * ap_item;
	struct ap_item_convert_module * ap_item_convert;
	struct ap_packet_module * ap_packet;
	struct ap_tick_module * ap_tick;
	struct au_packet packet;
	struct ap_admin recipe_admin;
	struct option_pool option_drop_pool[AP_ITEM_OPTION_MAX_TYPE][AP_ITEM_PART_COUNT];
};

static boolean onregister(
	struct ap_refinery_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_item, AP_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_item_convert, AP_ITEM_CONVERT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_packet, AP_PACKET_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_tick, AP_TICK_MODULE_NAME);
	return TRUE;
}

static void onclose(struct ap_refinery_module * mod)
{
}

static void onshutdown(struct ap_refinery_module * mod)
{
	ap_admin_destroy(&mod->recipe_admin);
}

struct ap_refinery_module * ap_refinery_create_module()
{
	struct ap_refinery_module * mod = ap_module_instance_new(AP_REFINERY_MODULE_NAME,
		sizeof(*mod), onregister, NULL, onclose, onshutdown);
	au_packet_init(&mod->packet, sizeof(uint16_t),
		AU_PACKET_TYPE_INT8, 1, /* Packet Type */
		AU_PACKET_TYPE_INT32, 1, /* CID */
		AU_PACKET_TYPE_INT32, 1, /* Item TID */
		AU_PACKET_TYPE_INT32, 10, /* Source Item IDs */
		AU_PACKET_TYPE_INT32, 1, /* Result */
		AU_PACKET_TYPE_INT32, 1, /* Result Item TID */
		AU_PACKET_TYPE_END);
	ap_admin_init(&mod->recipe_admin, sizeof(struct ap_refinery_recipe), 128);
	return mod;
}

void ap_refinery_add_callback(
	struct ap_refinery_module * mod,
	enum ap_refinery_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	ap_module_add_callback(mod, id, callback_module, callback);
}

size_t ap_refinery_attach_data(
	struct ap_refinery_module * mod,
	enum ap_refinery_module_data_index data_index,
	size_t data_size,
	ap_module_t callback_module,
	ap_module_default_t constructor,
	ap_module_default_t destructor)
{
	return ap_module_attach_data(mod, data_index, data_size, 
		callback_module, constructor, destructor);
}

enum recipe_table_column_id {
	TABLE_COLUMN_RECIPE_ID,
	TABLE_COLUMN_MAKEITEM_TOTAL,
	TABLE_COLUMN_MAKEITEM_TID,
	TABLE_COLUMN_MAKEITEM_NAME,
	TABLE_COLUMN_MAKEITEM_COUNT,
	TABLE_COLUMN_MAKEITEM_ISFAIL,
	TABLE_COLUMN_BONUSITEM_TID,
	TABLE_COLUMN_BONUSITEM_NAME,
	TABLE_COLUMN_ITEM_OPTION_MIN,
	TABLE_COLUMN_ITEM_OPTION_MAX,
	TABLE_COLUMN_SOCKET_INCHANT_MIN,
	TABLE_COLUMN_SOCKET_INCHANT_MAX,
	TABLE_COLUMN_COMBINE_USE_PRICE,
	TABLE_COLUMN_TIMER_MSEC,
	TABLE_COLUMN_RESOURCE_TOTALCOUNT,
	TABLE_COLUMN_RESOURCE_TID,
	TABLE_COLUMN_RESOURCE_NAME,
	TABLE_COLUMN_RESOURCE_COUNT,
};

boolean ap_refinery_read_recipe_table(
	struct ap_refinery_module * mod,
	const char * file_path,
	boolean decrypt)
{
	struct au_table * table = au_table_open(file_path, decrypt);
	boolean r = TRUE;
	if (!table) {
		ERROR("Failed to open file (%s).", file_path);
		return FALSE;
	}
	r &= au_table_set_column(table, "Recipe ID", TABLE_COLUMN_RECIPE_ID);
	r &= au_table_set_column(table, "MakeItem Total", TABLE_COLUMN_MAKEITEM_TOTAL);
	r &= au_table_set_column(table, "MakeItem TID", TABLE_COLUMN_MAKEITEM_TID);
	r &= au_table_set_column(table, "MakeItem Name", TABLE_COLUMN_MAKEITEM_NAME);
	r &= au_table_set_column(table, "MakeItem Count", TABLE_COLUMN_MAKEITEM_COUNT);
	r &= au_table_set_column(table, "MakeItem IsFail", TABLE_COLUMN_MAKEITEM_ISFAIL);
	r &= au_table_set_column(table, "BonusItem TID", TABLE_COLUMN_BONUSITEM_TID);
	r &= au_table_set_column(table, "BonusItem Name", TABLE_COLUMN_BONUSITEM_NAME);
	r &= au_table_set_column(table, "Item Option Min", TABLE_COLUMN_ITEM_OPTION_MIN);
	r &= au_table_set_column(table, "Item Option Max", TABLE_COLUMN_ITEM_OPTION_MAX);
	r &= au_table_set_column(table, "Socket Inchant Min", TABLE_COLUMN_SOCKET_INCHANT_MIN);
	r &= au_table_set_column(table, "Socket Inchant Max", TABLE_COLUMN_SOCKET_INCHANT_MAX);
	r &= au_table_set_column(table, "Combine Use Price", TABLE_COLUMN_COMBINE_USE_PRICE);
	r &= au_table_set_column(table, "timer(msec)", TABLE_COLUMN_TIMER_MSEC);
	r &= au_table_set_column(table, "Resource TotalCount", TABLE_COLUMN_RESOURCE_TOTALCOUNT);
	r &= au_table_set_column(table, "Resource TID", TABLE_COLUMN_RESOURCE_TID);
	r &= au_table_set_column(table, "Resource Name", TABLE_COLUMN_RESOURCE_NAME);
	r &= au_table_set_column(table, "Resource Count", TABLE_COLUMN_RESOURCE_COUNT);
	if (!r) {
		ERROR("Failed to setup columns.");
		au_table_destroy(table);
		return FALSE;
	}
	while (au_table_read_next_line(table)) {
		struct ap_refinery_recipe recipe = { 0 };
		struct ap_refinery_product product = { 0 };
		static char resourcestr[1024];
		struct ap_refinery_recipe * addedrecipe;
		while (au_table_read_next_column(table)) {
			enum recipe_table_column_id id = au_table_get_column(table);
			char * value = au_table_get_value(table);
			switch (id) {
			case TABLE_COLUMN_RECIPE_ID:
				recipe.id = strtoul(value, NULL, 10);
				break;
			case TABLE_COLUMN_MAKEITEM_TOTAL:
				break;
			case TABLE_COLUMN_MAKEITEM_TID:
				product.item_tid = strtoul(value, NULL, 10);
				break;
			case TABLE_COLUMN_MAKEITEM_NAME:
				break;
			case TABLE_COLUMN_MAKEITEM_COUNT:
				product.stack_count = strtoul(value, NULL, 10);
				break;
			case TABLE_COLUMN_MAKEITEM_ISFAIL:
				product.is_failure = strtol(value, NULL, 10) != 0;
				break;
			case TABLE_COLUMN_BONUSITEM_TID:
				break;
			case TABLE_COLUMN_BONUSITEM_NAME:
				break;
			case TABLE_COLUMN_ITEM_OPTION_MIN:
				product.min_option_count = strtoul(value, NULL, 10);
				break;
			case TABLE_COLUMN_ITEM_OPTION_MAX:
				product.max_option_count = strtoul(value, NULL, 10);
				break;
			case TABLE_COLUMN_SOCKET_INCHANT_MIN:
				product.min_socket_count = strtoul(value, NULL, 10);
				break;
			case TABLE_COLUMN_SOCKET_INCHANT_MAX:
				product.max_socket_count = strtoul(value, NULL, 10);
				break;
			case TABLE_COLUMN_COMBINE_USE_PRICE:
				recipe.required_gold = strtoul(value, NULL, 10);
				break;
			case TABLE_COLUMN_TIMER_MSEC:
				break;
			case TABLE_COLUMN_RESOURCE_TOTALCOUNT:
				break;
			case TABLE_COLUMN_RESOURCE_TID:
				assert(recipe.resource_count < AP_REFINERY_MAX_RESOURCE_COUNT);
				recipe.resources[recipe.resource_count++].item_tid = 
					strtoul(value, NULL, 10);
				break;
			case TABLE_COLUMN_RESOURCE_NAME:
				break;
			case TABLE_COLUMN_RESOURCE_COUNT:
				assert(recipe.resource_count != 0);
				recipe.resources[recipe.resource_count - 1].stack_count = 
					strtoul(value, NULL, 10);
				break;
			default:
				ERROR("Invalid column id (%u).", id);
				au_table_destroy(table);
				return FALSE;
			}
		}
		qsort(recipe.resources, recipe.resource_count, sizeof(recipe.resources[0]),
			ap_refinery_sort_resources);
		ap_refinery_make_resource_string(resourcestr, sizeof(resourcestr),
			recipe.resources, recipe.resource_count);
		addedrecipe = ap_admin_get_object_by_name(&mod->recipe_admin, resourcestr);
		if (!addedrecipe) {
			addedrecipe = ap_admin_add_object_by_name(&mod->recipe_admin, resourcestr);
			if (!addedrecipe) {
				ERROR("Failed to add recipe (%u).", recipe.id);
				au_table_destroy(table);
				return FALSE;
			}
			memcpy(addedrecipe, &recipe, sizeof(recipe));
			memcpy(&addedrecipe->products[addedrecipe->product_count++],
				&product, sizeof(product));
		}
		else {
			assert(addedrecipe->product_count < AP_REFINERY_MAX_PRODUCT_COUNT);
			memcpy(&addedrecipe->products[addedrecipe->product_count++],
				&product, sizeof(product));
		}
	}
	au_table_destroy(table);
	return TRUE;
}

void ap_refinery_make_resource_string(
	char * dst, 
	size_t max_count, 
	const struct ap_refinery_resource * resources, 
	uint32_t resource_count)
{
	uint32_t i;
	dst[0] = '\0';
	for (i = 0; i < resource_count; i++) {
		char resource[32];
		snprintf(resource, sizeof(resource), "%u,%u:", resources[i].item_tid,
			resources[i].stack_count);
		strlcat(dst, resource, max_count);
	}
}

struct ap_refinery_recipe * ap_refinery_find_recipe(
	struct ap_refinery_module * mod,
	const char * resource_string)
{
	return ap_admin_get_object_by_name(&mod->recipe_admin, resource_string);
}

boolean ap_refinery_on_receive(
	struct ap_refinery_module * mod,
	const void * data,
	uint16_t length,
	void * user_data)
{
	struct ap_refinery_cb_receive cb = { 0 };
	if (!au_packet_get_field(&mod->packet, TRUE, data, length,
			&cb.type, /* Packet Type */
			NULL, /* CID */
			NULL, /* Item TID */
			cb.resource_item_id, /* Source Item IDs */
			NULL, /* Result */
			NULL)) { /* Result Item TID */
		return FALSE;
	}
	cb.user_data = user_data;
	return ap_module_enum_callback(mod, AP_REFINERY_CB_RECEIVE, &cb);
}

void ap_refinery_make_result_packet(
	struct ap_refinery_module * mod, 
	uint32_t character_id,
	enum ap_refinery_result_type result_type,
	uint32_t result_item_tid)
{
	uint8_t type = AP_REFINERY_PACKET_RESULT;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	uint16_t length = 0;
	au_packet_make_packet(&mod->packet, 
		buffer, TRUE, &length,
		AP_REFINERY_PACKET_TYPE, 
		&type, /* Packet Type */
		&character_id, /* CID */
		NULL, /* Item TID */
		NULL, /* Source Item IDs */
		&result_type, /* Result */
		&result_item_tid); /* Result Item TID */
	ap_packet_set_length(mod->ap_packet, length);
}
