#include "public/ap_event_npc_trade.h"

#include "core/file_system.h"
#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"
#include "core/vector.h"

#include "public/ap_admin.h"
#include "public/ap_base.h"
#include "public/ap_character.h"
#include "public/ap_packet.h"

#include "utility/au_packet.h"

#include <assert.h>
#include <stdlib.h>

#define STREAM_START "NPCTradeStart"
#define STREAM_END "NPCTradeEnd"
#define STREAM_TEMPLATE "Template"

struct ap_event_npc_trade_module {
	struct ap_module_instance instance;
	struct ap_event_manager_module * ap_event_manager;
	struct ap_item_module * ap_item;
	struct ap_packet_module * ap_packet;
	struct au_packet packet;
	struct au_packet packet_detail_info;
	struct ap_admin trade_template_admin;
};

static boolean eventctor(struct ap_event_npc_trade_module * mod, void * data)
{
	struct ap_event_manager_event * e = data;
	struct ap_event_npc_trade_data * trade;
	trade = alloc(sizeof(struct ap_event_npc_trade_data));
	memset(trade, 0, sizeof(struct ap_event_npc_trade_data));
	trade->item_grid = ap_grid_new(AP_ITEM_NPCTRADEBOX_LAYER, 
		AP_ITEM_NPCTRADEBOX_ROW, AP_ITEM_NPCTRADEBOX_COLUMN, 
		AP_GRID_ITEM_TYPE_ITEM);
	e->data = trade;
	return TRUE;
}

static void cleartradegrid(
	struct ap_event_npc_trade_module * mod, 
	struct ap_event_npc_trade_data * data)
{
	uint32_t i;
	for (i = 0; i < data->item_grid->grid_count; i++) {
		struct ap_item * item = ap_grid_get_object_by_index(data->item_grid, i);
		if (item) {
			ap_grid_set_empty(data->item_grid, 
				item->grid_pos[AP_ITEM_GRID_POS_TAB],
				item->grid_pos[AP_ITEM_GRID_POS_ROW],
				item->grid_pos[AP_ITEM_GRID_POS_COLUMN]);
			ap_item_free(mod->ap_item, item);
		}
	}
	assert(data->item_grid->item_count == 0);
}

static boolean eventdtor(struct ap_event_npc_trade_module * mod, void * data)
{
	struct ap_event_manager_event * e = data;
	struct ap_event_npc_trade_data * trade = e->data;
	cleartradegrid(mod, trade);
	dealloc(trade);
	e->data = NULL;
	return TRUE;
}

static boolean eventread(
	struct ap_event_npc_trade_module * mod, 
	struct ap_event_manager_event * event,
	struct ap_module_stream * stream)
{
	if (!ap_module_stream_read_next_value(stream))
		return TRUE;
	if (strcmp(ap_module_stream_get_value_name(stream), STREAM_START) != 0)
		return TRUE;
	while (ap_module_stream_read_next_value(stream)) {
		const char * value_name = 
			ap_module_stream_get_value_name(stream);
		if (strcmp(value_name, STREAM_TEMPLATE) == 0) {
			uint32_t tid;
			if (!ap_module_stream_get_i32(stream, &tid)) {
				ERROR("Failed to read event npc trade template id.");
				return FALSE;
			}
			ap_event_npc_trade_set_template(mod, event, tid);
		}
		else if (strcmp(value_name, STREAM_END) == 0) {
			break;
		}
		else {
			assert(0);
		}
	}
	return TRUE;
}

static boolean eventwrite(
	struct ap_event_npc_trade_module * mod, 
	struct ap_event_manager_event * event,
	struct ap_module_stream * stream)
{
	struct ap_event_npc_trade_data * e = 
		ap_event_npc_trade_get_data(event);
	if (!ap_module_stream_write_i32(stream, STREAM_START, 0) ||
		!ap_module_stream_write_i32(stream, STREAM_TEMPLATE, 
			e->npc_trade_template_id) ||
		!ap_module_stream_write_i32(stream, STREAM_END, 0)) {
		ERROR("Failed to write event npc trade stream.");
		return FALSE;
	}
	return TRUE;
}

