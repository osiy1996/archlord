#ifndef _AP_PRIVATE_TRADE_H_
#define _AP_PRIVATE_TRADE_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_define.h"
#include "public/ap_grid.h"
#include "public/ap_module_instance.h"

#include <assert.h>

#define AP_PRIVATE_TRADE_MODULE_NAME "AgpmPrivateTrade"

extern size_t g_ApPrivateTradeCharacterAttachmentOffset;

BEGIN_DECLS

enum ap_private_trade_packet_type {
	AP_PRIVATE_TRADE_PACKET_RESULT = 0,
	AP_PRIVATE_TRADE_PACKET_REQUEST_TRADE,
	AP_PRIVATE_TRADE_PACKET_WAIT_CONFIRM,
	AP_PRIVATE_TRADE_PACKET_REQUEST_CONFIRM,
	AP_PRIVATE_TRADE_PACKET_CONFIRM,
	AP_PRIVATE_TRADE_PACKET_CANCEL,
	AP_PRIVATE_TRADE_PACKET_START,
	AP_PRIVATE_TRADE_PACKET_EXECUTE,
	AP_PRIVATE_TRADE_PACKET_END,
	AP_PRIVATE_TRADE_PACKET_ADD_TO_TRADE_GRID,
	AP_PRIVATE_TRADE_PACKET_REMOVE_FROM_TRADE_GRID,
	AP_PRIVATE_TRADE_PACKET_MOVE_TRADE_GRID,
	AP_PRIVATE_TRADE_PACKET_MOVE_INVEN_GRID,
	AP_PRIVATE_TRADE_PACKET_ADD_TO_TARGET_TRADE_GRID,
	AP_PRIVATE_TRADE_PACKET_REMOVE_FROM_TARGET_TRADE_GRID,
	AP_PRIVATE_TRADE_PACKET_MOVE_TARGET_TRADE_GRID,
	AP_PRIVATE_TRADE_PACKET_LOCK,
	AP_PRIVATE_TRADE_PACKET_TARGET_LOCKED,
	AP_PRIVATE_TRADE_PACKET_UNLOCK,
	AP_PRIVATE_TRADE_PACKET_TARGET_UNLOCKED,
	AP_PRIVATE_TRADE_PACKET_ACTIVE_READY_TO_EXCHANGE,
	AP_PRIVATE_TRADE_PACKET_READY_TO_EXCHANGE,
	AP_PRIVATE_TRADE_PACKET_TARGET_READY_TO_EXCHANGE,
	AP_PRIVATE_TRADE_PACKET_UPDATE_MONEY,
	AP_PRIVATE_TRADE_PACKET_TARGET_UPDATE_MONEY,
	AP_PRIVATE_TRADE_PACKET_TRADE_REFUSE,
	AP_PRIVATE_TRADE_PACKET_UPDATE_SELF_CC,
	AP_PRIVATE_TRADE_PACKET_UPDATE_PEER_CC,
};

enum ap_private_trade_callback_id {
	AP_PRIVATE_TRADE_CB_RECEIVE,
	AP_PRIVATE_TRADE_CB_CANCEL,
};

struct ap_private_trade_character_attachment {
	uint64_t gold_amount;
	uint64_t chantra_coin_amount;
	struct ap_character * target;
	uint32_t optional_item_id;
	uint32_t bound_item_count;
	uint32_t inventory_item_count;
	uint32_t cash_item_count;
	boolean locked;
	boolean ready;
	boolean awaiting_confirmation;
};

struct ap_private_trade_cb_receive {
	enum ap_private_trade_packet_type type;
	uint32_t character_id;
	uint32_t target_id;
	uint32_t item_id;
	uint64_t currency_amount;
	uint32_t arg;
	void * user_data;
};

struct ap_private_trade_cb_cancel {
	struct ap_character * character;
	struct ap_private_trade_character_attachment * attachment;
	boolean recursive;
};

struct ap_private_trade_module * ap_private_trade_create_module();

void ap_private_trade_add_callback(
	struct ap_private_trade_module * mod,
	enum ap_private_trade_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

size_t ap_private_trade_attach_data(
	struct ap_private_trade_module * mod,
	enum ap_private_trade_module_data_index data_index,
	size_t data_size,
	ap_module_t callback_module,
	ap_module_default_t constructor,
	ap_module_default_t destructor);

void ap_private_trade_cancel(
	struct ap_private_trade_module * mod,
	struct ap_character * character,
	boolean recursive);

boolean ap_private_trade_on_receive(
	struct ap_private_trade_module * mod,
	const void * data,
	uint16_t length,
	void * user_data);

void ap_private_trade_make_packet(
	struct ap_private_trade_module * mod, 
	enum ap_private_trade_packet_type type,
	uint32_t * character_id,
	uint32_t * target_id,
	uint64_t * amount);

void ap_private_trade_make_result_packet(
	struct ap_private_trade_module * mod, 
	uint32_t character_id);

static inline struct ap_private_trade_character_attachment * ap_private_trade_get_character_attachment(
	struct ap_character * character)
{
	assert(g_ApPrivateTradeCharacterAttachmentOffset != SIZE_MAX);
	return (struct ap_private_trade_character_attachment *)ap_module_get_attached_data(character, g_ApPrivateTradeCharacterAttachmentOffset);
}

static inline void ap_private_trade_clear(struct ap_private_trade_character_attachment * attachment)
{
	attachment->gold_amount = 0;
	attachment->chantra_coin_amount = 0;
	attachment->target = NULL;
	attachment->optional_item_id = 0;
	attachment->bound_item_count = 0;
	attachment->inventory_item_count = 0;
	attachment->cash_item_count = 0;
	attachment->locked = FALSE;
	attachment->ready = FALSE;
	attachment->awaiting_confirmation = FALSE;
}

END_DECLS

#endif /* _AP_PRIVATE_TRADE_H_ */