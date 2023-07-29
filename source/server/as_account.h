#ifndef _AS_ACCOUNT_H_
#define _AS_ACCOUNT_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_item.h"
#include "public/ap_login.h"

#include "server/as_character.h"

#define AS_ACCOUNT_MODULE_NAME "AgsmAccount"

#define AS_ACCOUNT_MAX_CHARACTER 3
#define AS_ACCOUNT_PW_SALT_SIZE 16
#define AS_ACCOUNT_PW_HASH_SIZE 32

BEGIN_DECLS

struct as_account;

typedef void (*as_account_deferred_t)(
	ap_module_t mod,
	const char * account_id, 
	struct as_account * account,
	void * user_data);

enum as_account_database_id {
	AS_ACCOUNT_DB_ACCOUNT_ID,
	AS_ACCOUNT_DB_PW_SALT,
	AS_ACCOUNT_DB_PW_HASH,
	AS_ACCOUNT_DB_EMAIL,
	AS_ACCOUNT_DB_CREATION_DATE,
	AS_ACCOUNT_DB_FLAGS,
	AS_ACCOUNT_DB_EXTRA_BANK_SLOTS,
	AS_ACCOUNT_DB_BANK_GOLD,
	AS_ACCOUNT_DB_CHANTRA_COINS,
	AS_ACCOUNT_DB_CHARACTER,
};

enum as_account_flag_bits {
	AS_ACCOUNT_BLOCKED = 0x01,
};

enum as_account_module_data_index {
	AS_ACCOUNT_MDI_ACCOUNT,
};

enum as_account_callback_id {
	AS_ACCOUNT_CB_PREPROCESS_CHARACTER,
	AS_ACCOUNT_CB_PREPROCESS_COMPLETE,
	AS_ACCOUNT_CB_PRE_COMMIT,
	/** \brief Decode database data. */
	AS_ACCOUNT_CB_DECODE,
	/** \brief Encode database data. */
	AS_ACCOUNT_CB_ENCODE,
};

struct as_account {
	char account_id[AP_LOGIN_MAX_ID_LENGTH + 1];
	uint8_t pw_salt[AS_ACCOUNT_PW_SALT_SIZE];
	uint8_t pw_hash[AS_ACCOUNT_PW_HASH_SIZE];
	char email[128];
	time_t creation_date;
	uint32_t flags;
	uint8_t extra_bank_slots;
	uint64_t bank_gold;
	uint64_t chantra_coins;
	uint32_t character_count;
	struct as_character_db * characters[AS_ACCOUNT_MAX_CHARACTER];

	uint64_t last_commit;
	/** 
	 * This state indicates that the account should be 
	 * commited in the same transaction with other accounts 
	 * that are in this state.
	 * 
	 * This state is set when a modification affects 
	 * more than one account. Examples of this are 
	 * player trades, adding/removing friends, auction 
	 * house purchases, etc. */
	boolean commit_linked;
	boolean committing;
	boolean unloading;
	uint32_t refcount;
};

/** \brief AS_ACCOUNT_CB_PREPROCESS_CHARACTER callback data. */
struct as_account_cb_preprocess_char {
	struct as_account * account;
	struct as_character_db * character;
};

struct as_account_cb_pre_commit {
	struct as_account * account;
};

/**
 * \brief AS_ACCOUNT_CB_DECODE callback data.
 */
struct as_account_cb_decode {
	struct as_account * account;
	struct as_database_codec * codec;
	uint32_t module_id;
	uint32_t field_id;
};

/**
 * \brief AS_ACCOUNT_CB_ENCODE callback data.
 */
struct as_account_cb_encode {
	const struct as_account * account;
	struct as_database_codec * codec;
};

struct as_account_module * as_account_create_module();

struct as_account * as_account_new(struct as_account_module * mod);

void as_account_free(struct as_account_module * mod, struct as_account * account);

boolean as_account_validate_account_id(const char * account_id);

void as_account_add_callback(
	struct as_account_module * mod,
	enum as_account_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

size_t as_account_attach_data(
	struct as_account_module * mod,
	enum as_account_module_data_index data_index,
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
boolean as_account_set_database_stream(
	struct as_account_module * mod,
	uint32_t module_id,
	ap_module_t callback_module,
	ap_module_default_t cb_decode,
	ap_module_default_t cb_encode);

boolean as_account_preprocess(struct as_account_module * mod);

struct as_account * as_account_load_from_db(
	struct as_account_module * mod,
	PGconn * conn, 
	const char * account_id);

struct as_account * as_account_load_from_cache(
	struct as_account_module * mod,
	const char * account_id,
	boolean is_reference_held);

void * as_account_load_deferred(
	struct as_account_module * mod,
	const char * account_id,
	ap_module_t callback_module,
	as_account_deferred_t callback,
	size_t user_data_size);

/*
 * Creates an account in database.
 *
 * After an account is created in database, 
 * its account id needs to be cached by 
 * calling as_account_cache_id (in main thread).
 */
boolean as_account_create_in_db(
	struct as_account_module * mod,
	PGconn * conn,
	struct as_account * account);

/*
 * Cache account id that was created 
 * externally (i.e. in database).
 *
 * This function will ONLY cache the account id, 
 * if account data needs to be cached, 
 * use as_account_cache.
 */
boolean as_account_cache_id(struct as_account_module * mod, const char * account_id);

/*
 * Cache account data.
 * 
 * This function will only cache account data.
 * If the account is newly created, its account id 
 * needs to be cached separately with as_account_cache_id.
 *
 * If reference is set to TRUE, account's reference 
 * count will be set to 1.
 */
boolean as_account_cache(
	struct as_account_module * mod,
	struct as_account * account, 
	boolean reference);

void as_account_reference(struct as_account * account);

/*
 * Release cached account.
 */
void as_account_release(struct as_account * account);

void as_account_commit(struct as_account_module * mod, boolean force);

END_DECLS

#endif /* _AS_ACCOUNT_H_ */
