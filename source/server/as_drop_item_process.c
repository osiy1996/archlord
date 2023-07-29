#include "server/as_drop_item_process.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/vector.h"

#include "public/ap_ai2.h"
#include "public/ap_chat.h"
#include "public/ap_config.h"
#include "public/ap_drop_item.h"
#include "public/ap_item.h"
#include "public/ap_item_convert.h"
#include "public/ap_module.h"
#include "public/ap_party.h"
#include "public/ap_party_item.h"
#include "public/ap_random.h"
#include "public/ap_skill.h"
#include "public/ap_tick.h"

#include "server/as_drop_item.h"
#include "server/as_item.h"
#include "server/as_map.h"
#include "server/as_party.h"
#include "server/as_player.h"

#include <assert.h>
#include <stdlib.h>

#define DROP_PROCESS_DELAY_MS 800
#define DROP_EXPIRE_DELAY_MS 45000

struct as_drop_item_process_module {
	struct ap_module_instance instance;
	struct ap_ai2_module * ap_ai2;
	struct ap_character_module * ap_character;
	struct ap_chat_module * ap_chat;
	struct ap_config_module * ap_config;
	struct ap_drop_item_module * ap_drop_item;
	struct ap_item_module * ap_item;
	struct ap_item_convert_module * ap_item_convert;
	struct ap_party_module * ap_party;
	struct ap_party_item_module * ap_party_item;
	struct ap_random_module * ap_random;
	struct ap_skill_module * ap_skill;
	struct ap_tick_module * ap_tick;
	struct as_character_module * as_character;
	struct as_drop_item_module * as_drop_item;
	struct as_item_module * as_item;
	struct as_map_module * as_map;
	struct as_party_module * as_party;
	struct as_player_module * as_player;
	struct ap_character ** list;
	struct ap_item * gold;
	uint32_t * drop_index_list;
	float drop_rate;
	float rare_drop_rate;
	float unique_accessory_drop_rate;
	float gold_drop_rate;
};

static void randomizedropposition(
	struct as_drop_item_process_module * mod,
	const struct au_pos * center,
	struct au_pos * position)
{
	position->x = center->x + ap_random_float(mod->ap_random, -150.0f, 150.0f);
	position->y = center->y;
	position->z = center->z + ap_random_float(mod->ap_random, -150.0f, 150.0f);
}

static void makedrop(
	struct as_drop_item_process_module * mod,
	struct ap_character * character,
	struct ap_character * owner,
	uint32_t item_tid)
{
	struct ap_item * item = as_item_generate(mod->as_item, item_tid);
	item->character_id = owner->id;
	randomizedropposition(mod, &character->pos, &item->position);
	item->status = AP_ITEM_STATUS_FIELD;
	as_map_add_item_drop(mod->as_map, item, 45000);
}

static inline void distributegold(
	struct as_drop_item_process_module * mod,
	struct ap_character * character,
	struct ap_character * owner,
	struct ap_skill_character * skill_attachment,
	struct ap_party * party)
{
	uint32_t gold;
	boolean isoutoflevelrange = 
		ap_character_is_target_out_of_level_range(owner, character);
	if (FALSE && isoutoflevelrange) {
		/* \todo Branch is disabled for testing purposes. */
		gold = 1;
	}
	else {
		uint32_t diff = character->temp->gold_max - character->temp->gold_min;
		assert(character->temp->gold_max >= character->temp->gold_min);
		gold = character->temp->gold_min + ap_random_get(mod->ap_random, diff);
		gold = (uint32_t)(gold * mod->gold_drop_rate *
			(100.0f + skill_attachment->factor_effect_arg.bonus_gold_rate) / 100.0f);
	}
	mod->gold->stack_count = gold;
	randomizedropposition(mod, &character->pos, &mod->gold->position);
	if (0) {
		/* \todo Check if pet is available and auto-collect is turned-on. */
		as_character_deposit_gold(mod->as_character, owner, gold);
		ap_item_make_pick_up_item_result_packet(mod->ap_item, mod->gold,
			AP_ITEM_PICK_UP_RESULT_SUCCESS);
		as_player_send_packet(mod->as_player, owner);
	}
	else if (!party || party->item_division_type == AP_PARTY_ITEM_DIVISION_FREE) {
		struct ap_item * golditem = ap_item_create(mod->ap_item, 
			AP_ITEM_GOLD_TEMPLATE_ID);
		golditem->character_id = owner->id;
		golditem->stack_count = gold;
		golditem->status = AP_ITEM_STATUS_FIELD;
		randomizedropposition(mod, &character->pos, &golditem->position);
		as_map_add_item_drop(mod->as_map, golditem, DROP_EXPIRE_DELAY_MS);
	}
	else {
		/* \todo Round-robin and contribution based distribution of gold. */
		as_character_deposit_gold(mod->as_character, owner, gold);
		ap_party_item_make_packet(mod->ap_party_item, mod->gold);
		as_party_send_packet(mod->as_party, party);
	}
}

