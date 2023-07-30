#include "server/as_skill.h"

#include "core/log.h"
#include "core/malloc.h"

#include "public/ap_admin.h"

#include "server/as_character.h"
#include "server/as_database.h"
#include "server/as_player.h"

#include <assert.h>

#define DB_STREAM_MODULE_ID 3
#define MAKE_ID(ID) AS_DATABASE_MAKE_ID(DB_STREAM_MODULE_ID, ID)

struct as_skill_module {
	struct ap_module_instance instance;
	struct ap_character_module * ap_character;
	struct ap_skill_module * ap_skill;
	struct as_character_module * as_character;
	struct as_player_module * as_player;
	size_t skill_offset;
	size_t character_offset;
	size_t character_db_offset;
};

static boolean cbcharctor(
	struct as_skill_module * mod,
	struct ap_character * character)
{
	as_skill_get_character(mod, character)->sync_passive_buff_level_up = INT32_MAX;
	return TRUE;
}

static boolean cbcharstopaction(
	struct as_skill_module * mod,
	struct ap_character_cb_stop_action * cb)
{
	struct ap_character * c = cb->character;
	struct as_skill_character * sc = as_skill_get_character(mod, c);
	sc->cast_queue_cursor = 0;
	sc->cast_queue_count = 0;
	return TRUE;
}

static boolean cbcharsetlevel(
	struct as_skill_module * mod,
	struct ap_character_cb_set_level * cb)
{
	struct ap_character * c = cb->character;
	struct ap_skill_character * sc;
	uint32_t level;
	int32_t sp = 0;
	int32_t hp = 0;
	uint32_t i;
	if (!CHECK_BIT(c->char_type, AP_CHARACTER_TYPE_PC))
		return TRUE;
	sc = ap_skill_get_character(mod->ap_skill, c);
	level = ap_character_get_level(c);
	sp = level;
	if (level > 49) {
		/* Bonus points between Lv50-Lv71. */
		sp += MIN(21, level - 49);
	}
	if (hp >= 90) {
		hp = 1 + (MIN(level, 110) - 90) * 2;
		if (level >= 111) 
			hp += (MIN(level, 120) - 110) * 3;
	}
	for (i = 0; i < sc->skill_count; i++) {
		struct ap_skill * skill = sc->skill[i];
		uint32_t slevel = ap_skill_get_level(skill);
		uint32_t j;
		assert(slevel < AP_SKILL_MAX_SKILL_CAP);
		for (j = 1; j <= slevel; j++) {
			const float * factors = 
				&skill->temp->used_const_factor[j][0];
			sp -= (int32_t)factors[AP_SKILL_CONST_REQUIRE_POINT];
			hp -= (int32_t)factors[AP_SKILL_CONST_REQUIRE_HEROIC_POINT];
		}
	}
	c->factor.dirt.skill_point = sp;
	c->factor.dirt.heroic_point = hp;
	ap_character_update(mod->ap_character, c, AP_FACTORS_BIT_SKILL_POINT | 
		AP_FACTORS_BIT_HEROIC_POINT, TRUE);
	return TRUE;
}

