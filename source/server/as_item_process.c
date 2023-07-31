#include "server/as_item_process.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/vector.h"

#include "public/ap_chat.h"
#include "public/ap_event_teleport.h"
#include "public/ap_item.h"
#include "public/ap_item_convert.h"
#include "public/ap_module.h"
#include "public/ap_pvp.h"
#include "public/ap_random.h"
#include "public/ap_skill.h"
#include "public/ap_system_message.h"
#include "public/ap_tick.h"

#include "server/as_drop_item.h"
#include "server/as_event_binding.h"
#include "server/as_item.h"
#include "server/as_map.h"
#include "server/as_player.h"
#include "server/as_skill.h"

#include <assert.h>
#include <stdlib.h>

#define CHARACTER_RNG_MODULE_ID 1
#define MAKE_RNG_GUID(RNG_ID) \
	AS_CHARACTER_MAKE_RNG_GUID(CHARACTER_RNG_MODULE_ID, RNG_ID)

enum rng_group_id {
	RNG_GROUP_LOTTERY_BOX = 1,
};

struct as_item_process_module {
	struct ap_module_instance instance;
	struct ap_character_module * ap_character;
	struct ap_chat_module * ap_chat;
	struct ap_event_teleport_module * ap_event_teleport;
	struct ap_item_module * ap_item;
	struct ap_item_convert_module * ap_item_convert;
	struct ap_random_module * ap_random;
	struct ap_pvp_module * ap_pvp;
	struct ap_skill_module * ap_skill;
	struct ap_system_message_module * ap_system_message;
	struct ap_tick_module * ap_tick;
	struct as_character_module * as_character;
	struct as_drop_item_module * as_drop_item;
	struct as_event_binding_module * as_event_binding;
	struct as_item_module * as_item;
	struct as_map_module * as_map;
	struct as_player_module * as_player;
	struct as_skill_module * as_skill;
	struct ap_character ** list;
};

static inline uint64_t makelotteryguid(uint32_t item_tid)
{
	return MAKE_RNG_GUID(((uint64_t)RNG_GROUP_LOTTERY_BOX << 32) | item_tid);
}

static void trymerge(
	struct as_item_process_module * mod,
	struct ap_character * c,
	struct ap_item * item, 
	struct ap_item * target)
{
	uint32_t combinedstack;
	if (item == target)
		return;
	combinedstack = item->stack_count + target->stack_count;
	if (combinedstack > item->temp->max_stackable_count)
		return;
	as_item_delete(mod->as_item, c, item);
	ap_item_set_stack_count(target, combinedstack);
	ap_item_make_update_packet(mod->ap_item, target, AP_ITEM_UPDATE_STACK_COUNT);
	as_player_send_packet(mod->as_player, c);
}

static boolean equipavatar(
	struct as_item_process_module * mod,
	struct ap_character * character,
	struct ap_item * item)
{
	const struct ap_item_avatar_set * set = &item->temp->usable.avatar_set;
	uint32_t i;
	for (i = 0; i < set->count; i++) {
		if (ap_item_get_equipment(mod->ap_item, character, 
				set->temp[i]->equip.part)) {
			return FALSE;
		}
	}
	for (i = 0; i < set->count; i++) {
		struct ap_item * avatarpart = ap_item_create(mod->ap_item,
			set->temp[i]->tid);
		if (!avatarpart)
			continue;
		avatarpart->character_id = character->id;
		avatarpart->stack_count = 1;
		if (!ap_item_equip(mod->ap_item, character, avatarpart, 
				AP_ITEM_EQUIP_BY_ITEM_IN_USE | AP_ITEM_EQUIP_WITH_NO_STATS)) {
			ap_item_free(mod->ap_item, avatarpart);
			continue;
		}
	}
	return TRUE;
}

void unequipavatar(
	struct as_item_process_module * mod,
	struct ap_character * character,
	struct ap_item * item)
{
	const struct ap_item_avatar_set * set = &item->temp->usable.avatar_set;
	uint32_t i;
	for (i = 0; i < set->count; i++) {
		struct ap_item * avatarpart = ap_item_get_equipment(mod->ap_item, character, 
			set->temp[i]->equip.part);
		if (avatarpart) {
			assert(avatarpart->equip_flags & AP_ITEM_EQUIP_BY_ITEM_IN_USE);
			if (!ap_item_unequip(mod->ap_item, character, avatarpart)) {
				assert(0);
			}
			ap_item_free(mod->ap_item, avatarpart);
		}
	}
}

static boolean checkuseskill(
	struct as_item_process_module * mod,
	struct ap_character * character,
	struct ap_item * item)
{
	const struct ap_item_template * temp = item->temp;
	const struct ap_item_template_usable * usable = &temp->usable;
	const struct ap_skill_template * skilltemp;
	if (!usable->use_skill_tid || !usable->use_skill_level)
		return TRUE;
	if (!temp->enable_on_ride && 
		ap_item_get_equipment(mod->ap_item, character, AP_ITEM_PART_RIDE)) {
		return FALSE;
	}
	skilltemp = ap_skill_get_template(mod->ap_skill, usable->use_skill_tid);
	if (!skilltemp || !(skilltemp->attribute & AP_SKILL_ATTRIBUTE_BUFF))
		return FALSE;
	if (ap_skill_is_stronger_or_equal_effect_applied(mod->ap_skill, character, 
			skilltemp, usable->use_skill_level)) {
		ap_pvp_make_system_message_packet(mod->ap_pvp, 
			AP_PVP_SYSTEM_CODE_SKILL_CANNOT_APPLY_EFFECT, 
			UINT32_MAX, UINT32_MAX, NULL, NULL);
		as_player_send_packet(mod->as_player, character);
		return FALSE;
	}
	return TRUE;
}

static void useskill(
	struct as_item_process_module * mod,
	struct ap_character * character,
	struct ap_item * item)
{
	const struct ap_item_template * temp = item->temp;
	const struct ap_item_template_usable * usable = &temp->usable;
	uint32_t duration;
	struct ap_skill_buff_list * buff;
	const struct ap_skill_template * skilltemp;
	if (!usable->use_skill_tid || !usable->use_skill_level)
		return;
	skilltemp = ap_skill_get_template(mod->ap_skill, usable->use_skill_tid);
	if (!skilltemp || !(skilltemp->attribute & AP_SKILL_ATTRIBUTE_BUFF))
		return;
	if (AP_ITEM_IS_CASH_ITEM(item)) {
		switch (temp->cash_item_type) {
		case AP_ITEM_CASH_ITEM_TYPE_REAL_TIME:
		case AP_ITEM_CASH_ITEM_TYPE_ONE_ATTACK:
		case AP_ITEM_CASH_ITEM_TYPE_STAMINA:
			duration = UINT32_MAX / 4;
			break;
		default:
			if (item->remain_time)
				duration = (uint32_t)item->remain_time;
			else
				duration = (uint32_t)item->temp->remain_time;
			break;
		}
	}
	else {
		duration = (uint32_t)temp->remain_time;
	}
	as_skill_clear_similar_buffs(mod->as_skill, character, skilltemp, 
		usable->use_skill_level);
	buff = as_skill_add_buff(mod->as_skill, character, 0, 
		usable->use_skill_level, skilltemp, duration, 
		character->id, character->tid);
	if (AP_ITEM_IS_CASH_ITEM(item)) {
		ap_item_get_skill_buff_attachment(mod->ap_item, buff)->bound_item_id = 
			item->id;
	}
}

