#ifndef _AC_CHARACTER_H_
#define _AC_CHARACTER_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_character.h"

#include "client/ac_lod.h"
#include "client/ac_render.h"

#include "vendor/RenderWare/rwcore.h"
#include "vendor/RenderWare/rphanim.h"

#include <assert.h>

#define AC_CHARACTER_MODULE_NAME "AgcmCharacter"

#define	AC_CHARACTER_MAX_DFF_NAME_SIZE 64
#define AC_CHARACTER_MAX_STR 256
#define AC_CHARACTER_LINK_ANIM_SPEED_OFFSET 0.01f
#define AC_CHARACTER_LINK_ANIM_SPEED_OFFSET_MAX 0.25f
#define AC_CHARACTER_MAX_ALLOC_ANIM_DATA 20
#define AC_CHARACTER_ANIMATION_ATTACHED_DATA_KEY "AGCD_CHARACTER"

#define	AC_CHARACTER_MAX_ACTION_QUEUE_SIZE 20
#define	AC_CHARACTER_MAX_ACTION_ATTACK_PRESERVE_TIME 1000
#define	AC_CHARACTER_ACTION_SELF_CHARACTER_INTERVAL 100
#define	AC_CHARACTER_MAX_WAIT_TIME_FOR_COMBO_ACTION 300
#define AC_CHARACTER_MAX_PREENGAGED_EVENT_EFFECT 20

#define AC_CHARACTER_AT2_COMMON_DEFAULT	0
#define AC_CHARACTER_MAX_ANIM_TYPE2 AC_CHARACTER_AT2_WARRIOR_COUNT

#define AC_CHARACTER_MAX_CUSTOMIZATION_INFO_COUNT 128
#define AC_CHARACTER_MAX_CUSTOMIZATION_INFO_SIZE 64

BEGIN_DECLS

extern size_t AC_CHARACTER_ATTACHMENT_OFFSET;
extern size_t AC_CHARACTER_TEMPLATE_ATTACHMENT_OFFSET;

enum ac_character_anim_type {
	AC_CHARACTER_ANIM_TYPE_SOCIAL_REPEAT = -13,
	AC_CHARACTER_ANIM_TYPE_SOCIAL = -12,
	AC_CHARACTER_ANIM_TYPE_SKILL = -11,
	AC_CHARACTER_ANIM_TYPE_CUST_START = -10,
	AC_CHARACTER_ANIM_TYPE_NONE = -1,
	AC_CHARACTER_ANIM_TYPE_WAIT =  0,
	AC_CHARACTER_ANIM_TYPE_WALK,
	AC_CHARACTER_ANIM_TYPE_RUN,
	AC_CHARACTER_ANIM_TYPE_ATTACK,
	AC_CHARACTER_ANIM_TYPE_STRUCK,
	AC_CHARACTER_ANIM_TYPE_DEAD,
	AC_CHARACTER_ANIM_TYPE_CUSTOMIZE_PREVIEW,
	AC_CHARACTER_ANIM_TYPE_LEFTSIDE_STEP,
	AC_CHARACTER_ANIM_TYPE_RIGHTSIDE_STEP,
	AC_CHARACTER_ANIM_TYPE_BACKWARD_STEP,
	AC_CHARACTER_ANIM_TYPE_PICKUP,
	AC_CHARACTER_ANIM_TYPE_EAT,
	AC_CHARACTER_ANIM_TYPE_SUMMON_CONVEYANCE,
	AC_CHARACTER_ANIM_TYPE_EMBARK,
	AC_CHARACTER_ANIM_TYPE_RIDE,
	AC_CHARACTER_ANIM_TYPE_ABNORMAL,
	AC_CHARACTER_ANIM_TYPE_COUNT
};

enum ac_character_anim_type2 {
	AC_CHARACTER_AT2_BASE = 0,
	AC_CHARACTER_AT2_WARRIOR,
	AC_CHARACTER_AT2_ARCHER,
	AC_CHARACTER_AT2_WIZARD,
	AC_CHARACTER_AT2_BOSS_MONSTER,
	AC_CHARACTER_AT2_ARCHLORD,
	AC_CHARACTER_AT2_COUNT
};

