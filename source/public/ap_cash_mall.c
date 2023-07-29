#include "public/ap_cash_mall.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"
#include "core/vector.h"

#include "public/ap_admin.h"
#include "public/ap_character.h"
#include "public/ap_packet.h"

#include "utility/au_packet.h"
#include "utility/au_table.h"

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>

#define MAX_ENCODED_STRING_SIZE 8192

enum data_column_id {
	DCID_CATEGORY_NAME,
	DCID_CASHITEM_TID,
	DCID_CASHITEM_STACK,
	DCID_CASHITEM_PRICE,
	DCID_CASHITEM_TYPE,
	DCID_CASHITEM_DESCRIPTION,
};

struct ap_cash_mall_module {
	struct ap_module_instance instance;
	struct ap_character_module * ap_character;
	struct ap_packet_module * ap_packet;
	struct au_packet packet;
	struct ap_cash_mall_tab tabs[AP_CASH_MALL_MAX_TAB_COUNT];
	uint32_t tab_count;
	char encoded_string[MAX_ENCODED_STRING_SIZE];
	uint16_t encoded_string_length;
};

static boolean onregister(
	struct ap_cash_mall_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_character, AP_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_packet, AP_PACKET_MODULE_NAME);
	return TRUE;
}

static void onshutdown(struct ap_cash_mall_module * mod)
{
	uint32_t i;
	for (i = 0; i < mod->tab_count; i++)
		vec_free(mod->tabs[i].items);
}

struct ap_cash_mall_module * ap_cash_mall_create_module()
{
	struct ap_cash_mall_module * mod = ap_module_instance_new(AP_CASH_MALL_MODULE_NAME,
		sizeof(*mod), onregister, NULL, NULL, onshutdown);
	au_packet_init(&mod->packet, sizeof(uint16_t),
		AU_PACKET_TYPE_INT8, 1, /* operation */
		AU_PACKET_TYPE_INT8, 1, /* result */
		AU_PACKET_TYPE_UINT8, 1, /* mall list version */
		AU_PACKET_TYPE_INT32, 1, /* cid */
		AU_PACKET_TYPE_INT32, 1, /* tab */
		AU_PACKET_TYPE_INT32, 1, /* product id */
		AU_PACKET_TYPE_MEMORY_BLOCK, 1, /* mall tab list */
		AU_PACKET_TYPE_MEMORY_BLOCK, 1, /* item list */
		AU_PACKET_TYPE_INT32, 1, /* type */
		AU_PACKET_TYPE_END);
	return mod;
}

boolean ap_cash_mall_read_import_data(
	struct ap_cash_mall_module * mod, 
	const char * file_path, 
	boolean decrypt)
{
	struct au_table * table = au_table_open(file_path, decrypt);
	boolean r = TRUE;
	struct ap_cash_mall_tab * tab = NULL;
	uint32_t i;
	uint32_t itemid = 0;
	if (!table) {
		ERROR("Failed to open file (%s).", file_path);
		return FALSE;
	}
	r &= au_table_set_column(table, "Category_Name", DCID_CATEGORY_NAME);
	r &= au_table_set_column(table, "CashItem_TID", DCID_CASHITEM_TID);
	r &= au_table_set_column(table, "CashItem_Stack", DCID_CASHITEM_STACK);
	r &= au_table_set_column(table, "CashItem_Price", DCID_CASHITEM_PRICE);
	r &= au_table_set_column(table, "CashItem_Type", DCID_CASHITEM_TYPE);
	r &= au_table_set_column(table, "CashItem_Description", DCID_CASHITEM_DESCRIPTION);
	if (!r) {
		ERROR("Failed to setup columns.");
		au_table_destroy(table);
		return FALSE;
	}
	while (au_table_read_next_line(table)) {
		struct ap_cash_mall_item * item = NULL;
		while (au_table_read_next_column(table)) {
			enum data_column_id id = au_table_get_column(table);
			char * value = au_table_get_value(table);
			if (id == DCID_CATEGORY_NAME) {
				assert(mod->tab_count < AP_CASH_MALL_MAX_TAB_COUNT);
				tab = &mod->tabs[mod->tab_count];
				tab->id = mod->tab_count++;
				tab->items = vec_new_reserved(sizeof(*tab->items), 32);
				strlcat(tab->name, value, sizeof(tab->name));
				break;
			}
			assert(tab != NULL);
			if (id == DCID_CASHITEM_TID) {
				assert(item == NULL);
				item = vec_add_empty(&tab->items);
				item->id = ++itemid;
				item->tid = strtoul(value, NULL, 10);
				continue;
			}
			assert(item != NULL);
			switch (id) {
			case DCID_CASHITEM_STACK:
				item->stack_count = strtoul(value, NULL, 10);
				break;
			case DCID_CASHITEM_PRICE:
				item->price = strtoul(value, NULL, 10);
				break;
			case DCID_CASHITEM_TYPE:
				item->special_flag = strtoul(value, NULL, 10);
				break;
			case DCID_CASHITEM_DESCRIPTION:
				strlcpy(item->description, value, sizeof(item->description));
				break;
			default:
				ERROR("Invalid column id (%u).", id);
				au_table_destroy(table);
				return FALSE;
			}
		}
	}
	au_table_destroy(table);
	memset(mod->encoded_string, 0, sizeof(mod->encoded_string));
	for (i = 0; i < mod->tab_count; i++) {
		struct ap_cash_mall_tab * tab = &mod->tabs[i];
		uint32_t j;
		static char buffer[1024];
		snprintf(buffer, sizeof(buffer), "%s;", tab->name);
		strlcat(mod->encoded_string, buffer, sizeof(mod->encoded_string));
		for (j = 0; j < vec_count(mod->tabs[i].items); j++) {
			struct ap_cash_mall_item * item = &mod->tabs[i].items[j];
			snprintf(buffer, sizeof(buffer), "%u,", item->id);
			strlcat(mod->encoded_string, buffer, sizeof(mod->encoded_string));
			snprintf(buffer, sizeof(buffer),
				"%u,%u,%u,%u,%u,%s:", item->id, item->tid, item->stack_count, 
				item->price, item->special_flag, item->description);
			strlcat(tab->encoded_item_list, buffer, sizeof(tab->encoded_item_list));
		}
		snprintf(buffer, sizeof(buffer), ":");
		strlcat(mod->encoded_string, buffer, sizeof(mod->encoded_string));
		tab->encoded_item_list_length = (uint16_t)strlen(tab->encoded_item_list);
	}
	mod->encoded_string_length = (uint16_t)strlen(mod->encoded_string);
	return TRUE;
}

