#include "server/as_party_process.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"
#include "core/vector.h"

#include "public/ap_chat.h"
#include "public/ap_module.h"
#include "public/ap_optimized_packet2.h"
#include "public/ap_party.h"
#include "public/ap_tick.h"

#include "server/as_map.h"
#include "server/as_party.h"
#include "server/as_player.h"

struct as_party_process_module {
	struct ap_module_instance instance;
	struct ap_character_module * ap_character;
	struct ap_chat_module * ap_chat;
	struct ap_optimized_packet2_module * ap_optimized_packet2;
	struct ap_party_module * ap_party;
	struct ap_tick_module * ap_tick;
	struct as_map_module * as_map;
	struct as_party_module * as_party;
	struct as_player_module * as_player;
};

static boolean cbreceive(
	struct as_party_process_module * mod,
	struct ap_party_cb_receive * cb)
{
	struct ap_character * c = cb->user_data;
	struct ap_party * party = NULL;
	switch (cb->type) {
	case AP_PARTY_PACKET_UPDATE_EXP_TYPE: {
		struct ap_party * party;
		party = ap_party_get_character_attachment(mod->ap_party, c)->party;
		if (!party || party->members[AP_PARTY_LEADER_INDEX] != c)
			break;
		if (party->members[AP_PARTY_LEADER_INDEX] != c)
			break;
		switch (cb->exp_type) {
		case AP_PARTY_EXP_TYPE_BY_DAMAGE:
		case AP_PARTY_EXP_TYPE_BY_LEVEL:
			party->exp_type = cb->exp_type;
			ap_party_make_update_exp_packet(mod->ap_party, party);
			as_party_send_packet(mod->as_party, party);
			break;
		}
		break;
	}
	case AP_PARTY_PACKET_UPDATE_ITEM_DIVISION: {
		struct ap_party * party;
		party = ap_party_get_character_attachment(mod->ap_party, c)->party;
		if (!party || party->members[AP_PARTY_LEADER_INDEX] != c)
			break;
		if (party->members[AP_PARTY_LEADER_INDEX] != c)
			break;
		switch (cb->item_division_type) {
		case AP_PARTY_ITEM_DIVISION_SEQUENCE:
		case AP_PARTY_ITEM_DIVISION_FREE:
		case AP_PARTY_ITEM_DIVISION_DAMAGE:
			party->item_division_type = cb->item_division_type;
			ap_party_make_update_item_packet(mod->ap_party, party);
			as_party_send_packet(mod->as_party, party);
			break;
		}
		break;
	}
	}
	return TRUE;
}

