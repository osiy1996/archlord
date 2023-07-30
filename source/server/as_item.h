#ifndef _AS_ITEM_H_
#define _AS_ITEM_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_define.h"
#include "public/ap_item.h"
#include "public/ap_module.h"

#define AS_ITEM_MODULE_NAME "AgsmItem"

#define AS_ITEM_MAX_CHARACTER_ITEM_COUNT 256
#define AS_ITEM_MAX_COOLDOWN_COUNT 128
#define AS_ITEM_MAX_ACCOUNT_ITEM_COUNT 256

BEGIN_DECLS

struct as_database_codec;

enum as_item_database_id {
	AS_ITEM_DB_END = 0,
	AS_ITEM_DB_TID,
	AS_ITEM_DB_STACK_COUNT,
	AS_ITEM_DB_STATUS,
	AS_ITEM_DB_STATUS_FLAGS,
	AS_ITEM_DB_LAYER,
	AS_ITEM_DB_ROW,
	AS_ITEM_DB_COLUMN,
	/** \brief [DEPRECATED] Moved to item convert module. */
	AS_ITEM_DB_PHYSICAL_RANK,
	/** \brief [DEPRECATED] Moved to item convert module. */
	AS_ITEM_DB_SOCKET_COUNT,
	/** \brief [DEPRECATED] Moved to item convert module. */
	AS_ITEM_DB_CONVERT_TID,
	AS_ITEM_DB_DURABILITY,
	AS_ITEM_DB_MAX_DURABILITY,
	AS_ITEM_DB_IN_USE,
	AS_ITEM_DB_REMAIN_TIME,
	AS_ITEM_DB_EXPIRE_TIME,
	AS_ITEM_DB_AUCTION_ID,
	AS_ITEM_DB_AUCTION_PRICE,
	AS_ITEM_DB_AUCTION_STATUS,
	AS_ITEM_DB_AUCTION_EXPIRE,
	AS_ITEM_DB_OPTION_TID,
};

enum as_item_character_database_id {
	AS_ITEM_CHARACTER_DB_ITEM_LIST,
};

enum as_item_account_database_id {
	AS_ITEM_ACCOUNT_DB_ITEM_LIST,
};

enum as_item_module_data_index {
	AS_ITEM_MDI_DATABASE,
};

enum as_item_callback_id {
	/** \brief Decode database data. */
	AS_ITEM_CB_DECODE,
	/** \brief Encode database data. */
	AS_ITEM_CB_ENCODE,
	/** \brief Initialize generated item. */
	AS_ITEM_CB_INIT_GENERATED,
	/** \brief Initialize item with database state.  */
	AS_ITEM_CB_LOAD,
	/** \brief Reflect item object on database state. */
	AS_ITEM_CB_REFLECT,
	/** \brief Copy item database object. */
	AS_ITEM_CB_COPY,
};

struct as_item_db {
	uint32_t tid;
	uint32_t stack_count;
	enum ap_item_status status;
	uint32_t status_flags;
	uint16_t layer;
	uint16_t row;
	uint16_t column;
	uint32_t physical_rank;
	uint32_t durability;
	uint32_t max_durability;
	uint16_t option_tid[AP_ITEM_OPTION_MAX_COUNT];
	boolean in_use;
	uint64_t remain_time;
	time_t expire_time;
	uint32_t auction_id;
	uint32_t auction_price;
	uint32_t auction_status;
	time_t auction_expire;

	struct as_item_db * next;
};

/** \brief struct as_character_db attachment. */
struct as_item_character_db {
	uint32_t item_count;
	struct as_item_db * items[AS_ITEM_MAX_CHARACTER_ITEM_COUNT];
};

/** \brief struct as_account attachment. */
struct as_item_account_attachment {
	uint32_t item_count;
	struct as_item_db * items[AS_ITEM_MAX_ACCOUNT_ITEM_COUNT];
};

struct as_item {
	struct as_item_db * db;
};

struct as_item_player_session_attachment {
	struct ap_item_cooldown cooldowns[AS_ITEM_MAX_COOLDOWN_COUNT];
	uint32_t cooldown_count;
	uint64_t hp_potion_cooldown_end_tick;
	uint64_t mp_potion_cooldown_end_tick;
};

