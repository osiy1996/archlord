#include "server/as_event_skill_master_process.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/vector.h"

#include "public/ap_chat.h"
#include "public/ap_event_skill_master.h"
#include "public/ap_skill.h"
#include "public/ap_tick.h"

#include "server/as_map.h"
#include "server/as_player.h"
#include "server/as_skill.h"

#include <assert.h>

struct as_event_skill_master_process_module {
	struct ap_module_instance instance;
	struct ap_character_module * ap_character;
	struct ap_chat_module * ap_chat;
	struct ap_event_manager_module * ap_event_manager;
	struct ap_event_skill_master_module * ap_event_skill_master;
	struct ap_skill_module * ap_skill;
	struct ap_tick_module * ap_tick;
	struct as_character_module * as_character;
	struct as_map_module * as_map;
	struct as_player_module * as_player;
	struct as_skill_module * as_skill;
	struct ap_character ** list;
};

static struct ap_event_manager_event * findevent(
	struct as_event_skill_master_process_module * mod,
	struct ap_event_manager_base_packet * packet)
{
	struct ap_character * npc;
	if (!packet || packet->source_type != AP_BASE_TYPE_CHARACTER)
		return NULL;
	npc = as_map_get_npc(mod->as_map, packet->source_id);
	if (!npc)
		return NULL;
	return ap_event_manager_get_function(mod->ap_event_manager, npc, 
		AP_EVENT_MANAGER_FUNCTION_SKILLMASTER);
}