static boolean cbloadchar(
	struct as_item_process_module * mod, 
	struct as_character_cb_load * cb)
{
	struct ap_item_character * ichar = 
		ap_item_get_character(mod->ap_item, cb->character);
	struct as_item_character_db * db = 
		as_item_get_character_db(mod->as_item, cb->db);
	uint32_t i;
	for (i = 0; i < db->item_count; i++) {
		struct as_item_db * idb = db->items[i];
		struct ap_item * item = as_item_from_db(mod->as_item, idb);
		struct ap_grid * grid;
		if (!item) {
			ERROR("Failed to create database character item (character = %s, item_tid = %u).",
				cb->character->name, idb->tid);
			memmove(&db->items[i], &db->items[i + 1], 
				(--db->item_count - i) * sizeof(db->items[0]));
			i--;
			continue;
		}
		switch (item->status) {
		case AP_ITEM_STATUS_INVENTORY:
			grid = ichar->inventory;
			break;
		case AP_ITEM_STATUS_EQUIP:
			grid = ichar->equipment;
			break;
		case AP_ITEM_STATUS_TRADE_GRID:
			grid = ichar->trade;
			break;
		case AP_ITEM_STATUS_SUB_INVENTORY:
			grid = ichar->sub_inventory;
			break;
		case AP_ITEM_STATUS_CASH_INVENTORY:
			grid = ichar->cash_inventory;
			break;
		case AP_ITEM_STATUS_BANK:
			grid = ichar->bank;
			break;
		default:
			assert(0);
			ERROR("Invalid database character item status (character = %s, item_tid = %u, item_status = %d).",
				cb->character->name, idb->tid, idb->status);
			ap_item_free(mod->ap_item, item);
			continue;
		}
		item->character_id = cb->character->id;
		if (item->status == AP_ITEM_STATUS_EQUIP) {
			uint32_t flags = AP_ITEM_EQUIP_SILENT;
			if (!ap_item_check_equip_restriction(mod->ap_item, item, cb->character))
				flags |= AP_ITEM_EQUIP_WITH_NO_STATS;
			if (!ap_item_equip(mod->ap_item, cb->character, item, flags)) {
				ERROR("Failed to equip item (character = %s, item = [%u] %s.",
					cb->character->name, idb->tid, 
					item->temp->name);
				ap_item_free(mod->ap_item, item);
				memmove(&db->items[i], &db->items[i + 1], 
					(--db->item_count - i) * sizeof(db->items[0]));
				i--;
				continue;
			}
		}
		else if (!ap_grid_is_free(grid, idb->layer, idb->row, idb->column)) {
			ERROR("Grid is already occupied by another item (character = %s, item_tid = %u, item_status = %d).",
				cb->character->name, idb->tid, idb->status);
			assert(0);
			ap_item_free(mod->ap_item, item);
			continue;
		}
		else {
			ap_grid_add_item(grid, idb->layer, idb->row, 
				idb->column, AP_GRID_ITEM_TYPE_ITEM, item->id, 
				item->tid, item);
		}
		if (AP_ITEM_IS_CASH_ITEM(item) && 
			item->temp->type == AP_ITEM_TYPE_USABLE &&
			item->in_use) {
			boolean useok = TRUE;
			if (!checkuseskill(mod, cb->character, item)) {
				if (item->temp->can_stop_using) {
					item->in_use = FALSE;
					item->update_flags |= AP_ITEM_UPDATE_IN_USE;
				}
			}
			else {
				switch (item->temp->usable.usable_type) {
				case AP_ITEM_USABLE_TYPE_AVATAR:
					if (!equipavatar(mod, cb->character, item)) {
						if (item->temp->can_stop_using) {
							item->in_use = FALSE;
							item->update_flags |= AP_ITEM_UPDATE_IN_USE;
						}
						useok = FALSE;
					}
					break;
				}
				useskill(mod, cb->character, item);
			}
		}
	}
	return TRUE;
}

static void cbchatcreate(
	struct as_item_process_module * mod,
	struct ap_character * c,
	uint32_t argc,
	const char * const * argv)
{
	uint32_t tid;
	uint32_t count = 1;
	if (argc < 1)
		return;
	tid = strtoul(argv[0], NULL, 10);
	if (argc > 1) {
		count = strtoul(argv[1], NULL, 10);
		if (!count)
			count = 1;
	}
	as_drop_item_distribute(mod->as_drop_item, c, tid, count);
}

static boolean cbcharprocess(
	struct as_item_process_module * mod,
	struct ap_character_cb_process * cb)
{
	struct ap_character * character =  cb->character;
	struct ap_item_character * attachment = 
		ap_item_get_character(mod->ap_item, character);
	uint32_t i;
	struct ap_grid * grid;
	uint64_t dt;
	time_t t;
	if (cb->tick < attachment->recent_inventory_process_tick + 10000)
		return TRUE;
	dt = cb->tick - attachment->recent_inventory_process_tick;
	/** \todo Process inventory and sub-inventory to remove expired items. */
	/** \todo Process equipments to unequip and remove expired items. */
	grid = attachment->cash_inventory;
	t = time(NULL);
	for (i = 0; i < grid->grid_count; i++) {
		struct ap_item * item = ap_grid_get_object_by_index(grid, i);
		boolean enduse = FALSE;
		if (!item)
			continue;
		if (item->expire_time && t >= item->expire_time) {
			enduse = TRUE;
			item->expire_time = 0;
			item->update_flags |= AP_ITEM_UPDATE_EXPIRE_TIME;
		}
		if (item->in_use && item->remain_time) {
			if (dt >= item->remain_time) {
				enduse = TRUE;
				item->remain_time = 0;
			}
			else {
				item->remain_time -= dt;
				ap_item_make_update_use_time_packet(mod->ap_item, item);
				as_player_send_packet(mod->as_player, character);
			}
			item->update_flags |= AP_ITEM_UPDATE_REMAIN_TIME;
		}
		if (enduse) {
			item->in_use = FALSE;
			item->update_flags |= AP_ITEM_UPDATE_IN_USE;
			if (item->temp->type == AP_ITEM_TYPE_USABLE && 
				item->temp->usable.use_skill_tid) {
				as_skill_remove_buff_with_tid(mod->as_skill, character, 
					item->temp->usable.use_skill_tid);
			}
			ap_item_make_update_use_time_packet(mod->ap_item, item);
			as_player_send_packet(mod->as_player, character);
			as_item_consume(mod->as_item, character, item, 1);
		}
	}
	attachment->recent_inventory_process_tick = cb->tick;
	return TRUE;
}