static boolean onregister(
	struct ap_event_npc_trade_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_manager, AP_EVENT_MANAGER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_item, AP_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_packet, AP_PACKET_MODULE_NAME);
	if (!ap_event_manager_register_event(mod->ap_event_manager,
			AP_EVENT_MANAGER_FUNCTION_NPCTRADE, mod,
			eventctor, eventdtor, 
			eventread, eventwrite, NULL)) {
		ERROR("Failed to register event.");
		return FALSE;
	}
	return TRUE;
}

static void onclose(struct ap_event_npc_trade_module * mod)
{
}

static void onshutdown(struct ap_event_npc_trade_module * mod)
{
	size_t index = 0;
	struct ap_event_npc_trade_template * temp = NULL;
	while (ap_admin_iterate_id(&mod->trade_template_admin, &index, &temp))
		vec_free(temp->items);
	ap_admin_destroy(&mod->trade_template_admin);
}

struct ap_event_npc_trade_module * ap_event_npc_trade_create_module()
{
	struct ap_event_npc_trade_module * mod = ap_module_instance_new(AP_EVENT_NPC_TRADE_MODULE_NAME,
		sizeof(*mod), onregister, NULL, onclose, onshutdown);
	au_packet_init(&mod->packet, sizeof(uint16_t),
		AU_PACKET_TYPE_PACKET, 1, /* Event Base Packet */
		AU_PACKET_TYPE_UINT8, 1, /* Packet Type */ 
		AU_PACKET_TYPE_INT32, 1, /* RequesterCID */
		AU_PACKET_TYPE_INT32, 1, /* lIID */
		AU_PACKET_TYPE_PACKET, 1, /* DetailInfo */
		AU_PACKET_TYPE_INT32, 1, /* TimeStamp */
		AU_PACKET_TYPE_INT32, 1, /* Result */
		AU_PACKET_TYPE_INT8, 1, /* Layer */
		AU_PACKET_TYPE_INT8, 1, /* Row */
		AU_PACKET_TYPE_INT8, 1, /* Column */
		AU_PACKET_TYPE_END);
	au_packet_init(&mod->packet_detail_info, sizeof(uint16_t),
		AU_PACKET_TYPE_INT32, 1, /* Item TID */
		AU_PACKET_TYPE_INT32, 1, /* Item Price */
		AU_PACKET_TYPE_INT32, 1, /* Item Count */
		AU_PACKET_TYPE_INT16, 1, /* Item status */
		AU_PACKET_TYPE_INT16, 1, /* Layer */
		AU_PACKET_TYPE_INT16, 1, /* Row */
		AU_PACKET_TYPE_INT16, 1, /* Column */
		AU_PACKET_TYPE_END);
	ap_admin_init(&mod->trade_template_admin, 
		sizeof(struct ap_event_npc_trade_template), 128);
	return mod;
}

boolean ap_event_npc_trade_read_trade_lists(
	struct ap_event_npc_trade_module * mod,
	const char * file_path)
{
	file file = open_file(file_path, FILE_ACCESS_READ);
	char line[1024];
	uint32_t count = 0;
	struct ap_event_npc_trade_template * temp = NULL;
	if (!file) {
		ERROR("Failed to open file (%s).", file_path);
		return FALSE;
	}
	while (read_line(file, line, sizeof(line))) {
		char * token = strtok(line, "\t");
		if (strcmp(token, "Npc") == 0) {
			uint32_t id;
			token = strtok(NULL, "\t");
			assert(token != NULL);
			assert(temp == NULL);
			id = strtoul(token, NULL, 10);
			temp = ap_admin_add_object_by_id(&mod->trade_template_admin, id);
			if (!temp) {
				ERROR("Failed to add npc trade list template.");
				close_file(file);
				return FALSE;
			}
			temp->items = vec_new_reserved(sizeof(*temp->items), 32);
			count++;
		}
		else if (strcmp(token, "Item") == 0) {
			struct ap_event_npc_trade_item item = { 0 };
			assert(temp != NULL);
			token = strtok(NULL, "\t");
			assert(token != NULL);
			item.tid = strtoul(token, NULL, 10);
			token = strtok(NULL, "\t");
			assert(token != NULL);
			item.count = strtoul(token, NULL, 10);
			vec_push_back(&temp->items, &item);
		}
		else if (strcmp(token, "-End") == 0) {
			temp = NULL;
		}
	}
	close_file(file);
	INFO("Loaded %u npc trade list templates.", count);
	return TRUE;
}

