#ifndef _AS_CHARACTER_H_
#define _AS_CHARACTER_H_

#include <time.h>

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_admin.h"
#include "public/ap_character.h"
#include "public/ap_define.h"

#include "server/as_database.h"

#define AS_CHARACTER_MODULE_NAME "AgsmCharacter"

#define AS_CHARACTER_MAKE_RNG_GUID(MODULE_ID, RNG_ID) \
	((((uint64_t)(MODULE_ID) & 0xFF) << 56) | ((RNG_ID) & 0xFFFFFFFFFFFFFF))

#define AS_CHARACTER_STATUS_DEAD                         0x0001
#define AS_CHARACTER_STATUS_DO_NOT_RECEIVE_EXPERIENCE    0x0002
#define AS_CHARACTER_STATUS_AUTO_ATTACK_AFTER_SKILL_CAST 0x0004

BEGIN_DECLS

enum as_character_database_id {
	AS_CHARACTER_DB_END = 0,
	AS_CHARACTER_DB_NAME,
	AS_CHARACTER_DB_CREATION_DATE,
	AS_CHARACTER_DB_STATUS,
	AS_CHARACTER_DB_SLOT,
	AS_CHARACTER_DB_TID,
	AS_CHARACTER_DB_LEVEL,
	AS_CHARACTER_DB_GOLD,
	AS_CHARACTER_DB_CHARISMA,
	AS_CHARACTER_DB_HP,
	AS_CHARACTER_DB_MP,
	AS_CHARACTER_DB_EXP,
	AS_CHARACTER_DB_HAIR,
	AS_CHARACTER_DB_FACE,
	AS_CHARACTER_DB_NICKNAME,
	AS_CHARACTER_DB_PVP_WIN,
	AS_CHARACTER_DB_PVP_LOSE,
	AS_CHARACTER_DB_VILLAIN_POINTS,
	AS_CHARACTER_DB_CRIMINAL_STATUS,
	AS_CHARACTER_DB_CRIMINAL_DURATION,
	AS_CHARACTER_DB_ROGUE_DURATION,
	AS_CHARACTER_DB_BOUND_REGION_ID,
	AS_CHARACTER_DB_POSITION,
	AS_CHARACTER_DB_UNIQUE_ACCESSORY_DROP_BONUS,
	/** \brief [DEPRECATED]. */
	AS_CHARACTER_DB_QUICK_SLOTS,
	/** \brief [DEPRECATED]. */
	AS_CHARACTER_DB_SKILLS,
	AS_CHARACTER_DB_RNG_STATE_LIST,
};

enum as_character_flag_bits {
	AS_CHARACTER_BLOCKED = 0x01,
};

enum as_character_module_data_index {
	AS_CHARACTER_MDI_DATABASE,
};

enum as_character_callback_id {
	/** \brief Decode database data. */
	AS_CHARACTER_CB_DECODE,
	/** \brief Encode database data. */
	AS_CHARACTER_CB_ENCODE,
	/** \brief Initialize character with database state.  */
	AS_CHARACTER_CB_LOAD,
	/** \brief Reflect runtime character object 
	 *         on database state. */
	AS_CHARACTER_CB_REFLECT,
	/** \brief Copy database object. */
	AS_CHARACTER_CB_COPY,
};

struct as_character_rng_state {
	uint64_t guid;
	uint64_t state;
};

struct as_character_db {
	char name[AP_CHARACTER_MAX_NAME_LENGTH + 1];
	time_t creation_date;
	uint64_t status;
	uint8_t slot;
	uint32_t tid;
	uint32_t level;
	uint64_t inventory_gold;
	uint32_t charisma;
	uint32_t hp;
	uint32_t mp;
	uint64_t exp;
	uint32_t hair;
	uint32_t face;
	char nickname[AP_CHARACTER_MAX_NICKNAME_SIZE];
	uint32_t pvp_win;
	uint32_t pvp_lose;
	uint32_t villain_points;
	boolean criminal_status;
	uint32_t criminal_duration;
	uint32_t rogue_duration;
	uint32_t bound_region_id;
	struct au_pos position;
	float unique_acc_drop_bonus;
	struct ap_admin rng_state_admin;

	struct as_character_db * next;
};