static boolean cbremovebuff(
	struct as_item_process_module * mod,
	struct ap_skill_cb_remove_buff * cb)
{
	struct ap_item * item;
	if (!(cb->buff->temp->effect_type2 & AP_SKILL_EFFECT2_DURATION_TYPE))
		return TRUE;
	if (!ap_skill_has_detail(cb->buff->temp, AP_SKILL_EFFECT_DETAIL_DURATION_TYPE,
			AP_SKILL_EFFECT_DETAIL_DURATION_TYPE2)) {
		return TRUE;
	}
	item = ap_item_find(mod->ap_item, cb->character,
		ap_item_get_skill_buff_attachment(mod->ap_item, cb->buff)->bound_item_id,
		1, AP_ITEM_STATUS_CASH_INVENTORY);
	if (!item || !item->in_use)
		return TRUE;
	assert(AP_ITEM_IS_CASH_ITEM(item));
	item->in_use = FALSE;
	item->update_flags |= AP_ITEM_UPDATE_IN_USE;
	ap_system_message_make_packet(mod->ap_system_message, 
		AP_SYSTEM_MESSAGE_CODE_CASH_ITEM_STOP_USING_ITEM,
		UINT32_MAX, UINT32_MAX, item->temp->name, NULL);
	as_player_send_packet(mod->as_player, cb->character);
	ap_item_make_update_use_time_packet(mod->ap_item, item);
	as_player_send_packet(mod->as_player, cb->character);
	return TRUE;
}