void ap_event_npc_trade_add_callback(
	struct ap_event_npc_trade_module * mod,
	enum ap_event_npc_trade_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	ap_module_add_callback(mod, id, callback_module, callback);
}

struct ap_event_npc_trade_template * ap_event_npc_trade_get_template(
	struct ap_event_npc_trade_module * mod, 
	uint32_t template_id)
{
	return ap_admin_get_object_by_id(&mod->trade_template_admin, template_id);
}

static void addtradeitem(
	struct ap_event_npc_trade_module * mod,
	struct ap_character * npc,
	struct ap_event_npc_trade_data * data,
	struct ap_event_npc_trade_item * trade_item)
{
	struct ap_item * item = ap_item_create(mod->ap_item, trade_item->tid);
	uint16_t layer = UINT16_MAX;
	uint16_t row = UINT16_MAX;
	uint16_t column = UINT16_MAX;
	uint32_t j;
	if (!item) {
		WARN("Failed to create npc trade item (item_tid = %u).", trade_item->tid);
		return;
	}
	if (!ap_grid_get_empty(data->item_grid, &layer, &row, &column)) {
		WARN("NPC trade grid is full (trade_template_id = %u).", 
			data->npc_trade_template_id);
		ap_item_free(mod->ap_item, item);
		return;
	}
	ap_grid_add_item(data->item_grid, layer, row, column, AP_GRID_ITEM_TYPE_ITEM,
		item->id, item->tid, item);
	item->character_id = npc->id;
	item->status = AP_ITEM_STATUS_NPC_TRADE;
	item->grid_pos[AP_ITEM_GRID_POS_TAB] = layer;
	item->grid_pos[AP_ITEM_GRID_POS_ROW] = row;
	item->grid_pos[AP_ITEM_GRID_POS_COLUMN] = column;
	item->stack_count = trade_item->count;
	item->option_count = item->temp->option_count;
	for (j = 0; j < item->temp->option_count; j++) {
		item->option_tid[j] = item->temp->option_tid[j];
		item->options[j] = item->temp->options[j];
	}
	item->factor.price.npc_price = item->temp->factor.price.npc_price;
	item->factor.price.pc_price = item->temp->factor.price.pc_price;
}

boolean ap_event_npc_trade_set_template(
	struct ap_event_npc_trade_module * mod, 
	struct ap_event_manager_event * event,
	uint32_t template_id)
{
	struct ap_event_npc_trade_data * data = ap_event_npc_trade_get_data(event);
	struct ap_event_npc_trade_template * temp = 
		ap_event_npc_trade_get_template(mod, template_id);
	uint32_t i;
	uint32_t count;
	assert(ap_base_get_type(event->source) == AP_BASE_TYPE_CHARACTER);
	if (!temp) {
		ERROR("Invalid npc trade template id (%u).", template_id);
		return FALSE;
	}
	data->npc_trade_template_id = template_id;
	data->temp = temp;
	count = vec_count(temp->items);
	for (i = 0; i < count; i++)
		addtradeitem(mod, event->source, data, &temp->items[i]);
	return TRUE;
}

boolean ap_event_npc_trade_on_receive(
	struct ap_event_npc_trade_module * mod,
	const void * data,
	uint16_t length,
	void * user_data)
{
	struct ap_event_npc_trade_cb_receive cb = { 0 };
	const void * basepacket = NULL;
	struct ap_event_manager_base_packet event = { 0 };
	if (!au_packet_get_field(&mod->packet, TRUE, data, length,
		&basepacket, /* Event Base Packet */
		&cb.type, /* Packet Type */ 
		&cb.character_id, /* RequesterCID */
		&cb.item_id, /* lIID */
		NULL, /* DetailInfo */
		NULL, /* TimeStamp */
		NULL, /* Result */
		NULL, /* Layer */
		NULL, /* Row */
		NULL)) { /* Column */
		return FALSE;
	}
	if (basepacket) {
		if (!ap_event_manager_get_base_packet(mod->ap_event_manager, basepacket, &event))
			return FALSE;
		cb.event = &event;
	}
	cb.user_data = user_data;
	return ap_module_enum_callback(mod, AP_EVENT_NPC_TRADE_CB_RECEIVE, &cb);
}