struct as_character_move_input {
	boolean not_processed;
	struct au_pos pos;
	struct au_pos dst_pos;
	uint8_t move_flags;
	uint8_t move_direction;
	uint8_t next_action_type;
};

struct as_character_id_link {
	uint32_t id;
	struct as_character_id_link * next;
};

struct as_character {
	struct as_character_db * db;
	struct as_character_move_input move_input;
	uint64_t next_recover_tick;
	boolean is_attacking;
	uint32_t attack_target_id;
	uint64_t next_attack_movement_tick;
	struct as_character_id_link * id_link;
};

/** \brief AS_CHARACTER_CB_DECODE callback data. */
struct as_character_cb_decode {
	struct as_character_db * character;
	struct as_database_codec * codec;
	uint32_t module_id;
	uint32_t field_id;
};

/** \brief AS_CHARACTER_CB_ENCODE callback data. */
struct as_character_cb_encode {
	struct as_character_db * character;
	struct as_database_codec * codec;
};

/** \brief AS_CHARACTER_CB_LOAD callback data. */
struct as_character_cb_load {
	struct ap_character * character;
	struct as_character_db * db;
};

/** \brief AS_CHARACTER_CB_REFLECT callback data. */
struct as_character_cb_reflect {
	struct ap_character * character;
	struct as_character_db * db;
};

/** \brief AS_CHARACTER_CB_COPY callback data. */
struct as_character_cb_copy {
	struct as_character_db * src;
	struct as_character_db * dst;
};

struct as_character_module * as_character_create_module();

void as_character_add_callback(
	struct as_character_module * mod,
	enum as_character_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

size_t as_character_attach_data(
	struct as_character_module * mod,
	enum as_character_module_data_index data_index,
	size_t data_size,
	ap_module_t callback_module,
	ap_module_default_t constructor,
	ap_module_default_t destructor);

/**
 * Set database encode/decode functions.
 * \param[in] module_id Unique module database id.
 * \param[in] cb_decode Decode callback. 
 *                      If data is successfully read, 
 *						this callback should return FALSE.
 *                      Returning TRUE implies that encoded 
 *                      database id does not match a valid 
 *                      module/field id.
 * \param[in] cb_encode Encode callback.
 *
 * \return TRUE if successful. Otherwise FALSE.
 */
boolean as_character_set_database_stream(
	struct as_character_module * mod,
	uint32_t module_id,
	ap_module_t callback_module,
	ap_module_default_t cb_decode,
	ap_module_default_t cb_encode);

struct as_character_db * as_character_decode(
	struct as_character_module * mod,
	struct as_database_codec * codec);

boolean as_character_encode(
	struct as_character_module * mod,
	struct as_database_codec * codec,
	struct as_character_db * character);

struct as_character_db * as_character_copy_database(
	struct as_character_module * mod,
	struct as_character_db * character);

boolean as_character_reserve_name(
	struct as_character_module * mod,
	const char * character_name,
	const char * account_id);

boolean as_character_free_name(
	struct as_character_module * mod,
	const char * character_name);

boolean as_character_is_name_reserved(
	struct as_character_module * mod,
	const char * character_name);

const char * as_character_get_account_id(
	struct as_character_module * mod,
	const char * character_name);

boolean as_character_is_name_valid(
	struct as_character_module * mod,
	const char * character_name);

struct ap_character * as_character_from_db(
	struct as_character_module * mod,
	struct as_character_db * db);

void as_character_reflect(
	struct as_character_module * mod,
	struct ap_character * character);

struct as_character * as_character_get(
	struct as_character_module * mod,
	struct ap_character * character);

struct as_character_db * as_character_new_db(struct as_character_module * mod);

void as_character_free_db(
	struct as_character_module * mod, 
	struct as_character_db * character);

struct as_character_rng_state * as_character_get_rng_state(
	struct as_character_db * character,
	uint64_t guid);

uint32_t as_character_generate_random(
	struct as_character_db * character,
	uint64_t guid,
	uint32_t upper_bound);

boolean as_character_consume_gold(
	struct as_character_module * mod,
	struct ap_character * character,
	uint64_t amount);

boolean as_character_deposit_gold(
	struct as_character_module * mod,
	struct ap_character * character,
	uint64_t amount);

END_DECLS

#endif /* _AS_CHARACTER_H_ */
