#include <assert.h>

#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"

#include "public/ap_guild.h"
#include "public/ap_packet.h"

#include "utility/au_packet.h"

struct ap_guild_module {
	struct ap_module_instance instance;
	struct ap_character_module * ap_character;
	struct ap_packet_module * ap_packet;
	size_t character_offset;
	struct au_packet packet;
	struct au_packet packet_member;
	struct au_packet packet_list;
	struct au_packet packet_list_item;
	struct ap_admin guild_admin;
	struct ap_admin character_admin;
};

static inline void * makememberpacket(
	struct ap_guild_module * mod,
	const struct ap_guild_member * member)
{
	void * buffer = ap_packet_get_temp_buffer(mod->ap_packet);
	au_packet_make_packet(&mod->packet_member, 
		buffer, FALSE, NULL, 0,
		member->name,
		&member->rank,
		&member->join_date,
		&member->level,
		&member->tid,
		&member->status);
	return buffer;
}

static inline void * makecustommemberpacket(
	struct ap_guild_module * mod,
	const char * name)
{
	void * buffer = ap_packet_get_temp_buffer(mod->ap_packet);
	au_packet_make_packet(&mod->packet_member, 
		buffer, FALSE, NULL, 0,
		name,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL);
	return buffer;
}

static inline void makepacket(
	struct ap_guild_module * mod,
	const enum ap_guild_packet_type type,
	const uint32_t * character_id,
	const char * guild_id,
	const char * master_name,
	const uint32_t * guild_tid,
	const enum ap_guild_rank * guild_rank,
	const time_t * creation_date,
	const uint32_t * max_member_count,
	const uint32_t * union_id,
	const char * guild_password, 
	const char * notice,
	const enum ap_guild_status * status,
	const uint32_t * win,
	const uint32_t * draw,
	const uint32_t * lose,
	const uint32_t * point,
	const void * member_packet,
	const void * battle_packet,
	const void * battle_person_packet,
	const void * battle_member_packet,
	const void * battle_member_list_packet,
	const void * guild_list_packet,
	const uint32_t * guild_mark_tid,
	const uint32_t * guild_mark_color,
	const boolean * guild_winner)
{
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	uint16_t length = 0;
	if (notice) {
		uint16_t len = (uint16_t)strlen(notice);
		au_packet_make_packet(&mod->packet, 
			buffer, TRUE, &length, AP_GUILD_PACKET_TYPE,
			&type,
			character_id,
			guild_id,
			master_name,
			guild_tid,
			guild_rank,
			creation_date,
			max_member_count,
			union_id,
			guild_password, 
			notice, &len,
			status,
			win,
			draw,
			lose,
			point,
			member_packet,
			battle_packet,
			battle_person_packet,
			battle_member_packet,
			battle_member_list_packet,
			guild_list_packet,
			guild_mark_tid,
			guild_mark_color,
			guild_winner);
	}
	else {
		au_packet_make_packet(&mod->packet, 
			buffer, TRUE, &length, AP_GUILD_PACKET_TYPE,
			&type,
			character_id,
			guild_id,
			master_name,
			guild_tid,
			guild_rank,
			creation_date,
			max_member_count,
			union_id,
			guild_password, 
			notice,
			status,
			win,
			draw,
			lose,
			point,
			member_packet,
			battle_packet,
			battle_person_packet,
			battle_member_packet,
			battle_member_list_packet,
			guild_list_packet,
			guild_mark_tid,
			guild_mark_color,
			guild_winner);
	}
	ap_packet_set_length(mod->ap_packet, length);
}

static void destroymemberadmin(
	struct ap_guild_module * mod,
	struct ap_admin * admin)
{
	size_t index = 0;
	struct ap_guild_member * member = NULL;
	while (ap_admin_iterate_name(admin, &index, &member))
		ap_module_destruct_module_data(mod, AP_GUILD_MDI_MEMBER, member);
	ap_admin_destroy(admin);
}

static boolean cbctor(struct ap_guild_module * mod, struct ap_guild * g)
{
	ap_admin_init(&g->member_admin, 
		sizeof(struct ap_guild_member), 16);
	ap_admin_init(&g->join_admin, 
		sizeof(struct ap_guild_member), 4);
	g->rank = AP_GUILD_RANK_BEGINNER;
	return TRUE;
}

