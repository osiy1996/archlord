#include "server/as_item.h"

#include "core/log.h"
#include "core/malloc.h"

#include "public/ap_admin.h"
#include "public/ap_item_convert.h"
#include "public/ap_skill.h"
#include "public/ap_tick.h"

#include "server/as_account.h"
#include "server/as_character.h"
#include "server/as_database.h"
#include "server/as_player.h"
#include "server/as_skill.h"

#include <assert.h>

#define ITEM_ID_OFFSET 10000

#define DB_STREAM_MODULE_ID 1
#define MAKE_ID(ID) AS_DATABASE_MAKE_ID(DB_STREAM_MODULE_ID, ID)

struct id_list {
	uint32_t id;
	struct id_list * next;
};

struct as_item_module {
	struct ap_module_instance instance;
	struct ap_character_module * ap_character;
	struct ap_item_module * ap_item;
	struct ap_item_convert_module * ap_item_convert;
	struct ap_skill_module * ap_skill;
	struct ap_tick_module * ap_tick;
	struct as_account_module * as_account;
	struct as_character_module * as_character;
	struct as_database_module * as_database;
	struct as_player_module * as_player;
	struct as_skill_module * as_skill;
	uint32_t item_id_counter;
	struct id_list * id_freelist;
	struct id_list * id_in_use;
	struct as_item_db * freelist;
	size_t item_ad_offset;
	size_t character_db_offset;
	size_t account_attachment_offset;
	size_t player_session_offset;
	uint32_t stream_module_id[AS_DATABASE_MAX_STREAM_COUNT];
	uint32_t stream_count;
};

static struct as_item_db * getnewdb(
	struct as_item_module * mod)
{
	struct as_item_db * i = mod->freelist;
	if (i) {
		mod->freelist = i->next;
		ap_module_construct_module_data(mod, AS_ITEM_MDI_DATABASE, i);
	}
	else {
		i = ap_module_create_module_data(mod, AS_ITEM_MDI_DATABASE);
	}
	return i;
}

static void freedb(
	struct as_item_module * mod,
	struct as_item_db * db)
{
	ap_module_destruct_module_data(mod, 
		AS_ITEM_MDI_DATABASE, db);
	db->next = mod->freelist;
	mod->freelist = db;
}

static boolean itemctor(struct as_item_module * mod, struct ap_item * item)
{
	struct id_list * id = mod->id_freelist;
	if (id) {
		mod->id_freelist = id->next;
	}
	else {
		if (mod->item_id_counter == UINT32_MAX) {
			/* Should be unreachable unless characters are 
			 * not discarded properly. */
			item->id = 0;
			return TRUE;
		}
		id = alloc(sizeof(*id));
		id->id = mod->item_id_counter++;
	}
	id->next = mod->id_in_use;
	mod->id_in_use = id;
	item->id = id->id;
	return TRUE;
}

static boolean itemdtor(struct as_item_module * mod, struct ap_item * item)
{
	struct id_list * id = mod->id_in_use;
	struct id_list * prev = NULL;
	while (id) {
		if (id->id == item->id) {
			if (prev)
				prev->next = id->next;
			else
				mod->id_in_use = id->next;
			break;
		}
		prev = id;
		id = id->next;
	}
	assert(id != NULL);
	if (id) {
		id->next = mod->id_freelist;
		mod->id_freelist = id;
	}
	item->id = 0;
	as_item_get(mod, item)->db = NULL;
	return TRUE;
}

static boolean cbchardbdtor(
	struct as_item_module * mod, 
	struct as_character_db * db)
{
	struct as_item_character_db * dbitem = 
		as_item_get_character_db(mod, db);
	uint32_t i;
	for (i = 0; i < dbitem->item_count; i++)
		as_item_free_db(mod, dbitem->items[i]);
	return TRUE;
}

static boolean cbaccountdtor(
	struct as_item_module * mod, 
	struct as_account * account)
{
	struct as_item_account_attachment * attachment = 
		as_item_get_account_attachment(mod, account);
	uint32_t i;
	for (i = 0; i < attachment->item_count; i++)
		as_item_free_db(mod, attachment->items[i]);
	return TRUE;
}

static boolean cbdecodechar(
	struct as_item_module * mod, 
	struct as_character_cb_decode * cb)
{
	struct as_item_character_db * db = 
		as_item_get_character_db(mod, cb->character);
	if (cb->module_id != DB_STREAM_MODULE_ID)
		return TRUE;
	switch (cb->field_id) {
	case AS_ITEM_CHARACTER_DB_ITEM_LIST: {
		uint32_t count = 0;
		uint32_t i;
		if (!AS_DATABASE_DECODE(cb->codec, count)) {
			ERROR("Failed to decode character item count (%s).",
				cb->character->name);
			return TRUE;
		}
		for (i = 0; i < count; i++) {
			struct as_item_db * item = as_item_decode(mod, cb->codec);
			if (!item) {
				ERROR("Failed to decode character item (%s).",
					cb->character->name);
				assert(0);
				return TRUE;
			}
			if (db->item_count >= AS_ITEM_MAX_CHARACTER_ITEM_COUNT) {
				WARN("Character exceeded max. number of items (%s).",
					cb->character->name);
				as_item_free_db(mod, item);
			}
			else {
				db->items[db->item_count++] = item;
			}
		}
		break;
	}
	default:
		assert(0);
		return TRUE;
	}
	return FALSE;
}

