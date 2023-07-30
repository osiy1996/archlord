#include "server/as_character.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"

#include "public/ap_admin.h"
#include "public/ap_character.h"
#include "public/ap_module.h"
#include "public/ap_optimized_packet2.h"
#include "public/ap_pvp.h"
#include "public/ap_tick.h"

#include "server/as_database.h"

#include "vendor/pcg/pcg_basic.h"

#include <assert.h>

#define CHAR_ID_OFFSET 10000

struct as_character_module {
	struct ap_module_instance instance;
	struct ap_character_module * ap_character;
	struct ap_factors_module * ap_factors;
	struct ap_pvp_module * ap_pvp;
	struct ap_tick_module * ap_tick;
	uint32_t character_id_counter;
	struct as_character_id_link * id_freelist;
	uint32_t in_use_id_count;
	size_t character_ad_offset;
	struct ap_admin name_admin;
	struct as_character_db * freelist;
	uint32_t stream_module_id[AS_DATABASE_MAX_STREAM_COUNT];
	uint32_t stream_count;
	pcg32_random_t rng;
};

static inline uint32_t getrandom(
	struct as_character_module * mod,
	uint32_t upperbound)
{
	return pcg32_boundedrand_r(&mod->rng, upperbound);
}

static boolean cbchardbctor(
	struct as_character_module * mod,
	struct as_character_db * db)
{
	ap_admin_init(&db->rng_state_admin, sizeof(struct as_character_rng_state), 32);
	return TRUE;
}

static boolean cbchardbdtor(
	struct as_character_module * mod,
	struct as_character_db * db)
{
	ap_admin_destroy(&db->rng_state_admin);
	return TRUE;
}

static boolean isattackmissed(
	struct as_character_module * mod,
	struct ap_character * c,
	struct ap_character * t,
	int defense_penalty)
{
	int32_t ar = c->factor.char_point_max.attack_rating;
	int32_t dr = t->factor.char_point_max.defense_rating;
	float arplusdr = (float)(ar + dr);
	int prehitrate = 0;
	int hitrate;
	uint32_t clevel = ap_character_get_level(c);
	uint32_t tlevel = ap_character_get_level(t);
	if (arplusdr != 0.f) {
		prehitrate = (int)(((float)clevel / (clevel + tlevel)) * 
			(float)((float)ar / arplusdr) * 290.0f);
	}
	if (ap_character_is_pc(c)) {
		if (tlevel > clevel)
			prehitrate = 90 - 7 * (tlevel - clevel);
		hitrate = prehitrate + c->factor.attack.accuracy;
		if (hitrate > 95)
			hitrate = 95;
		else if (hitrate < 10)
			hitrate = 10;
	}
	else {
		if (tlevel > clevel) {
			int x = 85 - 3 * (tlevel - clevel);
			if (x > 0 && x > prehitrate)
				prehitrate = x;
		}
		hitrate = prehitrate + c->factor.attack.accuracy;
		if (hitrate > 95)
			hitrate = 95;
		else if (hitrate < 30)
			hitrate = 30;
	}
	return !((int)getrandom(mod, 100) < hitrate);
}

static struct as_character_db * getnewdb(
	struct as_character_module * mod)
{
	struct as_character_db * c = mod->freelist;
	if (c) {
		mod->freelist = c->next;
		ap_module_construct_module_data(mod, AS_CHARACTER_MDI_DATABASE, c);
	}
	else {
		c = ap_module_create_module_data(mod, AS_CHARACTER_MDI_DATABASE);
	}
	return c;
}

static void freedb(
	struct as_character_module * mod,
	struct as_character_db * cdb)
{
	ap_module_destruct_module_data(mod, AS_CHARACTER_MDI_DATABASE, cdb);
	cdb->next = mod->freelist;
	mod->freelist = cdb;
}

static void freeid(
	struct as_character_module * mod, 
	struct as_character_id_link * id)
{
	assert(mod->in_use_id_count != 0);
	id->next = mod->id_freelist;
	mod->id_freelist = id;
	mod->in_use_id_count--;
}

