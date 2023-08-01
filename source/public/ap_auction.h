#ifndef _AP_AUCTION_H_
#define _AP_AUCTION_H_

#include "public/ap_event_manager.h"
#include "public/ap_module_instance.h"

#define AP_AUCTION_MODULE_NAME "AgpmAuction"

#define AP_AUCTION_MAX_USE_RANGE 1600.0f

BEGIN_DECLS

struct ap_event_manager_event;

enum ap_auction_packet_type {
	AP_AUCTION_PACKET_NONE = 0,
	AP_AUCTION_PACKET_ADD_SALES,
	AP_AUCTION_PACKET_REMOVE_SALES,
	AP_AUCTION_PACKET_UPDATE_SALES,
	AP_AUCTION_PACKET_SELECT,
	AP_AUCTION_PACKET_SELL,
	AP_AUCTION_PACKET_CANCEL,
	AP_AUCTION_PACKET_CONFIRM,
	AP_AUCTION_PACKET_BUY,
	AP_AUCTION_PACKET_NOTIFY,
	AP_AUCTION_PACKET_EVENT_REQUEST,
	AP_AUCTION_PACKET_EVENT_GRANT,
	AP_AUCTION_PACKET_LOGIN,
	AP_AUCTION_PACKET_SELECT2,
	AP_AUCTION_PACKET_REQUEST_ALL_SALES,
	AP_AUCTION_PACKET_OPEN_ANYWHERE,
};

enum ap_auction_callback_id {
	AP_AUCTION_CB_RECEIVE,
};

struct ap_auction_event {
	boolean has_auction_event;
};

struct ap_auction_sales_packet {
	uint32_t sales_id;
	uint32_t item_id;
	uint32_t price;
	uint16_t quantity;
};

struct ap_auction_select_packet {
	uint32_t item_tid;
	uint32_t page;
};

struct ap_auction_select2_packet {
	uint32_t item_tid;
	uint64_t doc_id;
};

struct ap_auction_sell_packet {
	uint32_t item_id;
	uint32_t price;
};

struct ap_auction_cancel_packet {
	uint32_t sales_id;
	uint32_t item_tid;
};

struct ap_auction_confirm_packet {
	uint32_t sales_id;
	uint32_t item_tid;
};

struct ap_auction_buy_packet {
	uint64_t doc_id;
};

struct ap_auction_cb_receive {
	enum ap_auction_packet_type type;
	struct ap_auction_sales_packet * sales;
	struct ap_auction_select_packet * select;
	struct ap_auction_select2_packet * select2;
	struct ap_auction_sell_packet * sell;
	struct ap_auction_cancel_packet * cancel;
	struct ap_auction_confirm_packet * confirm;
	struct ap_auction_buy_packet * buy;
	struct ap_event_manager_base_packet * event;
	void * user_data;
};

struct ap_auction_module * ap_auction_create_module();

void ap_auction_add_callback(
	struct ap_auction_module * mod,
	enum ap_auction_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

struct ap_auction_event * ap_auction_get_event(
	struct ap_auction_module * mod, 
	struct ap_event_manager_event * e);

boolean ap_auction_on_receive(
	struct ap_auction_module * mod,
	const void * data,
	uint16_t length,
	void * user_data);

END_DECLS

#endif /* _AP_AUCTION_H_ */