static boolean cbencodechar(
	struct as_item_module * mod, 
	struct as_character_cb_decode * cb)
{
	boolean result = TRUE;
	uint32_t i;
	struct as_item_character_db * db = 
		as_item_get_character_db(mod, cb->character);
	result &= AS_DATABASE_ENCODE(cb->codec, 
		MAKE_ID(AS_ITEM_CHARACTER_DB_ITEM_LIST),
		db->item_count);
	for (i = 0; i < db->item_count; i++)
		result &= as_item_encode(mod, cb->codec, db->items[i]);
	return result;
}

static boolean cbdecodeaccount(
	struct as_item_module * mod, 
	struct as_account_cb_decode * cb)
{
	struct as_item_account_attachment * attachment = 
		as_item_get_account_attachment(mod, cb->account);
	if (cb->module_id != DB_STREAM_MODULE_ID)
		return TRUE;
	switch (cb->field_id) {
	case AS_ITEM_ACCOUNT_DB_ITEM_LIST: {
		uint32_t count = 0;
		uint32_t i;
		if (!AS_DATABASE_DECODE(cb->codec, count)) {
			ERROR("Failed to decode bank item count (%s).", cb->account->account_id);
			return TRUE;
		}
		for (i = 0; i < count; i++) {
			struct as_item_db * item = as_item_decode(mod, cb->codec);
			if (!item) {
				ERROR("Failed to decode bank item (%s).", cb->account->account_id);
				assert(0);
				return TRUE;
			}
			if (attachment->item_count >= AS_ITEM_MAX_ACCOUNT_ITEM_COUNT) {
				WARN("Account exceeded max. number of items (%s).", cb->account->account_id);
				as_item_free_db(mod, item);
			}
			else {
				attachment->items[attachment->item_count++] = item;
			}
		}
		break;
	}
	default:
		assert(0);
		return TRUE;
	}
	return FALSE;
}

static boolean cbencodeaccount(
	struct as_item_module * mod, 
	struct as_account_cb_decode * cb)
{
	boolean result = TRUE;
	uint32_t i;
	struct as_item_account_attachment * attachment = 
		as_item_get_account_attachment(mod, cb->account);
	result &= AS_DATABASE_ENCODE(cb->codec, MAKE_ID(AS_ITEM_ACCOUNT_DB_ITEM_LIST),
		attachment->item_count);
	for (i = 0; i < attachment->item_count; i++)
		result &= as_item_encode(mod, cb->codec, attachment->items[i]);
	return result;
}

static boolean cbaccountcopy(
	struct as_item_module * mod, 
	struct as_account_cb_copy * cb)
{
	const struct as_item_account_attachment * src = 
		as_item_get_account_attachment(mod, cb->src);
	struct as_item_account_attachment * dst = 
		as_item_get_account_attachment(mod, cb->dst);
	uint32_t i;
	dst->item_count = src->item_count;
	for (i = 0; i < src->item_count; i++)
		dst->items[i] = as_item_copy_database(mod, src->items[i]);
	return TRUE;
}

static boolean cbreflectchar(
	struct as_item_module * mod, 
	struct as_character_cb_reflect * cb)
{
	struct ap_item_character * ichar = ap_item_get_character(mod->ap_item, cb->character);
	uint32_t i;
	struct ap_grid * grids[] = {
		ichar->inventory,
		ichar->equipment,
		ichar->trade,
		ichar->sub_inventory,
		ichar->cash_inventory,
		ichar->bank };
	for (i = 0; i < COUNT_OF(grids); i++) {
		uint32_t j;
		for (j = 0; j < grids[i]->grid_count; j++) {
			struct ap_item * item = ap_grid_get_object_by_index(grids[i], j);
			if (item)
				as_item_reflect_db(mod, item);
		}
	}
	return TRUE;
}

static boolean cbcharcopy(
	struct as_item_module * mod, 
	struct as_character_cb_copy * cb)
{
	struct as_item_character_db * src = as_item_get_character_db(mod, cb->src);
	struct as_item_character_db * dst = as_item_get_character_db(mod, cb->dst);
	uint32_t i;
	dst->item_count = src->item_count;
	for (i = 0; i < src->item_count; i++)
		dst->items[i] = as_item_copy_database(mod, src->items[i]);
	return TRUE;
}

