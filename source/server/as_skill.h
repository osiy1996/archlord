#ifndef _AS_SKILL_H_
#define _AS_SKILL_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_define.h"
#include "public/ap_skill.h"
#include "public/ap_module.h"

#define AS_SKILL_MODULE_NAME "AgsmSkill"

#define AS_SKILL_MAX_CAST_QUEUE_COUNT 32

BEGIN_DECLS

struct as_database_codec;

enum as_skill_character_database_id {
	AS_SKILL_CHARACTER_DB_SKILL_LIST,
};

enum as_skill_module_data_index {
	AS_SKILL_MDI_DATABASE,
};

enum as_skill_callback_id {
};

struct as_skill_db {
	uint16_t tid;
	uint16_t level;
	uint32_t exp;
};

/** \brief as_character_db attachment. */
struct as_skill_character_db {
	uint32_t skill_count;
	struct as_skill_db skills[AP_SKILL_MAX_SKILL_OWN];
};

struct as_skill {
	int dummy;
};

struct as_skill_cast_queue_entry {
	uint32_t skill_id;
	uint32_t target_id;
	struct au_pos pos;
	boolean forced_attack;
};

struct as_skill_character {
	/** \brief Ongoing skill cast information. */
	struct ap_skill_cast_info cast;
	/** \brief Scheduled skill cast information. */
	struct ap_skill_cast_info next_cast;
	uint32_t cast_queue_cursor;
	uint32_t cast_queue_count;
	struct as_skill_cast_queue_entry cast_queue[AS_SKILL_MAX_CAST_QUEUE_COUNT];
	int32_t sync_passive_buff_level_up;
};

struct as_skill_module * as_skill_create_module();

void as_skill_add_callback(
	struct as_skill_module * mod,
	enum as_skill_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

size_t as_skill_attach_data(
	struct as_skill_module * mod,
	enum as_skill_module_data_index data_index,
	size_t data_size,
	ap_module_t callback_module,
	ap_module_default_t constructor,
	ap_module_default_t destructor);

struct ap_skill * as_skill_from_db(
	struct as_skill_module * mod,
	struct as_skill_db * db);

struct as_skill * as_skill_get(struct as_skill_module * mod, struct ap_skill * skill);

struct as_skill_character * as_skill_get_character(
	struct as_skill_module * mod,
	struct ap_character * character);

/**
 * Retrieve attached data.
 * \param[in] character Character pointer.
 * 
 * \return Character database object attachment.
 */
struct as_skill_character_db * as_skill_get_character_db(
	struct as_skill_module * mod,
	struct as_character_db * character);

void as_skill_remove_buff_with_tid(
	struct as_skill_module * mod,
	struct ap_character * character,
	uint32_t skill_tid);

void as_skill_clear_similar_buffs(
	struct as_skill_module * mod,
	struct ap_character * character,
	const struct ap_skill_template * temp,
	uint32_t skill_level);

void as_skill_reset_all_buffs(
	struct as_skill_module * mod,
	struct ap_character * character);

void as_skill_clear_special_status_buffs(
	struct as_skill_module * mod,
	struct ap_character * character,
	uint64_t skill_special_status);

struct ap_skill_buff_list * as_skill_add_buff(
	struct as_skill_module * mod,
	struct ap_character * character,
	uint32_t skill_id,
	uint32_t skill_level,
	const struct ap_skill_template * temp,
	uint32_t duration,
	uint32_t caster_id,
	uint32_t caster_tid);

struct ap_skill_buff_list * as_skill_add_passive_buff(
	struct as_skill_module * mod,
	struct ap_character * character,
	struct ap_skill * skill);

END_DECLS

#endif /* _AS_SKILL_H_ */
