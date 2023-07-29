#ifndef _AP_CASH_MALL_H_
#define _AP_CASH_MALL_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_character.h"
#include "public/ap_define.h"
#include "public/ap_module_instance.h"

#include <assert.h>

#define AP_CASH_MALL_MODULE_NAME "AgpmCashMall"

#define	AP_CASH_MALL_MAX_TAB_NAME_SIZE 32
#define AP_CASH_MALL_MAX_TAB_COUNT 10
#define AP_CASH_MALL_MAX_TAB_ITEM_LIST_SIZE 65536
#define	AP_CASH_MALL_MAX_ITEM_DESCRIPTION_SIZE 256
#define AP_CASH_MALL_MAX_ITEM_BUY_MESSAGE_SIZE 256

BEGIN_DECLS

enum ap_cash_mall_special_flag_type {
	AP_CASH_MALL_SPECIAL_FLAG_NEW = 0x0001,
	AP_CASH_MALL_SPECIAL_FLAG_HOT = 0x0002,
	AP_CASH_MALL_SPECIAL_FLAG_BEST = 0x0004,
	AP_CASH_MALL_SPECIAL_FLAG_EVENT = 0x0008,
	AP_CASH_MALL_SPECIAL_FLAG_SALE = 0x0010,
	AP_CASH_MALL_SPECIAL_FLAG_SPC = 0x0020,
	AP_CASH_MALL_SPECIAL_FLAG_TPACK = 0x0040,
	AP_CASH_MALL_SPECIAL_FLAG_GPREMIUM = 0x0080,
};

enum ap_cash_mall_cash_type {
	AP_CASH_MALL_CASH_TYPE_NORMAL = 0,
	AP_CASH_MALL_CASH_TYPE_WCOIN = 508,
	AP_CASH_MALL_CASH_TYPE_WCOIN_PPCARD = 509,
};

enum ap_cash_mall_packet_type {
	AP_CASH_MALL_PACKET_REQUEST_MALL_PRODUCT_LIST = 1,
	AP_CASH_MALL_PACKET_RESPONSE_MALL_LIST,
	AP_CASH_MALL_PACKET_RESPONSE_PRODUCT_LIST,
	AP_CASH_MALL_PACKET_REQUEST_BUY_ITEM,
	AP_CASH_MALL_PACKET_RESPONSE_BUY_RESULT,
	AP_CASH_MALL_PACKET_REFRESH_CASH,
	AP_CASH_MALL_PACKET_CHECK_LIST_VERSION,	
};

enum ap_cash_mall_buy_result {
	AP_CASH_MALL_BUY_RESULT_SUCCESS = 1,
	AP_CASH_MALL_BUY_RESULT_NOT_ENOUGH_CASH,
	AP_CASH_MALL_BUY_RESULT_INVENTORY_FULL,
	AP_CASH_MALL_BUY_RESULT_NEED_NEW_ITEM_LIST,
	AP_CASH_MALL_BUY_RESULT_PCBANG_ONLY,
	AP_CASH_MALL_BUY_RESULT_LIMITED_COUNT,
	AP_CASH_MALL_BUY_RESULT_FAIL,
	AP_CASH_MALL_BUY_RESULT_PPCARD_ONLY,
	AP_CASH_MALL_BUY_RESULT_NOT_T_PCBANG,
	AP_CASH_MALL_BUY_RESULT_NOT_S_PCBANG,
	AP_CASH_MALL_BUY_RESULT_NOT_GPREMIUM,
};

enum ap_cash_mall_callback_id {
	AP_CASH_MALL_CB_RECEIVE,
};

struct ap_cash_mall_item {
	uint32_t id;
	uint32_t tid;
	uint32_t stack_count;
	uint32_t price;
	enum ap_cash_mall_special_flag_type special_flag;
	char description[AP_CASH_MALL_MAX_ITEM_DESCRIPTION_SIZE];
};

struct ap_cash_mall_tab {
	uint32_t id;
	char name[AP_CASH_MALL_MAX_TAB_NAME_SIZE];
	struct ap_cash_mall_item * items;
	char encoded_item_list[AP_CASH_MALL_MAX_TAB_ITEM_LIST_SIZE];
	uint16_t encoded_item_list_length;
};

struct ap_cash_mall_cb_receive {
	enum ap_cash_mall_packet_type type;
	uint32_t tab_id;
	uint32_t product_id;
	void * user_data;
};

struct ap_cash_mall_module * ap_cash_mall_create_module();

boolean ap_cash_mall_read_import_data(
	struct ap_cash_mall_module * mod, 
	const char * file_path, 
	boolean decrypt);

struct ap_cash_mall_tab * ap_cash_mall_get_tab(
	struct ap_cash_mall_module * mod,
	uint32_t tab_id);

struct ap_cash_mall_tab * ap_cash_mall_get_tab_list(
	struct ap_cash_mall_module * mod,
	uint32_t * count);

void ap_cash_mall_add_callback(
	struct ap_cash_mall_module * mod,
	enum ap_cash_mall_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

boolean ap_cash_mall_on_receive(
	struct ap_cash_mall_module * mod,
	const void * data,
	uint16_t length,
	void * user_data);

void ap_cash_mall_make_mall_list_packet(struct ap_cash_mall_module * mod);

void ap_cash_mall_make_product_list_packet(
	struct ap_cash_mall_module * mod,
	const struct ap_cash_mall_tab * tab);

void ap_cash_mall_make_buy_result_packet(
	struct ap_cash_mall_module * mod,
	enum ap_cash_mall_buy_result result);

END_DECLS

#endif /* _AP_CASH_MALL_H_ */