static boolean cbreceivemanagement(
	struct as_party_process_module * mod,
	struct ap_party_cb_receive_management * cb)
{
	struct ap_character * c = cb->user_data;
	struct ap_party * party = NULL;
	switch (cb->type) {
	case AP_PARTY_MANAGEMENT_PACKET_INVITE: {
		struct ap_character * target;
		struct ap_party_character_attachment * targetattachment;
		struct ap_party_character_attachment * attachment;
		if (cb->target_id)
			target = as_player_get_by_id(mod->as_player, cb->target_id);
		else
			target = as_player_get_by_name(mod->as_player, cb->target_name);
		if (!target) {
			ap_party_make_management_packet(mod->ap_party,
				AP_PARTY_MANAGEMENT_PACKET_INVITE_FAILED_NO_LOGIN_MEMBER,
				&c->id, NULL, cb->target_name);
			as_player_send_packet(mod->as_player, c);
			break;
		}
		if (c == target || !as_map_is_in_same_instance(mod->as_map, c, target))
			break;
		/* \todo Check party block option. */
		targetattachment = ap_party_get_character_attachment(mod->ap_party, target);
		if (targetattachment->party) {
			ap_party_make_management_packet(mod->ap_party,
				AP_PARTY_MANAGEMENT_PACKET_INVITE_FAILED_ALREADY_OTHER_PARTY_MEMBER,
				&c->id, &target->id, target->name);
			as_player_send_packet(mod->as_player, c);
			break;
		}
		attachment = ap_party_get_character_attachment(mod->ap_party, c);
		if (attachment->party) {
			if (c != attachment->party->members[AP_PARTY_LEADER_INDEX]) {
				ap_party_make_management_packet(mod->ap_party,
					AP_PARTY_MANAGEMENT_PACKET_INVITE_FAILED_NOT_LEADER,
					&c->id, &target->id, target->name);
				as_player_send_packet(mod->as_player, c);
				break;
			}
			if (attachment->party->member_count >= AP_PARTY_MAX_MEMBER_COUNT) {
				ap_party_make_management_packet(mod->ap_party,
					AP_PARTY_MANAGEMENT_PACKET_INVITE_FAILED_PARTY_MEMBER_IS_FULL,
					&c->id, &target->id, target->name);
				as_player_send_packet(mod->as_player, c);
				break;
			}
		}
		if (c->factor.char_status.murderer >= AP_CHARACTER_MURDERER_LEVEL1_POINT) {
			ap_party_make_management_packet(mod->ap_party,
				AP_PARTY_MANAGEMENT_PACKET_INVITE_FAILED_MURDERER_OPERATOR,
				&c->id, &target->id, target->name);
			as_player_send_packet(mod->as_player, c);
			break;
		}
		if (target->factor.char_status.murderer >= AP_CHARACTER_MURDERER_LEVEL1_POINT) {
			ap_party_make_management_packet(mod->ap_party,
				AP_PARTY_MANAGEMENT_PACKET_INVITE_FAILED_MURDERER_TARGET,
				&c->id, &target->id, target->name);
			as_player_send_packet(mod->as_player, c);
			break;
		}
		if (!ap_party_add_invitation(attachment, target->id))
			break;
		ap_party_make_management_packet(mod->ap_party,
			AP_PARTY_MANAGEMENT_PACKET_INVITE,
			&c->id, &target->id, c->name);
		as_player_send_packet(mod->as_player, target);
		break;
	}
	case AP_PARTY_MANAGEMENT_PACKET_REJECT: {
		struct ap_character * target;
		struct ap_party_character_attachment * targetattachment;
		if (cb->target_id)
			target = as_player_get_by_id(mod->as_player, cb->target_id);
		else
			target = as_player_get_by_name(mod->as_player, cb->target_name);
		if (!target)
			break;
		targetattachment = ap_party_get_character_attachment(mod->ap_party, target);
		if (!ap_party_consume_invitation(targetattachment, c->id))
			break;
		ap_party_make_management_packet(mod->ap_party,
			AP_PARTY_MANAGEMENT_PACKET_REJECT,
			&target->id, &c->id, c->name);
		as_player_send_packet(mod->as_player, target);
		break;
	}
	case AP_PARTY_MANAGEMENT_PACKET_INVITE_ACCEPT: {
		struct ap_character * target;
		struct ap_party_character_attachment * targetattachment;
		struct ap_party_character_attachment * attachment;
		struct ap_party * party;
		if (cb->target_id)
			target = as_player_get_by_id(mod->as_player, cb->target_id);
		else
			target = as_player_get_by_name(mod->as_player, cb->target_name);
		if (!target)
			break;
		targetattachment = ap_party_get_character_attachment(mod->ap_party, target);
		if (!ap_party_consume_invitation(targetattachment, c->id))
			break;
		attachment = ap_party_get_character_attachment(mod->ap_party, c);
		if (attachment->party) {
			ap_party_make_management_packet(mod->ap_party,
				AP_PARTY_MANAGEMENT_PACKET_INVITE_FAILED_ALREADY_PARTY_MEMBER,
				&target->id, &c->id, c->name);
			as_player_send_packet(mod->as_player, c);
			break;
		}
		/* \todo Check if party is disabled in region. */
		if (!as_map_is_in_same_instance(mod->as_map, c, target))
			break;
		if (!ap_party_can_join(mod->ap_party, c))
			break;
		party = targetattachment->party;
		if (party) {
			if (ap_party_is_full(party)) {
				ap_party_make_management_packet(mod->ap_party,
					AP_PARTY_MANAGEMENT_PACKET_INVITE_FAILED_PARTY_MEMBER_IS_FULL,
					&target->id, &c->id, c->name);
				as_player_send_packet(mod->as_player, c);
				break;
			}
			ap_party_add_member(mod->ap_party, party, c);
		}
		else {
			party = ap_party_form(mod->ap_party, 2, target, c);
		}
		ap_party_make_management_packet(mod->ap_party,
			AP_PARTY_MANAGEMENT_PACKET_INVITE_ACCEPT,
			&target->id, &c->id, c->name);
		as_player_send_packet(mod->as_player, target);
		break;
	}
	case AP_PARTY_MANAGEMENT_PACKET_LEAVE: {
		struct ap_party_character_attachment * attachment =
			ap_party_get_character_attachment(mod->ap_party, c);
		if (!attachment->party)
			break;
		if (!ap_party_can_leave(mod->ap_party, c))
			break;
		ap_party_remove_member(mod->ap_party, attachment->party, c);
		break;
	}
	case AP_PARTY_MANAGEMENT_PACKET_BANISH: {
		struct ap_party_character_attachment * attachment;
		struct ap_character * target = NULL;
		struct ap_party_character_attachment * targetattachment;
		struct ap_party * party;
		if (cb->target_id)
			target = as_player_get_by_id(mod->as_player, cb->target_id);
		else if (!strisempty(cb->target_name))
			target = as_player_get_by_name(mod->as_player, cb->target_name);
		if (!target)
			break;
		targetattachment = ap_party_get_character_attachment(mod->ap_party, target);
		if (!targetattachment->party)
			break;
		attachment = ap_party_get_character_attachment(mod->ap_party, c);
		party = attachment->party;
		if (party != targetattachment->party)
			break;
		if (party->members[AP_PARTY_LEADER_INDEX] != c)
			break;
		ap_party_remove_member(mod->ap_party, party, target);
		ap_party_make_management_packet(mod->ap_party, 
			AP_PARTY_MANAGEMENT_PACKET_BANISH, &target->id, NULL, NULL);
		as_player_send_packet(mod->as_player, target);
		break;
	}
	case AP_PARTY_MANAGEMENT_PACKET_DELEGATION_LEADER: {
		struct ap_party_character_attachment * attachment;
		struct ap_character * target = NULL;
		struct ap_party_character_attachment * targetattachment;
		struct ap_party * party;
		uint32_t i;
		if (cb->target_id)
			target = as_player_get_by_id(mod->as_player, cb->target_id);
		else if (!strisempty(cb->target_name))
			target = as_player_get_by_name(mod->as_player, cb->target_name);
		if (!target)
			break;
		targetattachment = ap_party_get_character_attachment(mod->ap_party, target);
		if (!targetattachment->party)
			break;
		attachment = ap_party_get_character_attachment(mod->ap_party, c);
		party = attachment->party;
		if (party != targetattachment->party)
			break;
		if (party->members[AP_PARTY_LEADER_INDEX] != c)
			break;
		for (i = 0; i < party->member_count; i++) {
			if (party->members[i] == target) { 
				party->members[i] = c;
				party->members[AP_PARTY_LEADER_INDEX] = target;
				break;
			}
		}
		ap_party_make_management_packet(mod->ap_party, 
			AP_PARTY_MANAGEMENT_PACKET_DELEGATION_LEADER, &target->id, &c->id, NULL);
		as_party_send_packet(mod->as_party, party);
		break;
	}
	}
	return TRUE;
}