static boolean char_ctor(
	struct as_character_module * mod, 
	struct ap_character * character)
{
	struct as_character_id_link * id = mod->id_freelist;
	if (id) {
		uint8_t nextseq = (uint8_t)(id->id >> 24) + 1;
		mod->id_freelist = id->next;
		id->id = ((uint32_t)nextseq << 24) | (id->id & 0x00FFFFFF);
	}
	else {
		if (mod->character_id_counter == 0xFFFFFF) {
			/* Should be unreachable unless characters are 
			 * not discarded properly. */
			character->id = 0;
			return TRUE;
		}
		id = alloc(sizeof(*id));
		id->id = 0x01000000 | mod->character_id_counter++;
	}
	character->id = id->id;
	as_character_get(mod, character)->id_link = id;
	mod->in_use_id_count++;
	return TRUE;
}

static boolean char_dtor(
	struct as_character_module * mod, 
	struct ap_character * character)
{
	struct as_character * cs = as_character_get(mod, character);
	if (cs->id_link) {
		freeid(mod, cs->id_link);
		character->id = 0;
		cs->id_link = NULL;
	}
	cs->db = NULL;
	return TRUE;
}

static boolean cbfreeid(
	struct as_character_module * mod, 
	struct ap_character_cb_free_id * cb)
{
	struct as_character * cs = as_character_get(mod, cb->character);
	if (cs->id_link) {
		freeid(mod, cs->id_link);
		cs->id_link = NULL;
	}
	return TRUE;
}

static boolean cbcharsetlevel(
	struct as_character_module * mod, 
	struct ap_character_cb_set_level * cb)
{
	struct ap_character * c = cb->character;
	struct as_character_db * db = as_character_get(mod, c)->db;
	uint32_t level = c->factor.char_status.level;
	if (db)
		db->level = level;
	if (c->char_type & AP_CHARACTER_TYPE_PC) {
		ap_character_set_grow_up_factor(mod->ap_character, c, cb->prev_level, level);
		ap_factors_set_max_exp(&c->factor,
			ap_character_get_level_up_exp(mod->ap_character, level));
	}
	ap_character_update_factor(mod->ap_character, c, 0);
	return TRUE;
}

static boolean cbcharattemptattack(
	struct as_character_module * mod,
	struct ap_character_cb_attempt_attack * cb)
{
	struct ap_character * c = cb->character;
	struct ap_character * t = cb->target;
	struct ap_character_action_info * a = cb->action;
	int rate;
	if (CHECK_BIT(t->special_status, 
			AP_CHARACTER_SPECIAL_STATUS_NORMAL_ATK_INVINCIBLE)) {
		a->result = AP_CHARACTER_ACTION_RESULT_TYPE_EVADE;
		return FALSE;
	}
	rate = (int)MIN(t->factor.defense.rate.physical_block, 80.0f) - 
		a->defense_penalty;
	if ((int)getrandom(mod, 100) < rate) {
		a->result = AP_CHARACTER_ACTION_RESULT_TYPE_BLOCK;
		return FALSE;
	}
	if (isattackmissed(mod, c, t, a->defense_penalty)) {
		a->result = AP_CHARACTER_ACTION_RESULT_TYPE_MISS;
		return FALSE;
	}
	if (c->factor.char_type.cs == AU_CHARCLASS_TYPE_RANGER ||
		c->factor.char_type.cs == AU_CHARCLASS_TYPE_SCION ||
		c->factor.char_type.cs == AU_CHARCLASS_TYPE_MAGE ||
		c->temp->range_type == AP_CHARACTER_RANGE_TYPE_RANGE) {
		rate = (int)MIN(t->factor.attack.dodge_rate, 80.0f) - 
			a->defense_penalty;
		if ((int)getrandom(mod, 100) < rate) {
			a->result = AP_CHARACTER_ACTION_RESULT_TYPE_DODGE;
			return FALSE;
		}
	}
	else {
		rate = (int)MIN(t->factor.attack.evade_rate, 80.0f) - 
			a->defense_penalty;
		if ((int)getrandom(mod, 100) < rate) {
			a->result = AP_CHARACTER_ACTION_RESULT_TYPE_EVADE;
			return FALSE;
		}
	}
	return TRUE;
}