static boolean cbdecodechar(
	struct as_skill_module * mod,
	struct as_character_cb_decode * cb)
{
	boolean result = TRUE;
	struct as_skill_character_db * db = 
		as_skill_get_character_db(mod, cb->character);
	if (cb->module_id != DB_STREAM_MODULE_ID)
		return TRUE;
	switch (cb->field_id) {
	case AS_SKILL_CHARACTER_DB_SKILL_LIST: {
		uint32_t count = 0;
		uint32_t i;
		if (!AS_DATABASE_DECODE(cb->codec, count)) {
			ERROR("Failed to decode character skill count (%s).",
				cb->character->name);
			return TRUE;
		}
		for (i = 0; i < count; i++) {
			uint64_t encoded = 0;
			if (!AS_DATABASE_DECODE(cb->codec, encoded)) {
				ERROR("Failed to decode character skill (%s).",
					cb->character->name);
				return TRUE;
			}
			if (db->skill_count >= AP_SKILL_MAX_SKILL_OWN) {
				WARN("Character exceeded max. number of skills (%s).",
					cb->character->name);
			}
			else {
				struct as_skill_db * s = 
					&db->skills[db->skill_count++];
				s->tid = (uint16_t)(encoded & 0xFFFF);
				s->level = (uint16_t)((encoded >> 16) & 0xFFFF);
				s->exp = (uint32_t)((encoded >> 32) & 0xFFFFFFFF);
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
	struct as_skill_module * mod,
	struct as_character_cb_decode * cb)
{
	boolean result = TRUE;
	uint32_t i;
	struct as_skill_character_db * db = 
		as_skill_get_character_db(mod, cb->character);
	result &= AS_DATABASE_ENCODE(cb->codec, 
		MAKE_ID(AS_SKILL_CHARACTER_DB_SKILL_LIST),
		db->skill_count);
	for (i = 0; i < db->skill_count; i++) {
		const struct as_skill_db * s = &db->skills[i];
		uint64_t encoded = ((uint64_t)s->exp << 32) |
			((uint64_t)s->level << 16) | s->tid;
		result &= as_database_write_encoded(cb->codec,
			&encoded, sizeof(encoded));
	}
	return result;
}

static boolean cbloadchar(
	struct as_skill_module * mod,
	struct as_character_cb_load * cb)
{
	struct ap_character * c = cb->character;
	struct ap_skill_character * schar = ap_skill_get_character(mod->ap_skill, c);
	struct as_skill_character_db * db = 
		as_skill_get_character_db(mod, cb->db);
	uint32_t i;
	for (i = 0; i < db->skill_count; i++) {
		struct as_skill_db * sdb = &db->skills[i];
		struct ap_skill * skill = as_skill_from_db(mod, sdb);
		uint32_t level;
		uint32_t j;
		if (!skill) {
			ERROR("Failed to create database character skill (character = %s, skill_tid = %u).",
				cb->character->name, sdb->tid);
			continue;
		}
		level = ap_skill_get_level(skill);
		assert(level < AP_SKILL_MAX_SKILL_CAP);
		for (j = 1; j <= level; j++) {
			const float * factors = 
				&skill->temp->used_const_factor[j][0];
			c->factor.dirt.skill_point -= 
				(int32_t)factors[AP_SKILL_CONST_REQUIRE_POINT];
			c->factor.dirt.heroic_point -= 
				(int32_t)factors[AP_SKILL_CONST_REQUIRE_HEROIC_POINT];
		}
		schar->skill_id[schar->skill_count] = skill->id;
		schar->skill[schar->skill_count++] = skill;
	}
	return TRUE;
}

static boolean cbreflectchar(
	struct as_skill_module * mod,
	struct as_character_cb_reflect * cb)
{
	struct ap_skill_character * schar = 
		ap_skill_get_character(mod->ap_skill, cb->character);
	struct as_skill_character_db * db = 
		as_skill_get_character_db(mod, cb->db);
	uint32_t i;
	db->skill_count = 0;
	for (i = 0; i < schar->skill_count; i++) {
		const struct ap_skill * skill = schar->skill[i];
		struct as_skill_db * s;
		if (skill->is_temporary)
			continue;
		assert(skill->temp->id <= UINT16_MAX);
		assert(ap_skill_get_level(skill) <= UINT16_MAX);
		s = &db->skills[db->skill_count++];
		s->tid = (uint16_t)skill->temp->id;
		s->level = (uint16_t)ap_skill_get_level(skill);
		s->exp = ap_skill_get_exp(skill);
	}
	return TRUE;
}

static boolean cbcharcopy(
	struct as_skill_module * mod,
	struct as_character_cb_copy * cb)
{
	const struct as_skill_character_db * src = as_skill_get_character_db(mod, cb->src);
	struct as_skill_character_db * dst = as_skill_get_character_db(mod, cb->dst);
	memcpy(dst, src, sizeof(*src));
	return TRUE;
}

static boolean onregister(
	struct as_skill_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_character, AP_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_skill, AP_SKILL_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_character, AS_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_player, AS_PLAYER_MODULE_NAME);
	mod->character_offset = ap_character_attach_data(mod->ap_character, 
		AP_CHARACTER_MDI_CHAR, sizeof(struct as_skill_character),
		mod, cbcharctor, NULL);
	mod->skill_offset = ap_skill_attach_data(mod->ap_skill, AP_SKILL_MDI_SKILL,
		sizeof(struct as_skill), mod, NULL, NULL);
	mod->character_db_offset = as_character_attach_data(mod->as_character,
		AS_CHARACTER_MDI_DATABASE, sizeof(struct as_skill_character_db), 
		mod, NULL, NULL);
	ap_character_add_callback(mod->ap_character, AP_CHARACTER_CB_STOP_ACTION, mod, cbcharstopaction);
	ap_character_add_callback(mod->ap_character, AP_CHARACTER_CB_SET_LEVEL, mod, cbcharsetlevel);
	if (!as_character_set_database_stream(mod->as_character, DB_STREAM_MODULE_ID,
			mod, cbdecodechar, cbencodechar)) {
		ERROR("Failed to set character database stream.");
		return FALSE;
	}
	as_character_add_callback(mod->as_character, AS_CHARACTER_CB_LOAD, mod, cbloadchar);
	as_character_add_callback(mod->as_character, AS_CHARACTER_CB_REFLECT, mod, cbreflectchar);
	as_character_add_callback(mod->as_character, AS_CHARACTER_CB_COPY, mod, cbcharcopy);
	return TRUE;
}

struct as_skill_module * as_skill_create_module()
{
	struct as_skill_module * mod = ap_module_instance_new(AS_SKILL_MODULE_NAME,
		sizeof(*mod), onregister, NULL, NULL, NULL);
	ap_module_set_module_data(mod, AS_SKILL_MDI_DATABASE,
		sizeof(struct as_skill_db), NULL, NULL);
	return mod;
}

void as_skill_add_callback(
	struct as_skill_module * mod,
	enum as_skill_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	ap_module_add_callback(mod, id, callback_module, callback);
}

size_t as_skill_attach_data(
	struct as_skill_module * mod,
	enum as_skill_module_data_index data_index,
	size_t data_size,
	ap_module_t callback_module,
	ap_module_default_t constructor,
	ap_module_default_t destructor)
{
	return ap_module_attach_data(mod, data_index, data_size, 
		callback_module, constructor, destructor);
}

struct ap_skill * as_skill_from_db(
	struct as_skill_module * mod, 
	struct as_skill_db * db)
{
	struct ap_skill_template * temp = 
		ap_skill_get_template(mod->ap_skill, db->tid);
	struct ap_skill * skill;
	if (!temp)
		return NULL;
	skill = ap_skill_new(mod->ap_skill);
	skill->tid = db->tid;
	skill->factor.dirt.skill_level = db->level;
	skill->factor.dirt.skill_point = db->level;
	skill->temp = temp;
	skill->status = AP_SKILL_STATUS_NOT_CAST;
	skill->is_temporary = FALSE;
	return skill;
}

struct as_skill * as_skill_get(
	struct as_skill_module * mod,
	struct ap_skill * skill)
{
	return ap_module_get_attached_data(skill, mod->skill_offset);
}

struct as_skill_character * as_skill_get_character(
	struct as_skill_module * mod,
	struct ap_character * character)
{
	return ap_module_get_attached_data(character, 
		mod->character_offset);
}

struct as_skill_character_db * as_skill_get_character_db(
	struct as_skill_module * mod,
	struct as_character_db * character)
{
	return ap_module_get_attached_data(character, 
		mod->character_db_offset);
}

static boolean listhasbuff(
	const struct ap_skill_buff_list * list,
	uint32_t count,
	uint32_t skill_tid)
{
	uint32_t i;
	for (i = 0; i < count; i++) {
		if (list[i].skill_tid == skill_tid)
			return TRUE;
	}
	return FALSE;
}

static boolean overridenbyunion(
	struct ap_skill_character * attachment,
	struct ap_skill_buff_list * buff,
	struct ap_skill_buff_list * list,
	uint32_t count,
	const struct ap_skill_template * temp,
	uint32_t skill_level)
{
	if (!ap_skill_get_skill_union(temp, skill_level))
		return FALSE;
	if (!ap_skill_get_skill_union(buff->temp, buff->skill_level))
		return FALSE;
	if (listhasbuff(list, count, temp->id))
		return FALSE;
	if (attachment->skill_union_count < attachment->extra_skill_union + 1)
		return FALSE;
	return TRUE;
}

static inline boolean overridenbyclassification(
	struct ap_skill_buff_list * buff,
	const struct ap_skill_template * temp,
	uint32_t skill_level)
{
	uint32_t id = ap_skill_get_classify_id(temp, skill_level);
	uint32_t buffid = ap_skill_get_classify_id(buff->temp, buff->skill_level);
	uint32_t level = ap_skill_get_classify_level(temp, skill_level);
	uint32_t bufflevel = ap_skill_get_classify_level(buff->temp, buff->skill_level);
	return (id && id == buffid && bufflevel <= level);
}

void as_skill_remove_buff_with_tid(
	struct as_skill_module * mod,
	struct ap_character * character,
	uint32_t skill_tid)
{
	struct ap_skill_character * attachment = 
		ap_skill_get_character(mod->ap_skill, character);
	uint32_t i;
	for (i = 0; i < attachment->buff_count; i++) {
		struct ap_skill_buff_list * buff = &attachment->buff_list[i];
		if (buff->skill_tid == skill_tid) {
			ap_skill_remove_buff(mod->ap_skill, character, i);
			break;
		}
	}
}

void as_skill_clear_similar_buffs(
	struct as_skill_module * mod,
	struct ap_character * character,
	const struct ap_skill_template * temp,
	uint32_t skill_level)
{
	struct ap_skill_character * attachment = 
		ap_skill_get_character(mod->ap_skill, character);
	uint32_t i;
	for (i = 0; i < attachment->buff_count;) {
		struct ap_skill_buff_list * buff = &attachment->buff_list[i];
		if ((buff->temp->attribute & AP_SKILL_ATTRIBUTE_PASSIVE) || 
			((buff->temp->effect_type2 & AP_SKILL_EFFECT2_ITEM_USE_TYPE) && 
				buff->skill_tid != temp->id)) {
			i++;
			continue;
		}
		else if ((temp->id != buff->skill_tid || skill_level < buff->skill_level) &&
			!overridenbyunion(attachment, buff, &attachment->buff_list[i + 1],
					attachment->buff_count - i - 1, temp, skill_level) &&
			!overridenbyclassification(buff, temp, skill_level)) {
			i++;
			continue;
		}
		i = ap_skill_remove_buff(mod->ap_skill, character, i);
	}
}

void as_skill_reset_all_buffs(
	struct as_skill_module * mod,
	struct ap_character * character)
{
	struct ap_skill_character * attachment = 
		ap_skill_get_character(mod->ap_skill, character);
	uint32_t i;
	for (i = 0; i < attachment->buff_count;) {
		struct ap_skill_buff_list * buff = &attachment->buff_list[i];
		if (CHECK_BIT(buff->temp->attribute, AP_SKILL_ATTRIBUTE_PASSIVE) || 
			ap_skill_has_effect2_and_detail(buff->temp,
				AP_SKILL_EFFECT2_DURATION_TYPE,
				AP_SKILL_EFFECT_DETAIL_DURATION_TYPE,
				AP_SKILL_EFFECT_DETAIL_DURATION_TYPE2)) {
			i++;
			continue;
		}
		if (buff->caster_id == character->id && buff->skill_id) {
			struct ap_skill * skill = ap_skill_find(mod->ap_skill, character,
				buff->skill_id);
			if (skill) {
				skill->cooldown_tick = 0;
				if (ap_character_is_pc(character)) {
					ap_skill_make_init_cooltime_packet(mod->ap_skill, character,
						skill);
					as_player_send_packet(mod->as_player, character);
				}
			}
		}
		i = ap_skill_remove_buff(mod->ap_skill, character, i);
	}
}

void as_skill_clear_special_status_buffs(
	struct as_skill_module * mod,
	struct ap_character * character,
	uint64_t skill_special_status)
{
	struct ap_skill_character * attachment = 
		ap_skill_get_character(mod->ap_skill, character);
	uint32_t i;
	for (i = 0; i < attachment->buff_count;) {
		struct ap_skill_buff_list * buff = &attachment->buff_list[i];
		if (buff->temp->attribute & AP_SKILL_ATTRIBUTE_PASSIVE) {
			i++;
			continue;
		}
		if (!(buff->temp->special_status & skill_special_status)) {
			i++;
			continue;
		}
		i = ap_skill_remove_buff(mod->ap_skill, character, i);
	}
}

struct ap_skill_buff_list * as_skill_add_buff(
	struct as_skill_module * mod,
	struct ap_character * character,
	uint32_t skill_id,
	uint32_t skill_level,
	const struct ap_skill_template * temp,
	uint32_t duration,
	uint32_t caster_id,
	uint32_t caster_tid)
{
	struct ap_skill_character * attachment = 
		ap_skill_get_character(mod->ap_skill, character);
	struct ap_skill_buff_list * buff;
	if (ap_skill_is_buff_list_full(attachment))
		return NULL;
	if (ap_skill_is_stronger_effect_applied(mod->ap_skill, character, 
			temp, skill_level)) {
		return NULL;
	}
	as_skill_clear_similar_buffs(mod, character, temp, skill_level);
	buff = ap_skill_add_buff(mod->ap_skill, character, skill_id, skill_level, temp,
		duration, caster_id, caster_tid);
	return buff;
}

struct ap_skill_buff_list * as_skill_add_passive_buff(
	struct as_skill_module * mod,
	struct ap_character * character,
	struct ap_skill * skill)
{
	struct ap_skill_character * attachment = ap_skill_get_character(mod->ap_skill, 
		character);
	uint32_t modifiedlevel = ap_skill_get_modified_level(mod->ap_skill, 
		character, skill);
	if (ap_skill_is_buff_list_full(attachment))
		return NULL;
	if (ap_skill_is_stronger_effect_applied(mod->ap_skill, character, 
			skill->temp, modifiedlevel)) {
		return NULL;
	}
	return ap_skill_add_buff(mod->ap_skill, character, skill->id, modifiedlevel, 
		skill->temp, 0, character->id, character->tid);
}