enum ac_character_base_anim_type2 {
	AC_CHARACTER_AT2_BASE_DEFAULT = AC_CHARACTER_AT2_COMMON_DEFAULT,
	AC_CHARACTER_AT2_BASE_COUNT
};

enum ac_character_warrior_anim_type2 {
	AC_CHARACTER_AT2_WARRIOR_DEFAULT = AC_CHARACTER_AT2_COMMON_DEFAULT,
	AC_CHARACTER_AT2_WARRIOR_ONE_HAND_SLASH,
	AC_CHARACTER_AT2_WARRIOR_ONE_HAND_BLUNT,
	AC_CHARACTER_AT2_WARRIOR_TWO_HAND_SLASH,
	AC_CHARACTER_AT2_WARRIOR_TWO_HAND_BLUNT,
	AC_CHARACTER_AT2_WARRIOR_ONE_HAND_POLEARM,
	AC_CHARACTER_AT2_WARRIOR_ONE_HAND_SCYTHE,
	AC_CHARACTER_AT2_WARRIOR_DEFAULT_RIDE,
	AC_CHARACTER_AT2_WARRIOR_WEAPON_RIDE,
	AC_CHARACTER_AT2_WARRIOR_STANDARD_RIDE,
	AC_CHARACTER_AT2_WARRIOR_ONE_HAND_CHARON,
	AC_CHARACTER_AT2_WARRIOR_TWO_HAND_CHARON,
	AC_CHARACTER_AT2_WARRIOR_COUNT
};

enum ac_character_archer_anim_type2 {
	AC_CHARACTER_AT2_ARCHER_DEFAULT	= AC_CHARACTER_AT2_COMMON_DEFAULT,
	AC_CHARACTER_AT2_ARCHER_TWO_HAND_BOW,
	AC_CHARACTER_AT2_ARCHER_TWO_HAND_CROSSBOW,
	AC_CHARACTER_AT2_ARCHER_TWO_HAND_GLAVE,
	AC_CHARACTER_AT2_ARCHER_DEFAULT_RIDE,
	AC_CHARACTER_AT2_ARCHER_WEAPON_RIDE,
	AC_CHARACTER_AT2_ARCHER_TWO_HAND_KATARIYA,
	AC_CHARACTER_AT2_ARCHER_STANDARD_RIDE,
	AC_CHARACTER_AT2_ARCHER_ONE_HAND_ZENNON,
	AC_CHARACTER_AT2_ARCHER_TWO_HAND_ZENNON,
	AC_CHARACTER_AT2_ARCHER_ONE_HAND_CHARON,
	AC_CHARACTER_AT2_ARCHER_TWO_HAND_CHARON,
	AC_CHARACTER_AT2_ARCHER_COUNT
};

enum ac_character_wizard_anim_type2 {
	AC_CHARACTER_AT2_WIZARD_DEFAULT	= AC_CHARACTER_AT2_COMMON_DEFAULT,
	AC_CHARACTER_AT2_WIZARD_STAFF,
	AC_CHARACTER_AT2_WIZARD_TROPHY,
	AC_CHARACTER_AT2_WIZARD_HOOP,
	AC_CHARACTER_AT2_WIZARD_DEFAULT_RIDE,
	AC_CHARACTER_AT2_WIZARD_WEAPON_RIDE,
	AC_CHARACTER_AT2_WIZARD_TWO_HAND_CHAKRAM,
	AC_CHARACTER_AT2_WIZARD_STANDARD_RIDE,
	AC_CHARACTER_AT2_WIZARD_ONE_HAND_CHARON,
	AC_CHARACTER_AT2_WIZARD_TWO_HAND_CHARON,
	AC_CHARACTER_AT2_WIZARD_COUNT
};

