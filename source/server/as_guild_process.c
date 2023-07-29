#include "server/as_guild_process.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"
#include "core/vector.h"

#include "public/ap_chat.h"
#include "public/ap_guild.h"

#include "server/as_character.h"
#include "server/as_guild.h"
#include "server/as_player.h"

#include <assert.h>

#define GUILDCREATECOST 10000

struct as_guild_process_module {
	struct ap_module_instance instance;
	struct ap_chat_module * ap_chat;
	struct ap_guild_module * ap_guild;
	struct as_character_module * as_character;
	struct as_guild_module * as_guild;
	struct as_player_module * as_player;
	struct ap_character ** list;
};

static boolean cbreceive(
	struct as_guild_process_module * mod,
	struct ap_guild_cb_receive * cb)
{
	struct ap_character * c = cb->user_data;
	struct ap_guild_character * gc = ap_guild_get_character(mod->ap_guild, c);
	struct ap_guild * g = gc->guild;
	struct ap_guild_member * m = gc->member;
	switch (cb->type) {
	case AP_GUILD_PACKET_TYPE_CREATE: {
		const char * id;
		boolean invalidid = FALSE;
		struct as_guild * gs;
		if (g) {
			ap_guild_make_system_message_packet(mod->ap_guild, 
				AP_GUILD_SYSTEM_CODE_ALREADY_MEMBER, 
				NULL, NULL, NULL, NULL);
			as_player_send_packet(mod->as_player, c);
			break;
		}
		if (ap_guild_get(mod->ap_guild, cb->guild_id)) {
			ap_guild_make_system_message_packet(mod->ap_guild, 
				AP_GUILD_SYSTEM_CODE_EXIST_GUILD_ID, 
				NULL, NULL, NULL, NULL);
			as_player_send_packet(mod->as_player, c);
			break;
		}
		if (c->factor.char_status.level < 20) {
			ap_guild_make_system_message_packet(mod->ap_guild, 
				AP_GUILD_SYSTEM_CODE_NEED_MORE_LEVEL, 
				NULL, NULL, NULL, NULL);
			as_player_send_packet(mod->as_player, c);
			break;
		}
		if (c->inventory_gold < GUILDCREATECOST) {
			uint32_t gold = GUILDCREATECOST;
			ap_guild_make_system_message_packet(mod->ap_guild, 
				AP_GUILD_SYSTEM_CODE_NEED_MORE_MONEY, 
				NULL, NULL, &gold, NULL);
			as_player_send_packet(mod->as_player, c);
			break;
		}
		id = cb->guild_id;
		while (*id) {
			char c = *id++;
			if (!(c >= 'A' && c <= 'Z') &&
				!(c >= 'a' && c <= 'z') &&
				!(c >= '0' && c <= '9')) {
				invalidid = TRUE;
				break;
			}
		}
		invalidid |= strlen(cb->guild_id) == 0;
		if (invalidid) {
			ap_guild_make_system_message_packet(mod->ap_guild, 
				AP_GUILD_SYSTEM_CODE_USE_SPECIAL_CHAR, 
				NULL, NULL, NULL, NULL);
			as_player_send_packet(mod->as_player, c);
			break;
		}
		g = ap_guild_add(mod->ap_guild, cb->guild_id);
		if (!g)
			break;
		strlcpy(g->master_name, c->name, sizeof(g->master_name));
		strlcpy(g->password, cb->guild_password, 
			sizeof(g->password));
		g->creation_date = time(NULL);
		g->max_member_count = 50;
		m = ap_guild_add_member(mod->ap_guild, g, c->name);
		assert(m != NULL);
		m->rank = AP_GUILD_MEMBER_RANK_MASTER;
		m->join_date = time(NULL);
		m->level = c->factor.char_status.level;
		m->tid = c->tid;
		m->status = AP_GUILD_MEMBER_STATUS_ONLINE;
		gc->guild = g;
		gc->member = m;
		strlcpy(gc->guild_id, g->id, sizeof(gc->guild_id));
		gc->guild_mark_tid = g->guild_mark_tid;
		gc->guild_mark_color = g->guild_mark_color;
		gc->battle_rank = g->battle_rank;
		gs = as_guild_get(mod->as_guild, g);
		gs->db = alloc(sizeof(*gs->db));
		gs->await_create = TRUE;
		memset(gs->db, 0, sizeof(*gs->db));
		as_guild_reflect(mod->as_guild, g);
		as_character_consume_gold(mod->as_character, c, GUILDCREATECOST);
		ap_guild_make_add_packet(mod->ap_guild, g);
		as_player_send_packet(mod->as_player, c);
		ap_guild_make_char_data_packet(mod->ap_guild, g, m, c->id);
		as_player_send_packet(mod->as_player, c);
		ap_guild_make_system_message_packet(mod->ap_guild, 
			AP_GUILD_SYSTEM_CODE_GUILD_CREATE_COMPLETE, 
			NULL, NULL, NULL, NULL);
		as_player_send_packet(mod->as_player, c);
		break;
	}
	case AP_GUILD_PACKET_TYPE_JOIN_REQUEST: {
		struct ap_character * target = NULL;
		uint32_t i;
		boolean alreadyinvited = FALSE;
		if (!g)
			break;
		/* Only guild master and submaster can invite members. */
		if (m->rank != AP_GUILD_MEMBER_RANK_MASTER &&
			m->rank != AP_GUILD_MEMBER_RANK_SUBMASTER) {
			break;
		}
		target = as_player_get_by_name(mod->as_player, cb->member_name);
		if (!target)
			break;
		/** \todo Check blocking options. */
		for (i = 0; i < g->invited_character_count; i++) {
			if (g->invited_character_id[i] == target->id) {
				alreadyinvited = TRUE;
				break;
			}
		}
		if (alreadyinvited)
			break;
		gc = ap_guild_get_character(mod->ap_guild, target);
		/* Target is already in a guild. */
		if (gc->guild)
			break;
		/* If invitation list is full, clear it. */
		if (g->invited_character_count >= COUNT_OF(g->invited_character_id))
			g->invited_character_count = 0;
		g->invited_character_id[g->invited_character_count++] = 
			target->id;
		ap_guild_make_join_request_packet(mod->ap_guild, g, c->name, target->name);
		as_player_send_packet(mod->as_player, target);
		break;
	}
	case AP_GUILD_PACKET_TYPE_JOIN_REJECT: {
		struct ap_character * master;
		uint32_t i;
		boolean hadinvite = FALSE;
		g = ap_guild_get(mod->ap_guild, cb->guild_id);
		if (!g)
			break;
		for (i = 0; i < g->invited_character_count; i++) {
			if (g->invited_character_id[i] == c->id) {
				hadinvite = TRUE;
				g->invited_character_id[i] = 
					g->invited_character_id[--g->invited_character_count];
				break;
			}
		}
		if (!hadinvite)
			break;
		master = as_player_get_by_name(mod->as_player, g->master_name);
		if (master) {
			ap_guild_make_system_message_packet(mod->ap_guild, 
				AP_GUILD_SYSTEM_CODE_GUILD_JOIN_REJECT, 
				NULL, NULL, NULL, NULL);
			as_player_send_packet(mod->as_player, master);
		}
		break;
	}
	case AP_GUILD_PACKET_TYPE_JOIN: {
		struct ap_guild_member * member;
		if (g) {
			/* Player that made the request is already in a 
			 * guild, in which case this packet is meant to 
			 * approve a join request. */
			struct ap_character * mc;
			member = ap_guild_get_member(mod->ap_guild, g, cb->member_name);
			if (!member || 
				member->rank != AP_GUILD_MEMBER_RANK_JOIN_REQUEST) {
				break;
			}
			/* Join requests can only be approved by guild 
			 * master or submaster. */
			if (gc->member->rank != AP_GUILD_MEMBER_RANK_MASTER &&
				gc->member->rank != AP_GUILD_MEMBER_RANK_SUBMASTER) {
				break;
			}
			member->rank = AP_GUILD_MEMBER_RANK_NORMAL;
			mc = as_player_get_by_name(mod->as_player, member->name);
			ap_guild_make_char_data_packet(mod->ap_guild, g, m, c->id);
			as_guild_send_packet(mod->as_guild, g);
			ap_guild_make_system_message_packet(mod->ap_guild, 
				AP_GUILD_SYSTEM_CODE_GUILD_JOIN, 
				member->name, NULL, NULL, NULL);
			if (mc) {
				/* Now that the guild member has become a 
				 * normal member, we can send guild 
				 * info (members, notice, etc) to member. */
				as_guild_broadcast_members(mod->as_guild, g, mc);
			}
			else {
				as_guild_send_packet(mod->as_guild, g);
			}
		}
		else {
			/* Player is not yet in a guild, so they are either 
			 * applying to join a guild or accepting a guild 
			 * invitation. */
			uint32_t i;
			enum ap_guild_member_rank rank = 
				AP_GUILD_MEMBER_RANK_JOIN_REQUEST;
			enum ap_guild_system_code sys = 
				AP_GUILD_SYSTEM_CODE_JOIN_REQUEST_SELF;
			g = ap_guild_get(mod->ap_guild, cb->guild_id);
			if (!g) {
				ap_guild_make_system_message_packet(mod->ap_guild, 
					AP_GUILD_SYSTEM_CODE_JOIN_FAIL, 
					NULL, NULL, NULL, NULL);
				as_player_send_packet(mod->as_player, c);
				break;
			}
			if (ap_guild_is_full(g)) {
				ap_guild_make_system_message_packet(mod->ap_guild, 
					AP_GUILD_SYSTEM_CODE_MAX_MEMBER, 
					NULL, NULL, NULL, NULL);
				as_player_send_packet(mod->as_player, c);
				break;
			}
			m = ap_guild_add_member(mod->ap_guild, g, c->name);
			if (!m) {
				ap_guild_make_system_message_packet(mod->ap_guild, 
					AP_GUILD_SYSTEM_CODE_JOIN_FAIL, 
					NULL, NULL, NULL, NULL);
				as_player_send_packet(mod->as_player, c);
				break;
			}
			for (i = 0; i < g->invited_character_count; i++) {
				if (g->invited_character_id[i] == c->id) {
					rank = AP_GUILD_MEMBER_RANK_NORMAL;
					sys = AP_GUILD_SYSTEM_CODE_GUILD_JOIN;
					g->invited_character_id[i] = 
						g->invited_character_id[--g->invited_character_count];
					break;
				}
			}
			m->rank = rank;
			m->join_date = time(NULL);
			m->level = c->factor.char_status.level;
			m->tid = c->tid;
			m->status = AP_GUILD_MEMBER_STATUS_ONLINE;
			gc->guild = g;
			gc->member = m;
			strlcpy(gc->guild_id, g->id, sizeof(gc->guild_id));
			gc->guild_mark_tid = g->guild_mark_tid;
			gc->guild_mark_color = g->guild_mark_color;
			gc->battle_rank = g->battle_rank;
			ap_guild_make_add_packet(mod->ap_guild, g);
			as_player_send_packet(mod->as_player, c);
			ap_guild_make_char_data_packet(mod->ap_guild, g, m, c->id);
			as_player_send_packet(mod->as_player, c);
			ap_guild_make_join_packet(mod->ap_guild, g, m);
			as_guild_send_packet(mod->as_guild, g);
			ap_guild_make_system_message_packet(mod->ap_guild, sys, 
				m->name, NULL, NULL, NULL);
			as_guild_send_packet_with_exception(mod->as_guild, g, c);
			if (rank == AP_GUILD_MEMBER_RANK_NORMAL)
				as_guild_broadcast_members(mod->as_guild, g, c);
		}
		break;
	}
	}
	return TRUE;
}

