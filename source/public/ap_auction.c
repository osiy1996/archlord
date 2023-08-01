#include "public/ap_auction.h"

#include "core/log.h"
#include "core/malloc.h"

#include "public/ap_event_manager.h"
#include "public/ap_object.h"

#include "utility/au_packet.h"

#include <assert.h>
#include <stdlib.h>

#define INI_NAME_START "AuctionStart"
#define INI_NAME_END "AuctionEnd"
#define INI_NAME_HAVE_EVENT "HaveAuctionEvent"

struct ap_auction_module {
	struct ap_module_instance instance;
	struct ap_event_manager_module * ap_event_manager;
	struct au_packet packet;
	struct au_packet packet_sales;
	struct au_packet packet_select;
	struct au_packet packet_select2;
	struct au_packet packet_sell;
	struct au_packet packet_cancel;
	struct au_packet packet_confirm;
	struct au_packet packet_buy;
	struct au_packet packet_rowset;
};

static boolean event_ctor(struct ap_auction_module * mod, void * data)
{
	struct ap_event_manager_event * e = data;
	e->data = alloc(sizeof(struct ap_auction_event));
	((struct ap_auction_event *)e->data)->has_auction_event = FALSE;
	return TRUE;
}

static boolean event_dtor(struct ap_auction_module * mod, void * data)
{
	struct ap_event_manager_event * e = data;
	dealloc(e->data);
	e->data = NULL;
	return TRUE;
}

static boolean event_read(
	struct ap_auction_module * mod, 
	void * data, 
	struct ap_module_stream * stream)
{
	struct ap_auction_event * e = ap_auction_get_event(mod, data);
	const char * value_name;
	if (!ap_module_stream_read_next_value(stream))
		return TRUE;
	value_name = ap_module_stream_get_value_name(stream);
	if (strcmp(value_name, INI_NAME_START) != 0)
		return TRUE;
	while (ap_module_stream_read_next_value(stream)) {
		value_name = ap_module_stream_get_value_name(stream);
		if (strcmp(value_name, INI_NAME_HAVE_EVENT) == 0) {
			e->has_auction_event = 
				strtol(ap_module_stream_get_value(stream), 
					NULL, 10) != 0;
		}
		else if (strcmp(value_name, INI_NAME_END) == 0) {
			break;
		}
		else {
			assert(0);
		}
	}
	return TRUE;
}

static boolean event_write(
	struct ap_auction_module * mod, 
	void * data, 
	struct ap_module_stream * stream)
{
	struct ap_auction_event * e = ap_auction_get_event(mod, data);
	if (!ap_module_stream_write_i32(stream, INI_NAME_START, 0) ||
		!ap_module_stream_write_i32(stream, INI_NAME_HAVE_EVENT, 
				e->has_auction_event) ||
		!ap_module_stream_write_i32(stream, INI_NAME_END, 0)) {
		return FALSE;
	}
	return TRUE;
}

static boolean onregister(
	struct ap_auction_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_manager, AP_EVENT_MANAGER_MODULE_NAME);
	if (!ap_event_manager_register_event(mod->ap_event_manager,
			AP_EVENT_MANAGER_FUNCTION_AUCTION, mod, event_ctor, event_dtor, 
			event_read, event_write, NULL)) {
		ERROR("Failed to register event.");
		return FALSE;
	}
	return TRUE;
}

struct ap_auction_module * ap_auction_create_module()
{
	struct ap_auction_module * mod = ap_module_instance_new(AP_AUCTION_MODULE_NAME,
		sizeof(*mod), onregister, NULL, NULL, NULL);
	au_packet_init(&mod->packet, sizeof(uint16_t),
		AU_PACKET_TYPE_INT8, 1, /* packet type */
		AU_PACKET_TYPE_INT32, 1, /* cid */
		AU_PACKET_TYPE_PACKET, 1, /* embedded packet */
		AU_PACKET_TYPE_INT32, 1, /* result */
		AU_PACKET_TYPE_END);
	au_packet_init(&mod->packet_sales, sizeof(uint16_t),
		AU_PACKET_TYPE_INT32, 1, /* sales id */
		AU_PACKET_TYPE_UINT64, 1, /* doc id. */
		AU_PACKET_TYPE_UINT64, 1, /* item seq */
		AU_PACKET_TYPE_INT32, 1, /* item id */
		AU_PACKET_TYPE_INT32, 1, /* price */
		AU_PACKET_TYPE_INT16, 1, /* quantity */
		AU_PACKET_TYPE_INT16, 1, /* status */
		AU_PACKET_TYPE_CHAR, 33, /* reg date */
		AU_PACKET_TYPE_INT32, 1, /* item tid */
		AU_PACKET_TYPE_END);
	au_packet_init(&mod->packet_select, sizeof(uint8_t),
		AU_PACKET_TYPE_INT32, 1, /* item tid */
		AU_PACKET_TYPE_UINT32, 1, /* page */
		AU_PACKET_TYPE_PACKET, 1, /* item info.(rowset) */
		AU_PACKET_TYPE_END);
	au_packet_init(&mod->packet_select2, sizeof(uint8_t),
		AU_PACKET_TYPE_INT32, 1, /* item tid */
		AU_PACKET_TYPE_UINT64, 1, /* doc id.(max, min) */
		AU_PACKET_TYPE_INT16, 1, /* flag(forward, backward) */
		AU_PACKET_TYPE_INT32, 1, /* total no. of sales of Item TID */
		AU_PACKET_TYPE_PACKET, 1, /* item info.(rowset) */
		AU_PACKET_TYPE_END);
	au_packet_init(&mod->packet_sell, sizeof(uint8_t),
		AU_PACKET_TYPE_INT32, 1, /* item id */
		AU_PACKET_TYPE_INT16, 1, /* quantity */
		AU_PACKET_TYPE_INT32, 1, /* price */
		AU_PACKET_TYPE_CHAR, 33, /* date */
		AU_PACKET_TYPE_END);
	au_packet_init(&mod->packet_cancel, sizeof(uint8_t),
		AU_PACKET_TYPE_INT32, 1, /* sales id */
		AU_PACKET_TYPE_INT32, 1, /* item tid */
		AU_PACKET_TYPE_INT16, 1, /* quantity remained */
		AU_PACKET_TYPE_END);
	au_packet_init(&mod->packet_confirm, sizeof(uint8_t),
		AU_PACKET_TYPE_INT32, 1, /* sales id */
		AU_PACKET_TYPE_INT32, 1, /* item tid */
		AU_PACKET_TYPE_INT16, 1, /* quantity selled */
		AU_PACKET_TYPE_INT32, 1, /* income */
		AU_PACKET_TYPE_END);
	au_packet_init(&mod->packet_buy, sizeof(uint8_t),
		AU_PACKET_TYPE_INT64, 1, /* doc id */
		AU_PACKET_TYPE_INT16, 1, /* quantity */
		AU_PACKET_TYPE_INT32, 1, /* item tid */
		AU_PACKET_TYPE_INT32, 1, /* price */
		AU_PACKET_TYPE_END);
	au_packet_init(&mod->packet_rowset, sizeof(uint16_t),
		AU_PACKET_TYPE_INT32, 1, /* Query Index */
		AU_PACKET_TYPE_MEMORY_BLOCK, 1, /* Query Text */
		AU_PACKET_TYPE_MEMORY_BLOCK, 1, /* Headers */
		AU_PACKET_TYPE_UINT32, 1, /* No. of Rows */
		AU_PACKET_TYPE_UINT32, 1, /* No. of Cols */
		AU_PACKET_TYPE_UINT32, 1, /* Row Buffer Size */
		AU_PACKET_TYPE_MEMORY_BLOCK, 1, /* Buffer */
		AU_PACKET_TYPE_MEMORY_BLOCK, 1, /* Offset */
		AU_PACKET_TYPE_END);
	return mod;
}