enum ac_character_archlord_anim_type2 {
	AC_CHARACTER_AT2_ARCHLORD_DEFAULT = AC_CHARACTER_AT2_COMMON_DEFAULT,
	AC_CHARACTER_AT2_ARCHLORD_DEFAULT_RIDE,
	AC_CHARACTER_AT2_ARCHLORD_STANDARD_RIDE,
	AC_CHARACTER_AT2_ARCHLORD_COUNT
};

enum ac_character_wait_anim_type {
	AC_CHARACTER_WAIT_ANIM_TYPE_NORMAL = 0,
	AC_CHARACTER_WAIT_ANIM_TYPE_ATTACK,
	AC_CHARACTER_WAIT_ANIM_TYPE_COUNT
};

enum ac_character_sub_anim_type {
	AC_CHARACTER_SUBANIM_TYPE_NONE = -1,
	AC_CHARACTER_SUBANIM_TYPE_SPINE = 0,
	AC_CHARACTER_SUBANIM_TYPE_COUNT
};

enum ac_character_sub_anim_node_index {
	AC_CHARACTER_SUBANIM_NODEINDEX_ERROR = -1,
	AC_CHARACTER_SUBANIM_NODEINDEX_SPINE = 12
};

enum ac_character_start_anim_callback_point {
	AC_CHARACTER_START_ANIM_CB_POINT_DEFAULT = 0,
	AC_CHARACTER_START_ANIM_CB_POINT_BLEND,
	AC_CHARACTER_START_ANIM_CB_POINT_ANIMEND,
	AC_CHARACTER_START_ANIM_CB_POINT_ANIMENDTHENCHANGE
};

enum ac_character_status_flag_bits {
	AC_CHARACTER_STATUS_FLAG_NONE					= 0x0000,
	AC_CHARACTER_STATUS_FLAG_INIT_HIERARCHY			= 0x0001,
	AC_CHARACTER_STATUS_FLAG_LOAD_CLUMP				= 0x0002,
	AC_CHARACTER_STATUS_FLAG_EQUIP_DEFAULT_ARMOUR	= 0x0004,
	AC_CHARACTER_STATUS_FLAG_ADDED_WORLD			= 0x0010,
	AC_CHARACTER_STATUS_FLAG_INIT_COMPLETE			= 0x0008,
	AC_CHARACTER_STATUS_FLAG_REMOVED				= 0x0020,
	AC_CHARACTER_STATUS_FLAG_ATTACHED_HIERARCHY		= 0x0040,
	AC_CHARACTER_STATUS_FLAG_NEW_CREATE				= 0x0080,
	AC_CHARACTER_STATUS_FLAG_START_ANIM				= 0x0100
};

enum ac_character_anim_cb_data_array {
	AC_CHARACTER_ACDA_CHARACTER_ID = 0,
	AC_CHARACTER_ACDA_ANIM_TYPE,
	AC_CHARACTER_ACDA_START_ANIM_CB_POINT,
	AC_CHARACTER_ACDA_CUST_CALLBACK,
	AC_CHARACTER_ACDA_CUST_ANIM_TYPE,
	AC_CHARACTER_ACDA_INIT_ANIM,
	AC_CHARACTER_ACDA_COUNT
};

enum ac_character_anim_flag_bits {
	AC_CHARACTER_ANIM_FLAG_NONE = 0x0000,
	AC_CHARACTER_ANIM_FLAG_BLEND = 0x0001,
	AC_CHARACTER_ANIM_FLAG_BLEND_EX = 0X0002,
	AC_CHARACTER_ANIM_FLAG_NO_UNEQUIP = 0X0004,
};

enum ac_character_anim_cust_flag_bits
{
	AC_CHARACTER_ANIM_CUST_FLAG_NONE = 0x0000,
	AC_CHARACTER_ANIM_CUST_FLAG_LOCK_LINK_ANIM = 0x0001,
	AC_CHARACTER_ANIM_CUST_FLAG_CLUMP_SHOW_START_TIME = 0x0002,
	AC_CHARACTER_ANIM_CUST_FLAG_CLUMP_HIDE_END_TIME = 0x0004
};