static boolean cbchardeath(
	struct as_character_module * mod, 
	struct ap_character_cb_death * cb)
{
	struct ap_character * c = cb->character;
	uint64_t flags = AP_CHARACTER_BIT_ACTION_STATUS;
	if (c->factor.char_point.hp != 0) {
		c->factor.char_point.hp = 0;
		flags |= AP_FACTORS_BIT_HP;
	}
	c->action_status = AP_CHARACTER_ACTION_STATUS_DEAD;
	ap_character_update(mod->ap_character, c, flags, FALSE);
	return TRUE;
}

static boolean onregister(
	struct as_character_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_character, AP_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_factors, AP_FACTORS_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_pvp, AP_PVP_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_tick, AP_TICK_MODULE_NAME);
	mod->character_ad_offset = ap_character_attach_data(mod->ap_character,
		AP_CHARACTER_MDI_CHAR, sizeof(struct as_character), mod,
		char_ctor, char_dtor);
	if (mod->character_ad_offset == SIZE_MAX) {
		ERROR("Failed to attach data.");
		return FALSE;
	}
	ap_character_add_callback(mod->ap_character, AP_CHARACTER_CB_FREE_ID, 
		mod, cbfreeid);
	ap_character_add_callback(mod->ap_character, AP_CHARACTER_CB_SET_LEVEL, 
		mod, cbcharsetlevel);
	ap_character_add_callback(mod->ap_character, 
		AP_CHARACTER_CB_ATTEMPT_ATTACK, mod, cbcharattemptattack);
	ap_character_add_callback(mod->ap_character, AP_CHARACTER_CB_DEATH, 
		mod, cbchardeath);
	return TRUE;
}

static void onshutdown(struct as_character_module * mod)
{
	struct as_character_id_link * id = mod->id_freelist;
	struct as_character_db * db = mod->freelist;
	assert(mod->in_use_id_count == 0);
	while (id) {
		struct as_character_id_link * next = id->next;
		dealloc(id);
		id = next;
	}
	while (db) {
		struct as_character_db * next = db->next;
		dealloc(db);
		db = next;
	}
	ap_admin_destroy(&mod->name_admin);
}

struct as_character_module * as_character_create_module()
{
	struct as_character_module * mod = ap_module_instance_new(AS_CHARACTER_MODULE_NAME,
		sizeof(*mod), onregister, NULL, NULL, onshutdown);
	ap_admin_init(&mod->name_admin, 
		AP_CHARACTER_MAX_NAME_LENGTH + 1, 128);
	pcg32_srandom_r(&mod->rng, (uint64_t)time(NULL), 
		(uint64_t)time);
	mod->character_id_counter = CHAR_ID_OFFSET;
	ap_module_set_module_data(mod, AS_CHARACTER_MDI_DATABASE, 
		sizeof(struct as_character_db), cbchardbctor, cbchardbdtor);
	return mod;
}

void as_character_add_callback(
	struct as_character_module * mod,
	enum as_character_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	ap_module_add_callback(mod, id, callback_module, callback);
}

size_t as_character_attach_data(
	struct as_character_module * mod,
	enum as_character_module_data_index data_index,
	size_t data_size,
	ap_module_t callback_module,
	ap_module_default_t constructor,
	ap_module_default_t destructor)
{
	return ap_module_attach_data(mod, data_index, data_size, 
		callback_module, constructor, destructor);
}

boolean as_character_set_database_stream(
	struct as_character_module * mod,
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
	as_character_add_callback(mod, AS_CHARACTER_CB_DECODE, 
		callback_module, cb_decode);
	as_character_add_callback(mod, AS_CHARACTER_CB_ENCODE, 
			callback_module, cb_encode);
	mod->stream_module_id[mod->stream_count++] = module_id;
	return TRUE;
}