void ap_event_npc_trade_make_response_item_list_packet(
	struct ap_event_npc_trade_module * mod,
	const struct ap_event_manager_event * event,
	const struct ap_item * item)
{
	uint8_t type = AP_EVENT_NPC_TRADE_PACKET_RESPONSE_ITEM_LIST;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	void * base = NULL;
	void * detail = ap_packet_get_temp_buffer(mod->ap_packet);
	uint16_t length = 0;
	float price = (float)item->factor.price.npc_price;
	if (event) {
		base = ap_packet_get_temp_buffer(mod->ap_packet);
		ap_event_manager_make_base_packet(mod->ap_event_manager, event, base);
	}
	au_packet_make_packet(&mod->packet_detail_info, detail, 
		FALSE, NULL, 0,
		&item->tid, /* Item TID */
		&price, /* Item Price */
		&item->stack_count, /* Item Count */
		&item->status, /* Item status */
		&item->grid_pos[0], /* Layer */
		&item->grid_pos[1], /* Row */
		&item->grid_pos[2]); /* Column */
	au_packet_make_packet(&mod->packet, buffer, 
		TRUE, &length, AP_EVENT_NPCTRADE_PACKET_TYPE,
		base, /* Event Base Packet */
		&type, /* Packet Type */ 
		NULL, /* RequesterCID */
		&item->id, /* lIID */
		detail, /* DetailInfo */
		NULL, /* TimeStamp */
		NULL, /* Result */
		NULL, /* Layer */
		NULL, /* Row */
		NULL); /* Column */
	ap_packet_set_length(mod->ap_packet, length);
	ap_packet_reset_temp_buffers(mod->ap_packet);
}

void ap_event_npc_trade_make_send_all_item_info_packet(
	struct ap_event_npc_trade_module * mod,
	uint32_t character_id)
{
	uint8_t type = AP_EVENT_NPC_TRADE_PACKET_SEND_ALL_ITEM_INFO;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	uint16_t length = 0;
	au_packet_make_packet(&mod->packet, buffer, 
		TRUE, &length, AP_EVENT_NPCTRADE_PACKET_TYPE,
		NULL, /* Event Base Packet */
		&type, /* Packet Type */ 
		&character_id, /* RequesterCID */
		NULL, /* lIID */
		NULL, /* DetailInfo */
		NULL, /* TimeStamp */
		NULL, /* Result */
		NULL, /* Layer */
		NULL, /* Row */
		NULL); /* Column */
	ap_packet_set_length(mod->ap_packet, length);
}

void ap_event_npc_trade_make_buy_result_packet(
	struct ap_event_npc_trade_module * mod,
	uint32_t character_id,
	enum ap_event_npc_trade_result_type result)
{
	uint8_t type = AP_EVENT_NPC_TRADE_PACKET_BUY_ITEM;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	uint16_t length = 0;
	au_packet_make_packet(&mod->packet, buffer, 
		TRUE, &length, AP_EVENT_NPCTRADE_PACKET_TYPE,
		NULL, /* Event Base Packet */
		&type, /* Packet Type */ 
		&character_id, /* RequesterCID */
		NULL, /* lIID */
		NULL, /* DetailInfo */
		NULL, /* TimeStamp */
		&result, /* Result */
		NULL, /* Layer */
		NULL, /* Row */
		NULL); /* Column */
	ap_packet_set_length(mod->ap_packet, length);
}

void ap_event_npc_trade_make_sell_result_packet(
	struct ap_event_npc_trade_module * mod,
	uint32_t character_id,
	enum ap_event_npc_trade_result_type result)
{
	uint8_t type = AP_EVENT_NPC_TRADE_PACKET_SELL_ITEM;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	uint16_t length = 0;
	au_packet_make_packet(&mod->packet, buffer, 
		TRUE, &length, AP_EVENT_NPCTRADE_PACKET_TYPE,
		NULL, /* Event Base Packet */
		&type, /* Packet Type */ 
		&character_id, /* RequesterCID */
		NULL, /* lIID */
		NULL, /* DetailInfo */
		NULL, /* TimeStamp */
		&result, /* Result */
		NULL, /* Layer */
		NULL, /* Row */
		NULL); /* Column */
	ap_packet_set_length(mod->ap_packet, length);
}
