#ifndef _AS_GUILD_H_
#define _AS_GUILD_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_define.h"
#include "public/ap_guild.h"
#include "public/ap_module.h"

#define AS_GUILD_MODULE_NAME "AgsmGuild"

BEGIN_DECLS

enum as_guild_database_id {
	AS_GUILD_DB_END,
	AS_GUILD_DB_ID,
	AS_GUILD_DB_PASSWORD,
	AS_GUILD_DB_NOTICE,
	AS_GUILD_DB_RANK,
	AS_GUILD_DB_CREATION_DATE,
	AS_GUILD_DB_MAX_MEMBER_COUNT,
	AS_GUILD_DB_UNION_ID,
	AS_GUILD_DB_GUILD_MARK_TID,
	AS_GUILD_DB_GUILD_MARK_COLOR,
	AS_GUILD_DB_TOTAL_BATTLE_POINT,
};

enum as_guild_character_database_id {
	AS_GUILD_CHARACTER_DB_GUILD_ID,
	AS_GUILD_CHARACTER_DB_RANK,
	AS_GUILD_CHARACTER_DB_JOIN_DATE,
};

enum as_guild_callback_id {
	AS_GUILD_CB_,
};

struct as_guild_db {
	char id[AP_GUILD_MAX_ID_LENGTH + 1];
	char password[AP_GUILD_MAX_PASSWORD_LENGTH + 1];
	char notice[AP_GUILD_MAX_NOTICE_LENGTH + 1];
	enum ap_guild_rank rank;
	time_t creation_date;
	uint32_t max_member_count;
	uint32_t union_id;
	uint32_t guild_mark_tid;
	uint32_t guild_mark_color;
	uint32_t total_battle_point;
};

struct as_guild {
	struct as_guild_db * db;
	boolean await_create;
};

/** \brief struct as_character_db attachment. */
struct as_guild_character_db {
	char guild_id[AP_GUILD_MAX_ID_LENGTH + 1];
	enum ap_guild_member_rank rank;
	time_t join_date;
};

struct as_guild_module * as_guild_create_module();

void as_guild_add_callback(
	struct as_guild_module * mod,
	enum as_guild_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

size_t as_guild_attach_data(
	struct as_guild_module * mod,
	enum as_guild_module_data_index data_index,
	size_t data_size,
	ap_module_t callback_module,
	ap_module_default_t constructor,
	ap_module_default_t destructor);

struct as_guild * as_guild_get(struct as_guild_module * mod, struct ap_guild * guild);

/**
 * Retrieve attached data.
 * \param[in] character Character database object pointer.
 * 
 * \return Character database object attachment.
 */
struct as_guild_character_db * as_guild_get_character_db(
	struct as_guild_module * mod,
	struct as_character_db * character);

void as_guild_reflect(struct as_guild_module * mod, struct ap_guild * guild);

void as_guild_commit(struct as_guild_module * mod, boolean force);

void as_guild_send_packet(struct as_guild_module * mod, struct ap_guild * guild);

void as_guild_send_packet_with_exception(
	struct as_guild_module * mod,
	struct ap_guild * guild,
	struct ap_character * character);

void as_guild_broadcast_members(
	struct as_guild_module * mod,
	struct ap_guild * guild,
	struct ap_character * character);

END_DECLS

#endif /* _AS_GUILD_H_ */