struct as_character_db * as_character_decode(
	struct as_character_module * mod,
	struct as_database_codec * codec)
{
	struct as_character_db * c = getnewdb(mod);
	struct as_character_cb_decode cb = { c, codec };
	uint32_t id;
	boolean result = TRUE;
	boolean end = FALSE;
	while (!end && as_database_decode(codec, &id)) {
		switch (id) {
		case AS_CHARACTER_DB_END:
			end = TRUE;
			break;
		case AS_CHARACTER_DB_NAME:
			result &= as_database_read_decoded(codec, 
				c->name, sizeof(c->name));
			break;
		case AS_CHARACTER_DB_CREATION_DATE:
			result &= AS_DATABASE_DECODE(codec, 
				c->creation_date);
			break;
		case AS_CHARACTER_DB_STATUS:
			result &= AS_DATABASE_DECODE(codec, 
				c->status);
			break;
		case AS_CHARACTER_DB_SLOT:
			result &= AS_DATABASE_DECODE(codec, 
				c->slot);
			break;
		case AS_CHARACTER_DB_TID:
			result &= AS_DATABASE_DECODE(codec, 
				c->tid);
			break;
		case AS_CHARACTER_DB_LEVEL:
			result &= AS_DATABASE_DECODE(codec, 
				c->level);
			break;
		case AS_CHARACTER_DB_GOLD:
			result &= AS_DATABASE_DECODE(codec, 
				c->inventory_gold);
			break;
		case AS_CHARACTER_DB_CHARISMA:
			result &= AS_DATABASE_DECODE(codec, 
				c->charisma);
			break;
		case AS_CHARACTER_DB_HP:
			result &= AS_DATABASE_DECODE(codec, 
				c->hp);
			break;
		case AS_CHARACTER_DB_MP:
			result &= AS_DATABASE_DECODE(codec, 
				c->mp);
			break;
		case AS_CHARACTER_DB_EXP:
			result &= AS_DATABASE_DECODE(codec, 
				c->exp);
			break;
		case AS_CHARACTER_DB_HAIR:
			result &= AS_DATABASE_DECODE(codec, 
				c->hair);
			break;
		case AS_CHARACTER_DB_FACE:
			result &= AS_DATABASE_DECODE(codec, 
				c->face);
			break;
		case AS_CHARACTER_DB_NICKNAME:
			result &= as_database_read_decoded(codec, 
				c->nickname, sizeof(c->nickname));
			break;
		case AS_CHARACTER_DB_PVP_WIN:
			result &= AS_DATABASE_DECODE(codec, 
				c->pvp_win);
			break;
		case AS_CHARACTER_DB_PVP_LOSE:
			result &= AS_DATABASE_DECODE(codec, 
				c->pvp_lose);
			break;
		case AS_CHARACTER_DB_VILLAIN_POINTS:
			result &= AS_DATABASE_DECODE(codec, 
				c->villain_points);
			break;
		case AS_CHARACTER_DB_CRIMINAL_STATUS:
			result &= AS_DATABASE_DECODE(codec, 
				c->criminal_status);
			break;
		case AS_CHARACTER_DB_CRIMINAL_DURATION:
			result &= AS_DATABASE_DECODE(codec, 
				c->criminal_duration);
			break;
		case AS_CHARACTER_DB_ROGUE_DURATION:
			result &= AS_DATABASE_DECODE(codec, 
				c->rogue_duration);
			break;
		case AS_CHARACTER_DB_BOUND_REGION_ID:
			result &= AS_DATABASE_DECODE(codec, 
				c->bound_region_id);
			break;
		case AS_CHARACTER_DB_POSITION:
			result &= AS_DATABASE_DECODE(codec, 
				c->position);
			break;
		case AS_CHARACTER_DB_UNIQUE_ACCESSORY_DROP_BONUS:
			result &= AS_DATABASE_DECODE(codec, 
				c->unique_acc_drop_bonus);
			break;
		case AS_CHARACTER_DB_QUICK_SLOTS:
			result &= as_database_advance_codec(codec, 256);
			break;
		case AS_CHARACTER_DB_SKILLS:
			result &= as_database_advance_codec(codec, 1024);
			break;
		case AS_CHARACTER_DB_RNG_STATE_LIST: {
			uint32_t count = 0;
			uint32_t i;
			if (!AS_DATABASE_DECODE(codec, count)) {
				ERROR("Failed to decode character rng state count (%s).", c->name);
				assert(0);
				as_character_free_db(mod, c);
				return NULL;
			}
			for (i = 0; i < count; i++) {
				uint64_t guid = 0;
				uint64_t state = 0;
				struct as_character_rng_state * rng;
				if (!AS_DATABASE_DECODE(codec, guid) ||
					!AS_DATABASE_DECODE(codec, state)) {
					ERROR("Failed to decode character rng state (%s).", c->name);
					assert(0);
					as_character_free_db(mod, c);
					return NULL;
				}
				rng = ap_admin_add_object_by_id(&c->rng_state_admin, guid);
				if (!rng) {
					ERROR("Failed to add character rng state (%s, guid = %llu).", 
						c->name, guid);
					assert(0);
					as_character_free_db(mod, c);
					return NULL;
				}
				rng->guid = guid;
				rng->state = state;
			}
			break;
		}
		default:
			cb.module_id = AS_DATABASE_GET_MODULE_ID(id);
			cb.field_id = AS_DATABASE_GET_FIELD_ID(id);
			if (ap_module_enum_callback(mod, 
					AS_CHARACTER_CB_DECODE, &cb)) {
				ERROR("Invalid decoded id (%d).", id);
				assert(0);
				as_character_free_db(mod, c);
				return NULL;
			}
		}
	}
	if (!result) {
		assert(0);
		as_character_free_db(mod, c);
		return NULL;
	}
	return c;
}

