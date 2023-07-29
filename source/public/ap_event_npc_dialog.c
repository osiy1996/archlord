#include "public/ap_event_npc_dialog.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"

#include "public/ap_character.h"
#include "public/ap_packet.h"

#include "utility/au_packet.h"

#include <assert.h>
#include <stdlib.h>

#define STREAM_START "NPCDialogStart"
#define STREAM_END "NPCDialogEnd"
#define STREAM_TEMPLATE "Template"

#define STACKSIZE 2048

struct ap_event_npc_dialog_module {
	struct ap_module_instance instance;
	struct ap_character_module * ap_character;
	struct ap_event_manager_module * ap_event_manager;
	struct ap_packet_module * ap_packet;
	struct au_packet packet;
	struct au_packet packet_event_data;
	void * menu_item_stack_head;
	struct ap_event_npc_dialog_menu_item * menu_item_stack[STACKSIZE];
	uint32_t menu_item_stack_index;
	size_t character_attachment_offset;
	uint32_t event_id_counter;
};

static boolean eventctor(struct ap_event_npc_dialog_module * mod, void * data)
{
	struct ap_event_manager_event * e = data;
	e->data = alloc(sizeof(struct ap_event_npc_dialog_data));
	memset(e->data, 0, sizeof(struct ap_event_npc_dialog_data));
	return TRUE;
}

static boolean eventdtor(struct ap_event_npc_dialog_module * mod, void * data)
{
	struct ap_event_manager_event * e = data;
	dealloc(e->data);
	e->data = NULL;
	return TRUE;
}

