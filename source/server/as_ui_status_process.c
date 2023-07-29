#include <assert.h>

#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"

#include "public/ap_item.h"
#include "public/ap_skill.h"
#include "public/ap_ui_status.h"

#include "server/as_ui_status_process.h"
#include "server/as_player.h"

struct as_ui_status_process_module {
	struct ap_module_instance instance;
	struct ap_item_module * ap_item;
	struct ap_skill_module * ap_skill;
	struct ap_ui_status_module * ap_ui_status;
};

static boolean cbreceive(
	struct as_ui_status_process_module * mod,
	struct ap_ui_status_cb_receive * cb)
{
	struct ap_character * c = cb->user_data;
	struct ap_ui_status_character * uc = 
		ap_ui_status_get_character(mod->ap_ui_status, c);
	switch (cb->type) {
	case AP_UI_STATUS_PACKET_TYPE_UPDATE_ITEM: {
		uint32_t tid = AP_ITEM_INVALID_IID;
		if (cb->quickbelt_update) {
			if (cb->quickbelt_index >= AP_UI_STATUS_MAX_QUICKBELT_ITEM_COUNT)
				break;
			switch (cb->quickbelt_update->type) {
			case AP_BASE_TYPE_ITEM: {
				struct ap_item * item = ap_item_find(mod->ap_item, c, 
					cb->quickbelt_update->id, 2, 
					AP_ITEM_STATUS_INVENTORY, 
					AP_ITEM_STATUS_CASH_INVENTORY);
				if (item)
					tid = item->tid;
				break;
			}
			case AP_BASE_TYPE_SKILL: {
				struct ap_skill * skill = 
					ap_skill_find(mod->ap_skill, c, cb->quickbelt_update->id);
				if (skill)
					tid = skill->tid;
				break;
			}
			}
			if (tid != AP_ITEM_INVALID_IID) {
				uint32_t begin = (cb->quickbelt_index / 10) * 10;
				uint32_t end = ((cb->quickbelt_index / 10) + 1) * 10;
				uint32_t i;
				for (i = begin; i < end; i++) {
					if (uc->items[i].tid == tid) {
						memset(&uc->items[i], 0, 
							sizeof(uc->items[i]));
					}
				}
				uc->items[cb->quickbelt_index].base.type = 
					cb->quickbelt_update->type;
				uc->items[cb->quickbelt_index].base.id = 
					cb->quickbelt_update->id;
				uc->items[cb->quickbelt_index].tid = tid;
			}
			else {
				memset(&uc->items[cb->quickbelt_index], 0, sizeof(uc->items[0]));
			}
		}
		else if (cb->quickbelt_index < AP_UI_STATUS_MAX_QUICKBELT_ITEM_COUNT) {
			memset(&uc->items[cb->quickbelt_index], 0, sizeof(uc->items[0]));
		}
		if (cb->hp_potion_tid)
			uc->hp_potion_tid = cb->hp_potion_tid;
		if (cb->mp_potion_tid)
			uc->mp_potion_tid = cb->mp_potion_tid;
		if (cb->auto_use_hp_gauge)
			uc->auto_use_hp_gauge = cb->auto_use_hp_gauge;
		if (cb->auto_use_mp_gauge)
			uc->auto_use_mp_gauge = cb->auto_use_mp_gauge;
		break;
	}
	}
	return TRUE;
}

static boolean onregister(
	struct as_ui_status_process_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_item, AP_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_skill, AP_SKILL_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_ui_status, AP_UI_STATUS_MODULE_NAME);
	ap_ui_status_add_callback(mod->ap_ui_status, AP_UI_STATUS_CB_RECEIVE, mod, cbreceive);
	return TRUE;
}

struct as_ui_status_process_module * as_ui_status_process_create_module()
{
	struct as_ui_status_process_module * mod = ap_module_instance_new(AS_UI_STATUS_PROCESS_MODULE_NAME, 
		sizeof(*mod), onregister, NULL, NULL, NULL);
	return mod;
}