boolean as_character_encode(
	struct as_character_module * mod,
	struct as_database_codec * codec,
	struct as_character_db * character)
{
	boolean result = TRUE;
	struct as_character_cb_encode cb = { character, codec };
	size_t index = 0;
	uint32_t count;
	struct as_character_rng_state * rng = NULL;
	result &= as_database_encode(codec, AS_CHARACTER_DB_NAME, 
		character->name, sizeof(character->name));
	result &= AS_DATABASE_ENCODE(codec, 
		AS_CHARACTER_DB_CREATION_DATE, character->creation_date);
	result &= AS_DATABASE_ENCODE(codec, 
		AS_CHARACTER_DB_STATUS, character->status);
	result &= AS_DATABASE_ENCODE(codec, 
		AS_CHARACTER_DB_SLOT, character->slot);
	result &= AS_DATABASE_ENCODE(codec, 
		AS_CHARACTER_DB_TID, character->tid);
	result &= AS_DATABASE_ENCODE(codec, 
		AS_CHARACTER_DB_LEVEL, character->level);
	result &= AS_DATABASE_ENCODE(codec, 
		AS_CHARACTER_DB_GOLD, character->inventory_gold);
	result &= AS_DATABASE_ENCODE(codec, 
		AS_CHARACTER_DB_CHARISMA, character->charisma);
	result &= AS_DATABASE_ENCODE(codec, 
		AS_CHARACTER_DB_HP, character->hp);
	result &= AS_DATABASE_ENCODE(codec, 
		AS_CHARACTER_DB_MP, character->mp);
	result &= AS_DATABASE_ENCODE(codec, 
		AS_CHARACTER_DB_EXP, character->exp);
	result &= AS_DATABASE_ENCODE(codec, 
		AS_CHARACTER_DB_HAIR, character->hair);
	result &= AS_DATABASE_ENCODE(codec, 
		AS_CHARACTER_DB_FACE, character->face);
	result &= as_database_encode(codec, AS_CHARACTER_DB_NICKNAME, 
		character->nickname, sizeof(character->nickname));
	result &= AS_DATABASE_ENCODE(codec, 
		AS_CHARACTER_DB_PVP_WIN, character->pvp_win);
	result &= AS_DATABASE_ENCODE(codec, 
		AS_CHARACTER_DB_PVP_LOSE, character->pvp_lose);
	result &= AS_DATABASE_ENCODE(codec, 
		AS_CHARACTER_DB_VILLAIN_POINTS, character->villain_points);
	result &= AS_DATABASE_ENCODE(codec, 
		AS_CHARACTER_DB_CRIMINAL_STATUS, 
		character->criminal_status);
	result &= AS_DATABASE_ENCODE(codec, 
		AS_CHARACTER_DB_CRIMINAL_DURATION, 
		character->criminal_duration);
	result &= AS_DATABASE_ENCODE(codec, 
		AS_CHARACTER_DB_ROGUE_DURATION, character->rogue_duration);
	result &= AS_DATABASE_ENCODE(codec, 
		AS_CHARACTER_DB_BOUND_REGION_ID, 
		character->bound_region_id);
	result &= AS_DATABASE_ENCODE(codec, 
		AS_CHARACTER_DB_POSITION, character->position);
	result &= AS_DATABASE_ENCODE(codec, 
		AS_CHARACTER_DB_UNIQUE_ACCESSORY_DROP_BONUS, 
		character->unique_acc_drop_bonus);
	count = ap_admin_get_object_count(&character->rng_state_admin);
	result &= AS_DATABASE_ENCODE(codec, 
		AS_CHARACTER_DB_RNG_STATE_LIST, 
		count);
	while (ap_admin_const_iterate_id(&character->rng_state_admin, &index, &rng)) {
		result &= as_database_write_encoded(codec, &rng->guid, sizeof(&rng->guid));
		result &= as_database_write_encoded(codec, &rng->state, sizeof(&rng->state));
	}
	result &= ap_module_enum_callback(mod, 
		AS_CHARACTER_CB_ENCODE, &cb);
	result &= as_database_encode(codec, 
		AS_CHARACTER_DB_END, NULL, 0);
	return TRUE;
}