static void useitem(
	struct as_item_process_module * mod,
	struct ap_character * character,
	struct ap_item * item)
{
	const struct ap_item_template * temp = item->temp;
	const struct ap_item_template_usable * usable = &temp->usable;
	struct as_player_session * session = 
		as_player_get_character_ad(mod->as_player, character)->session;
	struct as_item_player_session_attachment * attachment = 
		as_item_get_player_session_attachment(mod->as_item, session);
	uint32_t i;
	struct ap_item_cooldown * cooldown = NULL;
	uint64_t cooldownendtick = 0;
	uint64_t tick = ap_tick_get(mod->ap_tick);
	boolean iscashitem = AP_ITEM_IS_CASH_ITEM(item);
	uint32_t itemtid = item->tid;
	for (i = 0; i < attachment->cooldown_count; i++) {
		struct ap_item_cooldown * cd = &attachment->cooldowns[i];
		if (cd->item_tid == item->tid) {
			if (tick < cd->end_tick) {
				/* Item is in cooldown. */
				return;
			}
			cooldown = cd;
			break;
		}
	}
	if (!cooldown) {
		if (attachment->cooldown_count >= AS_ITEM_MAX_COOLDOWN_COUNT)
			return;
		cooldown = &attachment->cooldowns[attachment->cooldown_count++];
		cooldown->item_tid = item->tid;
	}
	cooldownendtick = tick + usable->effect_apply_count * 
		usable->effect_apply_interval + usable->use_interval;
	switch (temp->cash_item_type) {
	case AP_ITEM_CASH_ITEM_TYPE_ONE_ATTACK:
		if (ap_item_find_item_in_use_by_cash_item_type(mod->ap_item, 
				character, AP_ITEM_CASH_ITEM_TYPE_ONE_ATTACK, 
				1, AP_ITEM_STATUS_CASH_INVENTORY)) {
			/* Already using a Lens Stone like item. */
			return;
		}
		break;
	case AP_ITEM_CASH_ITEM_TYPE_STAMINA:
		if (ap_item_find_item_in_use_by_cash_item_type(mod->ap_item, 
				character, AP_ITEM_CASH_ITEM_TYPE_STAMINA, 
				1, AP_ITEM_STATUS_CASH_INVENTORY)) {
			/* Already using a pet-like item. */
			return;
		}
		break;
	}
	if (!checkuseskill(mod, character, item))
		return;
	switch (usable->usable_type) {
	case AP_ITEM_USABLE_TYPE_POTION: {
		boolean recovershp;
		boolean recoversmp;
		if (character->action_status == AP_CHARACTER_ACTION_STATUS_DEAD)
			return;
		recovershp = temp->factor.char_point.hp;
		if (recovershp && 
			character->factor.char_point.hp >= character->factor.char_point_max.hp) {
			/* HP is full. */
			return;
		}
		recoversmp = temp->factor.char_point.mp;
		if (recoversmp && 
			character->factor.char_point.mp >= character->factor.char_point_max.mp) {
			/* MP is full. */
			return;
		}
		if (recovershp) {
			uint32_t amount;
			if (tick < attachment->hp_potion_cooldown_end_tick)
				return;
			attachment->hp_potion_cooldown_end_tick = cooldownendtick;
			if (usable->is_percent_potion) {
				amount = (uint32_t)(character->factor.char_point_max.hp * 
					temp->factor.char_point.hp / 100.0f);
			}
			else {
				amount = temp->factor.char_point.hp;
			}
			character->factor.char_point.hp += amount;
			ap_character_fix_hp(character);
			ap_character_update(mod->ap_character, character, 
				AP_FACTORS_BIT_HP, FALSE);
		}
		else if (recoversmp) {
			uint32_t amount;
			if (tick < attachment->mp_potion_cooldown_end_tick)
				return;
			attachment->mp_potion_cooldown_end_tick = cooldownendtick;
			if (usable->is_percent_potion) {
				amount = (uint32_t)(character->factor.char_point_max.mp * 
					temp->factor.char_point.mp / 100.0f);
			}
			else {
				amount = temp->factor.char_point.mp;
			}
			character->factor.char_point.mp += amount;
			ap_character_fix_mp(character);
			ap_character_update(mod->ap_character, character, 
				AP_FACTORS_BIT_MP, FALSE);
		}
		break;
	}
	case AP_ITEM_USABLE_TYPE_TELEPORT_SCROLL: {
		struct as_map_region * region;
		const struct au_pos * destination = NULL;
		region = as_map_get_character_ad(mod->as_map, character)->region;
		if (region && region->temp->level_limit) {
			/* Disable teleport in level limited regions (i.e. CF). */
			return;
		}
		switch (usable->teleport_scroll_type) {
		case AP_ITEM_USABLE_TELEPORT_SCROLL_RETURN_TOWN:
			if (region) {
				if (region->temp->type.props.safety_type == AP_MAP_ST_SAFE) {
					return;
				}
				else if (region->temp->type.props.safety_type == AP_MAP_ST_DANGER &&
					region->temp->disabled_item_section != 1) {
					/* Roundtrip is disabled in dangerous battlefields, 
					 * with the exception of BG (disabled item section). */
					return;
				}
			}
			/* Fall through. */
		case AP_ITEM_USABLE_TELEPORT_SCROLL_GO_TOWN: {
			if (tick < character->combat_end_tick) {
				/* Disable teleport in combat. */
				return;
			}
			/* Fall through. */
		case AP_ITEM_USABLE_TELEPORT_SCROLL_GO_TOWN_NOW:
			destination = as_event_binding_find_by_type(mod->as_event_binding, 
				character->bound_region_id, AP_EVENT_BINDING_TYPE_RESURRECTION,
				NULL);
			break;
		case AP_ITEM_USABLE_TELEPORT_SCROLL_DESTINATION:
			destination = &usable->destination;
			break;
		}
		}
		if (!destination)
			return;
		if (usable->teleport_scroll_type == AP_ITEM_USABLE_TELEPORT_SCROLL_RETURN_TOWN) {
			struct ap_item_character * itemattachment = 
				ap_item_get_character(mod->ap_item, character);
			itemattachment->is_return_scroll_enabled = TRUE;
			itemattachment->return_scroll_position = character->pos;
			ap_item_make_enable_return_scroll_packet(mod->ap_item, character->id);
			as_player_send_packet(mod->as_player, character);
		}
		ap_event_teleport_start(mod->ap_event_teleport, character, destination);
		break;
	}
	case AP_ITEM_USABLE_TYPE_SKILL_SCROLL: {
		if (tick < item->recent_turn_off_tick + 1000) {
			/* Disable rapid toggle on/off of usable items. */
			return;
		}
		break;
	}
	case AP_ITEM_USABLE_TYPE_REVERSE_ORB:
		if (character->action_status != AP_CHARACTER_ACTION_STATUS_DEAD)
			return;
		character->action_status = AP_CHARACTER_ACTION_STATUS_NORMAL;
		character->factor.char_point.hp = character->factor.char_point_max.hp;
		character->factor.char_point.mp = character->factor.char_point_max.mp;
		ap_character_update(mod->ap_character, character, AP_FACTORS_BIT_HP | 
			AP_FACTORS_BIT_MP | AP_CHARACTER_BIT_ACTION_STATUS, FALSE);
		ap_character_special_status_on(mod->ap_character, character, 
			AP_CHARACTER_SPECIAL_STATUS_INVINCIBLE, 10000);
		ap_item_make_update_reuse_time_for_reverse_orb_packet(mod->ap_item,
			character->id, (uint32_t)(cooldownendtick - tick));
		as_player_send_packet(mod->as_player, character);
		break;
	case AP_ITEM_USABLE_TYPE_LOTTERY_BOX: {
		const struct ap_item_lottery_box * box = &usable->lottery_box;
		uint64_t guid = makelotteryguid(item->tid);
		uint32_t rate;
		uint32_t totalrate = 0;
		uint32_t potcount = vec_count(box->items);
		uint32_t potstack = 0;
		const struct ap_item_lottery_item  * potitem = NULL;
		struct as_character_db * characterdb = 
			as_character_get(mod->as_character, character)->db;
		if (!potcount || !characterdb)
			return;
		rate = as_character_generate_random(characterdb, guid, box->total_rate);
		while (potcount--) {
			const struct ap_item_lottery_item  * pot = &box->items[potcount];
			totalrate += pot->rate;
			if (!potcount || rate < totalrate) {
				potitem = pot;
				potstack = ap_random_between(mod->ap_random, 
					pot->min_stack_count,
					pot->min_stack_count);
				break;
			}
		}
		if (!potitem || !potstack)
			return;
		if (!as_drop_item_distribute(mod->as_drop_item, character, 
				potitem->item_tid, potstack)) {
			return;
		}
		ap_system_message_make_packet(mod->ap_system_message,
			AP_SYSTEM_MESSAGE_CODE_LOTTERY_ITEM_TO_POT_ITEM,
			potstack, UINT32_MAX, temp->name, potitem->name);
		as_player_send_packet(mod->as_player, character);
		break;
	}
	case AP_ITEM_USABLE_TYPE_AVATAR: {
		if (tick < item->recent_turn_off_tick + 1000) {
			/* Disable rapid toggle on/off of usable items. */
			return;
		}
		if (ap_item_find_item_in_use_by_usable_type(mod->ap_item, character, 
				AP_ITEM_USABLE_TYPE_AVATAR, 1, AP_ITEM_STATUS_CASH_INVENTORY)) {
			/* Cannot equip more than one avatar sets. */
			return;
		}
		if (!equipavatar(mod, character, item))
			return;
		break;
	}
	}
	useskill(mod, character, item);
	if (AP_ITEM_IS_CASH_ITEM(item)) {
		item->status_flags &= ~AP_ITEM_STATUS_FLAG_CASH_PPCARD;
		item->update_flags |= AP_ITEM_UPDATE_STATUS_FLAGS;
		switch (temp->cash_item_use_type) {
		case AP_ITEM_CASH_ITEM_USE_TYPE_CONTINUOUS:
			item->in_use = TRUE;
			item->update_flags |= AP_ITEM_UPDATE_IN_USE;
			switch (temp->cash_item_type) {
			case AP_ITEM_CASH_ITEM_TYPE_REAL_TIME:
				if (!item->expire_time) {
					item->expire_time = time(NULL) + temp->expire_time;
					item->update_flags |= AP_ITEM_UPDATE_EXPIRE_TIME;
				}
				break;
			case AP_ITEM_CASH_ITEM_TYPE_PLAY_TIME:
				if (!item->remain_time) {
					item->remain_time = temp->remain_time;
					item->update_flags |= AP_ITEM_UPDATE_REMAIN_TIME;
				}
				break;
			}
			ap_item_make_update_use_time_packet(mod->ap_item, item);
			as_player_send_packet(mod->as_player, character);
			break;
		case AP_ITEM_CASH_ITEM_USE_TYPE_ONCE:
			as_item_consume(mod->as_item, character, item, 1);
			break;
		}
	}
	else {
		as_item_consume(mod->as_item, character, item, 1);
	}
	cooldown->end_tick = cooldownendtick;
	ap_item_make_use_item_by_tid_packet(mod->ap_item, itemtid, character->id,
		character->id, (uint32_t)(cooldownendtick - tick));
	as_player_send_packet(mod->as_player, character);
}

