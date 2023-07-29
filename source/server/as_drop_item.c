#include "server/as_drop_item.h"

#include "core/log.h"
#include "core/malloc.h"

#include "public/ap_admin.h"
#include "public/ap_drop_item.h"
#include "public/ap_item_convert.h"

#include "server/as_character.h"
#include "server/as_item.h"
#include "server/as_player.h"

#include <assert.h>

struct as_drop_item_module {
	struct ap_module_instance instance;
	struct ap_character_module * ap_character;
	struct ap_drop_item_module * ap_drop_item;
	struct ap_item_module * ap_item;
	struct ap_item_convert_module * ap_item_convert;
	struct as_character_module * as_character;
	struct as_item_module * as_item;
	struct as_player_module * as_player;
	size_t character_attachment_offset;
};

static boolean onregister(
	struct as_drop_item_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_character, AP_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_drop_item, AP_DROP_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_item, AP_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_item_convert, AP_ITEM_CONVERT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_character, AS_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_item, AS_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_player, AS_PLAYER_MODULE_NAME);
	mod->character_attachment_offset = ap_character_attach_data(mod->ap_character,
		AP_CHARACTER_MDI_CHAR, sizeof(struct as_drop_item_character_attachment),
		mod, NULL, NULL);
	return TRUE;
}

struct as_drop_item_module * as_drop_item_create_module()
{
	struct as_drop_item_module * mod = ap_module_instance_new(AS_DROP_ITEM_MODULE_NAME,
		sizeof(*mod), onregister, NULL, NULL, NULL);
	return mod;
}

void as_drop_item_add_callback(
	struct as_drop_item_module * mod,
	enum as_drop_item_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	ap_module_add_callback(mod, id, callback_module, callback);
}

size_t as_drop_item_attach_data(
	struct as_drop_item_module * mod,
	enum as_drop_item_module_data_index data_index,
	size_t data_size,
	ap_module_t callback_module,
	ap_module_default_t constructor,
	ap_module_default_t destructor)
{
	return ap_module_attach_data(mod, data_index, data_size, 
		callback_module, constructor, destructor);
}

static struct ap_item * internalgenerate(
	struct as_drop_item_module * mod, 
	const struct ap_item_template * temp,
	uint32_t stack_count,
	void * unused)
{
	struct ap_item * item = ap_item_create(mod->ap_item, temp->tid);
	if (!item)
		return NULL;
	item->update_flags = AP_ITEM_UPDATE_ALL;
	item->stack_count = stack_count;
	if (temp->type == AP_ITEM_TYPE_EQUIP) {
		struct ap_item_convert_item * attachment;
		if (temp->option_count) {
			uint32_t i;
			item->option_count = temp->option_count;
			for (i = 0; i < temp->option_count; i++) {
				item->option_tid[i] = temp->option_tid[i];
				item->options[i] = temp->options[i];
			}
		}
		else {
			ap_drop_item_generate_options(mod->ap_drop_item, item);
		}
		if (AP_ITEM_IS_CASH_ITEM(item))
			item->status_flags |= AP_ITEM_STATUS_FLAG_CASH_PPCARD;
		if (temp->status_flags & AP_ITEM_STATUS_FLAG_BIND_ON_ACQUIRE)
			item->status_flags |= AP_ITEM_STATUS_FLAG_BIND_ON_OWNER;
		attachment = ap_item_convert_get_item(mod->ap_item_convert, item);
		attachment->socket_count = ap_drop_item_generate_socket_count(mod->ap_drop_item, 
			item->temp->min_socket_count, item->temp->max_socket_count);
		attachment->update_flags |= AP_ITEM_CONVERT_UPDATE_SOCKET_COUNT;
	}
	return item;
}

struct as_drop_item_character_attachment * as_drop_item_get_character_attachment(
	struct as_drop_item_module * mod,
	struct ap_character * character)
{
	return ap_module_get_attached_data(character, mod->character_attachment_offset);
}