static boolean cbdtor(struct ap_guild_module * mod, struct ap_guild * g)
{
	destroymemberadmin(mod, &g->member_admin);
	destroymemberadmin(mod, &g->join_admin);
	return TRUE;
}

static boolean onregister(
	struct ap_guild_module * mod,
	struct ap_module_registry * registry)
{
	mod->ap_character = ap_module_registry_get_module(registry, AP_CHARACTER_MODULE_NAME);
	if (!mod->ap_character) {
		ERROR("Failed to retrieve module (%s).", AP_CHARACTER_MODULE_NAME);
		return FALSE;
	}
	mod->character_offset = ap_character_attach_data(mod->ap_character,
		AP_CHARACTER_MDI_CHAR, sizeof(struct ap_guild_character),
		mod, NULL, NULL);
	if (mod->character_offset == SIZE_MAX) {
		ERROR("Failed to attach character data.");
		return FALSE;
	}
	mod->ap_packet = ap_module_registry_get_module(registry, AP_PACKET_MODULE_NAME);
	return (mod->ap_packet != NULL);
}

static void onclose(struct ap_guild_module * mod)
{
	size_t index = 0;
	struct ap_guild * g = NULL;
	while (ap_admin_iterate_name(&mod->guild_admin, &index, &g))
		ap_module_destruct_module_data(mod, AP_GUILD_MDI_GUILD, g);
	ap_admin_clear_objects(&mod->guild_admin);
	ap_admin_clear_objects(&mod->character_admin);
}

static void onshutdown(struct ap_guild_module * mod)
{
	ap_admin_destroy(&mod->guild_admin);
}