static boolean cbreceive(
	struct as_event_skill_master_process_module * mod,
	struct ap_event_skill_master_cb_receive * cb)
{
	struct ap_character * c = cb->user_data;
	struct ap_character * npc = NULL;
	struct ap_skill_character * sc = ap_skill_get_character(mod->ap_skill, c);
	switch (cb->type) {
	case AP_EVENT_SKILL_MASTER_PACKET_TYPE_LEARN_SKILL: {
		struct ap_event_manager_event * e = findevent(mod, cb->event);
		struct ap_skill_template * temp;
		uint32_t req;
		const float * factors;
		struct ap_skill * skill;
		uint32_t index;
		if (!e)
			break;
		if (sc->skill_count >= AP_SKILL_MAX_SKILL_OWN)
			break;
		temp = ap_skill_get_template(mod->ap_skill, cb->skill_tid);
		req = ap_skill_get_required_class(c->factor.char_type.race, 
			c->factor.char_type.cs);
		if (!temp || 
			temp->is_auto_attack ||
			req >= AP_SKILL_CONST_COUNT ||
			!temp->used_const_factor[1][req]) {
			ap_event_skill_master_make_packet(mod->ap_event_skill_master,
				AP_EVENT_SKILL_MASTER_PACKET_TYPE_LEARN_RESULT,
				e, &c->id, NULL, NULL, 
				AP_EVENT_SKILL_MASTER_LEARN_RESULT_NOT_LEARNABLE_CLASS, 
				NULL, NULL);
			as_player_send_packet(mod->as_player, c);
			break;
		}
		factors = temp->used_const_factor[1];
		if (c->inventory_gold < (uint64_t)factors[AP_SKILL_CONST_SKILL_COST])
			break;
		if (ap_character_get_absolute_level(c) < factors[AP_SKILL_CONST_REQUIRE_LEVEL]) {
			ap_event_skill_master_make_packet(mod->ap_event_skill_master,
				AP_EVENT_SKILL_MASTER_PACKET_TYPE_LEARN_RESULT,
				e, &c->id, NULL, NULL, 
				AP_EVENT_SKILL_MASTER_LEARN_RESULT_LOW_LEVEL, 
				NULL, NULL);
			as_player_send_packet(mod->as_player, c);
			break;
		}
		if (c->factor.dirt.skill_point < factors[AP_SKILL_CONST_REQUIRE_POINT]) {
			ap_event_skill_master_make_packet(mod->ap_event_skill_master,
				AP_EVENT_SKILL_MASTER_PACKET_TYPE_LEARN_RESULT,
				e, &c->id, NULL, NULL, 
				AP_EVENT_SKILL_MASTER_LEARN_RESULT_NOT_ENOUGH_SKILLPOINT, 
				NULL, NULL);
			as_player_send_packet(mod->as_player, c);
			break;
		}
		if (c->factor.dirt.heroic_point < factors[AP_SKILL_CONST_REQUIRE_HEROIC_POINT]) {
			ap_event_skill_master_make_packet(mod->ap_event_skill_master,
				AP_EVENT_SKILL_MASTER_PACKET_TYPE_LEARN_RESULT,
				e, &c->id, NULL, NULL, 
				AP_EVENT_SKILL_MASTER_LEARN_RESULT_NOT_ENOUGH_HEROICPOINT, 
				NULL, NULL);
			as_player_send_packet(mod->as_player, c);
			break;
		}
		/** \todo Check skill mastery requirements. */
		as_character_consume_gold(mod->as_character, c, 
			(uint64_t)factors[AP_SKILL_CONST_SKILL_COST]);
		c->factor.dirt.skill_point -= 
			(int32_t)factors[AP_SKILL_CONST_REQUIRE_POINT];
		c->factor.dirt.heroic_point -= 
			(int32_t)factors[AP_SKILL_CONST_REQUIRE_HEROIC_POINT];
		skill = ap_skill_new(mod->ap_skill);
		skill->tid = temp->id;
		skill->factor.dirt.skill_level = 1;
		skill->factor.dirt.skill_point = 1;
		skill->temp = temp;
		skill->status = AP_SKILL_STATUS_NOT_CAST;
		skill->is_temporary = FALSE;
		index = sc->skill_count++;
		sc->skill_id[index] = skill->id;
		sc->skill[index] = skill;
		ap_character_update(mod->ap_character, c, AP_FACTORS_BIT_SKILL_POINT | 
			AP_FACTORS_BIT_HEROIC_POINT, TRUE);
		ap_skill_make_add_packet(mod->ap_skill, skill, c);
		as_player_send_packet(mod->as_player, c);
		ap_event_skill_master_make_packet(mod->ap_event_skill_master,
			AP_EVENT_SKILL_MASTER_PACKET_TYPE_LEARN_RESULT,
			e, &c->id, NULL, NULL, 
			AP_EVENT_SKILL_MASTER_LEARN_RESULT_SUCCESS, 
			NULL, NULL);
		as_player_send_packet(mod->as_player, c);
		if (temp->attribute & AP_SKILL_ATTRIBUTE_PASSIVE)
			as_skill_add_passive_buff(mod->as_skill, c, skill);
		break;
	}
	case AP_EVENT_SKILL_MASTER_PACKET_TYPE_REQUEST_UPGRADE: {
		struct ap_event_manager_event * e = findevent(mod, cb->event);
		struct ap_skill_template * temp;
		const float * factors;
		struct ap_skill * skill;
		uint32_t level;
		if (!e)
			break;
		skill = ap_skill_find(mod->ap_skill, c, cb->skill_id);
		if (!skill)
			break;
		level = ap_skill_get_level(skill);
		if (level + 1 >= AP_SKILL_MAX_SKILL_CAP)
			break;
		temp = skill->temp;
		if (!temp->available_const_factor[level + 1])
			break;
		if (temp->is_auto_attack)
			break;
		if (!(temp->attribute & AP_SKILL_ATTRIBUTE_CAN_UPGRADE))
			break;
		factors = temp->used_const_factor[level + 1];
		if (!factors[AP_SKILL_CONST_SKILL_UPGRADE_COST])
			break;
		if (c->inventory_gold < (uint64_t)factors[AP_SKILL_CONST_SKILL_UPGRADE_COST])
			break;
		if (ap_character_get_absolute_level(c) < factors[AP_SKILL_CONST_REQUIRE_LEVEL])
			break;
		if (c->factor.dirt.skill_point < factors[AP_SKILL_CONST_REQUIRE_POINT])
			break;
		if (c->factor.dirt.heroic_point < factors[AP_SKILL_CONST_REQUIRE_HEROIC_POINT])
			break;
		as_character_consume_gold(mod->as_character, c, 
			(uint64_t)factors[AP_SKILL_CONST_SKILL_UPGRADE_COST]);
		c->factor.dirt.skill_point -= 
			(int32_t)factors[AP_SKILL_CONST_REQUIRE_POINT];
		c->factor.dirt.heroic_point -= 
			(int32_t)factors[AP_SKILL_CONST_REQUIRE_HEROIC_POINT];
		skill->factor.dirt.skill_level++;
		skill->factor.dirt.skill_point++;
		ap_character_update(mod->ap_character, c, AP_FACTORS_BIT_SKILL_POINT | 
			AP_FACTORS_BIT_HEROIC_POINT, TRUE);
		ap_skill_make_update_packet(mod->ap_skill, skill, c);
		as_player_send_packet(mod->as_player, c);
		ap_event_skill_master_make_packet(mod->ap_event_skill_master,
			AP_EVENT_SKILL_MASTER_PACKET_TYPE_RESPONSE_UPGRADE,
			e, &c->id, &skill->id, NULL, 0, NULL, NULL);
		as_player_send_packet(mod->as_player, c);
		if (temp->attribute & AP_SKILL_ATTRIBUTE_PASSIVE) {
			uint32_t modifiedlevel = ap_skill_get_modified_level(mod->ap_skill,
				c, skill);
			as_skill_remove_buff_with_tid(mod->as_skill, c, skill->tid);
			as_skill_add_passive_buff(mod->as_skill, c, skill);
		}
		break;
	}
	case AP_EVENT_SKILL_MASTER_PACKET_TYPE_REQUEST_EVENT: {
		struct ap_event_manager_event * e;
		boolean resetconsumed = FALSE;
		e = findevent(mod, cb->event);
		if (!e)
			break;
		ap_event_skill_master_make_packet(mod->ap_event_skill_master,
			AP_EVENT_SKILL_MASTER_PACKET_TYPE_RESPONSE_EVENT,
			e, &c->id, NULL, NULL, 
			AP_EVENT_SKILL_MASTER_EVENT_REQUEST_RESULT_SUCCESS, 
			NULL, &resetconsumed);
		as_player_send_packet(mod->as_player, c);
		break;
	}
	}
	return TRUE;
}