struct ap_cash_mall_tab * ap_cash_mall_get_tab(
	struct ap_cash_mall_module * mod,
	uint32_t tab_id)
{
	uint32_t i;
	for (i = 0; i < mod->tab_count; i++) {
		if (mod->tabs[i].id == tab_id)
			return &mod->tabs[i];
	}
	return NULL;
}

struct ap_cash_mall_tab * ap_cash_mall_get_tab_list(
	struct ap_cash_mall_module * mod,
	uint32_t * count)
{
	*count = mod->tab_count;
	return mod->tabs;
}

void ap_cash_mall_add_callback(
	struct ap_cash_mall_module * mod,
	enum ap_cash_mall_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	ap_module_add_callback(mod, id, callback_module, callback);
}

boolean ap_cash_mall_on_receive(
	struct ap_cash_mall_module * mod,
	const void * data,
	uint16_t length,
	void * user_data)
{
	struct ap_cash_mall_cb_receive cb = { 0 };
	if (!au_packet_get_field(&mod->packet, TRUE, data, length,
			&cb.type, /* operation */
			NULL, /* result */
			NULL, /* mall list version */
			NULL, /* cid */
			&cb.tab_id, /* tab */
			&cb.product_id, /* product id */
			NULL, NULL, /* mall tab list */
			NULL, NULL, /* item list */
			NULL)) { /* type */
		return FALSE;
	}
	cb.user_data = user_data;
	return ap_module_enum_callback(mod, AP_CASH_MALL_CB_RECEIVE, &cb);
}

void ap_cash_mall_make_mall_list_packet(struct ap_cash_mall_module * mod)
{
	uint8_t type = AP_CASH_MALL_PACKET_RESPONSE_MALL_LIST;
	uint8_t listversion = 1;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	uint16_t length = 0;
	au_packet_make_packet(&mod->packet, buffer, TRUE, &length,
		AP_CASH_MALL_PACKET_TYPE,
		&type, /* operation */
		NULL, /* result */
		&listversion, /* mall list version */
		NULL, /* cid */
		NULL, /* tab */
		NULL, /* product id */
		mod->encoded_string, &mod->encoded_string_length, /* mall tab list */
		NULL, /* item list */
		NULL); /* type */
	ap_packet_set_length(mod->ap_packet, length);
}

void ap_cash_mall_make_product_list_packet(
	struct ap_cash_mall_module * mod,
	const struct ap_cash_mall_tab * tab)
{
	uint8_t type = AP_CASH_MALL_PACKET_RESPONSE_PRODUCT_LIST;
	uint8_t listversion = 1;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	uint16_t length = 0;
	au_packet_make_packet(&mod->packet, buffer, TRUE, &length,
		AP_CASH_MALL_PACKET_TYPE,
		&type, /* operation */
		NULL, /* result */
		&listversion, /* mall list version */
		NULL, /* cid */
		&tab->id, /* tab */
		NULL, /* product id */
		NULL, /* mall tab list */
		tab->encoded_item_list, &tab->encoded_item_list_length, /* item list */
		NULL); /* type */
	ap_packet_set_length(mod->ap_packet, length);
}

void ap_cash_mall_make_buy_result_packet(
	struct ap_cash_mall_module * mod,
	enum ap_cash_mall_buy_result result)
{
	uint8_t type = AP_CASH_MALL_PACKET_RESPONSE_BUY_RESULT;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	uint16_t length = 0;
	au_packet_make_packet(&mod->packet, buffer, TRUE, &length,
		AP_CASH_MALL_PACKET_TYPE,
		&type, /* operation */
		&result, /* result */
		NULL, /* mall list version */
		NULL, /* cid */
		NULL, /* tab */
		NULL, /* product id */
		NULL, /* mall tab list */
		NULL, /* item list */
		NULL); /* type */
	ap_packet_set_length(mod->ap_packet, length);
}