struct ap_guild_module * ap_guild_create_module()
{
	struct ap_guild_module * mod = ap_module_instance_new(AP_GUILD_MODULE_NAME,
		sizeof(*mod), onregister, NULL, onclose, onshutdown);
	ap_module_set_module_data(mod, 
		AP_GUILD_MDI_GUILD, sizeof(struct ap_guild),
		cbctor, cbdtor);
	ap_module_set_module_data(mod,
		AP_GUILD_MDI_MEMBER, sizeof(struct ap_guild_member),
		NULL, NULL);
	au_packet_init(&mod->packet, sizeof(uint32_t),
		AU_PACKET_TYPE_UINT8, 1, /* Packet Type */ 
		AU_PACKET_TYPE_INT32, 1, /* CID */
		/* Guild ID */
		AU_PACKET_TYPE_CHAR, AP_GUILD_MAX_ID_LENGTH + 1,
		/* Master ID */
		AU_PACKET_TYPE_CHAR, AP_CHARACTER_MAX_NAME_LENGTH + 1,
		AU_PACKET_TYPE_INT32, 1, /* Guild TID */
		AU_PACKET_TYPE_INT32, 1, /* Guild Rank */
		AU_PACKET_TYPE_INT32, 1, /* Creation Date */
		AU_PACKET_TYPE_INT32, 1, /* Max Member Count */
		AU_PACKET_TYPE_INT32, 1, /* Union ID */
		/* Password */
		AU_PACKET_TYPE_CHAR, AP_GUILD_MAX_PASSWORD_LENGTH + 1, 
		AU_PACKET_TYPE_MEMORY_BLOCK, 1, /* Notice */
		AU_PACKET_TYPE_INT8, 1, /* Status */
		AU_PACKET_TYPE_INT32, 1, /* Win */
		AU_PACKET_TYPE_INT32, 1, /* Draw */
		AU_PACKET_TYPE_INT32, 1, /* Lose */
		AU_PACKET_TYPE_INT32, 1, /* Guild Point */
		AU_PACKET_TYPE_PACKET, 1, /* Member Packet */
		AU_PACKET_TYPE_PACKET, 1, /* Battle Packet */
		AU_PACKET_TYPE_PACKET, 1, /* BattlePerson Packet */
		AU_PACKET_TYPE_PACKET, 1, /* BattleMember Packet */
		AU_PACKET_TYPE_PACKET, 1, /* BattleMemberList Packet */
		AU_PACKET_TYPE_PACKET, 1, /* GuildList Packet */
		AU_PACKET_TYPE_INT32, 1, /* GuildMark TID */
		AU_PACKET_TYPE_INT32, 1, /* GuildMark COlor */
		AU_PACKET_TYPE_INT32, 1, /* Winner Guild (BOOL) */
		AU_PACKET_TYPE_END);
	au_packet_init(&mod->packet_member, sizeof(uint8_t),
		/* Member ID */
		AU_PACKET_TYPE_CHAR, AP_CHARACTER_MAX_NAME_LENGTH + 1, 
		AU_PACKET_TYPE_INT32, 1, /* Member Rank */
		AU_PACKET_TYPE_INT32, 1, /* Join Date */
		AU_PACKET_TYPE_INT32, 1, /* Member Level */
		AU_PACKET_TYPE_INT32, 1, /* Member TID */
		AU_PACKET_TYPE_INT8, 1, /* Status */
		AU_PACKET_TYPE_END);
	au_packet_init(&mod->packet_list, sizeof(uint32_t),
		AU_PACKET_TYPE_INT32, 1, /*  Total Guild Count */
		AU_PACKET_TYPE_INT32, 1, /*  Page Count */
		AU_PACKET_TYPE_INT16, 1, /*  Item Count */
		AU_PACKET_TYPE_PACKET, 1, /* 1 GuildList Item */
		AU_PACKET_TYPE_PACKET, 1, /* 2 GuildList Item */
		AU_PACKET_TYPE_PACKET, 1, /* 3 GuildList Item */
		AU_PACKET_TYPE_PACKET, 1, /* 4 GuildList Item */
		AU_PACKET_TYPE_PACKET, 1, /* 5 GuildList Item */
		AU_PACKET_TYPE_PACKET, 1, /* 6 GuildList Item */
		AU_PACKET_TYPE_PACKET, 1, /* 7 GuildList Item */
		AU_PACKET_TYPE_PACKET, 1, /* 8 GuildList Item */
		AU_PACKET_TYPE_PACKET, 1, /* 9 GuildList Item */
		AU_PACKET_TYPE_PACKET, 1, /* 10 GuildList Item */
		AU_PACKET_TYPE_PACKET, 1, /* 11 GuildList Item */
		AU_PACKET_TYPE_PACKET, 1, /* 12 GuildList Item */
		AU_PACKET_TYPE_PACKET, 1, /* 13 GuildList Item */
		AU_PACKET_TYPE_PACKET, 1, /* 14 GuildList Item */
		AU_PACKET_TYPE_PACKET, 1, /* 15 GuildList Item */
		AU_PACKET_TYPE_PACKET, 1, /* 16 GuildList Item */
		AU_PACKET_TYPE_PACKET, 1, /* 17 GuildList Item */
		AU_PACKET_TYPE_PACKET, 1, /* 18 GuildList Item */
		AU_PACKET_TYPE_PACKET, 1, /* 19 GuildList Item */
		AU_PACKET_TYPE_END);
	au_packet_init(&mod->packet_list_item, sizeof(uint16_t),
		/* Guild ID */
		AU_PACKET_TYPE_CHAR, AP_GUILD_MAX_ID_LENGTH + 1, 
		/* Master ID */
		AU_PACKET_TYPE_CHAR, AP_CHARACTER_MAX_NAME_LENGTH + 1, 
		/* SubMaster ID */
		AU_PACKET_TYPE_CHAR, AP_CHARACTER_MAX_NAME_LENGTH + 1, 
		AU_PACKET_TYPE_INT32, 1, /* Level; */
		AU_PACKET_TYPE_UINT32, 1, /* WinPoint; */
		AU_PACKET_TYPE_INT32, 1, /* MemberCount; */
		AU_PACKET_TYPE_INT32, 1, /* MaxMemberCount; */
		AU_PACKET_TYPE_INT32, 1, /* lGuildBattle; */
		AU_PACKET_TYPE_INT32, 1, /* lGuildMarkTID; */
		AU_PACKET_TYPE_INT32, 1, /* lGuildMarkColor; */
		AU_PACKET_TYPE_INT32, 1, /* IsWinner */
		AU_PACKET_TYPE_END);
	ap_admin_init(&mod->guild_admin, sizeof(struct ap_guild), 16);
	ap_admin_init(&mod->character_admin, 
		sizeof(struct ap_guild *), 256);
	return mod;
}

void ap_guild_add_callback(
	struct ap_guild_module * mod,
	enum ap_guild_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	ap_module_add_callback(mod, id, callback_module, callback);
}