boolean as_drop_item_distribute(
	struct as_drop_item_module * mod, 
	struct ap_character * character,
	uint32_t item_tid,
	uint32_t stack_count)
{
	return as_drop_item_distribute_custom(mod, character, item_tid, stack_count,
		mod, NULL, internalgenerate);
}

boolean as_drop_item_distribute_custom(
	struct as_drop_item_module * mod,
	struct ap_character * character,
	uint32_t item_tid,
	uint32_t stack_count,
	ap_module_t callback_module,
	void * user_data,
	as_drop_item_generate_t callback)
{
	struct as_character * cs = as_character_get(mod->as_character, character);
	struct as_character_db * dbchar = cs->db;
	struct as_item_character_db * dbichar = NULL;
	const struct ap_item_template * temp;
	assert(item_tid != 0);
	assert(stack_count != 0);
	temp = ap_item_get_template(mod->ap_item, item_tid);
	if (!temp)
		return FALSE;
	if (dbchar) {
		uint32_t require = ap_item_get_required_slot_count(temp, stack_count);
		dbichar = as_item_get_character_db(mod->as_item, dbchar);
		if (dbichar->item_count + require > AS_ITEM_MAX_CHARACTER_ITEM_COUNT)
			return FALSE;
	}
	if (temp->cash_item_type != AP_ITEM_CASH_ITEM_TYPE_NONE) {
		struct ap_grid * grid = ap_item_get_character_grid(mod->ap_item, character, 
			AP_ITEM_STATUS_CASH_INVENTORY);
		if (ap_item_get_required_slot_count(temp, stack_count) > ap_grid_get_empty_count(grid))
			return FALSE;
		while (stack_count) {
			uint32_t count = MAX(1, MIN(stack_count, temp->max_stackable_count));
			uint16_t layer = UINT16_MAX;
			uint16_t row = UINT16_MAX;
			uint16_t col = UINT16_MAX;
			struct ap_item * item = callback(callback_module, temp, count, user_data);
			ap_grid_get_empty(grid, &layer, &row, &col);
			item->character_id = character->id;
			item->status = AP_ITEM_STATUS_CASH_INVENTORY;
			item->grid_pos[AP_ITEM_GRID_POS_TAB] = layer;
			item->grid_pos[AP_ITEM_GRID_POS_ROW] = row;
			item->grid_pos[AP_ITEM_GRID_POS_COLUMN] = col;
			ap_grid_add_item(grid, layer, row, col, 
				AP_GRID_ITEM_TYPE_ITEM, item->id, item->tid, item);
			if (dbichar) {
				struct as_item_db * idb = as_item_new_db(mod->as_item);
				as_item_get(mod->as_item, item)->db = idb;
				dbichar->items[dbichar->item_count++] = idb;
			}
			stack_count -= count;
			ap_item_make_add_packet(mod->ap_item, item);
			as_player_send_packet(mod->as_player, character);
		}
	}
	else {
		enum ap_item_status status[2] = { AP_ITEM_STATUS_INVENTORY, 
			AP_ITEM_STATUS_SUB_INVENTORY };
		uint32_t statuscount = 1;
		uint32_t maxstack = MAX(1, temp->max_stackable_count);
		uint32_t freestack = 0;
		uint32_t addstack;
		uint32_t requireslot;
		uint32_t freeslot = 0;
		uint32_t i;
		if (0) {
			/* \todo Check if pet inventory is available. */
			statuscount++;
		}
		for (i = 0; i < statuscount; i++) {
			freestack += ap_item_get_free_slot_count(mod->ap_item, character,
				status[i], item_tid);
		}
		addstack = (freestack >= stack_count) ? 0 : (stack_count - freestack);
		requireslot = ap_item_get_required_slot_count(temp, addstack);
		for (i = 0; i < statuscount; i++) {
			freeslot += ap_grid_get_empty_count(
				ap_item_get_character_grid(mod->ap_item, character, status[i]));
		}
		if (requireslot > freeslot)
			return FALSE;
		if (freestack) {
			for (i = 0; i < statuscount && stack_count; i++) {
				struct ap_grid * grid = ap_item_get_character_grid(mod->ap_item,
					character, status[i]);
				uint32_t j;
				for (j = 0; j < grid->grid_count && stack_count; j++) {
					struct ap_item * item = ap_grid_get_object_by_index(grid, j);
					if (item && 
						item->tid == item_tid && 
						item->stack_count < temp->max_stackable_count) {
						uint32_t available = temp->max_stackable_count - item->stack_count;
						if (stack_count > available) {
							ap_item_set_stack_count(item, temp->max_stackable_count);
							stack_count -= available;
						}
						else {
							ap_item_increase_stack_count(item, stack_count);
							stack_count = 0;
						}
						ap_item_make_update_packet(mod->ap_item, item, 
							AP_ITEM_UPDATE_STACK_COUNT);
						as_player_send_packet(mod->as_player, character);
					}
				}
			}
		}
		for (i = 0; i < statuscount && stack_count; i++) {
			struct ap_grid * grid = ap_item_get_character_grid(mod->ap_item,
				character, status[i]);
			while (stack_count) {
				uint32_t count;
				uint16_t layer = UINT16_MAX;
				uint16_t row = UINT16_MAX;
				uint16_t col = UINT16_MAX;
				struct ap_item * item;
				if (!ap_grid_get_empty(grid, &layer, &row, &col))
					break;
				count = MAX(1, MIN(stack_count, temp->max_stackable_count));
				item = callback(callback_module, temp, count, user_data);
				item->character_id = character->id;
				item->status = status[i];
				item->grid_pos[AP_ITEM_GRID_POS_TAB] = layer;
				item->grid_pos[AP_ITEM_GRID_POS_ROW] = row;
				item->grid_pos[AP_ITEM_GRID_POS_COLUMN] = col;
				ap_grid_add_item(grid, layer, row, col, 
					AP_GRID_ITEM_TYPE_ITEM, item->id, item->tid, item);
				if (dbichar) {
					struct as_item_db * idb = as_item_new_db(mod->as_item);
					as_item_get(mod->as_item, item)->db = idb;
					dbichar->items[dbichar->item_count++] = idb;
				}
				stack_count -= count;
				ap_item_make_add_packet(mod->ap_item, item);
				as_player_send_packet(mod->as_player, character);
				if (item->temp->type == AP_ITEM_TYPE_EQUIP) {
					ap_item_convert_make_add_packet(mod->ap_item_convert, item);
					as_player_send_packet(mod->as_player, character);
				}
			}
		}
		assert(stack_count == 0);
	}
	return TRUE;
}