static boolean cbjoin(
	struct as_party_process_module * mod,
	struct ap_party_cb_join * cb)
{
	struct ap_character * c = cb->character;
	struct ap_party * party = cb->party;
	uint32_t i;
	if (!cb->is_party_newly_formed) {
		ap_optimized_packet2_make_char_view_packet(mod->ap_optimized_packet2, c);
		as_party_send_packet_except(mod->as_party, party, c);
		ap_party_make_member_packet(mod->ap_party, AP_PARTY_PACKET_ADD_MEMBER,
			party, c->id);
		as_party_send_packet_except(mod->as_party, party, c);
	}
	for (i = 0; i < party->member_count; i++) {
		if (party->members[i] != c) {
			ap_optimized_packet2_make_char_view_packet(mod->ap_optimized_packet2, 
				party->members[i]);
			as_player_send_packet(mod->as_player, c);
		}
	}
	ap_party_make_add_packet(mod->ap_party, party);
	as_player_send_packet(mod->as_player, c);
	for (i = 0; i < party->member_count; i++) {
		ap_party_make_member_packet(mod->ap_party, AP_PARTY_PACKET_ADD_MEMBER,
			party, party->member_character_ids[i]);
		as_player_send_packet(mod->as_player, c);
	}
	return TRUE;
}

