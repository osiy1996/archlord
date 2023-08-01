#include "editor/ae_item.h"

#include "core/log.h"
#include "core/malloc.h"

#include "public/ap_admin.h"
#include "public/ap_item_convert.h"
#include "public/ap_skill.h"
#include "public/ap_tick.h"

#include <assert.h>

#define ITEM_ID_OFFSET 10000

struct id_list {
	uint32_t id;
	struct id_list * next;
};

struct ae_item_module {
	struct ap_module_instance instance;
	struct ap_character_module * ap_character;
	struct ap_item_module * ap_item;
	struct ap_item_convert_module * ap_item_convert;
	struct ap_skill_module * ap_skill;
	struct ap_tick_module * ap_tick;
	uint32_t item_id_counter;
	struct id_list * id_freelist;
	struct id_list * id_in_use;
	struct ae_item_db * freelist;
};

static boolean itemctor(struct ae_item_module * mod, struct ap_item * item)
{
	struct id_list * id = mod->id_freelist;
	if (id) {
		mod->id_freelist = id->next;
	}
	else {
		if (mod->item_id_counter == UINT32_MAX) {
			/* Should be unreachable unless characters are 
			 * not discarded properly. */
			item->id = 0;
			return TRUE;
		}
		id = alloc(sizeof(*id));
		id->id = mod->item_id_counter++;
	}
	id->next = mod->id_in_use;
	mod->id_in_use = id;
	item->id = id->id;
	return TRUE;
}

static boolean itemdtor(struct ae_item_module * mod, struct ap_item * item)
{
	struct id_list * id = mod->id_in_use;
	struct id_list * prev = NULL;
	while (id) {
		if (id->id == item->id) {
			if (prev)
				prev->next = id->next;
			else
				mod->id_in_use = id->next;
			break;
		}
		prev = id;
		id = id->next;
	}
	assert(id != NULL);
	if (id) {
		id->next = mod->id_freelist;
		mod->id_freelist = id;
	}
	item->id = 0;
	return TRUE;
}

static boolean onregister(
	struct ae_item_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_character, AP_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_item, AP_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_item_convert, AP_ITEM_CONVERT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_skill, AP_SKILL_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_tick, AP_TICK_MODULE_NAME);
	ap_item_attach_data(mod->ap_item, AP_ITEM_MDI_ITEM, 0, mod, itemctor, itemdtor);
	return TRUE;
}

static void onshutdown(struct ae_item_module * mod)
{
	struct id_list * id = mod->id_freelist;
	if (!mod)
		return;
	assert(mod->id_in_use == NULL);
	while (id) {
		struct id_list * next = id->next;
		dealloc(id);
		id = next;
	}
}

struct ae_item_module * ae_item_create_module()
{
	struct ae_item_module * mod = ap_module_instance_new(AE_ITEM_MODULE_NAME,
		sizeof(*mod), onregister, NULL, NULL, onshutdown);
	mod->item_id_counter = ITEM_ID_OFFSET;
	return mod;
}

void ae_item_add_callback(
	struct ae_item_module * mod,
	enum ae_item_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	ap_module_add_callback(mod, id, callback_module, callback);
}