void ap_auction_add_callback(
	struct ap_auction_module * mod,
	enum ap_auction_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	ap_module_add_callback(mod, id, callback_module, callback);
}

struct ap_auction_event * ap_auction_get_event(
	struct ap_auction_module * mod, 
	struct ap_event_manager_event * e)
{
	return e->data;
}

boolean ap_auction_on_receive(
	struct ap_auction_module * mod,
	const void * data,
	uint16_t length,
	void * user_data)
{
	struct ap_auction_cb_receive cb = { 0 };
	void * packet = NULL;
	struct ap_auction_sales_packet sales = { 0 };
	struct ap_auction_select_packet select = { 0 };
	struct ap_auction_select2_packet select2 = { 0 };
	struct ap_auction_sell_packet sell = { 0 };
	struct ap_auction_cancel_packet cancel = { 0 };
	struct ap_auction_confirm_packet confirm = { 0 };
	struct ap_auction_buy_packet buy = { 0 };
	struct ap_event_manager_base_packet event = { 0 };
	if (!au_packet_get_field(&mod->packet, TRUE, data, length,
			&cb.type, /* packet type */
			NULL, /* cid */
			&packet, /* embedded packet */
			NULL)) { /* result */
		return FALSE;
	}
	if (packet) {
		switch (cb.type) {
		case AP_AUCTION_PACKET_SELL:
			if (!au_packet_get_field(&mod->packet_sell, FALSE, packet, 0,
					&sell.item_id, /* item id */
					NULL, /* quantity */
					&sell.price, /* price */
					NULL)) { /* date */
				return FALSE;
			}
			cb.sell = &sell;
			break;
		case AP_AUCTION_PACKET_CANCEL:
			if (!au_packet_get_field(&mod->packet_cancel, FALSE, packet, 0,
					&cancel.sales_id, /* sales id */
					NULL, /* item tid */
					NULL)) { /* quantity remained */
				return FALSE;
			}
			cb.cancel = &cancel;
			break;
		case AP_AUCTION_PACKET_CONFIRM:
			if (!au_packet_get_field(&mod->packet_confirm, FALSE, packet, 0,
					&confirm.sales_id, /* sales id */
					NULL, /* item tid */
					NULL, /* quantity selled */
					NULL)) { /* income */
				return FALSE;
			}
			cb.confirm = &confirm;
			break;
		case AP_AUCTION_PACKET_BUY:
			if (!au_packet_get_field(&mod->packet_buy, FALSE, packet, 0,
					&buy.doc_id, /* doc id */
					NULL, /* quantity */
					NULL, /* item tid */
					NULL)) { /* price */
				return FALSE;
			}
			cb.buy = &buy;
			break;
		case AP_AUCTION_PACKET_EVENT_REQUEST:
			if (!ap_event_manager_get_base_packet(mod->ap_event_manager, 
					packet, &event)) {
				return FALSE;
			}
			break;
		case AP_AUCTION_PACKET_SELECT2:
			if (!au_packet_get_field(&mod->packet_select2, FALSE, packet, 0,
					&select2.item_tid, /* item tid */
					&select2.doc_id, /* doc id.(max, min) */
					NULL, /* flag(forward, backward) */
					NULL, /* total no. of sales of Item TID */
					NULL)) { /* item info.(rowset) */
				return FALSE;
			}
			cb.select2 = &select2;
			break;
		}
	}
	cb.event = &event;
	cb.user_data = user_data;
	return ap_module_enum_callback(mod, AP_AUCTION_CB_RECEIVE, &cb);
}
