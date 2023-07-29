#ifndef _AP_EVENT_NPC_DIALOG_H_
#define _AP_EVENT_NPC_DIALOG_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_event_manager.h"
#include "public/ap_module_instance.h"

#include "utility/au_packet.h"

#define AP_EVENT_NPC_DIALOG_MODULE_NAME "AgpmEventNPCDialog"

#define	AP_EVENT_NPC_DIALOG_MAX_USE_RANGE 600

#define AP_EVENT_NPC_DIALOG_MAX_MESSAGE_LENGTH 512
#define AP_EVENT_NPC_DIALOG_MAX_BUTTON_TEXT_LENGTH 128
#define AP_EVENT_NPC_DIALOG_MAX_BUTTON_COUNT 10

BEGIN_DECLS

enum ap_event_npc_dialog_request_packet_type {
	AP_EVENT_NPC_DIALOG_REQUEST_MESSAGEBOX = 1,
	AP_EVENT_NPC_DIALOG_REQUEST_MENU,
};

enum ap_event_npc_dialog_grant_packet_type {
	AP_EVENT_NPC_DIALOG_GRANT_MESSAGEBOX = 1,
	AP_EVENT_NPC_DIALOG_GRANT_MENU,
};

enum ap_event_npc_dialog_msg_box_type {
	AP_EVENT_NPC_DIALOG_MSG_BOX_OK = 1,
	AP_EVENT_NPC_DIALOG_MSG_BOX_EDIT_OK,
	AP_EVENT_NPC_DIALOG_MSG_BOX_YES_NO,
	AP_EVENT_NPC_DIALOG_MSG_BOX_GRID_ITEM,
	AP_EVENT_NPC_DIALOG_MSG_BOX_WAIT,
};

enum ap_event_npc_dialog_packet_type {
	AP_EVENT_NPC_DIALOG_PACKET_TYPE_REQUEST = 0,
	AP_EVENT_NPC_DIALOG_PACKET_TYPE_GRANT,
	AP_EVENT_NPC_DIALOG_PACKET_TYPE_REQUEST_EX,
	AP_EVENT_NPC_DIALOG_PACKET_TYPE_GRANT_EX,
};

enum ap_event_npc_dialog_menu_item_type {
	AP_EVENT_NPC_DIALOG_MENU_ITEM_MENU,
	AP_EVENT_NPC_DIALOG_MENU_ITEM_BUTTON,
	AP_EVENT_NPC_DIALOG_MENU_ITEM_HIDDEN,
};

enum ap_event_npc_dialog_callback_id {
	AP_EVENT_NPC_DIALOG_CB_RECEIVE,
};

struct ap_event_npc_dialog_data {
	uint32_t npc_dialog_text_id;
};

union ap_event_npc_dialog_menu_item_data {
	struct {
		char text[AP_EVENT_NPC_DIALOG_MAX_BUTTON_TEXT_LENGTH + 1];
		char title[AP_EVENT_NPC_DIALOG_MAX_BUTTON_TEXT_LENGTH + 1];
		char dialog[AP_EVENT_NPC_DIALOG_MAX_MESSAGE_LENGTH + 1];
	} menu;
	struct {
		char text[AP_EVENT_NPC_DIALOG_MAX_BUTTON_TEXT_LENGTH + 1];
		ap_module_t callback_module;
		ap_module_default_t callback;
		void * callback_data;
	} button;
	struct {
		ap_module_t callback_module;
		ap_module_default_t callback;
		void * callback_data;
	} hidden;
};

struct ap_event_npc_dialog_menu_item {
	uint32_t event_id;
	enum ap_event_npc_dialog_menu_item_type type;
	union ap_event_npc_dialog_menu_item_data data;
	struct ap_event_npc_dialog_menu_item * child;
	struct ap_event_npc_dialog_menu_item * next;
};

struct ap_event_dialog_packet {
	struct au_packet_header header;
	uint8_t flag;
	uint8_t type;
};

struct ap_event_npc_dialog_request_packet {
	struct ap_event_dialog_packet base;
	uint32_t character_id;
	uint32_t npc_id;
	uint32_t event_index;
	uint32_t step;
	uint8_t request_type;
};

struct ap_event_npc_dialog_request_message_box_packet {
	struct ap_event_npc_dialog_request_packet base;
	boolean is_button_index;
	char message[AP_EVENT_NPC_DIALOG_MAX_MESSAGE_LENGTH + 1];
	uint32_t item_id;
	uint32_t item_tid;
};

struct ap_event_npc_dialog_request_menu_packet {
	struct ap_event_npc_dialog_request_packet base;
};

struct ap_event_npc_dialog_grant_packet {
	struct ap_event_dialog_packet base;
	uint32_t character_id;
	uint32_t npc_id;
	uint32_t event_index;
	uint32_t step;
	uint8_t grant_type;
};