static boolean cbcharupdatefactor(
	struct as_item_module * mod,
	struct ap_character_cb_update_factor * cb)
{
	struct ap_character * c = cb->character;
	struct ap_factor *  f = &c->factor;
	struct ap_factor *  fpt = &c->factor_point;
	struct ap_factor *  fper = &c->factor_percent;
	struct ap_item_character * ic = ap_item_get_character(mod->ap_item, c);
	if (CHECK_BIT(cb->update_flags, AP_FACTORS_BIT_MOVEMENT_SPEED)) {
		switch (ic->factor_adjust_movement) {
		case AP_ITEM_CHARACTER_FACTOR_ADJUST_MOUNT:
			f->char_status.movement = 
				ic->mount_movement;
			f->char_status.movement_fast = 
				ic->mount_movement_fast;
			break;
		}
	}
	if (CHECK_BIT(cb->update_flags, AP_FACTORS_BIT_PHYSICAL_DMG)) {
		/** \todo 
		 * Check if character is transformed. */
		switch (ic->factor_adjust_attack) {
		case AP_ITEM_CHARACTER_FACTOR_ADJUST_NONE:
			break;
		case AP_ITEM_CHARACTER_FACTOR_ADJUST_COMBAT:
			f->damage.min.physical = 
				fpt->damage.min.physical +
				ic->combat_weapon_min_damage;
			f->damage.max.physical = 
				fpt->damage.max.physical +
				ic->combat_weapon_max_damage;
			f->damage.min.physical *= 
				fper->damage.min.physical / 100.0f;
			f->damage.max.physical *= 
				fper->damage.max.physical / 100.0f;
			ap_character_adjust_combat_point(c);
			break;
		case AP_ITEM_CHARACTER_FACTOR_ADJUST_MOUNT:
			f->damage.min.physical = 
				fpt->damage.min.physical +
				ic->mount_weapon_min_damage;
			f->damage.max.physical = 
				fpt->damage.max.physical +
				ic->mount_weapon_max_damage;
			f->damage.min.physical *= 
				fper->damage.min.physical / 100.0f;
			f->damage.max.physical *= 
				fper->damage.max.physical / 100.0f;
			ap_character_adjust_combat_point(c);
			break;
		}
	}
	if (CHECK_BIT(cb->update_flags, AP_FACTORS_BIT_ATTACK_RANGE)) {
		/** \todo 
		 * Check if character is transformed. */
		switch (ic->factor_adjust_attack) {
		case AP_ITEM_CHARACTER_FACTOR_ADJUST_NONE:
			f->attack.attack_range = c->temp->factor.attack.attack_range;
			break;
		case AP_ITEM_CHARACTER_FACTOR_ADJUST_COMBAT:
			f->attack.attack_range = 
				ic->combat_weapon_attack_range;
			break;
		case AP_ITEM_CHARACTER_FACTOR_ADJUST_MOUNT:
			f->attack.attack_range = 
				ic->mount_weapon_attack_range;
			break;
		}
	}
	if (CHECK_BIT(cb->update_flags, AP_FACTORS_BIT_ATTACK_SPEED)) {
		/** \todo 
		 * Check if character is transformed. */
		switch (ic->factor_adjust_attack) {
		case AP_ITEM_CHARACTER_FACTOR_ADJUST_NONE:
			f->attack.attack_speed = c->temp->factor.attack.attack_speed;
			break;
		case AP_ITEM_CHARACTER_FACTOR_ADJUST_COMBAT:
			f->attack.attack_speed = (int)(ic->combat_weapon_attack_speed * 
				fper->attack.attack_speed / 100.0f);
			ap_character_fix_attack_speed(c);
			break;
		case AP_ITEM_CHARACTER_FACTOR_ADJUST_MOUNT:
			f->attack.attack_speed = ic->mount_weapon_attack_speed;
			ap_character_fix_attack_speed(c);
			break;
		}
	}
	return TRUE;
}