static boolean cbleave(
	struct as_party_process_module * mod,
	struct ap_party_cb_leave * cb)
{
	struct ap_character * c = cb->character;
	struct ap_party * party = cb->party;
	uint32_t i;
	ap_party_make_member_packet(mod->ap_party, AP_PARTY_PACKET_REMOVE_MEMBER,
		party, c->id);
	as_party_send_packet(mod->as_party, party);
	as_player_send_packet(mod->as_player, c);
	ap_party_make_remove_packet(mod->ap_party, party);
	as_player_send_packet(mod->as_player, c);
	for (i = 0; i < party->member_count; i++) {
		struct ap_character * other = party->members[i];
		if (!as_map_is_in_field_of_vision(mod->as_map, c, other)) {
			ap_character_make_packet(mod->ap_character, 
				AP_CHARACTER_PACKET_TYPE_REMOVE, c->id);
			as_player_send_packet(mod->as_player, other);
			ap_character_make_packet(mod->ap_character, 
				AP_CHARACTER_PACKET_TYPE_REMOVE, other->id);
			as_player_send_packet(mod->as_player, c);
		}
	}
	/* \todo If party leader has changed, update party stat bonuses. */
	if (!party->member_count)
		ap_party_free(mod->ap_party, party);
	else if (party->member_count == 1)
		ap_party_remove_member(mod->ap_party, party, party->members[0]);
	return TRUE;
}

static boolean cbplayerremove(
	struct as_party_process_module * mod, 
	struct as_player_cb_remove * cb)
{
	struct ap_party_character_attachment * attachment = 
		ap_party_get_character_attachment(mod->ap_party, cb->character);
	if (attachment->party)
		ap_party_remove_member(mod->ap_party, attachment->party, cb->character);
	return TRUE;
}

static boolean onregister(
	struct as_party_process_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_character, AP_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_chat, AP_CHAT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_optimized_packet2, AP_OPTIMIZED_PACKET2_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_party, AP_PARTY_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_tick, AP_TICK_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_map, AS_MAP_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_party, AS_PARTY_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_player, AS_PLAYER_MODULE_NAME);
	ap_party_add_callback(mod->ap_party, AP_PARTY_CB_RECEIVE, mod, cbreceive);
	ap_party_add_callback(mod->ap_party, AP_PARTY_CB_RECEIVE_MANAGEMENT, mod, cbreceivemanagement);
	ap_party_add_callback(mod->ap_party, AP_PARTY_CB_JOIN, mod, cbjoin);
	ap_party_add_callback(mod->ap_party, AP_PARTY_CB_LEAVE, mod, cbleave);
	as_player_add_callback(mod->as_player, AS_PLAYER_CB_REMOVE, mod, cbplayerremove);
	return TRUE;
}

static void onshutdown(struct as_party_process_module * mod)
{
}

struct as_party_process_module * as_party_process_create_module()
{
	struct as_party_process_module * mod = ap_module_instance_new(AS_PARTY_PROCESS_MODULE_NAME, 
		sizeof(*mod), onregister, NULL, NULL, onshutdown);
	return mod;
}
