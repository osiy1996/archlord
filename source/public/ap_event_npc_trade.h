#ifndef _AP_EVENT_NPC_TRADE_H_
#define _AP_EVENT_NPC_TRADE_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_event_manager.h"
#include "public/ap_item.h"
#include "public/ap_module_instance.h"

#define AP_EVENT_NPC_TRADE_MODULE_NAME "AgpmEventNPCTrade"

#define	AP_EVENT_NPC_TRADE_MAX_USE_RANGE 600

BEGIN_DECLS

enum ap_event_npc_trade_packet_type {
	AP_EVENT_NPC_TRADE_PACKET_REQUEST_ITEM_LIST = 0,
	AP_EVENT_NPC_TRADE_PACKET_RESPONSE_ITEM_LIST = 0,
	AP_EVENT_NPC_TRADE_PACKET_SEND_ALL_ITEM_INFO,
	AP_EVENT_NPC_TRADE_PACKET_BUY_ITEM,
	AP_EVENT_NPC_TRADE_PACKET_SELL_ITEM
};

enum ap_event_npc_trade_result_type {
	AP_EVENT_NPC_TRADE_RESULT_SELL_SUCCEEDED = 0,
	AP_EVENT_NPC_TRADE_RESULT_SELL_EXCEED_MONEY,
	AP_EVENT_NPC_TRADE_RESULT_BUY_SUCCEEDED,
	AP_EVENT_NPC_TRADE_RESULT_BUY_NOT_ENOUGH_MONEY,
	AP_EVENT_NPC_TRADE_RESULT_BUY_FULL_INVENTORY,
	AP_EVENT_NPC_TRADE_RESULT_INVALID_ITEMID,
	AP_EVENT_NPC_TRADE_RESULT_INVALID_ITEM_COUNT,
	AP_EVENT_NPC_TRADE_RESULT_INVALID_USE_CLASS,
	AP_EVENT_NPC_TRADE_RESULT_CANNOT_BUY,
	AP_EVENT_NPC_TRADE_RESULT_NO_SELL_TO_MURDERER,
};

enum ap_event_npc_trade_callback_id {
	AP_EVENT_NPC_TRADE_CB_RECEIVE,
};

struct ap_event_npc_trade_item {
	uint32_t tid;
	uint32_t count;
};

struct ap_event_npc_trade_template {
	uint32_t id;
	struct ap_event_npc_trade_item * items;
};

struct ap_event_npc_trade_data {
	uint32_t npc_trade_template_id;
	struct ap_event_npc_trade_template * temp;
	struct ap_grid * item_grid;
};

struct ap_event_npc_trade_cb_receive {
	enum ap_event_npc_trade_packet_type type;
	struct ap_event_manager_base_packet * event;
	uint32_t character_id;
	uint32_t item_id;
	void * user_data;
};

struct ap_event_npc_trade_module * ap_event_npc_trade_create_module();

boolean ap_event_npc_trade_read_trade_lists(
	struct ap_event_npc_trade_module * mod,
	const char * file_path);

void ap_event_npc_trade_add_callback(
	struct ap_event_npc_trade_module * mod,
	enum ap_event_npc_trade_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

struct ap_event_npc_trade_template * ap_event_npc_trade_get_template(
	struct ap_event_npc_trade_module * mod, 
	uint32_t template_id);

boolean ap_event_npc_trade_set_template(
	struct ap_event_npc_trade_module * mod, 
	struct ap_event_manager_event * event,
	uint32_t template_id);

boolean ap_event_npc_trade_on_receive(
	struct ap_event_npc_trade_module * mod,
	const void * data,
	uint16_t length,
	void * user_data);

/**
 * Make item list response packet.
 * \param[in] event Event pointer.
 * \param[in] item  Item pointer.
 */
void ap_event_npc_trade_make_response_item_list_packet(
	struct ap_event_npc_trade_module * mod,
	const struct ap_event_manager_event * event,
	const struct ap_item * item);

/**
 * Make the packet that marks the end of item list.
 * \param[in] character_id Character id.
 */
void ap_event_npc_trade_make_send_all_item_info_packet(
	struct ap_event_npc_trade_module * mod,
	uint32_t character_id);

/**
 * Make purchase result packet.
 * \param[in] character_id Character id.
 * \param[in] result       Result.
 */
void ap_event_npc_trade_make_buy_result_packet(
	struct ap_event_npc_trade_module * mod,
	uint32_t character_id,
	enum ap_event_npc_trade_result_type result);

/**
 * Make sale result packet.
 * \param[in] character_id Character id.
 * \param[in] result       Result.
 */
void ap_event_npc_trade_make_sell_result_packet(
	struct ap_event_npc_trade_module * mod,
	uint32_t character_id,
	enum ap_event_npc_trade_result_type result);

static inline struct ap_event_npc_trade_data * ap_event_npc_trade_get_data(
	struct ap_event_manager_event * event)
{
	return (struct ap_event_npc_trade_data *)event->data;
}

END_DECLS

#endif /* _AP_EVENT_NPC_TRADE_H_ */