static boolean onregister(
	struct as_item_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_character, AP_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_item, AP_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_item_convert, AP_ITEM_CONVERT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_skill, AP_SKILL_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_tick, AP_TICK_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_account, AS_ACCOUNT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_character, AS_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_database, AS_DATABASE_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_player, AS_PLAYER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_skill, AS_SKILL_MODULE_NAME);
	mod->item_ad_offset = ap_item_attach_data(mod->ap_item, AP_ITEM_MDI_ITEM,
		sizeof(struct as_item), mod, itemctor, itemdtor);
	if (mod->item_ad_offset == SIZE_MAX) {
		ERROR("Failed to attach item data.");
		return FALSE;
	}
	mod->character_db_offset = as_character_attach_data(mod->as_character, 
		AS_CHARACTER_MDI_DATABASE, sizeof(struct as_item_character_db), 
		mod, NULL, cbchardbdtor);
	mod->account_attachment_offset = as_account_attach_data(mod->as_account, 
		AS_ACCOUNT_MDI_ACCOUNT, sizeof(struct as_item_account_attachment), 
		mod, NULL, cbaccountdtor);
	if (!as_character_set_database_stream(mod->as_character, DB_STREAM_MODULE_ID,
			mod, cbdecodechar, cbencodechar)) {
		ERROR("Failed to set character database stream.");
		return FALSE;
	}
	if (!as_account_set_database_stream(mod->as_account, DB_STREAM_MODULE_ID,
			mod, cbdecodeaccount, cbencodeaccount)) {
		ERROR("Failed to set account database stream.");
		return FALSE;
	}
	ap_character_add_callback(mod->ap_character, AP_CHARACTER_CB_UPDATE_FACTOR, mod, cbcharupdatefactor);
	as_account_add_callback(mod->as_account, AS_ACCOUNT_CB_COPY, mod, cbaccountcopy);
	as_character_add_callback(mod->as_character, AS_CHARACTER_CB_REFLECT, mod, cbreflectchar);
	as_character_add_callback(mod->as_character, AS_CHARACTER_CB_COPY, mod, cbcharcopy);
	mod->player_session_offset = as_player_attach_data(mod->as_player, 
		AS_PLAYER_MDI_SESSION, sizeof(struct as_item_player_session_attachment), 
		mod, NULL, NULL);
	return TRUE;
}

static void onshutdown(struct as_item_module * mod)
{
	struct id_list * id = mod->id_freelist;
	struct as_item_db * db = mod->freelist;
	if (!mod)
		return;
	assert(mod->id_in_use == NULL);
	while (id) {
		struct id_list * next = id->next;
		dealloc(id);
		id = next;
	}
	while (db) {
		struct as_item_db * next = db->next;
		dealloc(db);
		db = next;
	}
}

struct as_item_module * as_item_create_module()
{
	struct as_item_module * mod = ap_module_instance_new(AS_ITEM_MODULE_NAME,
		sizeof(*mod), onregister, NULL, NULL, onshutdown);
	mod->item_id_counter = ITEM_ID_OFFSET;
	ap_module_set_module_data(mod, AS_ITEM_MDI_DATABASE,
		sizeof(struct as_item_db), NULL, NULL);
	return mod;
}

void as_item_add_callback(
	struct as_item_module * mod,
	enum as_item_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	ap_module_add_callback(mod, id, callback_module, callback);
}

size_t as_item_attach_data(
	struct as_item_module * mod,
	enum as_item_module_data_index data_index,
	size_t data_size,
	ap_module_t callback_module,
	ap_module_default_t constructor,
	ap_module_default_t destructor)
{
	return ap_module_attach_data(mod, data_index, data_size, 
		callback_module, constructor, destructor);
}

boolean as_item_set_database_stream(
	struct as_item_module * mod,
	uint32_t module_id,
	ap_module_t callback_module,
	ap_module_default_t cb_decode,
	ap_module_default_t cb_encode)
{
	uint32_t i;
	assert(cb_decode != NULL && cb_encode != NULL);
	if (mod->stream_count >= AS_DATABASE_MAX_STREAM_COUNT)
		return FALSE;
	for (i = 0; i < mod->stream_count; i++) {
		if (mod->stream_module_id[i] == module_id) {
			ERROR("Module database id is not unique (%u).",
				module_id);
			return FALSE;
		}
	}
	as_item_add_callback(mod, AS_ITEM_CB_DECODE, callback_module, cb_decode);
	as_item_add_callback(mod, AS_ITEM_CB_ENCODE, callback_module, cb_encode);
	mod->stream_module_id[mod->stream_count++] = module_id;
	return TRUE;
}

struct as_item_db * as_item_new_db(struct as_item_module * mod)
{
	return getnewdb(mod);
}

void as_item_free_db(struct as_item_module * mod, struct as_item_db * db)
{
	freedb(mod, db);
}

