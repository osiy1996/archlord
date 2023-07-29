#ifndef _AP_AI2_H_
#define _AP_AI2_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_character.h"
#include "public/ap_define.h"
#include "public/ap_module_instance.h"

#define AP_AI2_MODULE_NAME "AgpmAI2"

#define AP_AI2_MAX_USABLE_ITEM_COUNT 10
#define AP_AI2_MAX_USABLE_SKILL_COUNT 10
#define AP_AI2_MAX_RACE_CLASS_ADJUST 10
#define AP_AI2_MAX_ACTION_ADJUST 10
#define AP_AI2_MAX_FELLOW_WORKER_COUNT 5
#define AP_AI2_MAX_FOLLOWER_COUNT 10
#define AP_AI2_MAX_PREEMPTIVECONDITON 3

BEGIN_DECLS

struct ap_ai2_module;

enum ap_ai2_action_adjust_type {
	AP_AI2_ACTION_ADJUST_NONE = 0,
	AP_AI2_ACTION_ADJUST_MELEE_ATTACK,
	AP_AI2_ACTION_ADJUST_MELEE_ATTACK_TRY,
	AP_AI2_ACTION_ADJUST_BUFF,
	AP_AI2_ACTION_ADJUST_BUFF_TRY,
	AP_AI2_ACTION_ADJUST_DEBUFF,
	AP_AI2_ACTION_ADJUST_DEBUFF_TRY,
};

enum ap_ai2_search_enemy_type {
	AP_AI2_SEARCH_ENEMY_NONE = 0,
	AP_AI2_SEARCH_ENEMY_RUSH1,
	AP_AI2_SEARCH_ENEMY_RUSH2,
	AP_AI2_SEARCH_ENEMY_IN_CASTLE,
	AP_AI2_SEARCH_ENEMY_COUNT
};

enum ap_ai2_stop_fight_type {
	AP_AI2_STOP_FIGHT_NONE = 0,
	AP_AI2_STOP_FIGHT_NEAR,
	AP_AI2_STOP_FIGHT_NORMAL,
	AP_AI2_STOP_FIGHT_COUNT
};

enum ap_ai2_escape_type {
	AP_AI2_ESCAPE_NONE = 0,
	AP_AI2_ESCAPE_ESCAPE,
	AP_AI2_ESCAPE_RESPONSIVE_ATTACK,
	AP_AI2_ESCAPE_COUNT
};

enum ap_ai2_pc_ai_state {
	AP_AI2_PC_AI_STATE_NONE = 0,
	AP_AI2_PC_AI_STATE_HOLD,
	AP_AI2_PC_AI_STATE_CONFUSION,
	AP_AI2_PC_AI_STATE_FEAR,
	AP_AI2_PC_AI_STATE_BERSERK,
	AP_AI2_PC_AI_STATE_SHRINK,
	AP_AI2_PC_AI_STATE_DISEASE,
	AP_AI2_PC_AI_STATE_MAX
};

enum ap_ai2_npc_type {
	AP_AI2_NPC_TYPE_MOB = 0,
	AP_AI2_NPC_TYPE_FIXED,
	AP_AI2_NPC_TYPE_PATROL
};

enum ap_ai2_callback_id {
	AP_AI2_CB_,
};

struct ap_ai2_condition_table {
	int aggro_parameter;
	boolean accept_aggro;
	int target_parameter;
	int parameter;
	boolean is_percent;
	int operator_;
	int condition_check;
	int prev_process_time;
	int timer_count;
	int probability;
	int max_usable_count;
};

struct ap_ai2_use_result {
	int used_count;
	int clock;
};

struct ap_ai2_skill_simple_condition {
	uint32_t hp_limit;
	uint32_t skill_count;
};

struct ap_ai2_use_skill {
	struct ap_ai2_condition_table condition_table;
	struct ap_ai2_skill_simple_condition skill_condition;
	uint32_t skill_tid;
	uint32_t skill_level;
	struct ap_skill_template * skill_template;
};

struct ap_ai2_template {
	boolean use_normal_attack;
	boolean do_not_move;
	enum ap_ai2_npc_type npc_type;
	uint32_t tid;
	int agressive_point;
	struct ap_ai2_use_skill use_skill[AP_AI2_MAX_USABLE_SKILL_COUNT];
	uint32_t use_skill_count;
	uint32_t link_monster_tid;
	float link_monster_sight;
	float maintenance_range;
	boolean select_random_target;
	uint32_t fellow_worker_refresh_time;
	uint32_t m_ulPinchMonsterTID;
	float visibility;
};

struct ap_ai2_pc_ai {
	uint32_t target_id;
	uint32_t ai_count;
	struct au_pos first_position;
	enum ap_ai2_pc_ai_state ai_state;
	int	reserve_param1;
	int	reserve_param2;
	uint32_t reserve_param3;
};

struct ap_ai2_aggro_target {
	uint32_t character_id;
	uint32_t damage;
	uint64_t begin_aggro_tick;
	uint64_t end_aggro_tick;
};

struct ap_ai2_character_attachment {
	struct ap_ai2_template * temp;
	struct ap_ai2_aggro_target * aggro_targets;
	boolean is_scheduled_to_clear_aggro;
	uint64_t last_attack_tick;
	uint32_t attack_interval;
	uint64_t prev_process_tick;
	uint64_t process_interval;
	uint64_t last_path_find_tick;
	uint64_t next_patrol_tick;
	uint64_t next_target_scan_tick;
	uint64_t next_area_scan_tick;
	uint32_t next_dialog_target_pc;
	uint64_t start_runaway_tick;
	uint32_t escape_count;
	struct ap_ai2_use_result skill_result[AP_AI2_MAX_USABLE_SKILL_COUNT];
	uint32_t skill_result_count;
	boolean is_scream_used;
	uint64_t scream_start_tick;
	struct au_pos target_position;
	struct au_pos spawn_position;
	boolean use_ai;
	struct au_pos base_spawn_position;
	float spawn_radius;
	uint32_t target_id;
	uint32_t ready_target_id;
	struct ap_ai2_pc_ai pc_ai;
};

struct ap_ai2_module * ap_ai2_create_module();

size_t ap_ai2_attach_data(
	struct ap_ai2_module * mod,
	enum ap_ai2_module_data_index data_index,
	size_t data_size,
	ap_module_t callback_module,
	ap_module_default_t constructor,
	ap_module_default_t destructor);

boolean ap_ai2_read_data_table(
	struct ap_ai2_module * mod,
	const char * file_path, 
	boolean decrypt);

struct ap_ai2_character_attachment * ap_ai2_get_character_attachment(
	struct ap_ai2_module * mod,
	struct ap_character * character);

void ap_ai2_add_aggro_damage(
	struct ap_ai2_module * mod, 
	struct ap_character * character,
	uint32_t aggro_character_id,
	uint32_t damage);

END_DECLS

#endif /* _AP_AI2_H_ */