/**
 * \brief AS_ITEM_CB_DECODE callback data.
 */
struct as_item_cb_decode {
	struct as_item_db * item;
	struct as_database_codec * codec;
	uint32_t module_id;
	uint32_t field_id;
};

/**
 * \brief AS_ITEM_CB_ENCODE callback data.
 */
struct as_item_cb_encode {
	const struct as_item_db * item;
	struct as_database_codec * codec;
};

/**
 * \brief AS_ITEM_CB_INIT_GENERATED callback data.
 */
struct as_item_cb_init_generated {
	struct ap_item * item;
};

/**
 * \brief AS_ITEM_CB_LOAD callback data.
 */
struct as_item_cb_load {
	struct ap_item * item;
	struct as_item_db * db;
};

/**
 * \brief AS_ITEM_CB_REFLECT callback data.
 */
struct as_item_cb_reflect {
	struct ap_item * item;
	struct as_item_db * db;
};

/** \brief AS_ITEM_CB_COPY callback data. */
struct as_item_cb_copy {
	struct as_item_db * src;
	struct as_item_db * dst;
};

struct as_item_module * as_item_create_module();

void as_item_add_callback(
	struct as_item_module * mod,
	enum as_item_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

size_t as_item_attach_data(
	struct as_item_module * mod,
	enum as_item_module_data_index data_index,
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
boolean as_item_set_database_stream(
	struct as_item_module * mod,
	uint32_t module_id,
	ap_module_t callback_module,
	ap_module_default_t cb_decode,
	ap_module_default_t cb_encode);

struct as_item_db * as_item_new_db(struct as_item_module * mod);

void as_item_free_db(struct as_item_module * mod, struct as_item_db * db);

struct as_item_db * as_item_decode(
	struct as_item_module * mod,
	struct as_database_codec * codec);

boolean as_item_encode(
	struct as_item_module * mod,
	struct as_database_codec * codec,
	const struct as_item_db * item);

/**
 * Generate new item.
 * Default stack count of generated items is 1.
 * \param[in] tid Item template id.
 *
 * \return Item pointer if successful. Otherwise NULL.
 */
struct ap_item * as_item_generate(struct as_item_module * mod, uint32_t tid);

struct ap_item * as_item_from_db(
	struct as_item_module * mod, 
	struct as_item_db * db);

void as_item_reflect_db(struct as_item_module * mod, struct ap_item * item);

struct as_item_db * as_item_copy_database(
	struct as_item_module * mod, 
	struct as_item_db * src);

struct as_item * as_item_get(struct as_item_module * mod, struct ap_item * item);

/**
 * Retrieve attached data.
 * \param[in] character Character pointer.
 * 
 * \return Character database object attachment.
 */
struct as_item_character_db * as_item_get_character_db(
	struct as_item_module * mod,
	struct as_character_db * character);

struct as_item_account_attachment * as_item_get_account_attachment(
	struct as_item_module * mod,
	struct as_account * account);

struct as_item_player_session_attachment * as_item_get_player_session_attachment(
	struct as_item_module * mod,
	struct as_player_session * session);

/**
 * Attempt to generate a new item in character inventory.
 * \param[in,out] character   Character pointer.
 * \param[in]     item_status Target inventory type.
 * \param[in]     item_tid    Item template id.
 *
 * \return Generated item pointer if successful. Otherwise NULL.
 */
struct ap_item * as_item_try_generate(
	struct as_item_module * mod,
	struct ap_character * character,
	enum ap_item_status item_status,
	uint32_t item_tid);

boolean as_item_try_add(
	struct as_item_module * mod,
	struct ap_character * character,
	enum ap_item_status item_status,
	struct ap_item * item);

void as_item_delete(
	struct as_item_module * mod,
	struct ap_character * character, 
	struct ap_item * item);

void as_item_consume(
	struct as_item_module * mod,
	struct ap_character * character,
	struct ap_item * item,
	uint32_t stack_count);

END_DECLS

#endif /* _AS_ITEM_H_ */