size_t ap_guild_attach_data(
	struct ap_guild_module * mod,
	enum ap_guild_module_data_index data_index,
	size_t data_size,
	ap_module_t callback_module,
	ap_module_default_t constructor,
	ap_module_default_t destructor)
{
	return ap_module_attach_data(mod, data_index, data_size, 
		callback_module, constructor, destructor);
}

struct ap_guild * ap_guild_add(struct ap_guild_module * mod, const char * guild_id)
{
	struct ap_guild * g = ap_admin_add_object_by_name(&mod->guild_admin, guild_id);
	if (!g)
		return NULL;
	ap_module_construct_module_data(mod, AP_GUILD_MDI_GUILD, g);
	strlcpy(g->id, guild_id, sizeof(g->id));
	return g;
}

void ap_guild_remove(struct ap_guild_module * mod, struct ap_guild * guild)
{
	if (!ap_admin_remove_object_by_name(&mod->guild_admin, 
			guild->id)) {
		ERROR("Failed to remove guild (%s).", guild->id);
		assert(0);
		return;
	}
	ap_module_destruct_module_data(mod, AP_GUILD_MDI_GUILD, guild);
}

struct ap_guild_member * ap_guild_add_member(
	struct ap_guild_module * mod,
	struct ap_guild * guild,
	const char * member_name)
{
	struct ap_guild_member * m;
	struct ap_guild ** obj = ap_admin_add_object_by_name(
		&mod->character_admin, member_name);
	if (!obj)
		return NULL;
	m = ap_admin_add_object_by_name(&guild->member_admin, 
		member_name);
	if (!m) {
		assert(0);
		ap_admin_remove_object_by_name(&mod->character_admin,
			member_name);
		return NULL;
	}
	ap_module_construct_module_data(mod, AP_GUILD_MDI_MEMBER, m);
	strlcpy(m->name, member_name, sizeof(m->name));
	*obj = guild;
	return m;
}

void ap_guild_remove_member(
	struct ap_guild_module * mod,
	struct ap_guild * guild,
	struct ap_guild_member * member)
{
	if (!ap_admin_remove_object_by_name(&guild->member_admin, 
			member->name)) {
		ERROR("Failed to remove guild member (%s).", member->name);
		assert(0);
		return;
	}
	ap_module_destruct_module_data(mod, AP_GUILD_MDI_MEMBER, member);
}

struct ap_guild_character * ap_guild_get_character(
	struct ap_guild_module * mod,
	struct ap_character * character)
{
	return ap_module_get_attached_data(character, mod->character_offset);
}

struct ap_guild * ap_guild_get(struct ap_guild_module * mod, const char * guild_id)
{
	return ap_admin_get_object_by_name(&mod->guild_admin, 
		guild_id);
}

struct ap_guild * ap_guild_iterate(struct ap_guild_module * mod, size_t * index)
{
	struct ap_guild * g = NULL;
	if (!ap_admin_iterate_name(&mod->guild_admin, index, &g))
		return NULL;
	return g;
}

struct ap_guild * ap_guild_get_character_guild(
	struct ap_guild_module * mod,
	const char * character_name)
{
	struct ap_guild ** obj = ap_admin_get_object_by_name(
		&mod->character_admin, character_name);
	if (!obj)
		return NULL;
	return *obj;
}

struct ap_guild_member * ap_guild_get_member(
	struct ap_guild_module * mod,
	struct ap_guild * guild,
	const char * character_name)
{
	return ap_admin_get_object_by_name(&guild->member_admin, 
		character_name);
}

struct ap_guild_member * ap_guild_iterate_member(
	struct ap_guild_module * mod,
	struct ap_guild * guild,
	size_t * index)
{
	struct ap_guild_member * m = NULL;
	if (!ap_admin_iterate_name(&guild->member_admin, index, &m))
		return NULL;
	return m;
}

