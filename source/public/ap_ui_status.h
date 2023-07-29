#ifndef _AP_UI_STATUS_H_
#define _AP_UI_STATUS_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_base.h"
#include "public/ap_define.h"
#include "public/ap_module_instance.h"

#define AP_UI_STATUS_MODULE_NAME "AgpmUIStatus"

#define	AP_UI_STATUS_MAX_QUICKBELT_STRING 512

#define	AP_UI_STATUS_MAX_QUICKBELT_COLUMN 10
#define	AP_UI_STATUS_MAX_QUICKBELT_LAYER 4
#define AP_UI_STATUS_MAX_QUICKBELT_ITEM_COUNT \
	AP_UI_STATUS_MAX_QUICKBELT_LAYER * AP_UI_STATUS_MAX_QUICKBELT_COLUMN

#define	AP_UI_STATUS_HALF_QUICKBELT_COLUMN 5

#define AP_UI_STATUS_MAX_COOLDOWN_STRING 512

BEGIN_DECLS

struct ap_ui_status_module;

enum ap_ui_status_callback_id {
	AP_UI_STATUS_CB_RECEIVE,
};

enum ap_ui_status_packet_type {
	AP_UI_STATUS_PACKET_TYPE_ADD = 0,
	AP_UI_STATUS_PACKET_TYPE_UPDATE_ITEM,
	AP_UI_STATUS_PACKET_TYPE_ENCODED_STRING,
	AP_UI_STATUS_PACKET_TYPE_UPDATE_VIEW_HELMET_OPTION,
};

struct ap_ui_status_item {
	struct ap_base base;
	uint32_t tid;
};

struct ap_ui_status_character {
	struct ap_ui_status_item items[AP_UI_STATUS_MAX_QUICKBELT_ITEM_COUNT];
	uint32_t hp_potion_tid;
	uint32_t mp_potion_tid;
	boolean option_view_helmet;
	uint8_t auto_use_hp_gauge;
	uint8_t auto_use_mp_gauge;
};

struct ap_ui_status_cb_receive {
	enum ap_ui_status_packet_type type;
	uint32_t character_id;
	uint8_t quickbelt_index;
	struct ap_base * quickbelt_update;
	uint32_t hp_potion_tid;
	uint32_t mp_potion_tid;
	boolean option_view_helmet;
	uint8_t auto_use_hp_gauge;
	uint8_t auto_use_mp_gauge;
	void * user_data;
};

struct ap_ui_status_module * ap_ui_status_create_module();

void ap_ui_status_add_callback(
	struct ap_ui_status_module * mod,
	enum ap_ui_status_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

size_t ap_ui_status_attach_data(
	struct ap_ui_status_module * mod,
	enum ap_ui_status_module_data_index data_index,
	size_t data_size,
	ap_module_t callback_module,
	ap_module_default_t constructor,
	ap_module_default_t destructor);

struct ap_ui_status_character * ap_ui_status_get_character(
	struct ap_ui_status_module * mod,
	struct ap_character * character);

boolean ap_ui_status_on_receive(
	struct ap_ui_status_module * mod,
	const void * data,
	uint16_t length,
	void * user_data);

void ap_ui_status_make_add_packet(
	struct ap_ui_status_module * mod,
	struct ap_character * character);

END_DECLS

#endif /* _AP_UI_STATUS_H_ */