static void shuffledropgroup(
	struct as_drop_item_process_module * mod,
	uint32_t drop_count)
{
	uint32_t i;
	vec_clear(mod->drop_index_list);
	for (i = 0; i < drop_count; i++)
		vec_push_back(&mod->drop_index_list, &i);
	if (drop_count > 1) {
		uint32_t i;
		for (i = 0; i < drop_count - 1; i++) {
			uint32_t j = i + ap_random_get(mod->ap_random, drop_count - i);
			uint32_t t = mod->drop_index_list[j];
			mod->drop_index_list[j] = mod->drop_index_list[i];
			mod->drop_index_list[i] = t;
		}
	}
}

static boolean cbcharprocessmonster(
	struct as_drop_item_process_module * mod,
	struct ap_character_cb_process_monster * data)
{
	struct ap_character * character = data->character;
	struct ap_drop_item_character_template_attachment * attachment;
	struct as_drop_item_character_attachment * attachmentsrv;
	struct ap_character * owner;
	struct ap_party * party = NULL;
	const struct ap_character_template * temp = character->temp;
	struct ap_skill_character * skillattachment;
	struct as_character_db * characterdb;
	float bonusdroprate = 0.0f;
	float bonusraredroprate = 0.0f;
	uint32_t i;
	uint32_t count;
	const uint32_t * dropranks;
	boolean monsterdropsuniqueaccessory = FALSE;
	attachmentsrv = as_drop_item_get_character_attachment(mod->as_drop_item, character);
	if (!attachmentsrv->process_drop_tick || data->tick < attachmentsrv->process_drop_tick)
		return TRUE;
	attachmentsrv->process_drop_tick = 0;
	attachment = ap_drop_item_get_character_template_attachment(mod->ap_drop_item, 
		character->temp);
	dropranks = ap_drop_item_get_drop_rank_rates(mod->ap_drop_item, 
		attachment->drop_rank);
	if (!dropranks)
		return TRUE;
	owner = as_map_get_character(mod->as_map, attachmentsrv->drop_owner_character_id);
	if (!owner)
		return TRUE;
	party = ap_party_get_character_party(mod->ap_party, owner);
	skillattachment = ap_skill_get_character(mod->ap_skill, owner);
	distributegold(mod, character, owner, skillattachment, party);
	if (ap_character_is_target_out_of_level_range(owner, character) && FALSE) {
		/* \todo Branch is disable for testing purposes. */
		return TRUE;
	}
	bonusdroprate = (float)skillattachment->factor_effect_arg.bonus_drop_rate;
	bonusraredroprate = (float)skillattachment->factor_effect_arg.bonus_rare_drop_rate;
	if (0) {
		/* \todo Region based drop bonuses. */
	}
	for (i = 0; i <= AP_DROP_ITEM_MAX_DROP_GROUP_ID; i++) {
		float bonusdrop = ((100.f + bonusdroprate) / 100.f);
		uint32_t rate = 0;
		int bonusrate = ap_drop_item_get_drop_group_bonus(mod->ap_drop_item, i, 
			ap_character_get_level(owner));
		uint32_t totalrate = 0;
		uint32_t droprank = UINT32_MAX;
		uint32_t dropcount = 0;
		uint32_t j;
		const struct ap_drop_item_drop_group * dropgroup = &attachment->drop_groups[i];
		const struct ap_drop_item * dropitem = NULL;
		rate = (uint32_t)((float)ap_drop_item_get_drop_group_rate(
			mod->ap_drop_item, i) * mod->drop_rate * bonusdrop);
		if (ap_drop_item_is_drop_group_affected_by_drop_meditation(mod->ap_drop_item, i))
			rate = (uint32_t)(rate * attachment->drop_mediation);
		if ((int)rate + bonusrate > 0)
			rate += bonusrate;
		else
			rate = 0;
		if (ap_random_get(mod->ap_random, 100000) >= rate)
			continue;
		rate = ap_random_get(mod->ap_random, 100);
		for (j = 1; j < AP_DROP_ITEM_MAX_DROP_RANK; j++) {
			totalrate += dropranks[j];
			if (vec_count(dropgroup->items[j])) {
				droprank = j;
				if (rate < totalrate)
					break;
			}
		}
		if (droprank == UINT32_MAX)
			continue;
		dropcount = vec_count(dropgroup->items[droprank]);
		if (!dropgroup || !dropcount)
			continue;
		totalrate = 0;
		rate = ap_random_get(mod->ap_random, dropgroup->rank_total_rate[droprank]);
		shuffledropgroup(mod, dropcount);
		for (j = 0; j < dropcount; j++) {
			uint32_t index = mod->drop_index_list[j];
			const struct ap_drop_item * drop = &dropgroup->items[droprank][index];
			uint32_t classrate = drop->drop_rate;
			if (drop->temp->type== AP_ITEM_TYPE_EQUIP && 
				(drop->temp->factor_restrict.char_type.cs & (1u << owner->factor.char_type.cs))) {
				classrate *= 3;
			}
			if (rate <= totalrate + classrate) {
				dropitem = drop;
				break;
			}
			else {
				totalrate += drop->drop_rate;
			}
		}
		if (!dropitem || (dropitem->temp->is_villain_only && 
				owner->factor.char_status.murderer < AP_CHARACTER_MURDERER_LEVEL1_POINT)) {
			continue;
		}
		makedrop(mod, character, owner, dropitem->tid);
	}
	characterdb = as_character_get(mod->as_character, owner)->db;
	count = vec_count(attachment->drop_items);
	if (count) {
		/* Drop table is assumed to be sorted by ascending rate.
		 * First go through items that must be dropped. */
		int index;
		for (index = count - 1; index >= 0; index--) {
			const struct ap_drop_item * dropitem = &attachment->drop_items[index];
			uint32_t stackcount;
			if (dropitem->drop_rate != 100000)
				break;
			if (dropitem->temp->is_villain_only && 
				owner->factor.char_status.murderer < AP_CHARACTER_MURDERER_LEVEL1_POINT) {
				continue;
			}
			stackcount = ap_random_between(mod->ap_random, 
				dropitem->min_stack_count, dropitem->max_stack_count);
			for (i = 0; i < stackcount; i++)
				makedrop(mod, character, owner, dropitem->tid);
		}
		for (index = count - 1; index >= 0; index--) {
			const struct ap_drop_item * dropitem = &attachment->drop_items[index];
			uint32_t stackcount;
			uint32_t rate;
			struct ap_drop_item_template_attachment * dropattachment;
			if (dropitem->drop_rate == 100000)
				continue;
			dropattachment = ap_drop_item_get_item_template_attachment(mod->ap_drop_item, 
				dropitem->temp);
			if (dropattachment->is_droppable_unique_accessory) {
				float bonusrate = bonusraredroprate;
				if (characterdb)
					bonusrate += characterdb->unique_acc_drop_bonus;
				rate = (uint32_t)(dropitem->drop_rate * mod->unique_accessory_drop_rate * 
					(100.0f + bonusrate) / 100.0f);
				monsterdropsuniqueaccessory = TRUE;
			}
			else {
				rate = (uint32_t)(dropitem->drop_rate * mod->rare_drop_rate * 
					(100.0f + bonusraredroprate) / 100.0f);
			}
			if (ap_random_get(mod->ap_random, 100000) >= rate)
				continue;
			if (dropitem->temp->is_villain_only && 
				owner->factor.char_status.murderer < AP_CHARACTER_MURDERER_LEVEL1_POINT) {
				continue;
			}
			/* Once a unique accessory drops, clear accumulated bonus. */
			if (dropattachment->is_droppable_unique_accessory && characterdb)
				characterdb->unique_acc_drop_bonus = 0.0f;
			stackcount = ap_random_between(mod->ap_random, 
				dropitem->min_stack_count, dropitem->max_stack_count);
			for (i = 0; i < stackcount; i++)
				makedrop(mod, character, owner, dropitem->tid);
		}
	}
	if (monsterdropsuniqueaccessory && characterdb)
		characterdb->unique_acc_drop_bonus += 0.1f;
	return TRUE;
}