struct as_item_db * as_item_decode(
	struct as_item_module * mod,
	struct as_database_codec * codec)
{
	struct as_item_db * db = getnewdb(mod);
	uint32_t id;
	boolean result = TRUE;
	boolean end = FALSE;
	struct as_item_cb_decode cb = { db, codec };
	while (!end && as_database_decode(codec, &id)) {
		switch (id) {
		case AS_ITEM_DB_END:
			end = TRUE;
			break;
		case AS_ITEM_DB_TID:
			result &= AS_DATABASE_DECODE(codec, db->tid);
			break;
		case AS_ITEM_DB_STACK_COUNT:
			result &= AS_DATABASE_DECODE(codec, db->stack_count);
			break;
		case AS_ITEM_DB_STATUS:
			result &= AS_DATABASE_DECODE(codec, db->status);
			break;
		case AS_ITEM_DB_STATUS_FLAGS:
			result &= AS_DATABASE_DECODE(codec, db->status_flags);
			break;
		case AS_ITEM_DB_LAYER:
			result &= AS_DATABASE_DECODE(codec, db->layer);
			break;
		case AS_ITEM_DB_ROW:
			result &= AS_DATABASE_DECODE(codec, db->row);
			break;
		case AS_ITEM_DB_COLUMN:
			result &= AS_DATABASE_DECODE(codec, db->column);
			break;
		case AS_ITEM_DB_PHYSICAL_RANK:
			result &= as_database_advance_codec(codec, 4);
			break;
		case AS_ITEM_DB_SOCKET_COUNT:
			result &= as_database_advance_codec(codec, 4);
			break;
		case AS_ITEM_DB_CONVERT_TID:
			result &= as_database_advance_codec(codec, 40);
			break;
		case AS_ITEM_DB_DURABILITY:
			result &= AS_DATABASE_DECODE(codec, db->durability);
			break;
		case AS_ITEM_DB_MAX_DURABILITY:
			result &= AS_DATABASE_DECODE(codec, db->max_durability);
			break;
		case AS_ITEM_DB_OPTION_TID:
			result &= as_database_read_decoded(codec, 
				db->option_tid, sizeof(db->option_tid));
			break;
		case AS_ITEM_DB_IN_USE:
			result &= AS_DATABASE_DECODE(codec, db->in_use);
			break;
		case AS_ITEM_DB_REMAIN_TIME:
			result &= AS_DATABASE_DECODE(codec, db->remain_time);
			break;
		case AS_ITEM_DB_EXPIRE_TIME: {
			uint64_t expire = 0;
			result &= AS_DATABASE_DECODE(codec, expire);
			db->expire_time = (time_t)expire;
			break;
		}
		case AS_ITEM_DB_AUCTION_ID:
			result &= AS_DATABASE_DECODE(codec, db->auction_id);
			break;
		case AS_ITEM_DB_AUCTION_PRICE:
			result &= AS_DATABASE_DECODE(codec, db->auction_price);
			break;
		case AS_ITEM_DB_AUCTION_STATUS:
			result &= AS_DATABASE_DECODE(codec, db->auction_status);
			break;
		case AS_ITEM_DB_AUCTION_EXPIRE: {
			uint64_t expire = 0;
			result &= AS_DATABASE_DECODE(codec, expire);
			db->auction_expire = (time_t)expire;
			break;
		}
		default:
			cb.module_id = AS_DATABASE_GET_MODULE_ID(id);
			cb.field_id = AS_DATABASE_GET_FIELD_ID(id);
			if (ap_module_enum_callback(mod,
					AS_ITEM_CB_DECODE, &cb)) {
				ERROR("Invalid decoded id (%u).", id);
				freedb(mod, db);
				assert(0);
				return NULL;
			}
		}
	}
	if (!result) {
		freedb(mod, db);
		assert(0);
		return NULL;
	}
	return db;
}

boolean as_item_encode(
	struct as_item_module * mod,
	struct as_database_codec * codec,
	const struct as_item_db * item)
{
	struct as_item_cb_encode cb = { item, codec };
	boolean result = TRUE;
	result &= AS_DATABASE_ENCODE(codec, 
		AS_ITEM_DB_TID,
		item->tid);
	result &= AS_DATABASE_ENCODE(codec, 
		AS_ITEM_DB_STACK_COUNT,
		item->stack_count);
	result &= AS_DATABASE_ENCODE(codec, 
		AS_ITEM_DB_STATUS,
		item->status);
	result &= AS_DATABASE_ENCODE(codec, 
		AS_ITEM_DB_STATUS_FLAGS,
		item->status_flags);
	result &= AS_DATABASE_ENCODE(codec, 
		AS_ITEM_DB_LAYER,
		item->layer);
	result &= AS_DATABASE_ENCODE(codec, 
		AS_ITEM_DB_ROW,
		item->row);
	result &= AS_DATABASE_ENCODE(codec, 
		AS_ITEM_DB_COLUMN,
		item->column);
	result &= AS_DATABASE_ENCODE(codec, 
		AS_ITEM_DB_PHYSICAL_RANK,
		item->physical_rank);
	result &= AS_DATABASE_ENCODE(codec, 
		AS_ITEM_DB_DURABILITY,
		item->durability);
	result &= AS_DATABASE_ENCODE(codec, 
		AS_ITEM_DB_MAX_DURABILITY,
		item->max_durability);
	result &= as_database_encode(codec, 
		AS_ITEM_DB_OPTION_TID,
		item->option_tid,
		sizeof(item->option_tid));
	result &= AS_DATABASE_ENCODE(codec, 
		AS_ITEM_DB_IN_USE,
		item->in_use);
	result &= AS_DATABASE_ENCODE(codec, 
		AS_ITEM_DB_REMAIN_TIME,
		item->remain_time);
	result &= AS_DATABASE_ENCODE(codec, 
		AS_ITEM_DB_EXPIRE_TIME,
		item->expire_time);
	result &= AS_DATABASE_ENCODE(codec, 
		AS_ITEM_DB_AUCTION_ID,
		item->auction_id);
	result &= AS_DATABASE_ENCODE(codec, 
		AS_ITEM_DB_AUCTION_PRICE,
		item->auction_price);
	result &= AS_DATABASE_ENCODE(codec, 
		AS_ITEM_DB_AUCTION_STATUS,
		item->auction_status);
	result &= AS_DATABASE_ENCODE(codec, 
		AS_ITEM_DB_AUCTION_EXPIRE,
		item->auction_expire);
	result &= ap_module_enum_callback(mod,
		AS_ITEM_CB_ENCODE, &cb);
	result &= as_database_encode(codec, 
		AS_ITEM_DB_END, NULL, 0);
	return TRUE;
}