struct as_character_db * as_character_copy_database(
	struct as_character_module * mod,
	struct as_character_db * character)
{
	struct as_character_cb_copy cb = { 0 };
	struct as_character_db * copy = as_character_new_db(mod);
	size_t index;
	struct as_character_rng_state * rng = NULL;
	memcpy(copy->name, character->name, sizeof(copy->name));
	copy->creation_date = character->creation_date;
	copy->status = character->status;
	copy->slot = character->slot;
	copy->tid = character->tid;
	copy->level = character->level;
	copy->inventory_gold = character->inventory_gold;
	copy->charisma = character->charisma;
	copy->hp = character->hp;
	copy->mp = character->mp;
	copy->exp = character->exp;
	copy->hair = character->hair;
	copy->face = character->face;
	memcpy(copy->nickname, character->nickname, sizeof(copy->nickname));
	copy->pvp_win = character->pvp_win;
	copy->pvp_lose = character->pvp_lose;
	copy->villain_points = character->villain_points;
	copy->criminal_status = character->criminal_status;
	copy->criminal_duration = character->criminal_duration;
	copy->rogue_duration = character->rogue_duration;
	copy->bound_region_id = character->bound_region_id;
	copy->position = character->position;
	copy->unique_acc_drop_bonus = character->unique_acc_drop_bonus;
	while (ap_admin_iterate_id(&character->rng_state_admin, &index, &rng)) {
		struct as_character_rng_state * rngcopy = ap_admin_add_object_by_id(
			&copy->rng_state_admin, rng->guid);
		if (rngcopy) {
			rngcopy->guid = rng->guid;
			rngcopy->state = rng->state;
		}
	}
	cb.src = character;
	cb.dst = copy;
	ap_module_enum_callback(mod, AS_CHARACTER_CB_COPY, &cb);
	return copy;
}