static void cbchatguildmember(
	struct as_guild_process_module * mod,
	struct ap_character * c,
	uint32_t argc,
	const char * const * argv)
{
	struct ap_guild * g;
	struct ap_guild_member * m = NULL;
	uint32_t i = 0;
	switch (argc) {
	case 1:
		g = ap_guild_get(mod->ap_guild, argv[0]);
		break;
	default:
		g = ap_guild_get_character(mod->ap_guild, c)->guild;
	}
	if (!g)
		return;
	if (ap_guild_get_member_count(g) >= g->max_member_count)
		return;
	while (i < 250) {
		char name[AP_CHARACTER_MAX_NAME_LENGTH + 1];
		snprintf(name, sizeof(name), "Dummy_%u", i++);
		m = ap_guild_add_member(mod->ap_guild, g, name);
		if (m)
			break;
	}
	if (!m)
		return;
	m->rank = AP_GUILD_MEMBER_RANK_NORMAL;
	m->join_date = time(NULL);
	m->level = 99;
	m->tid = 1;
	m->status = AP_GUILD_MEMBER_STATUS_ONLINE;
	ap_guild_make_join_packet(mod->ap_guild, g, m);
	as_guild_send_packet(mod->as_guild, g);
}

static boolean onregister(
	struct as_guild_process_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_chat, AP_CHAT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_guild, AP_GUILD_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_character, AS_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_guild, AS_GUILD_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_player, AS_PLAYER_MODULE_NAME);
	ap_guild_add_callback(mod->ap_guild, AP_GUILD_CB_RECEIVE, mod, cbreceive);
	ap_chat_add_command(mod->ap_chat, "/guildmember", mod, cbchatguildmember);
	return TRUE;
}

static void onshutdown(struct as_guild_process_module * mod)
{
	vec_free(mod->list);
}

struct as_guild_process_module * as_guild_process_create_module()
{
	struct as_guild_process_module * mod = ap_module_instance_new(AS_GUILD_PROCESS_MODULE_NAME, 
		sizeof(*mod), onregister, NULL, NULL, onshutdown);
	mod->list = vec_new_reserved(sizeof(*mod->list), 128);
	return mod;
}