boolean as_drop_item_distribute_exclusive(
	struct as_drop_item_module * mod,
	struct ap_character * character,
	uint32_t item_tid,
	uint32_t stack_count,
	ap_module_t callback_module,
	void * user_data,
	as_drop_item_generate_t callback)
{
	struct as_character * cs = as_character_get(mod->as_character, character);
	struct as_character_db * dbchar = cs->db;
	struct as_item_character_db * dbichar = NULL;
	const struct ap_item_template * temp;
	assert(item_tid != 0);
	assert(stack_count != 0);
	temp = ap_item_get_template(mod->ap_item, item_tid);
	if (!temp)
		return FALSE;
	if (dbchar) {
		uint32_t require = ap_item_get_required_slot_count(temp, stack_count);
		dbichar = as_item_get_character_db(mod->as_item, dbchar);
		if (dbichar->item_count + require > AS_ITEM_MAX_CHARACTER_ITEM_COUNT)
			return FALSE;
	}
	if (temp->cash_item_type != AP_ITEM_CASH_ITEM_TYPE_NONE) {
		struct ap_grid * grid = ap_item_get_character_grid(mod->ap_item, character, 
			AP_ITEM_STATUS_CASH_INVENTORY);
		if (ap_item_get_required_slot_count(temp, stack_count) > ap_grid_get_empty_count(grid))
			return FALSE;
		while (stack_count) {
			uint32_t count = MAX(1, MIN(stack_count, temp->max_stackable_count));
			uint16_t layer = UINT16_MAX;
			uint16_t row = UINT16_MAX;
			uint16_t col = UINT16_MAX;
			struct ap_item * item = callback(callback_module, temp, count, user_data);
			ap_grid_get_empty(grid, &layer, &row, &col);
			item->character_id = character->id;
			item->status = AP_ITEM_STATUS_CASH_INVENTORY;
			item->grid_pos[AP_ITEM_GRID_POS_TAB] = layer;
			item->grid_pos[AP_ITEM_GRID_POS_ROW] = row;
			item->grid_pos[AP_ITEM_GRID_POS_COLUMN] = col;
			ap_grid_add_item(grid, layer, row, col, 
				AP_GRID_ITEM_TYPE_ITEM, item->id, item->tid, item);
			if (dbichar) {
				struct as_item_db * idb = as_item_new_db(mod->as_item);
				as_item_get(mod->as_item, item)->db = idb;
				dbichar->items[dbichar->item_count++] = idb;
			}
			stack_count -= count;
			ap_item_make_add_packet(mod->ap_item, item);
			as_player_send_packet(mod->as_player, character);
		}
	}
	else {
		enum ap_item_status status[2] = { AP_ITEM_STATUS_INVENTORY, 
			AP_ITEM_STATUS_SUB_INVENTORY };
		uint32_t statuscount = 1;
		uint32_t requireslot = ap_item_get_required_slot_count(temp, stack_count);
		uint32_t freeslot = 0;
		uint32_t i;
		if (0) {
			/* \todo Check if pet inventory is available. */
			statuscount++;
		}
		for (i = 0; i < statuscount; i++) {
			freeslot += ap_grid_get_empty_count(
				ap_item_get_character_grid(mod->ap_item, character, status[i]));
		}
		if (requireslot > freeslot)
			return FALSE;
		for (i = 0; i < statuscount && stack_count; i++) {
			struct ap_grid * grid = ap_item_get_character_grid(mod->ap_item,
				character, status[i]);
			while (stack_count) {
				uint32_t count;
				uint16_t layer = UINT16_MAX;
				uint16_t row = UINT16_MAX;
				uint16_t col = UINT16_MAX;
				struct ap_item * item;
				if (!ap_grid_get_empty(grid, &layer, &row, &col))
					break;
				count = MAX(1, MIN(stack_count, temp->max_stackable_count));
				item = callback(callback_module, temp, count, user_data);
				item->character_id = character->id;
				item->status = status[i];
				item->grid_pos[AP_ITEM_GRID_POS_TAB] = layer;
				item->grid_pos[AP_ITEM_GRID_POS_ROW] = row;
				item->grid_pos[AP_ITEM_GRID_POS_COLUMN] = col;
				ap_grid_add_item(grid, layer, row, col, 
					AP_GRID_ITEM_TYPE_ITEM, item->id, item->tid, item);
				if (dbichar) {
					struct as_item_db * idb = as_item_new_db(mod->as_item);
					as_item_get(mod->as_item, item)->db = idb;
					dbichar->items[dbichar->item_count++] = idb;
				}
				stack_count -= count;
				ap_item_make_add_packet(mod->ap_item, item);
				as_player_send_packet(mod->as_player, character);
				if (item->temp->type == AP_ITEM_TYPE_EQUIP) {
					ap_item_convert_make_add_packet(mod->ap_item_convert, item);
					as_player_send_packet(mod->as_player, character);
				}
			}
		}
		assert(stack_count == 0);
	}
	return TRUE;
}