struct ap_item * as_item_generate(struct as_item_module * mod, uint32_t tid)
{
	struct ap_item * item = ap_item_create(mod->ap_item, tid);
	struct as_item_cb_init_generated cb = { item };
	uint32_t i;
	if (!item)
		return NULL;
	item->update_flags = AP_ITEM_UPDATE_ALL;
	item->stack_count = 1;
	item->option_count = item->temp->option_count;
	for (i = 0; i < item->temp->option_count; i++) {
		item->option_tid[i] = item->temp->option_tid[i];
		item->options[i] = item->temp->options[i];
	}
	if (!ap_module_enum_callback(mod, AS_ITEM_CB_INIT_GENERATED, &cb)) {
		ap_item_free(mod->ap_item, item);
		return NULL;
	}
	return item;
}

struct ap_item * as_item_from_db(
	struct as_item_module * mod, 
	struct as_item_db * db)
{
	struct ap_item * item = ap_item_create(mod->ap_item, db->tid);
	struct as_item_cb_load cb = { item, db };
	uint32_t i;
	if (!item)
		return NULL;
	item->stack_count = db->stack_count;
	item->status = db->status;
	item->status_flags = db->status_flags;
	item->grid_pos[AP_ITEM_GRID_POS_TAB] = db->layer;
	item->grid_pos[AP_ITEM_GRID_POS_ROW] = db->row;
	item->grid_pos[AP_ITEM_GRID_POS_COLUMN] = db->column;
	item->factor.item.durability = db->durability;
	item->factor.item.max_durability = db->max_durability;
	assert(sizeof(db->option_tid) == sizeof(item->option_tid));
	for (i = 0; i < COUNT_OF(db->option_tid); i++) {
		uint16_t tid = db->option_tid[i];
		struct ap_item_option_template * option;
		if (!tid)
			break;
		option = ap_item_get_option_template(mod->ap_item, tid);
		if (!option) {
			WARN("Invalid item option template id (item = [%u] %s, option_tid = %u).",
				item->tid, item->temp->name, tid);
			continue;
		}
		item->option_tid[item->option_count] = tid;
		item->options[item->option_count++] = option;
	}
	item->in_use = db->in_use;
	item->remain_time = db->remain_time;
	item->expire_time = db->expire_time;
	as_item_get(mod, item)->db = db;
	if (!ap_module_enum_callback(mod, AS_ITEM_CB_LOAD, &cb)) {
		ap_item_free(mod->ap_item, item);
		return NULL;
	}
	return item;
}

void as_item_reflect_db(struct as_item_module * mod, struct ap_item * item)
{
	struct as_item * is = as_item_get(mod, item);
	struct as_item_db * db = is->db;
	struct as_item_cb_reflect cb = { item, db };
	if (!db)
		return;
	if (item->update_flags & AP_ITEM_UPDATE_TID)
		db->tid = item->tid;
	if (item->update_flags & AP_ITEM_UPDATE_STACK_COUNT)
		db->stack_count = item->stack_count;
	if (item->update_flags & AP_ITEM_UPDATE_STATUS)
		db->status = item->status;
	if (item->update_flags & AP_ITEM_UPDATE_STATUS_FLAGS)
		db->status_flags = item->status_flags;
	if (item->update_flags & AP_ITEM_UPDATE_GRID_POS) {
		db->layer = item->grid_pos[AP_ITEM_GRID_POS_TAB];
		db->row = item->grid_pos[AP_ITEM_GRID_POS_ROW];
		db->column = item->grid_pos[AP_ITEM_GRID_POS_COLUMN];
	}
	if (item->update_flags & AP_ITEM_UPDATE_DURABILITY)
		db->durability = item->factor.item.durability;
	if (item->update_flags & AP_ITEM_UPDATE_MAX_DURABILITY)
		db->max_durability = item->factor.item.max_durability;
	if (item->update_flags & AP_ITEM_UPDATE_OPTION_TID) {
		assert(sizeof(item->option_tid) == sizeof(db->option_tid));
		memcpy(db->option_tid, item->option_tid, sizeof(db->option_tid));
	}
	if (item->update_flags & AP_ITEM_UPDATE_IN_USE)
		db->in_use = item->in_use;
	if (item->update_flags & AP_ITEM_UPDATE_REMAIN_TIME)
		db->remain_time = item->remain_time;
	if (item->update_flags & AP_ITEM_UPDATE_EXPIRE_TIME)
		db->expire_time = item->expire_time;
	item->update_flags = AP_ITEM_UPDATE_NONE;
	ap_module_enum_callback(mod, AS_ITEM_CB_REFLECT, &cb);
}