boolean ap_guild_on_receive(
	struct ap_guild_module * mod,
	const void * data,
	uint16_t length,
	void * user_data)
{
	struct ap_guild_cb_receive cb = { 0 };
	const char * guildid = NULL;
	const char * guildpwd = NULL;
	const char * notice = NULL;
	uint16_t noticelen = 0;
	void * memberpacket = NULL;
	if (!au_packet_get_field(&mod->packet, TRUE, data, length,
			&cb.type, /* Packet Type */ 
			&cb.character_id, /* CID */
			&guildid, /* Guild ID */
			NULL, /* Master ID */
			NULL, /* Guild TID */
			NULL, /* Guild Rank */
			NULL, /* Creation Date */
			NULL, /* Max Member Count */
			NULL, /* Union ID */
			&guildpwd, /* Password */
			&notice, &noticelen, /* Notice */
			NULL, /* Status */
			NULL, /* Win */
			NULL, /* Draw */
			NULL, /* Lose */
			NULL, /* Guild Point */
			&memberpacket, /* Member Packet */
			NULL, /* Battle Packet */
			NULL, /* BattlePerson Packet */
			NULL, /* BattleMember Packet */
			NULL, /* BattleMemberList Packet */
			NULL, /* GuildList Packet */
			&cb.guild_mark_tid, /* GuildMark TID */
			&cb.guild_mark_color, /* GuildMark COlor */
			NULL)) { /* Winner Guild (BOOL) */
		return FALSE;
	}
	if (guildid)
		strlcpy(cb.guild_id, guildid, sizeof(cb.guild_id));
	if (guildpwd) {
		strlcpy(cb.guild_password, guildpwd, 
			sizeof(cb.guild_password));
	}
	if (notice)
		strlcpy(cb.notice, notice, sizeof(cb.notice));
	if (memberpacket) {
		const char * name = NULL;
		if (!au_packet_get_field(&mod->packet_member, 
				FALSE, memberpacket, 0,
				&name, /* Member ID */
				NULL, /* Member Rank */
				NULL, /* Join Date */
				NULL, /* Member Level */
				NULL, /* Member TID */
				NULL  /* Status */)) {
			return FALSE;
		}
		if (name)
			strlcpy(cb.member_name, name, sizeof(cb.member_name));
	}
	cb.user_data = user_data;
	return ap_module_enum_callback(mod, AP_GUILD_CB_RECEIVE, &cb);
}

void ap_guild_make_add_packet(struct ap_guild_module * mod, struct ap_guild * guild)
{
	makepacket(mod, AP_GUILD_PACKET_TYPE_CREATE,
		NULL, guild->id, guild->master_name, NULL, &guild->rank,
		&guild->creation_date, &guild->max_member_count, NULL, 
		NULL, guild->notice, NULL, NULL, NULL, NULL, NULL, NULL, 
		NULL, NULL, NULL, NULL, NULL, &guild->guild_mark_tid, 
		&guild->guild_mark_color, NULL);
}

void ap_guild_make_join_request_packet(
	struct ap_guild_module * mod,
	const struct ap_guild * guild,
	const char * from,
	const char * to)
{
	void * memberpacket = makecustommemberpacket(mod, to);
	makepacket(mod, AP_GUILD_PACKET_TYPE_JOIN_REQUEST,
		NULL, guild->id, from, NULL, NULL, NULL, NULL, NULL, 
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, memberpacket, 
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
	ap_packet_reset_temp_buffers(mod->ap_packet);
}

void ap_guild_make_join_packet(
	struct ap_guild_module * mod,
	const struct ap_guild * guild,
	const struct ap_guild_member * member)
{
	void * memberpacket = makememberpacket(mod, member);
	makepacket(mod, AP_GUILD_PACKET_TYPE_JOIN,
		NULL, guild->id, NULL, NULL, NULL, NULL, NULL, NULL, 
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, memberpacket, 
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
	ap_packet_reset_temp_buffers(mod->ap_packet);
}

void ap_guild_make_char_data_packet(
	struct ap_guild_module * mod,
	const struct ap_guild * guild,
	const struct ap_guild_member * member,
	uint32_t character_id)
{
	void * memberpacket = makememberpacket(mod, member);
	makepacket(mod, AP_GUILD_PACKET_TYPE_CHAR_DATA,
		&character_id, guild->id, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
		NULL, memberpacket, NULL, NULL, NULL, NULL, NULL, 
		&guild->guild_mark_tid, &guild->guild_mark_color, NULL);
	ap_packet_reset_temp_buffers(mod->ap_packet);
}

void ap_guild_make_system_message_packet(
	struct ap_guild_module * mod,
	enum ap_guild_system_code code,
	const char * string1,
	const char * string2,
	uint32_t * num1,
	uint32_t * num2)
{
	makepacket(mod, AP_GUILD_PACKET_TYPE_SYSTEM_MESSAGE,
		(uint32_t *)&code,
		string1, string2, num1, (enum ap_guild_rank *)num2,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
}