static void tryconvert(
	struct as_item_process_module * mod,
	struct ap_character * c,
	struct ap_item * item, 
	struct ap_item * target)
{
	if (item->status == AP_ITEM_STATUS_BANK || target->status == AP_ITEM_STATUS_BANK)
		return;
	if (item->temp->type != AP_ITEM_TYPE_USABLE) {
		ap_item_convert_make_response_rune_check_result_packet(mod->ap_item_convert,
			c->id, target->id, item->id, AP_ITEM_CONVERT_RUNE_RESULT_INVALID_RUNE_ITEM);;
		as_player_send_packet(mod->as_player, c);
		return;
	}
	switch (item->temp->usable.usable_type) {
	case AP_ITEM_USABLE_TYPE_RUNE: {
		enum ap_item_convert_rune_result result = ap_item_convert_is_suitable_for_rune_convert(mod->ap_item_convert,
			target, ap_item_convert_get_item(mod->ap_item_convert, target), 
			item);
		ap_item_convert_make_response_rune_check_result_packet(mod->ap_item_convert,
			c->id, target->id, item->id, result);
		as_player_send_packet(mod->as_player, c);
		break;
	}
	case AP_ITEM_USABLE_TYPE_SPIRIT_STONE: {
		enum ap_item_convert_spirit_stone_result result = ap_item_convert_is_suitable_for_spirit_stone(mod->ap_item_convert,
			target, ap_item_convert_get_item(mod->ap_item_convert, target), 
			item);
		ap_item_convert_make_response_spirit_stone_check_result_packet(mod->ap_item_convert,
			c->id, target->id, item->id, result);
		as_player_send_packet(mod->as_player, c);
		break;
	}
	}
}

static void deposittobank(
	struct as_item_process_module * mod,
	struct ap_item_cb_receive * cb,
	struct ap_character * character,
	struct ap_item * item)
{
	struct ap_grid * bankgrid;
	struct ap_grid * sourcegrid;
	struct ap_item * target;
	if (!cb->bank)
		return;
	bankgrid = ap_item_get_character_grid(mod->ap_item, character, AP_ITEM_STATUS_BANK);
	sourcegrid = ap_item_get_character_grid(mod->ap_item, character, item->status);
	assert(bankgrid != NULL);
	assert(sourcegrid != NULL);
	target = ap_grid_get_object(bankgrid, cb->bank->layer,
		cb->bank->row, cb->bank->column);
	if (target) {
		/* Target inventory slot is occupied. 
		 * If applicable, we can merge items. */
		if (item->tid == target->tid)
			trymerge(mod, character, item, target);
	}
	else {
		struct as_item_db * db;
		if (cb->bank->layer == UINT16_MAX) {
			if (!ap_grid_get_empty(bankgrid, 
					&cb->bank->layer,
					&cb->bank->row,
					&cb->bank->column)) {
				return;
			}
		}
		else if (cb->bank->row == UINT16_MAX ||
				cb->bank->column == UINT16_MAX) {
			if (!ap_grid_get_empty_in_layer(bankgrid, 
					cb->bank->layer,
					&cb->bank->row,
					&cb->bank->column)) {
				return;
			}
		}
		db = as_item_get(mod->as_item, item)->db;
		if (db) {
			/* Transfer database record from character to account. */
			uint32_t i;
			struct as_item_character_db * charattachment;
			struct as_player_character * playerattachment = 
				as_player_get_character_ad(mod->as_player, character);
			struct as_item_account_attachment * attachment =
				as_item_get_account_attachment(mod->as_item, playerattachment->account) ;
			if (attachment->item_count >= AS_ITEM_MAX_ACCOUNT_ITEM_COUNT)
				return;
			charattachment = as_item_get_character_db(mod->as_item, 
				as_character_get(mod->as_character, character)->db);
			for (i = 0; i < charattachment->item_count; i++) {
				if (charattachment->items[i] == db) {
					memmove(&charattachment->items[i], &charattachment->items[i + 1],
						((size_t)--charattachment->item_count * sizeof(charattachment->items[0])));
					break;
				}
			}
			attachment->items[attachment->item_count++] = db;
		}
		ap_grid_set_empty(sourcegrid, item->grid_pos[AP_ITEM_GRID_POS_TAB],
			item->grid_pos[AP_ITEM_GRID_POS_ROW], item->grid_pos[AP_ITEM_GRID_POS_COLUMN]);
		ap_grid_add_item(bankgrid, cb->bank->layer, cb->bank->row,
			cb->bank->column, AP_GRID_ITEM_TYPE_ITEM, item->id, item->tid, item);
		ap_item_set_status(item, AP_ITEM_STATUS_BANK);
		ap_item_set_grid_pos(item, cb->bank->layer,
			cb->bank->row, cb->bank->column);
		ap_item_make_update_packet(mod->ap_item, item, 
			AP_ITEM_UPDATE_STATUS | AP_ITEM_UPDATE_GRID_POS);
		as_player_send_packet(mod->as_player, character);
	}
}

static void withdrawfrombank(
	struct as_item_process_module * mod,
	struct ap_item_cb_receive * cb,
	struct ap_character * character,
	struct ap_item * item)
{
	struct ap_grid * bankgrid;
	struct ap_grid * targetgrid;
	struct ap_item * target;
	if (!cb->inventory)
		return;
	bankgrid = ap_item_get_character_grid(mod->ap_item, character, AP_ITEM_STATUS_BANK);
	targetgrid = ap_item_get_character_grid(mod->ap_item, character, cb->status);
	assert(bankgrid != NULL);
	assert(targetgrid != NULL);
	target = ap_grid_get_object(targetgrid, cb->inventory->layer,
		cb->inventory->row, cb->inventory->column);
	if (target) {
		/* Target inventory slot is occupied. 
		 * If applicable, we can merge items. */
		if (item->tid == target->tid)
			trymerge(mod, character, item, target);
	}
	else {
		struct as_item_db * db;
		if (cb->inventory->layer == UINT16_MAX) {
			if (!ap_grid_get_empty(targetgrid, 
					&cb->inventory->layer,
					&cb->inventory->row,
					&cb->inventory->column)) {
				return;
			}
		}
		else if (cb->inventory->row == UINT16_MAX ||
				cb->inventory->column == UINT16_MAX) {
			if (!ap_grid_get_empty_in_layer(targetgrid, 
					cb->inventory->layer,
					&cb->inventory->row,
					&cb->inventory->column)) {
				return;
			}
		}
		db = as_item_get(mod->as_item, item)->db;
		if (db) {
			/* Transfer database record from account to character. */
			uint32_t i;
			struct as_player_character * playerattachment;
			struct as_item_account_attachment * attachment;
			struct as_item_character_db * charattachment = 
				as_item_get_character_db(mod->as_item, 
					as_character_get(mod->as_character, character)->db);
			if (charattachment->item_count >= AS_ITEM_MAX_CHARACTER_ITEM_COUNT)
				return;
			playerattachment = as_player_get_character_ad(mod->as_player, character);
			attachment = as_item_get_account_attachment(mod->as_item, playerattachment->account);
			for (i = 0; i < attachment->item_count; i++) {
				if (attachment->items[i] == db) {
					memmove(&attachment->items[i], &attachment->items[i + 1],
						((size_t)--attachment->item_count * sizeof(attachment->items[0])));
					break;
				}
			}
			charattachment->items[charattachment->item_count++] = db;
		}
		ap_grid_set_empty(bankgrid, item->grid_pos[AP_ITEM_GRID_POS_TAB],
			item->grid_pos[AP_ITEM_GRID_POS_ROW], item->grid_pos[AP_ITEM_GRID_POS_COLUMN]);
		ap_grid_add_item(targetgrid, cb->inventory->layer, cb->inventory->row,
			cb->inventory->column, AP_GRID_ITEM_TYPE_ITEM, item->id, item->tid, item);
		ap_item_set_status(item, cb->status);
		ap_item_set_grid_pos(item, cb->inventory->layer,
			cb->inventory->row, cb->inventory->column);
		ap_item_make_update_packet(mod->ap_item, item, 
			AP_ITEM_UPDATE_STATUS | AP_ITEM_UPDATE_GRID_POS);
		as_player_send_packet(mod->as_player, character);
	}
}