struct as_item_db * as_item_copy_database(
	struct as_item_module * mod, 
	struct as_item_db * src)
{
	struct as_item_cb_copy cb = { 0 };
	struct as_item_db * copy = as_item_new_db(mod);
	memcpy(copy, src, sizeof(*src));
	copy->next = NULL;
	cb.src = src;
	cb.dst = copy;
	ap_module_enum_callback(mod, AS_ITEM_CB_COPY, &cb);
	return copy;
}

struct as_item * as_item_get(struct as_item_module * mod, struct ap_item * item)
{
	return ap_module_get_attached_data(item, mod->item_ad_offset);
}

struct as_item_character_db * as_item_get_character_db(
	struct as_item_module * mod,
	struct as_character_db * character)
{
	return ap_module_get_attached_data(character, mod->character_db_offset);
}

struct as_item_account_attachment * as_item_get_account_attachment(
	struct as_item_module * mod,
	struct as_account * account)
{
	return ap_module_get_attached_data(account, mod->account_attachment_offset);
}

struct as_item_player_session_attachment * as_item_get_player_session_attachment(
	struct as_item_module * mod,
	struct as_player_session * session)
{
	return ap_module_get_attached_data(session, mod->player_session_offset);
}

struct ap_item * as_item_try_generate(
	struct as_item_module * mod,
	struct ap_character * character,
	enum ap_item_status item_status,
	uint32_t item_tid)
{
	struct as_character * cs = as_character_get(mod->as_character, character);
	struct as_character_db * dbchar = cs->db;
	struct as_item_character_db * dbichar = NULL;
	struct ap_item * item;
	struct ap_grid * grid;
	uint16_t layer = UINT16_MAX;
	uint16_t row = UINT16_MAX;
	uint16_t col = UINT16_MAX;
	if (dbchar) {
		dbichar = as_item_get_character_db(mod, dbchar);
		if (dbichar->item_count >= AS_ITEM_MAX_CHARACTER_ITEM_COUNT)
			return NULL;
	}
	switch (item_status) {
	case AP_ITEM_STATUS_INVENTORY:
		grid = ap_item_get_character(mod->ap_item, character)->inventory;
		break;
	default:
		return NULL;
	}
	if (!ap_grid_get_empty(grid, &layer, &row, &col))
		return NULL;
	item = as_item_generate(mod, item_tid);
	if (!item)
		return NULL;
	item->character_id = character->id;
	item->status = AP_ITEM_STATUS_INVENTORY;
	item->grid_pos[AP_ITEM_GRID_POS_TAB] = layer;
	item->grid_pos[AP_ITEM_GRID_POS_ROW] = row;
	item->grid_pos[AP_ITEM_GRID_POS_COLUMN] = col;
	ap_grid_add_item(grid, layer, row, col, 
		AP_GRID_ITEM_TYPE_ITEM, item->id, item->tid, item);
	if (dbichar) {
		struct as_item_db * idb = as_item_new_db(mod);
		as_item_get(mod, item)->db = idb;
		dbichar->items[dbichar->item_count++] = idb;
	}
	return item;
}

boolean as_item_try_add(
	struct as_item_module * mod,
	struct ap_character * character,
	enum ap_item_status item_status,
	struct ap_item * item)
{
	struct as_character * cs = as_character_get(mod->as_character, character);
	struct as_character_db * dbchar = cs->db;
	struct as_item_character_db * dbichar = NULL;
	struct ap_grid * grid;
	uint16_t layer = UINT16_MAX;
	uint16_t row = UINT16_MAX;
	uint16_t col = UINT16_MAX;
	/* Bank items are managed through account. */
	assert(item_status != AP_ITEM_STATUS_BANK);
	if (dbchar) {
		dbichar = as_item_get_character_db(mod, dbchar);
		if (dbichar->item_count >= AS_ITEM_MAX_CHARACTER_ITEM_COUNT)
			return FALSE;
	}
	switch (item_status) {
	case AP_ITEM_STATUS_INVENTORY:
		grid = ap_item_get_character(mod->ap_item, character)->inventory;
		break;
	default:
		return FALSE;
	}
	if (!ap_grid_get_empty(grid, &layer, &row, &col))
		return FALSE;
	item->character_id = character->id;
	item->status = AP_ITEM_STATUS_INVENTORY;
	item->grid_pos[AP_ITEM_GRID_POS_TAB] = layer;
	item->grid_pos[AP_ITEM_GRID_POS_ROW] = row;
	item->grid_pos[AP_ITEM_GRID_POS_COLUMN] = col;
	ap_grid_add_item(grid, layer, row, col, 
		AP_GRID_ITEM_TYPE_ITEM, item->id, item->tid, item);
	if (dbichar) {
		struct as_item_db * idb = as_item_new_db(mod);
		as_item_get(mod, item)->db = idb;
		dbichar->items[dbichar->item_count++] = idb;
	}
	return TRUE;
}