boolean as_character_reserve_name(
	struct as_character_module * mod,
	const char * character_name,
	const char * account_id)
{
	char * accid = ap_admin_add_object_by_name(&mod->name_admin, 
		character_name);
	if (!accid)
		return FALSE;
	strlcpy(accid, account_id, AP_CHARACTER_MAX_NAME_LENGTH + 1);
	return TRUE;
}

boolean as_character_free_name(
	struct as_character_module * mod,
	const char * character_name)
{
	return ap_admin_remove_object_by_name(&mod->name_admin, 
		character_name);
}

boolean as_character_is_name_reserved(
	struct as_character_module * mod,
	const char * character_name)
{
	return (ap_admin_get_object_by_name(&mod->name_admin, 
		character_name) != NULL);
}

const char * as_character_get_account_id(
	struct as_character_module * mod,
	const char * character_name)
{
	return ap_admin_get_object_by_name(&mod->name_admin, 
		character_name);
}

boolean as_character_is_name_valid(
	struct as_character_module * mod,
	const char * character_name)
{
	size_t len = strlen(character_name);
	const char * restricted_names[]	= { 
		"Admin", "admin", "GM", "gm", "Gm" };
	size_t i;
	if (len < 4 || len > 16)
		return FALSE;
	for (i = 0; i < len; i++) {
		char c = character_name[i];
		boolean num = (c >= '0' && c < '9');
		boolean upper = (c >= 'A' && c <= 'Z');
		boolean lower = (c >= 'a' && c <= 'z');
		if (i && upper)
			return FALSE;
		if (!num && !upper && !lower)
			return FALSE;
	}
	for (i = 0; i < COUNT_OF(restricted_names); i++) {
		if (strstr(character_name, restricted_names[i]))
			return FALSE;
	}
	return TRUE;

}

struct ap_character * as_character_from_db(
	struct as_character_module * mod,
	struct as_character_db * db)
{
	struct ap_character * c = ap_character_new(mod->ap_character);
	struct as_character_cb_load cb = { c, db };
	struct ap_character_template * temp = 
		ap_character_get_template(mod->ap_character, db->tid);
	assert(temp != NULL);
	ap_character_set_template(mod->ap_character, c, temp);
	as_character_get(mod, c)->db = db;
	strlcpy(c->name, db->name, sizeof(c->name));
	c->char_type = AP_CHARACTER_TYPE_PC |
		AP_CHARACTER_TYPE_ATTACKABLE |
		AP_CHARACTER_TYPE_TARGETABLE;
	if (db->status & AS_CHARACTER_STATUS_DEAD)
		c->action_status = AP_CHARACTER_ACTION_STATUS_DEAD;
	c->stop_experience = CHECK_BIT(db->status, 
		AS_CHARACTER_STATUS_DO_NOT_RECEIVE_EXPERIENCE);
	c->auto_attack_after_skill_cast = CHECK_BIT(db->status, 
		AS_CHARACTER_STATUS_AUTO_ATTACK_AFTER_SKILL_CAST);
	strlcpy(c->nickname, db->nickname, sizeof(c->nickname));
	c->pos = db->position;
	c->bound_region_id = db->bound_region_id;
	c->factor.char_status.level = 0;
	c->factor.char_status.murderer = db->villain_points;
	c->factor.char_point.hp = db->hp;
	c->factor.char_point.mp = db->mp;
	ap_character_set_level(mod->ap_character, c, db->level);
	ap_factors_set_current_exp(&c->factor, db->exp);
	c->login_status = AP_CHARACTER_STATUS_IN_LOGIN_PROCESS;
	c->criminal_status = db->criminal_status;
	c->inventory_gold = db->inventory_gold;
	c->face_index = db->face;
	c->hair_index = db->hair;
	if (!ap_module_enum_callback(mod, AS_CHARACTER_CB_LOAD, &cb)) {
		ap_character_free(mod->ap_character, c);
		return NULL;
	}
	return c;
}