struct ap_event_npc_dialog_grant_message_box_packet {
	struct ap_event_npc_dialog_grant_packet base;
	enum ap_event_npc_dialog_msg_box_type msg_box_type;
	char message[AP_EVENT_NPC_DIALOG_MAX_MESSAGE_LENGTH + 1];
	uint32_t item_tid;
	uint32_t skill_tid;
};

struct ap_event_npc_dialog_menu_button {
	uint32_t event_id;
	uint32_t step;
	uint32_t item_tid;
	char text[AP_EVENT_NPC_DIALOG_MAX_BUTTON_TEXT_LENGTH + 1];
};

struct ap_event_npc_dialog_grant_menu_packet {
	struct ap_event_npc_dialog_grant_packet base;
	int menu_type;
	char title[AP_EVENT_NPC_DIALOG_MAX_BUTTON_TEXT_LENGTH + 1];
	char dialog[AP_EVENT_NPC_DIALOG_MAX_MESSAGE_LENGTH + 1];
	struct ap_event_npc_dialog_menu_button buttons[AP_EVENT_NPC_DIALOG_MAX_BUTTON_COUNT];
};

struct ap_event_npc_dialog_character_attachment {
	struct ap_event_npc_dialog_menu_item * menu;
};

struct ap_event_npc_dialog_cb_receive {
	enum ap_event_npc_dialog_packet_type type;
	struct ap_event_manager_base_packet * event;
	uint32_t character_id;
	const void * packet;
	void * user_data;
};

struct ap_event_npc_dialog_menu_callback {
	struct ap_character * character;
	struct ap_character * npc;
	void * callback_data;
};

struct ap_event_npc_dialog_module * ap_event_npc_dialog_create_module();

void ap_event_npc_dialog_add_callback(
	struct ap_event_npc_dialog_module * mod,
	enum ap_event_npc_dialog_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

struct ap_event_npc_dialog_menu_item * ap_event_npc_dialog_alloc_menu_item(
	struct ap_event_npc_dialog_module * mod);

void ap_event_npc_dialog_free_menu_item(
	struct ap_event_npc_dialog_module * mod,
	struct ap_event_npc_dialog_menu_item * item);

struct ap_event_npc_dialog_menu_item * ap_event_npc_dialog_find_menu_item(
	struct ap_event_npc_dialog_menu_item * menu,
	uint32_t event_id);

struct ap_event_npc_dialog_menu_item * ap_event_npc_dialog_add_menu(
	struct ap_event_npc_dialog_module * mod,
	struct ap_event_npc_dialog_menu_item * parent_menu,
	const char * text,
	const char * title,
	const char * dialog);

struct ap_event_npc_dialog_menu_item * ap_event_npc_dialog_add_button(
	struct ap_event_npc_dialog_module * mod,
	struct ap_event_npc_dialog_menu_item * parent_menu,
	const char * text,
	ap_module_t callback_module,
	ap_module_default_t callback,
	void * callback_data);

struct ap_event_npc_dialog_menu_item * ap_event_npc_dialog_add_hidden(
	struct ap_event_npc_dialog_module * mod,
	struct ap_event_npc_dialog_menu_item * parent_menu,
	ap_module_t callback_module,
	ap_module_default_t callback,
	void * callback_data);

struct ap_event_npc_dialog_character_attachment * ap_event_npc_dialog_get_character_attachment(
	struct ap_event_npc_dialog_module * mod,
	struct ap_character * character);

boolean ap_event_npc_dialog_on_receive(
	struct ap_event_npc_dialog_module * mod,
	const void * data,
	uint16_t length,
	void * user_data);

/**
 * Make event npc dialog packet.
 * \param[in] type         Packet type.
 * \param[in] event        (optional) Event pointer.
 * \param[in] character_id (optional) Character id pointer.
 */
void ap_event_npc_dialog_make_packet(
	struct ap_event_npc_dialog_module * mod,
	enum ap_event_npc_dialog_packet_type type,
	const struct ap_event_manager_event * event,
	uint32_t * character_id);

	struct ap_event_npc_dialog_grant_packet base;
	enum ap_event_npc_dialog_msg_box_type msg_box_type;
	char message[AP_EVENT_NPC_DIALOG_MAX_MESSAGE_LENGTH + 1];
	uint32_t item_tid;
	uint32_t skill_tid;

void ap_event_npc_dialog_make_grant_msg_box_packet(
	struct ap_event_npc_dialog_module * mod,
	uint32_t character_id,
	uint32_t npc_id,
	uint32_t event_id,
	enum ap_event_npc_dialog_msg_box_type msg_box_type,
	const char * message);

void ap_event_npc_dialog_make_grant_menu_packet(
	struct ap_event_npc_dialog_module * mod,
	uint32_t character_id,
	uint32_t npc_id,
	struct ap_event_npc_dialog_menu_item * menu);

inline struct ap_event_npc_dialog_data * ap_event_npc_dialog_get_data(
	struct ap_event_manager_event * event)
{
	return (struct ap_event_npc_dialog_data *)event->data;
}

END_DECLS

#endif /* _AP_EVENT_NPC_DIALOG_H_ */