void as_item_delete(
	struct as_item_module * mod,
	struct ap_character * character, 
	struct ap_item * item)
{
	struct as_item * is = as_item_get(mod, item);
	struct as_item_db * db = is->db;
	ap_item_make_remove_packet(mod->ap_item, item);
	as_player_send_packet(mod->as_player, character);
	if (db) {
		if (item->status == AP_ITEM_STATUS_BANK) {
			struct as_player_character * playerattachment = 
				as_player_get_character_ad(mod->as_player, character);
			struct as_item_account_attachment * attachment = 
				as_item_get_account_attachment(mod, playerattachment->account);
			uint32_t i;
			for (i = 0; i < attachment->item_count; i++) {
				if (attachment->items[i] == db) {
					memmove(&attachment->items[i], &attachment->items[i + 1],
						((size_t)--attachment->item_count * sizeof(attachment->items[0])));
					break;
				}
			}
		}
		else {
			struct as_character * cs = as_character_get(mod->as_character, character);
			struct as_character_db * dbchar = cs->db;
			if (dbchar) {
				struct as_item_character_db * dbichar =
					as_item_get_character_db(mod, dbchar);
				uint32_t i;
				for (i = 0; i < dbichar->item_count; i++) {
					if (dbichar->items[i] == db) {
						memmove(&dbichar->items[i], &dbichar->items[i + 1],
							((size_t)--dbichar->item_count * sizeof(dbichar->items[0])));
						break;
					}
				}
			}
		}
		as_item_free_db(mod, db);
		is->db = NULL;
	}
	ap_item_remove_from_grid(mod->ap_item, character, item);
	ap_item_free(mod->ap_item, item);
}

void as_item_consume(
	struct as_item_module * mod,
	struct ap_character * character,
	struct ap_item * item,
	uint32_t stack_count)
{
	assert(item->stack_count >= stack_count);
	if (item->stack_count > stack_count) {
		ap_item_decrease_stack_count(item, stack_count);
		ap_item_make_update_packet(mod->ap_item, item, AP_ITEM_UPDATE_STACK_COUNT);
		as_player_send_packet(mod->as_player, character);
		if (CHECK_BIT(item->status_flags, AP_ITEM_STATUS_FLAG_CASH_PPCARD) &&
			AP_ITEM_IS_CASH_ITEM(item)) {
			item->status_flags &= ~AP_ITEM_STATUS_FLAG_CASH_PPCARD;
			item->update_flags |= AP_ITEM_UPDATE_STATUS_FLAGS;
			ap_item_make_update_packet(mod->ap_item, item, 0);
			as_player_send_packet(mod->as_player, character);
		}
	}
	else {
		as_item_delete(mod, character, item);
	}
}

boolean as_item_consume_by_template_id(
	struct as_item_module * mod,
	struct ap_character * character,
	uint32_t template_id,
	uint32_t stack_count)
{
	const struct ap_item_template * temp = ap_item_get_template(mod->ap_item, 
		template_id);
	enum ap_item_status status[2] = { 0 };
	uint32_t count = 0;
	uint32_t i;
	uint32_t stack = 0;
	if (!temp)
		return FALSE;
	if (temp->cash_item_type != AP_ITEM_CASH_ITEM_TYPE_NONE) {
		status[count++] = AP_ITEM_STATUS_CASH_INVENTORY;
	}
	else {
		status[count++] = AP_ITEM_STATUS_INVENTORY;
		status[count++] = AP_ITEM_STATUS_SUB_INVENTORY;
	}
	for (i = 0; i < count; i++) {
		struct ap_grid * grid = ap_item_get_character_grid(mod->ap_item, 
			character, status[i]);
		uint32_t j;
		for (j = 0; j < grid->grid_count; j++) {
			struct ap_item * item = ap_grid_get_object_by_index(grid, j);
			if (item && item->tid == template_id)
				stack += item->stack_count;
		}
	}
	if (stack < stack_count)
		return FALSE;
	for (i = 0; i < count && stack_count; i++) {
		struct ap_grid * grid = ap_item_get_character_grid(mod->ap_item, 
			character, status[i]);
		uint32_t j;
		for (j = 0; j < grid->grid_count && stack_count; j++) {
			struct ap_item * item = ap_grid_get_object_by_index(grid, j);
			if (item && item->tid == template_id) {
				uint32_t consume = MIN(stack_count, item->stack_count);
				as_item_consume(mod, character, item, consume);
				stack_count -= consume;
			}
		}
	}
	assert(stack_count == 0);
	return TRUE;
}