static void movebetweensubinventory(
	struct as_item_process_module * mod,
	struct ap_item_cb_receive * cb,
	struct ap_character * character,
	struct ap_item * item)
{
	struct ap_grid * sourcegrid;
	struct ap_grid * targetgrid;
	struct ap_item * target;
	if (!cb->inventory)
		return;
	sourcegrid = ap_item_get_character_grid(mod->ap_item, character, item->status);
	targetgrid = ap_item_get_character_grid(mod->ap_item, character, cb->status);
	assert(sourcegrid != NULL);
	assert(targetgrid != NULL);
	target = ap_grid_get_object(targetgrid, cb->inventory->layer,
		cb->inventory->row, cb->inventory->column);
	if (target) {
		/* Target inventory slot is occupied. 
		 * If applicable, we can merge items or 
		 * initiate convert process. */
		if (item->tid == target->tid)
			trymerge(mod, character, item, target);
		else
			tryconvert(mod, character, item, target);
	}
	else {
		if (cb->inventory->layer == UINT16_MAX) {
			if (!ap_grid_get_empty(targetgrid, 
					&cb->inventory->layer,
					&cb->inventory->row,
					&cb->inventory->column)) {
				return;
			}
		}
		else if (cb->inventory->row == UINT16_MAX ||
				cb->inventory->column == UINT16_MAX) {
			if (!ap_grid_get_empty_in_layer(targetgrid, 
					cb->inventory->layer,
					&cb->inventory->row,
					&cb->inventory->column)) {
				return;
			}
		}
		ap_grid_set_empty(sourcegrid, item->grid_pos[AP_ITEM_GRID_POS_TAB],
			item->grid_pos[AP_ITEM_GRID_POS_ROW], item->grid_pos[AP_ITEM_GRID_POS_COLUMN]);
		ap_grid_add_item(targetgrid, cb->inventory->layer, cb->inventory->row,
			cb->inventory->column, AP_GRID_ITEM_TYPE_ITEM, item->id, item->tid, item);
		ap_item_set_status(item, cb->status);
		ap_item_set_grid_pos(item, cb->inventory->layer,
			cb->inventory->row, cb->inventory->column);
		ap_item_make_update_packet(mod->ap_item, item, 
			AP_ITEM_UPDATE_STATUS | AP_ITEM_UPDATE_GRID_POS);
		as_player_send_packet(mod->as_player, character);
	}
}