static void cbchataddskillpoint(
	struct as_event_skill_master_process_module * mod,
	struct ap_character * c,
	uint32_t argc,
	const char * const * argv)
{
	uint32_t count = 1;
	if (argc > 0) {
		count = strtoul(argv[0], NULL, 10);
		if (count > 100)
			count = 100;
	}
	c->factor.dirt.skill_point += count;
	ap_character_update(mod->ap_character, c, AP_FACTORS_BIT_SKILL_POINT, TRUE);
}

static boolean onregister(
	struct as_event_skill_master_process_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_character, AP_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_chat, AP_CHAT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_manager, AP_EVENT_MANAGER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_event_skill_master, AP_EVENT_SKILL_MASTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_skill, AP_SKILL_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_tick, AP_TICK_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_character, AS_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_map, AS_MAP_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_player, AS_PLAYER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_skill, AS_SKILL_MODULE_NAME);
	ap_event_skill_master_add_callback(mod->ap_event_skill_master,
		AP_EVENT_SKILL_MASTER_CB_RECEIVE, mod, cbreceive);
	ap_chat_add_command(mod->ap_chat, "/addskillpoint", mod, cbchataddskillpoint);
	return TRUE;
}

static void onshutdown(struct as_event_skill_master_process_module * mod)
{
	vec_free(mod->list);
}

struct as_event_skill_master_process_module * as_event_skill_master_process_create_module()
{
	struct as_event_skill_master_process_module * mod = ap_module_instance_new(AS_EVENT_SKILL_MASTER_PROCESS_MODULE_NAME, 
		sizeof(*mod), onregister, NULL, NULL, onshutdown);
	mod->list = vec_new_reserved(sizeof(*mod->list), 128);
	return mod;
}