void as_character_reflect(
	struct as_character_module * mod,
	struct ap_character * character)
{
	struct as_character_db * db = as_character_get(mod, character)->db;
	struct as_character_cb_reflect cb = { character, db };
	if (character->action_status == AP_CHARACTER_ACTION_STATUS_DEAD)
		db->status |= AS_CHARACTER_STATUS_DEAD;
	else
		db->status &= ~AS_CHARACTER_STATUS_DEAD;
	if (character->stop_experience)
		db->status |= AS_CHARACTER_STATUS_DO_NOT_RECEIVE_EXPERIENCE;
	else 
		db->status &= ~AS_CHARACTER_STATUS_DO_NOT_RECEIVE_EXPERIENCE;
	if (character->auto_attack_after_skill_cast)
		db->status |= AS_CHARACTER_STATUS_AUTO_ATTACK_AFTER_SKILL_CAST;
	else 
		db->status &= ~AS_CHARACTER_STATUS_AUTO_ATTACK_AFTER_SKILL_CAST;
	db->level = character->factor.char_status.level;
	db->inventory_gold = character->inventory_gold;
	db->villain_points = character->factor.char_status.murderer;
	db->hp = character->factor.char_point.hp;
	db->mp = character->factor.char_point.mp;
	db->exp = ap_factors_get_current_exp(&character->factor);
	db->position = character->pos;
	db->bound_region_id = character->bound_region_id;
	ap_module_enum_callback(mod, 
		AS_CHARACTER_CB_REFLECT, &cb);
}

struct as_character * as_character_get(
	struct as_character_module * mod,
	struct ap_character * character)
{
	return ap_module_get_attached_data(character, mod->character_ad_offset);
}

struct as_character_db * as_character_new_db(struct as_character_module * mod)
{
	return getnewdb(mod);
}

void as_character_free_db(
	struct as_character_module * mod,
	struct as_character_db * character)
{
	freedb(mod, character);
}

struct as_character_rng_state * as_character_get_rng_state(
	struct as_character_db * character,
	uint64_t guid)
{
	struct as_character_rng_state * rng = 
		ap_admin_get_object_by_id(&character->rng_state_admin, guid);
	if (!rng) {
		pcg32_random_t r;
		rng = ap_admin_add_object_by_id(&character->rng_state_admin, guid);
		if (!rng) {
			assert(0);
			return NULL;
		}
		rng->guid = guid;
		pcg32_srandom_r(&r, time(NULL) ^ (intptr_t)&printf, guid);
		rng->state = r.state;
	}
	return rng;
}

uint32_t as_character_generate_random(
	struct as_character_db * character,
	uint64_t guid,
	uint32_t upper_bound)
{
	struct as_character_rng_state * state = 
		as_character_get_rng_state(character, guid);
	pcg32_random_t rng = { 0 };
	uint32_t n;
	rng.state = state->state;
	rng.inc = (guid << 1u) | 1u;
	n = pcg32_boundedrand_r(&rng, upper_bound);
	/* Update state. */
	state->state = rng.state;
	return n;
}

boolean as_character_consume_gold(
	struct as_character_module * mod,
	struct ap_character * character,
	uint64_t amount)
{
	if (amount > character->inventory_gold)
		return FALSE;
	character->inventory_gold -= amount;
	ap_character_update(mod->ap_character, character, 
		AP_CHARACTER_BIT_INVENTORY_GOLD, TRUE);
	return TRUE;
}

boolean as_character_deposit_gold(
	struct as_character_module * mod,
	struct ap_character * character,
	uint64_t amount)
{
	if (ap_character_exceeds_gold_limit(character, amount))
		return FALSE;
	character->inventory_gold += amount;
	ap_character_update(mod->ap_character, character, 
		AP_CHARACTER_BIT_INVENTORY_GOLD, TRUE);
	return TRUE;
}