static boolean cbreceive(
	struct as_item_process_module * mod,
	struct ap_item_cb_receive * cb)
{
	struct ap_character * c = cb->user_data;
	struct ap_item * item = NULL;
	switch (cb->type) {
	case AP_ITEM_PACKET_TYPE_UPDATE:
		item = ap_item_find(mod->ap_item, c, cb->item_id, 
			4, AP_ITEM_STATUS_INVENTORY, AP_ITEM_STATUS_EQUIP, 
			AP_ITEM_STATUS_SUB_INVENTORY, AP_ITEM_STATUS_BANK);
		if (!item)
			break;
		if (item->status == cb->status &&
			(cb->status == AP_ITEM_STATUS_INVENTORY ||
				cb->status == AP_ITEM_STATUS_SUB_INVENTORY ||
				cb->status == AP_ITEM_STATUS_BANK)) {
			/* Moving item within the same grid. */
			struct ap_grid * grid;
			struct ap_item * target;
			struct ap_item_grid_pos_in_packet * gridpos = 
				(cb->status == AP_ITEM_STATUS_BANK) ? cb->bank : cb->inventory;
			if (!gridpos)
				break;
			grid = ap_item_get_character_grid(mod->ap_item, c, item->status);
			assert(grid != NULL);
			target = ap_grid_get_object(grid, gridpos->layer,
				gridpos->row, gridpos->column);
			if (target) {
				/* Target inventory slot is occupied. 
				 * If applicable, we can merge items or 
				 * initiate item convert process. */
				if (item->tid == target->tid)
					trymerge(mod, c, item, target);
				else
					tryconvert(mod, c, item, target);
			}
			else {
				if (gridpos->layer == UINT16_MAX) {
					if (!ap_grid_get_empty(grid, 
							&gridpos->layer,
							&gridpos->row,
							&gridpos->column)) {
						break;
					}
				}
				else if (gridpos->row == UINT16_MAX ||
						gridpos->column == UINT16_MAX) {
					if (!ap_grid_get_empty_in_layer(grid, 
							gridpos->layer,
							&gridpos->row,
							&gridpos->column)) {
						break;
					}
				}
				if (!ap_grid_move_item(grid, item->grid_pos[0],
						item->grid_pos[1], item->grid_pos[2],
						gridpos->layer,
						gridpos->row,
						gridpos->column)) {
					break;
				}
				ap_item_set_grid_pos(item, gridpos->layer,
					gridpos->row, gridpos->column);
				ap_item_make_update_packet(mod->ap_item, item, 
					AP_ITEM_UPDATE_GRID_POS);
				as_player_send_packet(mod->as_player, c);
			}
		}
		else if (cb->status == AP_ITEM_STATUS_EQUIP &&
			(item->status == AP_ITEM_STATUS_INVENTORY ||
				item->status == AP_ITEM_STATUS_SUB_INVENTORY)) {
			/* Equiping an item. */
			struct ap_item * unequip[2] = { 0 };
			uint32_t count = 0;
			struct ap_grid * prevgrid;
			uint16_t prevpos[AP_ITEM_GRID_POS_COUNT];
			struct ap_grid * inven;
			uint32_t i;
			boolean result = TRUE;
			if (!ap_item_check_equip_restriction(mod->ap_item, item, c))
				break;
			count = ap_item_to_be_unequiped(mod->ap_item, c, item, unequip);
			inven = ap_item_get_character_grid(mod->ap_item, c, AP_ITEM_STATUS_INVENTORY);
			if (count > ap_grid_get_empty_count(inven)) {
				/* Not enough empty slots in inventory. */
				break;
			}
			/* Try to unequip current equipment. */
			for (i = 0; i < count; i++)
				result &= ap_item_unequip(mod->ap_item, c, unequip[i]);
			if (!result)
				break;
			prevgrid = ap_item_get_character_grid(mod->ap_item, c, item->status);
			memcpy(prevpos, item->grid_pos, sizeof(prevpos));
			if (!ap_item_equip(mod->ap_item, c, item, 0))
				break;
			/* Remove item from (sub)inventory. */
			ap_grid_set_empty(prevgrid, 
				prevpos[AP_ITEM_GRID_POS_TAB],
				prevpos[AP_ITEM_GRID_POS_ROW],
				prevpos[AP_ITEM_GRID_POS_COLUMN]);
			ap_character_update_factor(mod->ap_character, c, 0);
		}
		else if (item->status == AP_ITEM_STATUS_EQUIP &&
			(cb->status == AP_ITEM_STATUS_INVENTORY ||
				cb->status == AP_ITEM_STATUS_SUB_INVENTORY)) {
			/* Unequiping an item. */
			struct ap_item * other;
			if (item->equip_flags & AP_ITEM_EQUIP_BY_ITEM_IN_USE)
				break;
			other = ap_item_to_be_unequiped_together(mod->ap_item, c, item);
			if (other) {
				if (!ap_item_unequip(mod->ap_item, c, other))
					break;
			}
			if (!ap_item_unequip(mod->ap_item, c, item))
				break;
			ap_character_update_factor(mod->ap_character, c, 0);
		}
		else if (item->status == AP_ITEM_STATUS_BANK && 
			(cb->status == AP_ITEM_STATUS_INVENTORY ||
				cb->status == AP_ITEM_STATUS_SUB_INVENTORY)) {
			/* Moving item from bank to character inventory. */
			withdrawfrombank(mod, cb, c, item);
		}
		else if (cb->status == AP_ITEM_STATUS_BANK &&
			(item->status == AP_ITEM_STATUS_INVENTORY ||
			item->status == AP_ITEM_STATUS_SUB_INVENTORY)) {
			/* Moving item from character inventory to bank. */
			deposittobank(mod, cb, c, item);
		}
		else if ((cb->status == AP_ITEM_STATUS_INVENTORY || 
			cb->status == AP_ITEM_STATUS_SUB_INVENTORY) &&
			(item->status == AP_ITEM_STATUS_INVENTORY ||
			item->status == AP_ITEM_STATUS_SUB_INVENTORY)) {
			/* Moving item between character inventory and pet inventory. */
			movebetweensubinventory(mod, cb, c, item);
		}
		break;
	case AP_ITEM_PACKET_TYPE_USE_ITEM:
		item = ap_item_find(mod->ap_item, c, cb->item_id, 3,
			AP_ITEM_STATUS_INVENTORY,
			AP_ITEM_STATUS_CASH_INVENTORY,
			AP_ITEM_STATUS_SUB_INVENTORY);
		if (!item)
			break;
		if (!ap_item_check_use_restriction(mod->ap_item, item, c))
			break;
		assert(item->stack_count > 0);
		useitem(mod, c, item);
		break;
	case AP_ITEM_PACKET_TYPE_USE_RETURN_SCROLL: {
		struct ap_item_character * attachment = 
			ap_item_get_character(mod->ap_item, c);
		struct as_map_character * mapattachment;
		if (!attachment->is_return_scroll_enabled)
			break;
		if (ap_tick_get(mod->ap_tick) < c->combat_end_tick)
			break;
		mapattachment = as_map_get_character_ad(mod->as_map, c);
		if (mapattachment->region) {
			 const struct ap_map_region_template * temp = 
				 mapattachment->region->temp;
			if (temp->level_limit)
				break;
			if (temp->type.props.safety_type == AP_MAP_ST_DANGER &&
				temp->disabled_item_section != 1) {
				break;
			}
		}
		attachment->is_return_scroll_enabled = FALSE;
		ap_item_make_disable_return_scroll_packet(mod->ap_item, c->id);
		as_player_send_packet(mod->as_player, c);
		ap_event_teleport_start(mod->ap_event_teleport, c, &attachment->return_scroll_position);
		break;
	}
	case AP_ITEM_PACKET_TYPE_PICKUP_ITEM: {
		struct as_map_item_drop * drop;
		float distance;
		/* Prevent items from being picked-up while character is dead/invisible. */
		if (c->action_status != AP_CHARACTER_ACTION_STATUS_NORMAL)
			break;
		if (c->special_status & AP_CHARACTER_SPECIAL_STATUS_TRANSPARENT)
			break;
		drop = as_map_find_item_drop(mod->as_map, &c->pos, cb->item_id);
		if (!drop)
			break;
		item = drop->item;
		if (item->character_id != c->id && 
			ap_tick_get(mod->ap_tick) < drop->ownership_expire_tick) {
			break;
		}
		distance = au_distance2d(&c->pos, &item->position);
		if (distance > AP_ITEM_MAX_PICKUP_ITEM_DISTANCE) {
			vec2 dir = { item->position.x - c->pos.x, 
				item->position.z - c->pos.z };
			float len = distance - 
				0.90f * AP_ITEM_MAX_PICKUP_ITEM_DISTANCE;
			struct au_pos dst = { 0 };
			glm_vec2_normalize(dir);
			dst.x = c->pos.x + dir[0] * len;
			dst.y = c->pos.y;
			dst.z = c->pos.z + dir[1] * len;
			ap_character_set_movement(mod->ap_character, c, &dst, TRUE);
			break;
		}
		if (item->tid == AP_ITEM_GOLD_TEMPLATE_ID) {
			if (!as_character_deposit_gold(mod->as_character, c, item->stack_count))
				break;
		}
		else {
			if (!as_drop_item_distribute(mod->as_drop_item, c, item->tid, 
					item->stack_count)) {
				ap_item_make_pick_up_item_result_packet(mod->ap_item, item,
					AP_ITEM_PICK_UP_RESULT_FAIL);
				as_player_send_packet(mod->as_player, c);
				break;
			}
		}
		ap_item_make_pick_up_item_result_packet(mod->ap_item, item,
			AP_ITEM_PICK_UP_RESULT_SUCCESS);
		as_player_send_packet(mod->as_player, c);
		as_map_remove_item_drop(mod->as_map, drop);
		break;
	}
	case AP_ITEM_PACKET_TYPE_SPLIT_ITEM: {
		struct ap_grid * grid;
		struct ap_item * split;
		if (!cb->inventory)
			break;
		item = ap_item_find(mod->ap_item, c, cb->item_id, 
			2, AP_ITEM_STATUS_INVENTORY, 
			AP_ITEM_STATUS_SUB_INVENTORY);
		if (!item || cb->stack_count >= item->stack_count)
			break;
		grid = ap_item_get_character_grid(mod->ap_item, c, item->status);
		if (!grid)
			break;
		if (!ap_grid_is_free(grid, cb->inventory->layer,
				cb->inventory->row, cb->inventory->column)) {
			break;
		}
		split = as_item_try_generate(mod->as_item, c, item->status, item->tid);
		if (!split)
			break;
		ap_item_set_stack_count(split, cb->stack_count);
		ap_grid_set_empty(grid, 
			split->grid_pos[AP_ITEM_GRID_POS_TAB],
			split->grid_pos[AP_ITEM_GRID_POS_ROW],
			split->grid_pos[AP_ITEM_GRID_POS_COLUMN]);
		ap_grid_add_item(grid, cb->inventory->layer, 
			cb->inventory->row, cb->inventory->column,
			AP_GRID_ITEM_TYPE_ITEM, split->id, split->tid, split);
		split->grid_pos[AP_ITEM_GRID_POS_TAB] = 
			cb->inventory->layer;
		split->grid_pos[AP_ITEM_GRID_POS_ROW] = 
			cb->inventory->row;
		split->grid_pos[AP_ITEM_GRID_POS_COLUMN] = 
			cb->inventory->column;
		ap_item_decrease_stack_count(item, cb->stack_count);
		ap_item_make_add_packet(mod->ap_item, split);
		as_player_send_packet(mod->as_player, c);
		ap_item_make_update_packet(mod->ap_item, item, 
			AP_ITEM_UPDATE_STACK_COUNT);
		as_player_send_packet(mod->as_player, c);
		break;
	}
	case AP_ITEM_PACKET_TYPE_REQUEST_DESTROY_ITEM:
		item = ap_item_find(mod->ap_item, c, cb->item_id, 3, 
			AP_ITEM_STATUS_INVENTORY, 
			AP_ITEM_STATUS_SUB_INVENTORY,
			AP_ITEM_STATUS_CASH_INVENTORY);
		if (!item || item->in_use)
			break;
		as_item_delete(mod->as_item, c, item);
		break;
	case AP_ITEM_PACKET_TYPE_UNUSE_ITEM:
		item = ap_item_find(mod->ap_item, c, cb->item_id, 1, 
			AP_ITEM_STATUS_CASH_INVENTORY);
		if (!item || !item->in_use || !item->temp->can_stop_using)
			break;
		item->in_use = FALSE;
		item->update_flags |= AP_ITEM_UPDATE_IN_USE;
		item->recent_turn_off_tick = ap_tick_get(mod->ap_tick);
		if (item->temp->type == AP_ITEM_TYPE_USABLE &&
			item->temp->usable.use_skill_tid) {
			as_skill_remove_buff_with_tid(mod->as_skill, c, 
				item->temp->usable.use_skill_tid);
		}
		/** \todo If item is a pet, unsummon pet. */
		switch (item->temp->usable.usable_type) {
		case AP_ITEM_USABLE_TYPE_AVATAR:
			unequipavatar(mod, c, item);
			break;
		}
		ap_item_make_update_use_time_packet(mod->ap_item, item);
		as_player_send_packet(mod->as_player, c);
		break;
	}
	return TRUE;
}