static boolean eventread(
	struct ap_event_npc_dialog_module * mod, 
	void * data, 
	struct ap_module_stream * stream)
{
	struct ap_event_npc_dialog_data * e = ap_event_npc_dialog_get_data(data);
	if (!ap_module_stream_read_next_value(stream))
		return TRUE;
	if (strcmp(ap_module_stream_get_value_name(stream), 
			STREAM_START) != 0)
		return TRUE;
	while (ap_module_stream_read_next_value(stream)) {
		const char * value_name = ap_module_stream_get_value_name(stream);
		if (strcmp(value_name, STREAM_TEMPLATE) == 0) {
			if (!ap_module_stream_get_i32(stream, &e->npc_dialog_text_id)) {
				ERROR("Failed to read event npc dialog text id.");
				return FALSE;
			}
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
	struct ap_event_npc_dialog_module * mod, 
	void * data, 
	struct ap_module_stream * stream)
{
	struct ap_event_npc_dialog_data * e = ap_event_npc_dialog_get_data(data);
	if (!ap_module_stream_write_i32(stream, STREAM_START, 0) ||
		!ap_module_stream_write_i32(stream, STREAM_TEMPLATE, 
			e->npc_dialog_text_id) ||
		!ap_module_stream_write_i32(stream, STREAM_END, 0)) {
		ERROR("Failed to write event npc dialog stream.");
		return FALSE;
	}
	return TRUE;
}

static boolean cbmakedatapacket(
	struct ap_event_npc_dialog_module * mod,
	struct ap_event_manager_cb_make_packet * cb)
{
	struct ap_event_npc_dialog_data * d = 
		ap_event_npc_dialog_get_data(cb->event);
	au_packet_make_packet(&mod->packet_event_data, 
		cb->custom_data, FALSE, NULL, 0, 
		&d->npc_dialog_text_id);
	return TRUE;
}

static boolean onregister(
	struct ap_event_npc_dialog_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_character, AP_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_manager, AP_EVENT_MANAGER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_packet, AP_PACKET_MODULE_NAME);
	mod->character_attachment_offset = ap_character_attach_data(mod->ap_character, 
		AP_CHARACTER_MDI_TEMPLATE,
		sizeof(struct ap_event_npc_dialog_character_attachment),
		mod, NULL, NULL);
	if (!ap_event_manager_register_event(mod->ap_event_manager,
			AP_EVENT_MANAGER_FUNCTION_NPCDIALOG, mod,
			eventctor, eventdtor, 
			eventread, eventwrite, NULL)) {
		ERROR("Failed to register event.");
		return FALSE;
	}
	if (!ap_event_manager_register_packet(mod->ap_event_manager,
			AP_EVENT_MANAGER_FUNCTION_NPCDIALOG,
			cbmakedatapacket, NULL)) {
		ERROR("Failed to register event packet.");
		return FALSE;
	}
	return TRUE;
}

static void onshutdown(struct ap_event_npc_dialog_module * mod)
{
	assert(mod->menu_item_stack_index == STACKSIZE);
	dealloc(mod->menu_item_stack_head);
}

struct ap_event_npc_dialog_module * ap_event_npc_dialog_create_module()
{
	struct ap_event_npc_dialog_module * mod = ap_module_instance_new(AP_EVENT_NPC_DIALOG_MODULE_NAME,
		sizeof(*mod), onregister, NULL, NULL, NULL);
	uint32_t i;
	au_packet_init(&mod->packet, sizeof(uint8_t),
		AU_PACKET_TYPE_UINT8, 1, /* Packet Type */ 
		AU_PACKET_TYPE_PACKET, 1, /* Event Base Packet */
		AU_PACKET_TYPE_INT32, 1, /* CID */
		AU_PACKET_TYPE_END);
	au_packet_init(&mod->packet_event_data, sizeof(uint8_t),
		AU_PACKET_TYPE_INT32, 1, /* NPC Dialog TID */
		AU_PACKET_TYPE_END);
	mod->menu_item_stack_head = 
		alloc(STACKSIZE * sizeof(struct ap_event_npc_dialog_menu_item));
	for (i = 0; i < STACKSIZE; i++) {
		mod->menu_item_stack[i] = 
			&((struct ap_event_npc_dialog_menu_item *)mod->menu_item_stack_head)[i];
	}
	mod->menu_item_stack_index = STACKSIZE;
	return mod;
}

void ap_event_npc_dialog_add_callback(
	struct ap_event_npc_dialog_module * mod,
	enum ap_event_npc_dialog_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	ap_module_add_callback(mod, id, callback_module, callback);
}

struct ap_event_npc_dialog_menu_item * ap_event_npc_dialog_alloc_menu_item(
	struct ap_event_npc_dialog_module * mod)
{
	if (!mod->menu_item_stack_index) {
		assert(0);
		return NULL;
	}
	return mod->menu_item_stack[--mod->menu_item_stack_index];
}

void ap_event_npc_dialog_free_menu_item(
	struct ap_event_npc_dialog_module * mod,
	struct ap_event_npc_dialog_menu_item * item)
{
	assert(mod->menu_item_stack_index < COUNT_OF(mod->menu_item_stack));
	mod->menu_item_stack[mod->menu_item_stack_index++] = item;
}

static struct ap_event_npc_dialog_menu_item * findinbranch(
	struct ap_event_npc_dialog_menu_item * menu,
	uint32_t event_id)
{
	while (menu) {
		if (menu->event_id == event_id)
			return menu;
		if (menu->child) {
			struct ap_event_npc_dialog_menu_item * item = 
				findinbranch(menu->child, event_id);
			if (item)
				return item;
		}
		menu = menu->next;
	}
	return NULL;
}

struct ap_event_npc_dialog_menu_item * ap_event_npc_dialog_find_menu_item(
	struct ap_event_npc_dialog_menu_item * menu,
	uint32_t event_id)
{
	struct ap_event_npc_dialog_menu_item * item = menu;
	while (item) {
		struct ap_event_npc_dialog_menu_item * inbranch = findinbranch(item, event_id);
		if (inbranch)
			return inbranch;
		item = item->next;
	}
	return NULL;
}

static void addchild(
	struct ap_event_npc_dialog_menu_item * parent, 
	struct ap_event_npc_dialog_menu_item * child)
{
	struct ap_event_npc_dialog_menu_item * item;
	if (!parent->child) {
		parent->child = child;
		return;
	}
	item = parent->child;
	while (item->next)
		item = item->next;
	item->next = child;
	child->next = NULL;
}

struct ap_event_npc_dialog_menu_item * ap_event_npc_dialog_add_menu(
	struct ap_event_npc_dialog_module * mod,
	struct ap_event_npc_dialog_menu_item * parent_menu,
	const char * text,
	const char * title,
	const char * dialog)
{
	struct ap_event_npc_dialog_menu_item * menu = ap_event_npc_dialog_alloc_menu_item(mod);
	memset(menu, 0, sizeof(*menu));
	menu->event_id = ++mod->event_id_counter;
	menu->type = AP_EVENT_NPC_DIALOG_MENU_ITEM_MENU;
	if (text)
		strlcpy(menu->data.menu.text, text, sizeof(menu->data.menu.text));
	strlcpy(menu->data.menu.title, title, sizeof(menu->data.menu.title));
	strlcpy(menu->data.menu.dialog, dialog, sizeof(menu->data.menu.dialog));
	if (parent_menu)
		addchild(parent_menu, menu);
	return menu;
}

struct ap_event_npc_dialog_menu_item * ap_event_npc_dialog_add_button(
	struct ap_event_npc_dialog_module * mod,
	struct ap_event_npc_dialog_menu_item * parent_menu,
	const char * text,
	ap_module_t callback_module,
	ap_module_default_t callback,
	void * callback_data)
{
	struct ap_event_npc_dialog_menu_item * button = ap_event_npc_dialog_alloc_menu_item(mod);
	memset(button, 0, sizeof(*button));
	button->event_id = ++mod->event_id_counter;
	button->type = AP_EVENT_NPC_DIALOG_MENU_ITEM_BUTTON;
	strlcpy(button->data.button.text, text, sizeof(button->data.button.text));
	button->data.button.callback_module = callback_module;
	button->data.button.callback = callback;
	button->data.button.callback_data = callback_data;
	addchild(parent_menu, button);
	return button;
}

struct ap_event_npc_dialog_menu_item * ap_event_npc_dialog_add_hidden(
	struct ap_event_npc_dialog_module * mod,
	struct ap_event_npc_dialog_menu_item * parent_menu,
	ap_module_t callback_module,
	ap_module_default_t callback,
	void * callback_data)
{
	struct ap_event_npc_dialog_menu_item * hidden = ap_event_npc_dialog_alloc_menu_item(mod);
	memset(hidden, 0, sizeof(*hidden));
	hidden->event_id = ++mod->event_id_counter;
	hidden->type = AP_EVENT_NPC_DIALOG_MENU_ITEM_HIDDEN;
	hidden->data.hidden.callback_module = callback_module;
	hidden->data.hidden.callback = callback;
	hidden->data.hidden.callback_data = callback_data;
	addchild(parent_menu, hidden);
	return hidden;
}

struct ap_event_npc_dialog_character_attachment * ap_event_npc_dialog_get_character_attachment(
	struct ap_event_npc_dialog_module * mod,
	struct ap_character * character)
{
	return ap_module_get_attached_data(character, mod->character_attachment_offset);
}

boolean ap_event_npc_dialog_on_receive(
	struct ap_event_npc_dialog_module * mod,
	const void * data,
	uint16_t length,
	void * user_data)
{
	struct ap_event_npc_dialog_cb_receive cb = { 0 };
	const void * basepacket = NULL;
	struct ap_event_manager_base_packet event = { 0 };
	if (!au_packet_get_field(&mod->packet, TRUE, data, length,
			&cb.type,
			&basepacket,
			&cb.character_id)) {
		return FALSE;
	}
	if (basepacket) {
		if (!ap_event_manager_get_base_packet(mod->ap_event_manager, basepacket, &event))
			return FALSE;
		cb.event = &event;
	}
	cb.packet = data;
	cb.user_data = user_data;
	return ap_module_enum_callback(mod, AP_EVENT_NPC_DIALOG_CB_RECEIVE, &cb);
}

void ap_event_npc_dialog_make_packet(
	struct ap_event_npc_dialog_module * mod,
	enum ap_event_npc_dialog_packet_type type,
	const struct ap_event_manager_event * event,
	uint32_t * character_id)
{
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	void * base = NULL;
	uint16_t length = 0;
	if (event) {
		base = ap_packet_get_temp_buffer(mod->ap_packet);
		ap_event_manager_make_base_packet(mod->ap_event_manager, event, base);
	}
	au_packet_make_packet(&mod->packet, buffer, 
		TRUE, &length, AP_EVENT_NPCDIALOG_PACKET_TYPE,
		&type,
		base,
		character_id);
	ap_packet_set_length(mod->ap_packet, length);
	ap_packet_reset_temp_buffers(mod->ap_packet);
}

void ap_event_npc_dialog_make_grant_msg_box_packet(
	struct ap_event_npc_dialog_module * mod,
	uint32_t character_id,
	uint32_t npc_id,
	uint32_t event_id,
	enum ap_event_npc_dialog_msg_box_type msg_box_type,
	const char * message)
{
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	struct ap_event_npc_dialog_grant_message_box_packet * packet = buffer;
	memset(packet, 0, sizeof(*packet));
	packet->base.base.header.guard_byte = AU_PACKET_FRONT_GUARD_BYTE;
	packet->base.base.header.length = sizeof(*packet) + 1;
	packet->base.base.header.type = AP_EVENT_NPCDIALOG_PACKET_TYPE;
	packet->base.base.header.flags = 0;
	packet->base.base.header.owner_id = 0;
	packet->base.base.header.frame_tick = 0;
	packet->base.base.flag = 0x01;
	packet->base.base.type = AP_EVENT_NPC_DIALOG_PACKET_TYPE_GRANT_EX;
	packet->base.character_id = character_id;
	packet->base.npc_id = npc_id;
	packet->base.event_index = event_id;
	packet->base.step = 0;
	packet->base.grant_type = AP_EVENT_NPC_DIALOG_GRANT_MESSAGEBOX;
	packet->msg_box_type = msg_box_type;
	strlcpy(packet->message, message, sizeof(packet->message));
	packet->item_tid = 0;
	packet->skill_tid = 0;
	*(uint8_t *)((uintptr_t)buffer + sizeof(*packet)) = AU_PACKET_REAR_GUARD_BYTE;
	ap_packet_set_length(mod->ap_packet, packet->base.base.header.length);
}

void ap_event_npc_dialog_make_grant_menu_packet(
	struct ap_event_npc_dialog_module * mod,
	uint32_t character_id,
	uint32_t npc_id,
	struct ap_event_npc_dialog_menu_item * menu)
{
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	struct ap_event_npc_dialog_grant_menu_packet * packet = buffer;
	struct ap_event_npc_dialog_menu_item * item;
	uint32_t i = 0;
	memset(packet, 0, sizeof(*packet));
	packet->base.base.header.guard_byte = AU_PACKET_FRONT_GUARD_BYTE;
	packet->base.base.header.length = sizeof(*packet) + 1;
	packet->base.base.header.type = AP_EVENT_NPCDIALOG_PACKET_TYPE;
	packet->base.base.header.flags = 0;
	packet->base.base.header.owner_id = 0;
	packet->base.base.header.frame_tick = 0;
	packet->base.base.flag = 0x01;
	packet->base.base.type = AP_EVENT_NPC_DIALOG_PACKET_TYPE_GRANT_EX;
	packet->base.character_id = character_id;
	packet->base.npc_id = npc_id;
	packet->base.event_index = menu->event_id;
	packet->base.step = 0;
	packet->base.grant_type = AP_EVENT_NPC_DIALOG_GRANT_MENU;
	packet->menu_type = 0;
	strlcpy(packet->title, menu->data.menu.title, sizeof(packet->title));
	strlcpy(packet->dialog, menu->data.menu.dialog, sizeof(packet->dialog));
	item = menu->child;
	while (item && i < AP_EVENT_NPC_DIALOG_MAX_BUTTON_COUNT) {
		switch (item->type) {
		case AP_EVENT_NPC_DIALOG_MENU_ITEM_MENU:
			packet->buttons[i].event_id = item->event_id;
			packet->buttons[i].step = 0;
			packet->buttons[i].item_tid = 0;
			strlcpy(packet->buttons[i].text, item->data.menu.text, 
				sizeof(packet->buttons[i].text));
			i++;
			break;
		case AP_EVENT_NPC_DIALOG_MENU_ITEM_BUTTON:
			packet->buttons[i].event_id = item->event_id;
			packet->buttons[i].step = 0;
			packet->buttons[i].item_tid = 0;
			strlcpy(packet->buttons[i].text, item->data.button.text, 
				sizeof(packet->buttons[i].text));
			i++;
			break;
		case AP_EVENT_NPC_DIALOG_MENU_ITEM_HIDDEN:
			break;
		}
		item = item->next;
	}
	*(uint8_t *)((uintptr_t)buffer + sizeof(*packet)) = AU_PACKET_REAR_GUARD_BYTE;
	ap_packet_set_length(mod->ap_packet, packet->base.base.header.length);
}