enum ac_character_transparent_type {
	AC_CHARACTER_TRANSPARENT_NONE = 0,
	AC_CHARACTER_TRANSPARENT_HALF,
	AC_CHARACTER_TRANSPARENT_FULL,
};

enum ac_character_callback_id {
};

struct ac_character_action_queue_data {
	enum ap_character_action_type action_type;
	enum ap_character_action_result_type action_result_type;
	uint32_t actor_id;
	uint32_t skill_tid;
	struct ap_factor factor_damage;
	boolean is_set_packet_factor;
	char * packet_factor;
	uint32_t process_time;
	boolean is_dead;
	/* Number of times to show damage. */
	uint32_t divide_for_show_count; 
	/* Interval at which we need to show damage. */
	uint32_t divide_interval_msec;
	/* Check if damage should be applied immediately. */
	boolean update_now;
	uint32_t addition_effect;
};

struct ac_character_preengaged_event_effect {
	struct ap_character * target;
	uint32_t buffed_tid;
	uint32_t caster_tid;
	uint32_t common_effect_id;	
};

struct ac_character_attached_data {
	int8_t cust_type;
	uint8_t active_rate;
	uint16_t cust_flags;
	char * point;
	struct ac_anim_data2 * blending_data;
	struct ac_anim_data2 * sub_data;
	int32_t clump_show_offset_time;
	uint32_t clump_fade_in_time;
	int32_t clump_hide_offset_time;
	uint32_t clump_fade_out_time;
};

struct ac_character_animation {
	struct ac_animation2 * animation;
	struct ac_animation_flag * animation_flags;
};

struct ac_character_default_head_template {
	char face_info[AC_CHARACTER_MAX_CUSTOMIZATION_INFO_COUNT][AC_CHARACTER_MAX_CUSTOMIZATION_INFO_SIZE];
	char hair_info[AC_CHARACTER_MAX_CUSTOMIZATION_INFO_COUNT][AC_CHARACTER_MAX_CUSTOMIZATION_INFO_SIZE];
	struct ac_render_crt_data face_render_type;
	struct ac_render_crt_data hair_render_type;
	RwV3d near_camera;
	RwV3d far_camera;
};

struct ac_character_template {
	char label[AC_CHARACTER_MAX_DFF_NAME_SIZE];
	char dff_name[AC_CHARACTER_MAX_DFF_NAME_SIZE];
	char default_armour_dff_name[AC_CHARACTER_MAX_DFF_NAME_SIZE];
	char pick_dff_name[AC_CHARACTER_MAX_DFF_NAME_SIZE];
	RwRGBA pre_light;
	struct ac_character_default_head_template * default_head_data;
	struct ac_mesh_clump * clump;
	struct ac_mesh_clump * default_armour_clump;
	struct ac_mesh_geometry * picking_geometry;
	int32_t hair_number;
	int32_t face_number;
	struct ac_mesh_geometry * hair[32];
	struct ac_mesh_geometry * face[32];
	RwSphere bsphere;
	int32_t m_lAnimType2;
	struct ac_character_animation *** animation_data;
	RwV3d scale3d;
	float scale;
	int32_t height;
	int32_t rider_height;
	int32_t object_type;
	float depth;
	struct ac_lod lod;
	struct ac_render_crt clump_render_type;
	int32_t refcount;
	boolean is_loaded;
	//OcTreeRenderData2 m_stOcTreeData;
	int32_t picking_node_index[10];
	boolean tagging;
	uint32_t did_not_finish;
	uint32_t look_at_node;
	boolean use_bending;
	float bending_factor;
	float bending_degree;
	boolean non_picking_type;
	int32_t face_geometry_index;
	float default_anim_speed_rate[AC_CHARACTER_ANIM_TYPE_COUNT];
};

struct ac_character_sub_anim_info {
	RpHAnimHierarchy * in_hierarchy;
	RpHAnimHierarchy * in_hierarchy2;
	RpHAnimHierarchy * out_hierarchy;
	boolean stop_main_animation;
	uint32_t current_time;
	uint32_t duration;
	float force_time;
};