static boolean cbchardeath(
	struct as_drop_item_process_module * mod,
	struct ap_character_cb_death * data)
{
	struct ap_character * character = data->character;
	struct ap_drop_item_character_template_attachment * attachment;
	struct as_drop_item_character_attachment * srvattachment;
	struct ap_ai2_character_attachment * aiattachment;
	if (!(character->char_type & AP_CHARACTER_TYPE_MONSTER))
		return TRUE;
	aiattachment = ap_ai2_get_character_attachment(mod->ap_ai2, character);
	/* \todo Distribute loot based on damage contribution? */
	if (!data->killer)
		return TRUE;
	attachment = ap_drop_item_get_character_template_attachment(mod->ap_drop_item, 
		character->temp);
	if (!attachment->drop_rank)
		return TRUE;
	srvattachment = as_drop_item_get_character_attachment(mod->as_drop_item, character);
	srvattachment->process_drop_tick = ap_tick_get(mod->ap_tick) + 
		DROP_PROCESS_DELAY_MS;
	srvattachment->drop_owner_character_id = data->killer->id;
	return TRUE;
}

static boolean onregister(
	struct as_drop_item_process_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_ai2, AP_AI2_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_character, AP_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_chat, AP_CHAT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_config, AP_CONFIG_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_drop_item, AP_DROP_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_item, AP_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_item_convert, AP_ITEM_CONVERT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_party, AP_PARTY_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_party_item, AP_PARTY_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_random, AP_RANDOM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_skill, AP_SKILL_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_tick, AP_TICK_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_character, AS_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_drop_item, AS_DROP_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_item, AS_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_map, AS_MAP_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_party, AS_PARTY_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_player, AS_PLAYER_MODULE_NAME);
	ap_character_add_callback(mod->ap_character, AP_CHARACTER_CB_PROCESS_MONSTER, mod, cbcharprocessmonster);
	ap_character_add_callback(mod->ap_character, AP_CHARACTER_CB_DEATH, mod, cbchardeath);
	mod->drop_rate = strtof(ap_config_get(mod->ap_config, "DropRate"), NULL);
	mod->rare_drop_rate = strtof(ap_config_get(mod->ap_config, "RareDropRate"), NULL);
	mod->unique_accessory_drop_rate = strtof(ap_config_get(mod->ap_config, "UniqueAccessoryDropRate"), NULL);
	mod->gold_drop_rate = strtof(ap_config_get(mod->ap_config, "GoldDropRate"), NULL);
	return TRUE;
}

static void onclose(struct as_drop_item_process_module * mod)
{
	ap_item_free(mod->ap_item, mod->gold);
	mod->gold = NULL;
}

static void onshutdown(struct as_drop_item_process_module * mod)
{
	vec_free(mod->list);
	vec_free(mod->drop_index_list);
}

struct as_drop_item_process_module * as_drop_item_process_create_module()
{
	struct as_drop_item_process_module * mod = ap_module_instance_new(AS_DROP_ITEM_PROCESS_MODULE_NAME, 
		sizeof(*mod), onregister, NULL, onclose, onshutdown);
	mod->list = vec_new_reserved(sizeof(*mod->list), 128);
	mod->drop_index_list = vec_new_reserved(sizeof(*mod->drop_index_list), 128);
	return mod;
}

boolean as_drop_item_process_create_gold(struct as_drop_item_process_module * mod)
{
	struct ap_item * gold = ap_item_create(mod->ap_item, AP_ITEM_GOLD_TEMPLATE_ID);
	if (!gold) {
		ERROR("Failed to create gold item.");
		return FALSE;
	}
	gold->status = AP_ITEM_STATUS_FIELD;
	mod->gold = gold;
	return TRUE;
}