static boolean cbitemequip(
	struct as_item_process_module * mod,
	struct ap_item_cb_equip * cb)
{
	struct ap_character * character = cb->character;
	struct ap_item * item = cb->item;
	if (CHECK_BIT(item->equip_flags, AP_ITEM_EQUIP_SILENT))
		return TRUE;
	if (CHECK_BIT(item->equip_flags, AP_ITEM_EQUIP_BY_ITEM_IN_USE)) {
		ap_item_make_add_packet(mod->ap_item, item);
		as_map_broadcast(mod->as_map, character);
	}
	else {
		ap_item_make_update_packet(mod->ap_item, item, 
			AP_ITEM_UPDATE_STATUS | AP_ITEM_UPDATE_GRID_POS);
		as_player_send_packet(mod->as_player, character);
		ap_item_make_add_packet(mod->ap_item, item);
		as_map_broadcast_with_exception(mod->as_map, character, character);
		ap_item_convert_make_add_packet(mod->ap_item_convert, item);
		as_map_broadcast_with_exception(mod->as_map, character, character);
	}
	return TRUE;
}

static boolean cbitemunequip(
	struct as_item_process_module * mod,
	struct ap_item_cb_unequip * cb)
{
	struct ap_character * character = cb->character;
	struct ap_item * item = cb->item;
	if (CHECK_BIT(item->equip_flags, AP_ITEM_EQUIP_BY_ITEM_IN_USE)) {
		ap_item_make_remove_packet(mod->ap_item, item);
		as_map_broadcast(mod->as_map, character);
	}
	else {
		ap_item_make_update_packet(mod->ap_item, item, 
			AP_ITEM_UPDATE_STATUS | AP_ITEM_UPDATE_GRID_POS);
		as_map_broadcast(mod->as_map, character);
	}
	return TRUE;
}

static boolean onregister(
	struct as_item_process_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_character, AP_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_chat, AP_CHAT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_teleport, AP_EVENT_TELEPORT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_item, AP_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_item_convert, AP_ITEM_CONVERT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_random, AP_RANDOM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_pvp, AP_PVP_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_skill, AP_SKILL_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_system_message, AP_SYSTEM_MESSAGE_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_tick, AP_TICK_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_character, AS_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_drop_item, AS_DROP_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_event_binding, AS_EVENT_BINDING_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_item, AS_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_map, AS_MAP_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_player, AS_PLAYER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_skill, AS_SKILL_MODULE_NAME);
	ap_character_add_callback(mod->ap_character, AP_CHARACTER_CB_PROCESS, mod, cbcharprocess);
	ap_skill_add_callback(mod->ap_skill, AP_SKILL_CB_REMOVE_BUFF, mod, cbremovebuff);
	ap_item_add_callback(mod->ap_item, AP_ITEM_CB_RECEIVE, mod, cbreceive);
	ap_item_add_callback(mod->ap_item, AP_ITEM_CB_EQUIP, mod, cbitemequip);
	ap_item_add_callback(mod->ap_item, AP_ITEM_CB_UNEQUIP, mod, cbitemunequip);
	ap_chat_add_command(mod->ap_chat, "/create", mod, cbchatcreate);
	as_character_add_callback(mod->as_character, AS_CHARACTER_CB_LOAD, mod, cbloadchar);
	return TRUE;
}

static void onshutdown(struct as_item_process_module * mod)
{
	vec_free(mod->list);
}

struct as_item_process_module * as_item_process_create_module()
{
	struct as_item_process_module * mod = ap_module_instance_new(AS_ITEM_PROCESS_MODULE_NAME, 
		sizeof(*mod), onregister, NULL, NULL, onshutdown);
	mod->list = vec_new_reserved(sizeof(*mod->list), 128);
	return mod;
}