struct ac_character {
	void * cs;
	void * text_board;
	struct ac_character_template * temp;
	struct ac_mesh_clump * clump;
	struct ac_mesh_geometry * face;
	struct ac_mesh_geometry * hair;
	int32_t attach_face_id;
	int32_t attach_hair_id;
	boolean attachable_face;
	boolean attachable_hair;
	boolean view_helmet;
	boolean equip_helmet;
	int32_t ride_cid;
	struct ac_character * ride;
	RwFrame * ride_revision_frame;
	struct ac_character * owner;
	struct ac_mesh_geometry * pick_geometry;
	struct ac_mesh_geometry * org_pick_geometry;
	RpHAnimHierarchy * in_hierarchy;
	RpHAnimHierarchy * in_hierarchy2;
	RpHAnimHierarchy * out_hierarchy;
	struct ac_character_sub_anim_info sub_anim;
	boolean stop;
	uint32_t animation_flags;
	boolean force_animation;
	uint32_t prev_time;
	uint32_t start_wave_fx_time;
	struct ac_anim_data2 * cur_anim_data;
	enum ac_character_anim_type cur_anim_type;
	int32_t cur_anim_type2;
	struct ac_anim_data2 * next_anim_data;
	//struct ac_animation_time m_csAnimation;
	boolean set_blending_hierarchy;
	int32_t attacker_id;
	float anim_speed_rate[AC_CHARACTER_ANIM_TYPE_COUNT];
	float skill_speed_rate;
	float default_anim_speed_rate[AC_CHARACTER_ANIM_TYPE_COUNT];
	boolean update;
	int16_t nid;
	int32_t status;
	int32_t last_attack_time;
	uint32_t next_attack_time;
	uint32_t last_send_attack_time;
	uint32_t next_action_self_character_time;
	boolean is_lock_target;
	int32_t lock_target_id;
	boolean is_force_attack;
	boolean is_cast_skill;
	int32_t last_cast_skill_tid;
	int32_t action_data_count;
	boolean queued_death_event;
	struct ac_character_action_queue_data action_queue[AC_CHARACTER_MAX_ACTION_QUEUE_SIZE];
	boolean is_combo;
	boolean in_shadow;		
	struct ap_character_action next_action;
	uint32_t select_target_id;
	uint32_t char_id_color;
	float last_wink_time;
	float next_wink_time;
	RwV3d offset_bsphere;
	RwFrame * node_frame[10];
	boolean is_transforming;
	int8_t transparent_type;
	//stAgpmSkillActionData * m_pstSkillActionData;
	float last_bending_degree;
	uint8_t preengaged_event_effect_count;
	struct ac_character_preengaged_event_effect * preengaged_event_effect[AC_CHARACTER_MAX_PREENGAGED_EVENT_EFFECT];
	//AuCharacterLightInfo m_aLightInfo[ AGPMITEM_PART_V_BODY ];
};

struct ac_character_render_data {
	uint64_t state;
	bgfx_program_handle_t program;
	boolean no_texture;
};

struct ac_character_module * ac_character_create_module();

boolean ac_character_read_templates(
	struct ac_character_module * mod,
	const char * file_path, 
	boolean decrypt);

void ac_character_render(
	struct ac_character_module * mod,
	struct ap_character * character,
	struct ac_character_render_data * render_data);

static inline struct ac_character_template * ac_character_get_template(
	struct ap_character_template * temp)
{
	return (struct ac_character_template *)ap_module_get_attached_data(temp, 
		AC_CHARACTER_TEMPLATE_ATTACHMENT_OFFSET);
}

static inline struct ac_character * ac_character_get(
	struct ap_character * temp)
{
	return (struct ac_character *)ap_module_get_attached_data(temp, 
		AC_CHARACTER_ATTACHMENT_OFFSET);
}

END_DECLS

#endif /* _AC_CHARACTER_H_ */
