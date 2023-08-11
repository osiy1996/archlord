#ifndef _AC_EFFECT_H_
#define _AC_EFFECT_H_

#include "core/file_system.h"

#include "public/ap_admin.h"
#include "public/ap_module_instance.h"

#include "vendor/bgfx/c99/bgfx.h"
#include "vendor/cglm/cglm.h"
#include "vendor/RenderWare/rwcore.h"
#include "vendor/RenderWare/rpspline.h"
#include "vendor/RenderWare/rtanim.h"
#include "vendor/RenderWare/rphanim.h"

#define AC_EFFECT_MODULE_NAME "AgcmEff2"

#define AC_EFFECT_MAX_FILE_NAME_SIZE 64
#define	AC_EFFECT_MAX_BASE_TITLE_SIZE 32

#define AC_EFFECT_FLAG_BILLBOARD 0x00000001
#define AC_EFFECT_FLAG_BILLBOARDY 0x00000002
#define AC_EFFECT_FLAG_REPEAT 0x00000004

#define AC_EFFECT_FLAG_BASEDEPENDANCY 0x00000008
#define AC_EFFECT_FLAG_BASEDPND_PARENT 0x00000010
#define AC_EFFECT_FLAG_BASEDPND_CHILD 0x00000020
#define	AC_EFFECT_FLAG_MISSILE 0x00000040
#define AC_EFFECT_FLAG_DEPEND_ONLYPOS 0x00000080
#define AC_EFFECT_FLAG_BASEDPND_TOOBJ 0x00000100
/* If a missile object has a dependency, rotate it by -90 on the x-axis initially. */
#define	AC_EFFECT_FLAG_BASEDPND_TOMISSILEOBJ 0x00000200

/* Rotated around the Y-axis. */
#define AC_EFFECT_FLAG_TAIL_Y45 0x00010000
#define AC_EFFECT_FLAG_TAIL_Y90 0x00020000
#define AC_EFFECT_FLAG_TAIL_Y135 0x00040000

#define AC_EFFECT_FLAG_OBJ_DUMMY 0x00010000
#define AC_EFFECT_FLAG_OBJ_CHILDOFMISSILE 0x00020000

#define AC_EFFECT_FLAG_SOUND_STOPATEFFECTEND 0x00010000
#define AC_EFFECT_FLAG_SOUND_3DTOSAMEPLE 0x00020000
/* No frustum check. ambience sound-> no location. */
#define AC_EFFECT_FLAG_SOUND_NOFRUSTUMCHK 0x00040000

#define AC_EFFECT_FLAG_PSYS_RANDCOLR 0x00010000
#define AC_EFFECT_FLAG_PSYS_INTERPOLATION 0x00020000
#define AC_EFFECT_FLAG_PSYS_CHILDDEPENDANCY 0x00040000
#define AC_EFFECT_FLAG_PSYS_PARTICLEBILLBOARD 0x00080000
#define AC_EFFECT_FLAG_PSYS_PARTICLEBILLBOARDY 0x00100000
#define AC_EFFECT_FLAG_PSYS_CIRCLEEMITER 0x00200000	
#define	AC_EFFECT_FLAG_PSYS_FILLCIRCLE 0x00400000	
/* If the number of particles exceeds the capacity, no particle generation. */
#define AC_EFFECT_FLAG_PSYS_CAPACITYLIMIT 0x00800000
/* Calculate particle position in the direction of the target. */
#define AC_EFFECT_FLAG_PSYS_TARGETDIR 0x01000000

#define AC_EFFECT_FLAG_PSYSSBH_RANDCOLR 0x00010000
#define AC_EFFECT_FLAG_PSYSSBH_INTERPOLATION 0x00020000
#define AC_EFFECT_FLAG_PSYSSBH_CHILDDEPENDANCY 0x00040000
#define AC_EFFECT_FLAG_PSYSSBH_INVS 0x00080000
#define AC_EFFECT_FLAG_PSYSSBH_REGULARANGLE 0x00100000
/* If the number of particles exceeds the capacity, particles are not created. */
#define AC_EFFECT_FLAG_PSYSSBH_CAPACITYLIMIT 0x00200000
/* A phenomenon that is affected by the camera as before. */
#define	AC_EFFECT_FLAG_PSYSSBH_OLDTYPE 0x00400000
#define AC_EFFECT_FLAG_PSYSSBH_CIRCLEEMITER 0x00800000	
#define	AC_EFFECT_FLAG_PSYSSBH_FILLCIRCLE 0x01000000	
/* Process an exception if the generation angle is less than or equal to 1. Why? because it was originally */
#define AC_EFFECT_FLAG_PSYSSBH_MINANGLE 0x02000000

/* Use the saved center effect position when the flag is off. */
#define	AC_EFFECT_FLAG_POSTFX_CENTERLOCK 0x00010000
/* Only your character can see the PostFX effect. */
#define	AC_EFFECT_FLAG_POSTFX_UNSEENOTHER 0x00020000

/* Cannon direction vector option. */
#define AC_EFFECT_FLAG_PSYS_EMITER_SHOOTRAND 0x00000001
#define AC_EFFECT_FLAG_PSYS_EMITER_SHOOTCONE 0x00000002	
/* Angular velocity and limit angle calculation option. */
#define AC_EFFECT_FLAG_PSYS_EMITER_YAWONEDIR 0x00000004
#define AC_EFFECT_FLAG_PSYS_EMITER_YAWBIDIR 0x00000008
#define AC_EFFECT_FLAG_PSYS_EMITER_PITCHONEDIR 0x00000010
#define AC_EFFECT_FLAG_PSYS_EMITER_PITCHBIDIR 0x00000020
/* Emitter frame position only, direction only rotation speed. */
#define AC_EFFECT_FLAG_PSYS_EMITER_NODEPND 0x00000040
/* Particles do not move. */
#define AC_EFFECT_FLAG_PSYS_EMITER_SHOOTNOMOVE 0x00000080

#define AC_EFFECT_CTRL_SET_SCALE 0x00010000 
/* This effect is an effect that explodes on itself. */
#define AC_EFFECT_CTRL_SET_MAINCHARAC 0x00020000
/* Static effect as octree. */
#define AC_EFFECT_CTRL_SET_STATIC 0x00040000
/* Update callback is called when clump is updated. */
#define AC_EFFECT_CTRL_SET_UPDATEWITHCLUMP 0x00080000
/* Update callback is called when atomic is updated. */
#define AC_EFFECT_CTRL_SET_UPDATEWITHATOMIC 0x00100000
#define AC_EFFECT_CTRL_SET_BEUPDATED 0x00200000 
/* m_dwDelay is set and synchronizes when CTRL_STATE_BEGIN occurs. */
#define AC_EFFECT_CTRL_SET_WILLBEASYNCPOS 0x00400000
#define AC_EFFECT_CTRL_SET_CLUMPEMITER 0x00800000 
#define AC_EFFECT_CTRL_SET_ATOMICEMITER 0x01000000

#define AC_EFFECT_SET_FLAG_BILLBOARD 0x00000001 
#define AC_EFFECT_SET_FLAG_TARGET 0x00000002 
#define AC_EFFECT_SET_FLAG_MISSILE 0x00000004 
#define AC_EFFECT_SET_FLAG_ONLYTARGET 0x00000008 
/* If there is only sound.  */
#define AC_EFFECT_SET_FLAG_ONLYSOUND 0x00000 010
/* If there is only tail.  */
#define AC_EFFECT_SET_FLAG_ONLYTAIL 0x00000020
/* Moving frame  */
#define AC_EFFECT_SET_FLAG_MFRM 0x00000040
/* Moving frame source  */
#define AC_EFFECT_SET_FLAG_MFRM_SRC 0x00000080
/* Moving frame destnation  */
#define AC_EFFECT_SET_FLAG_MFRM_DST 0x00000100
/* Moving frame camera  */
#define AC_EFFECT_SET_FLAG_MFRM_CAM 0x00000200
/* With tail no base dependency there is. ex) Knife trajectory  */
#define AC_EFFECT_SET_FLAG_HASTAIL 0x00000400
#define AC_EFFECT_BASE_DEPENDANCY_FLAG_ONLYPOS 0x00000001 
/* When the parent is an object.. */
#define AC_EFFECT_BASE_DEPENDANCY_FLAG_HASNODE 0x00000002

BEGIN_DECLS

enum ac_effect_loop_option {
	AC_EFFECT_LOOP_NONE = 0,
	AC_EFFECT_LOOP_INFINITY,
	AC_EFFECT_LOOP_ONE_DIRECTION,
	AC_EFFECT_LOOP_POSITIVE,
	AC_EFFECT_LOOP_NEGATIVE,
	AC_EFFECT_LOOP_END,
	AC_EFFECT_LOOP_COUNT
};

enum ac_effect_animation_type {
	AC_EFFECT_ANIMATION_COLOR = 0, /* color */
	AC_EFFECT_ANIMATION_TUTV, /* uv */
	AC_EFFECT_ANIMATION_MISSILE, /* translate */
	AC_EFFECT_ANIMATION_LINEAR, /* translate */
	AC_EFFECT_ANIMATION_REVOLUTION, /* translate */
	AC_EFFECT_ANIMATION_ROTATION, /* rotation */
	AC_EFFECT_ANIMATION_RPSPLINE, /* translation + rotation */
	AC_EFFECT_ANIMATION_RTANIM, /* translation + rotation */
	AC_EFFECT_ANIMATION_SCALE, /* scale */
	AC_EFFECT_ANIMATION_PARTICLEPOSSCALE, /* for each particle */
	AC_EFFECT_ANIMATION_POSTFX,
	AC_EFFECT_ANIMATION_CAMERA,
	AC_EFFECT_ANIMATION_COUNT
};

enum ac_effect_type {
	AC_EFFECT_BOARD = 0,
	AC_EFFECT_PSYS,
	AC_EFFECT_PSYS_SIMPLEBLACKHOLE,
	AC_EFFECT_TAIL,
	AC_EFFECT_OBJECT,
	AC_EFFECT_LIGHT,
	AC_EFFECT_SOUND,
	AC_EFFECT_MOVINGFRAME,
	AC_EFFECT_TERRAINBOARD,
	AC_EFFECT_POSTFX,
	AC_EFFECT_CAMERA,
	AC_EFFECT_COUNT
};

enum ac_effect_blend_type {
	AC_EFFECT_BLEND_NONE = 0,	/* AddClumpToWorld */
	AC_EFFECT_BLEND_ADD_TEXSTAGE, /* AddAlphaBModeClumpToWorld */
	AC_EFFECT_BLEND_ADD_ONE_ONE,
	AC_EFFECT_BLEND_ADD_SRCA_ONE,
	AC_EFFECT_BLEND_ADD_SRCA_INVSRCA,
	AC_EFFECT_BLEND_ADD_SRCC_INVSRCC,
	AC_EFFECT_BLEND_REVSUB_ONE_ONE,
	AC_EFFECT_BLEND_REVSUB_SRCA_ONE,
	AC_EFFECT_BLEND_REVSUB_SRCA_INVSRCA,
	AC_EFFECT_BLEND_REVSUB_SRCC_INVSRCC,
	AC_EFFECT_BLEND_SUB_ONE_ONE,
	AC_EFFECT_BLEND_SUB_SRCA_ONE,
	AC_EFFECT_BLEND_SUB_SRCA_INVSRCA,
	AC_EFFECT_BLEND_SUB_SRCC_INVSRCC,
	AC_EFFECT_BLEND_COUNT
};

enum ac_effect_camera_type {
	AC_EFFECT_CAMERA_ZOOM,
	AC_EFFECT_CAMERA_ROTATE,
	AC_EFFECT_CAMERA_OGIRIN, /*	Return to original position */
	AC_EFFECT_CAMERA_COUNT,

};

enum ac_effect_light_type {
	AC_EFFECT_LIGHT_POINT = rpLIGHTPOINT,
	AC_EFFECT_LIGHT_SPOT = rpLIGHTSPOT,
	AC_EFFECT_LIGHT_SOFTSPOT = rpLIGHTSPOTSOFT,
};

enum ac_effect_moving_frame_type {
	AC_EFFECT_MFRM_SHAKE = 0,
	AC_EFFECT_MFRM_SPLINE,
	AC_EFFECT_MFRM_COUNT
};

enum ac_effect_oscillation_axis {
	AC_EFFECT_OSCILLATIONAXIS_LOCAL_X = 0,
	AC_EFFECT_OSCILLATIONAXIS_LOCAL_Y,
	AC_EFFECT_OSCILLATIONAXIS_LOCAL_Z,
	AC_EFFECT_OSCILLATIONAXIS_WORLD_X,
	AC_EFFECT_OSCILLATIONAXIS_WORLD_Y,
	AC_EFFECT_OSCILLATIONAXIS_WORLD_Z,
	AC_EFFECT_OSCILLATIONAXIS_CAMERA_X,
	AC_EFFECT_OSCILLATIONAXIS_CAMERA_Y,
	AC_EFFECT_OSCILLATIONAXIS_CAMERA_Z,
	AC_EFFECT_OSCILLATIONAXIS_RANDOM,
	AC_EFFECT_OSCILLATIONAXIS_COUNT
};

enum ac_effect_whose_frame {
	AC_EFFECT_FRAME_CAM = 0,
	AC_EFFECT_FRAME_SRC,
	AC_EFFECT_FRAME_DST,
	AC_EFFECT_FRAME_COUNT
};

enum ac_effect_particle_group_type {
	AC_EFFECT_PARTICLE_GROUP_POINT = 0,
	AC_EFFECT_PARTICLE_GROUP_BOX,
	AC_EFFECT_PARTICLE_GROUP_CYLINDER,
	AC_EFFECT_PARTICLE_GROUP_SPHERE,
	AC_EFFECT_PARTICLE_GROUP_COUNT
};

enum ac_effect_fx_type {
	AC_EFFECT_FX_WAVE = 0,
	AC_EFFECT_FX_RIPPLE,
	AC_EFFECT_FX_SHOCKWAVE,
	AC_EFFECT_FX_EDGEDETECT,
	AC_EFFECT_FX_SEPIA,
	AC_EFFECT_FX_GRAYSCALE,
	AC_EFFECT_FX_NEGATIVE,
	AC_EFFECT_FX_CLAMPINGCIRCLE,
	AC_EFFECT_FX_DARKEN,
	AC_EFFECT_FX_BRIGHTEN,
	AC_EFFECT_FX_TWIST,
	AC_EFFECT_FX_COUNT
};

enum ac_effect_sound_type {
	AC_EFFECT_SOUND_NONE = 0,
	AC_EFFECT_SOUND_3DSOUND,
	AC_EFFECT_SOUND_STREAM,
	AC_EFFECT_SOUND_SAMPLE,
	AC_EFFECT_SOUND_BGM,
	AC_EFFECT_SOUND_COUNT
};

enum ac_effect_ctrl_state {
	AC_EFFECT_CTRL_STATE_WAIT = 0,
	AC_EFFECT_CTRL_STATE_BEGIN,
	AC_EFFECT_CTRL_STATE_GOINGON,
	AC_EFFECT_CTRL_STATE_END,
	AC_EFFECT_CTRL_STATE_CONTINUE,
	AC_EFFECT_CTRL_STATE_MISSILEEND,
	AC_EFFECT_CTRL_STATE_COUNT
};

enum ac_effect_tail_info_type {
	AC_EFFECT_TAIL_INFO_NODEBASE = 0,
	AC_EFFECT_TAIL_INFO_HEIGHTBASE,
};

enum ac_effect_use_flag_bits {
	/* When you need to have a parent frame  */
	AC_EFFECT_USE_FLAG_LINKTOPARENT = 0x00000001,
	/* When you have a parent frame but don't want to be affected by the scale..  */
	AC_EFFECT_USE_FLAG_NOSCALE = 0x00000002,
	/* Is this an effect that your character uses - 3D Sound to Stereo  */
	AC_EFFECT_USE_FLAG_MAINCHARAC = 0x00000004,
	/*  Move the direction reference from parent to target.  */
	AC_EFFECT_USE_FLAG_DIR_PAR_TO_TAR = 0x00000008,
	/* Direction reference from target to parent.  */
	AC_EFFECT_USE_FLAG_DIR_TAR_TO_PAR = 0x00000010,
	/* Ignore the height value when setting the direction.  */
	AC_EFFECT_USE_FLAG_DIR_IGN_HEIGHT = 0x00000020,
	AC_EFFECT_USE_FLAG_EMITER_WITH_CLUMP = 0x00000040, 
	AC_EFFECT_USE_FLAG_EMITER_WITH_ATOMIC = 0x00000080, 
	/* static effect (things to put in octree) */
	AC_EFFECT_USE_FLAG_STATICEFFECT = 0x00010000,
};

enum ac_effect_process_type {
	AC_EFFECT_PROCESS_TYPE_NONE = 0,
	AC_EFFECT_PROCESS_TYPE_EFFECT_START, /* Start Effect */
	AC_EFFECT_PROCESS_TYPE_EFFECT_END, /* End Effect */
	/* Projectile reached! - The effect is not over */
	AC_EFFECT_PROCESS_TYPE_EFFECT_MISSLEEND,
};

enum ac_effect_render_flag {
	AC_EFFECT_RENDER_FLAG_RENDER_ADD = 0x01,
	AC_EFFECT_RENDER_FLAG_DONOT_CULL = 0x02
};

enum ac_effect_ctrl_update_flag_bits {
	AC_EFFECT_CTRL_UPDATEFLAG_BILLBOARD = 0x00000001,
	AC_EFFECT_CTRL_UPDATEFLAG_BILLBOARDY = 0x00000002,
	AC_EFFECT_CTRL_UPDATEFLAG_SCALE = 0x00000004,
	AC_EFFECT_CTRL_UPDATEFLAG_ROT = 0x00000008,
	AC_EFFECT_CTRL_UPDATEFLAG_TRANS = 0x00000010,
	AC_EFFECT_CTRL_UPDATEFLAG_RTANIM = 0x00000020,
	AC_EFFECT_CTRL_UPDATEFLAG_COLR = 0x00000040,
	AC_EFFECT_CTRL_UPDATEFLAG_UVRECT = 0x00000080,
};

enum ac_effect_tail_type {
	AC_EFFECT_TAIL_BEZ = 0, /* BEZIER */
	AC_EFFECT_TAIL_CMR, /* CATMULE-ROM */
	AC_EFFECT_TAIL_LINE, /* ONE-PLATE */
	AC_EFFECT_TAIL_COUNT
};

enum ac_effect_missile_move_type {
	AC_EFFECT_MISSILE_MOVE_LINEAR = 0,
	AC_EFFECT_MISSILE_MOVE_BEZIER3,
	AC_EFFECT_MISSILE_MOVE_LINEAR_ROT,
	AC_EFFECT_MISSILE_MOVE_BEZIER3_ROT,
	AC_EFFECT_MISSILE_MOVE_ZIGZAG,
	AC_EFFECT_MISSILE_MOVE_COUNT
};

enum ac_effect_set_resource_load_status {
	AC_EFFECT_SET_RESOURCELOADSTATUS_LOADING = 0,
	AC_EFFECT_SET_RESOURCELOADSTATUS_LOADED,
	AC_EFFECT_SET_RESOURCELOADSTATUS_UNKNOWN
};

struct ac_effect_uv_rect {
	float left;
	float top;
	float right;
	float bottom;
};

struct ac_effect_revolution {
	float yaw;
	float pitch;
	float roll;
};

struct ac_effect_time_table_loop {
	enum ac_effect_loop_option loop_option;
	uint32_t current_time; /* Accumulated time. */
};

struct ac_effect_animation {
	void (*Construct)(struct ac_effect_animation * anim, enum ac_effect_animation_type type, uint32_t flags);
	boolean (*GetValue)(struct ac_effect_animation * anim, void * output, uint32_t time);
	boolean (*SetValue)(struct ac_effect_animation * anim, int32_t index, void * input, uint32_t time);
	boolean (*InsertValue)(struct ac_effect_animation * anim, void * input, uint32_t time);
	boolean (*DeleteValue)(struct ac_effect_animation * anim, uint32_t time);
	void (*SetLife)(struct ac_effect_animation * anim, uint32_t life);
	int32_t (*UpdateValue)(struct ac_effect_animation * anim, uint32_t accumulate_time, struct ac_effect_control_base * base, uint32_t flag_reserved);
	int32_t (*StreamWrite)(struct ac_effect_module * mod, struct ac_effect_animation * anim, struct bin_stream * stream);
	int32_t (*StreamRead)(struct ac_effect_module * mod, struct ac_effect_animation * anim, struct bin_stream * stream);

	enum ac_effect_animation_type type;
	uint32_t flags;
	uint32_t life_time;
	enum ac_effect_loop_option loop_option;
};

struct ac_effect_time_value {
	uint32_t time;
	float value;
};

struct ac_effect_time_value_color {
	uint32_t time;
	RwRGBA value;
};

struct ac_effect_time_value_uv {
	uint32_t time;
	struct ac_effect_uv_rect value;
};

struct ac_effect_time_value_vec3 {
	uint32_t time;
	RwV3d value;
};

struct ac_effect_time_value_revolution {
	uint32_t time;
	struct ac_effect_revolution value;
};

struct ac_effect_animation_color {
	struct ac_effect_animation base;
	struct ac_effect_time_value_color * time_table;
};

struct ac_effect_animation_uv {
	struct ac_effect_animation base;
	struct ac_effect_time_value_uv * time_table;
};

struct ac_effect_animation_missile {
	struct ac_effect_animation base;
	float min_speed; /* lowest speed */
	float max_speed; /* top speed */
	float speed0; /* initial speed. (cm/sec) */
	float acceleration; /* acceleration<velocity direction> ( cm / sec^2 ) */
	float rotation; /* how many rotations per second */
	float radius; /* rotate , zigzag radius */
	float zigzag_length; /* length to be zigzag */
	float prev_cof; /* distance moved in previous frame */
	float prev_time; /* save all time */
};

struct ac_effect_animation_linear {
	struct ac_effect_animation base;
	struct ac_effect_time_value_vec3 * time_table;
};

struct ac_effect_animation_revolution {
	struct ac_effect_animation base;
	RwV3d rotation_axis;
	RwV3d rotation_side;
	struct ac_effect_time_value_revolution * time_table;
};

struct ac_effect_animation_rotation {
	struct ac_effect_animation base;
	RwV3d rotation_axis;
	struct ac_effect_time_value * time_table;
};

struct ac_effect_animation_spline {
	struct ac_effect_animation base;
	char spline_file_name[AC_EFFECT_MAX_FILE_NAME_SIZE];
	float inverse_life;
	RpSpline * spline;
};

struct ac_effect_animation_rt_anim {
	struct ac_effect_animation base;
	char anim_file_name[AC_EFFECT_MAX_FILE_NAME_SIZE];
	RtAnimAnimation * rt_anim;
};

struct ac_effect_animation_scale {
	struct ac_effect_animation base;
	struct ac_effect_time_value_vec3 * time_table;
};

struct ac_effect_animation_particle_pos_scale {
	struct ac_effect_animation base;
	struct ac_effect_time_value * time_table;
};

struct ac_effect_animation_post_fx {
	struct ac_effect_animation base;
};

struct ac_effect_animation_camera {
	struct ac_effect_animation base;
	int camera_type; /* Camera Type */
	RwV3d camera_pos; /* Start Camera Position ( Offset ) */
	RwV3d camera_dir; /* Camera Direction */
	float camera_speed; /* Camera Speed ( Seconds ) */
	float rotate_count; /* Rotate Camera */
	float camera_rotate;
	float move_length; /* Zoom Camera */
};

struct ac_effect_texture {
	int32_t index;
	char texture_name[AC_EFFECT_MAX_FILE_NAME_SIZE];
	char mask_name[AC_EFFECT_MAX_FILE_NAME_SIZE];
	int32_t refcount;
	bgfx_texture_handle_t texture;
};

struct ac_effect_texture_info {
	int32_t index;
	struct ac_effect_texture * texture;
};

struct ac_effect_var_size_info {
	uint32_t texture_count;
	uint32_t base_count;
	uint32_t base_dependancy_count;
};

struct ac_effect_base {
	const RwMatrix * (*GetTranslationMatrix)(const struct ac_effect_base * effect);
	const RwMatrix * (*GetRotationMatrix)(const struct ac_effect_base * effect);
	boolean (*IsRenderBase)(const struct ac_effect_base * effect);
	boolean (*StreamWrite)(struct ac_effect_module * mod, const struct ac_effect_base * effect, struct bin_stream * stream);
	boolean (*StreamRead)(struct ac_effect_module * mod, struct ac_effect_base * effect, struct bin_stream * stream);

	enum ac_effect_type type;
	char base_title[AC_EFFECT_MAX_BASE_TITLE_SIZE];
	uint32_t delay;
	uint32_t life;
	enum ac_effect_loop_option loop_option;
	uint32_t flags;
	uint32_t animation_count;
	struct ac_effect_animation ** animations;
	boolean force_immediate;
};

struct ac_effect_render_base {
	struct ac_effect_base base;
	enum ac_effect_blend_type blend_type;
	struct ac_effect_texture_info texture_info;
	RwV3d initial_position;
	struct ac_effect_revolution initial_revolution;
	float translate[4][4];
	float rotate[4][4];
};

struct ac_effect_board {
	struct ac_effect_render_base base;
};

struct ac_effect_camera {
	struct ac_effect_base base;
	enum ac_effect_camera_type camera_type;
	RwCamera * camera;
	RwV3d camera_pos;
	RwV3d camera_dir;
};

struct ac_effect_light {
	struct ac_effect_base base;
	enum ac_effect_light_type light_type;
	RwV3d center;
	struct ac_effect_revolution revolution; /* dir */
	float con_angle;
	RwSurfaceProperties surface_prop;
	RwRGBA color;
	RwMatrix translate;
	RwMatrix rotate;
};

struct ac_effect_moving_frame {
	struct ac_effect_base base;
	enum ac_effect_moving_frame_type type;
	enum ac_effect_oscillation_axis axis;
	enum ac_effect_whose_frame whose;
	float amplitude;
	uint32_t duration;
	float total_cycle;
	float speed;
	float cof;
};

struct ac_effect_object {
	struct ac_effect_render_base base;
	char clump_file_name[AC_EFFECT_MAX_FILE_NAME_SIZE];
	struct ac_mesh_clump * clump;
	RwRGBA prelit_color;
};

struct ac_effect_particle_group_box {
	float hwidth;
	float hheight;
	float hdepth;
};

struct ac_effect_particle_group_cylinder {
	float radius;
	float hheight;
};

struct ac_effect_particle_group_sphere {
	float radius;
};

struct ac_effect_environment_param {
	RwV3d gravity;
	RwV3d wind;
};

struct ac_effect_cof_environment_param {
	/* cof = Coefficient of friction. */
	float cof_gravity;
	float cof_air_resistance;
};

struct ac_effect_particle {
	RwV3d initial_pos;
	RwV3d initial_velocity;
	RwV3d scale;
	RwRGBA color;
	struct ac_effect_uv_rect uv;
	float omega; /* angular speed in PSyst, angle in PSystSBH. */
	uint32_t life;
	uint32_t start_time;
	struct ac_effect_cof_environment_param cof_environment;
	struct ac_effect_particle * next;
};

union ac_effect_particle_group_data {
	struct ac_effect_particle_group_box box;
	struct ac_effect_particle_group_cylinder cylinder;
	struct ac_effect_particle_group_sphere sphere;
};

struct ac_effect_particle_emitter {
	float power; /* Initial velocity.. ( cm / sec ) */
	float power_offset;
	float gun_length; /* gun barrel length.. ( cm )  */
	float gun_length_offset;			
	uint32_t one_shoot_count; /* Number of particles to fire at once.  */
	uint32_t one_shoot_count_offset;
	float omega_yaw_world;
	float omega_pitch_local; /* Emitter's angular velocity. ( deg / sec )  */
	float min_yaw;
	float max_yaw; /* currYawAngle = currYawAngle * m_fMaxYaw / 360.f; */
	float min_pitch;
	float max_pitch; /* currPitchAngle = currPitchAngle * m_fMaxPitch / 360.f; */
	RwV3d dir; /* Initial gun barrel direction */
	RwV3d side; /* gun barrel's side_vector */
	float con_angle; /* gun barrel cone angle */
	uint32_t emitter_flags; /* emiter's flag */
	enum ac_effect_particle_group_type group_type;
	union ac_effect_particle_group_data group_data;
};

struct ac_effect_particle_prop {
	float angular_speed; /* Angular velocity of the particle. */
	float angular_speed_offset;
	uint32_t particle_life; /* Life time of the particle. */
	uint32_t particle_life_offset;
	struct ac_effect_cof_environment_param cof_environment; /* Environmental impact factor. */
	struct ac_effect_cof_environment_param cof_environment_offset;
	uint32_t particle_flags; /* Particle's flag */
};

struct ac_effect_particle_system {
	struct ac_effect_render_base base;
	int32_t capacity; /* Total number of retained particles. */
	uint32_t shoot_delay; /* Fire interval. */
	boolean is_dir_of_target_use;
	char clump_file_name[AC_EFFECT_MAX_FILE_NAME_SIZE];
	struct ac_mesh_clump * lump;
	struct ac_effect_particle_emitter emitter;
	struct ac_effect_particle_prop particle_prop;
};

struct ac_effect_particle_system_simple_black_hole {
	struct ac_effect_render_base base;
	int32_t capacity;
	int32_t one_shoot_count;
	int32_t one_shoot_count_offset;
	uint32_t time_gap; /* ( milli sec ) */
	uint32_t particle_life; /* ( milli sec ) */
	float initial_speed; /* ( cm / sec ) */
	float initial_speed_offset; /* ( cm / sec ) */
	float roll_min; /* ( deg ) */
	float roll_max; /* ( deg ) */
	float roll_step; /* ( deg )	/ (m_fRollMax-m_fRollMin)/Capacity */
	float radius;
};

struct ac_effect_tail {
	struct ac_effect_render_base base;
	int32_t max_count; /* Maximum number of rectangles..  */
	uint32_t time_gap; /*  Time to add points.  */
	uint32_t point_life; /*  Duration of the added point.  */
	float inverse_point_life; /* 1.f/m_dwPointLife  */
	float height1; /* height base  */
	float height2; /* In case of node base, this value is ignored. */
};

struct ac_effect_post_fx {
	struct ac_effect_base base;
	enum ac_effect_fx_type fx_type;
	RwV2d center; /* FX Start Position */
	uint32_t flags;
};

struct ac_effect_sound {
	struct ac_effect_base base;
	enum ac_effect_sound_type sound_type;
	char sound_file_name[AC_EFFECT_MAX_FILE_NAME_SIZE];
	char mono_file_name[AC_EFFECT_MAX_FILE_NAME_SIZE];
	uint32_t loop_count;
	float volume;
	uint32_t sound_length;
};

struct ac_effect_terrain_board {
	struct ac_effect_render_base base;
};

struct ac_effect_notice_effect_process_data {
	enum ac_effect_process_type effect_process_type;
	uint32_t owner_cid;
	uint32_t target_cid;
	boolean is_missile;
	int32_t cust_data;
	int32_t cust_id;
};

typedef boolean (*ac_effect_notice_effect_process_t)(
	struct ac_effect_notice_effect_process_data	* data, 
	void * cs);

struct ac_effect_set_callback_info {
	boolean has_callback;
	void * base;
	struct ac_effect_notice_effect_process_data notice_effect_process_data;
	ac_effect_notice_effect_process_t NoticeEffectProcess;
	void * notice_cb_class;
};

struct ac_effect_tail_info_node_base {
	RwFrame * frame_node1;
	RwFrame * frame_node2;
};

struct ac_effect_tail_info_height_base {
	RwFrame * tail_target_frame;
	float height1;
	float height2;
};

struct ac_effect_tail_info {
	enum ac_effect_tail_info_type type;
	struct ac_effect_tail_info_node_base node_info;
	struct ac_effect_tail_info_height_base height_info;
};

struct ac_effect_missile_target_info {
	RwFrame * target_frame;
	RwV3d center; /*relative to ltm.pos : rwsphere.center - ltm.pos */
	RwV3d target;
	/* for linear_rot , bezier3_rot */
	RwV3d last_pos;	/* Rotate Data */
	int32_t rotate_speed;	/* Rotate Speed */
	/* for linear_rot , bezier3_rot , zigzag */
	int32_t radius;	
	/* for zigzag	0 = left right , 1 = up down */
	int32_t zigzag_type;
	/*for bezier3 */
	RwV3d gp0; /* GP : GuidPoint */
	RwV3d gp1;
	RwV3d last_pt;
};

struct ac_effect_use_info { 
	uint32_t effect_id;
	RwV3d center;
	float scale;
	float particle_num_scale;
	float time_scale;
	RwRGBA rgb_scale;
	RwFrame * parent_frame; 
	RwFrame * target_frame; 
	RpHAnimHierarchy * hierarchy; 
	RpClump * parent_clump; /* update & render */
	RpClump * emitter_clump; 
	RpAtomic * emitter_atomic; 
	uint32_t delay; 
	uint32_t life; 
	uint32_t flags; 
	void * base; 
	uint32_t owner_cid; 
	uint32_t target_cid; 
	int32_t cust_data; 
	void * notice_cb_class; 
	ac_effect_notice_effect_process_t notice_cb; 
	int32_t cust_id; 
	RtQuat quat_rotation; 
	void * data; 
	boolean is_add_update_list; 
	RwV3d base_dir; 
	struct ac_effect_tail_info tail_info; 
	struct ac_effect_missile_target_info missile_target_info;
};

struct ac_effect_missile_info {	
	enum ac_effect_missile_move_type missile_type;
	RwV3d offsetm_v3dOffset; /* Relative to missile direction. */
};

struct ac_effect_base_dependancy {
	int32_t parent_index;
	int32_t child_index;
	uint32_t flags;
};

struct ac_effect_set {
	uint32_t effect_set_id;
	char title[AC_EFFECT_MAX_BASE_TITLE_SIZE];
	uint32_t effect_set_life;
	enum ac_effect_loop_option loop_option;
	uint32_t effect_set_flags;
	struct ac_effect_missile_info missile_info;
	RwSphere bsphere;
	RwBBox bbox;
	struct ac_effect_var_size_info var_size_info;
	int32_t file_offset;
	int32_t file_size;
	int32_t refcount;
	int32_t accum_count;
	uint32_t last_shoot_time;
	struct ac_effect_base ** effects;
	struct ac_effect_texture * textures;
	struct ac_effect_base_dependancy * dependancies;
	int32_t current_num_of_loading_base;
	enum ac_effect_set_resource_load_status load_status;
	boolean force_immediate;
};

struct ac_effect_ctrl {
	uint32_t id; /* effSet's id, effBase's index */
	uint32_t flags;
	enum ac_effect_ctrl_state state;

	void (*SetState)(struct ac_effect_ctrl * ctrl, enum ac_effect_ctrl_state state);
};

struct ac_effect_ctrl_base {
	struct ac_effect_ctrl base;

	struct ac_mesh_clump * (*GetClump)(struct ac_effect_ctrl_base * ctrl);
	RwFrame * (*GetFrame)(struct ac_effect_ctrl_base * ctrl);
	boolean (*Update)(struct ac_effect_ctrl_base * ctrl, uint32_t accumulate_time);
	boolean (*Render)(struct ac_effect_ctrl_base * ctrl);
	void (*PostUpdateFrame)(struct ac_effect_ctrl_base * ctrl, uint32_t current_time);

	struct ac_effect_ctrl_set * ctrl_set;
	struct ac_effect_base * effect_base;
	struct ac_effect_anim_rt_anim * effect_rt_anim;
	struct ac_effect_anim_scale * effect_scale;
	RwV3d dir_for_missile;
	uint32_t update_flags;
	RwRGBA rgba;
	struct ac_effect_uv_rect uv;
	float accumulated_angle; /* Because rotation has angular velocity. */
};

struct ac_effect_ctrl_base_entry {
	struct ac_effect_ctl_base * ctrl_base;
	int32_t effect_id;
	int32_t create_id;
};

struct ac_effect_ctrl_async_data {
	RwFrame * parent_frame;
	RwFrame * target_frame;
	RwV3d offset;
	uint32_t use_flags;
};

struct ac_effect_in_frame_list {
	struct ac_effect_frame_list * next;
};

struct ac_effect_ctrl_set {
	struct ac_effect_ctrl base;
	union {
		struct ac_mesh_clump * emitter_clump;
		struct ac_mesh_geometry * mitter_geometry;
	};
	struct ac_mesh_clump * parent_clump;
	struct ac_effect_set * effect_set;
	RwFrame * frame;
	struct ac_effect_time_table_loop time_loop;
	struct ac_effect_missile_target_info missile_target_info;
	struct ac_effect_ctrl_async_data async_data;
	uint32_t life;
	uint32_t created_time;
	uint32_t delay;
	uint32_t continuation; /* missile_end -> time extension. */
	float scale;
	float particle_num_scale;
	float time_scale;
	RwRGBA rgba_scale;
	struct ac_effect_set_callback_info callback_info;
	RwSphere sphere;
	boolean added_to_render_or_octree;
	int32_t z_index_by_camera;
	RtQuat quat_rotation;
	struct ac_effect_ctrl_sound * sound_3d; /* Updates position every frame. */
	struct ac_effect_ctrl_sound * sound_no_frustum_chk;
	struct ap_admin effect_admin;
	struct ac_effect_in_frame_list pre_remove_frame_list;
};

struct ac_effect_ctrl_board {
	struct ac_effect_ctrl_base base;
	RwFrame * frame;
};

struct ac_effect_ctrl_terrain_board {
	struct ac_effect_ctrl_base base;
	RwFrame * frame;
	float radius;
};

struct ac_effect_ctrl_particle_system {
	struct ac_effect_ctrl_base base;
	RwFrame * parent;
	RwFrame * frame;
	struct ac_effect_particle * particle_list;
	uint32_t last_accumulate_time;
	uint32_t last_shoot_time;
	RwV3d before_pos;
	RwV3d base_dir;
	/* particle */
	struct ac_effect_animation_color * anim_color;
	struct ac_effect_animation_uv * anim_uv;
	struct ac_effect_animation_scale * anim_scale;
	/* emiter */
	struct ac_effect_animation_missile * anim_missile;
	struct ac_effect_animation_linear * anim_linear;
	struct ac_effect_animation_revolution * anim_revolution;
	struct ac_effect_animation_rotation * anim_rotation;
	struct ac_effect_animation_spline * anim_spline;
	struct ac_effect_animation_particle_pos_scale * anim_particle_pos_scale;
};

struct ac_effect_ctrl_particle_system_simple_black_hole {
	struct ac_effect_ctrl_base base;
	RwFrame * frame;
	struct ac_effect_particle * particle_list;
	uint32_t last_accumulate_time;
	uint32_t last_shoot_time;
	/* particle */
	struct ac_effect_animation_color * anim_color;
	struct ac_effect_animation_uv * anim_uv;
	struct ac_effect_animation_scale * anim_scale;
	struct ac_effect_animation_particle_pos_scale * anim_particle_pos_scale;
};

struct ac_effect_tail_point {
	uint32_t time;
	RwV3d p1;
	RwV3d p2;
	struct ac_effect_tail_point * next;
};

/* Function pointer to get the frame at certain instant. */
typedef const RwFrame * (*ac_effect_get_frame_t)(uint32_t time);		

struct ac_effect_ctrl_tail {
	struct ac_effect_ctrl_base base;
	enum ac_effect_tail_type type;
	struct ac_effect_tail_point * point_list;
	struct ac_effect_tail_point current_point;
	struct ac_effect_tail_point start_point;
	uint32_t last_accumulate_time;
	ac_effect_get_frame_t GetFrame;
	struct ac_effect_tail_info tail_info;
	struct ac_effect_animation_color * anim_color;
	struct ac_effect_animation_uv * anim_uv;
};

struct ac_effect_ctrl_object {
	struct ac_effect_ctrl_base base;
	RwFrame * frame;
	struct ac_mesh_clump * clump;
	RtAnimAnimation * rt_anim;
	struct ac_effect_in_frame_list pre_remove_frame_list;
	boolean added_to_world;
	enum ac_effect_blend_type current_blend_type;
};

struct ac_effect_ctrl_light {
	struct ac_effect_ctrl_base base;
	RpLight * light;
	boolean added_to_world;
};

struct ac_effect_ctrl_moving_frame {
	struct ac_effect_ctrl_base base;
	RwFrame * frame;
};

struct ac_effect_ctrl_sound {
	struct ac_effect_ctrl_base base;
	enum ac_effect_sound_type sound_type;
	uint32_t sound_index;
	boolean is_played;
	uint32_t loop_count;
	float volume;
	uint32_t played_time;
};

struct ac_effect_ctrl_post_fx {
	struct ac_effect_ctrl_base base;
};

struct ac_effect_ctrl_camera {
	enum ac_effect_camera_type type;
	RwCamera * camera;
	RwV3d camera_pos;
	RwV3d camera_dir;
	//Technique* m_pTechnique; // Technique;
};

struct ac_effect_module * ac_effect_create_module();

struct ac_effect_set * ac_effect_new_set(struct ac_effect_module * mod);

void ac_effect_release_set(struct ac_effect_module * mod, struct ac_effect_set * set);

struct ac_effect_base * ac_effect_new_effect(
	struct ac_effect_module * mod, 
	enum ac_effect_type type);

void ac_effect_free_effect(
	struct ac_effect_module * mod, 
	struct ac_effect_base * effect);

struct ac_effect_animation * ac_effect_new_animation(
	struct ac_effect_module * mod, 
	enum ac_effect_animation_type type);

void ac_effect_free_animation(
	struct ac_effect_module * mod, 
	struct ac_effect_animation * anim);

END_DECLS

#endif /* _AC_EFFECT_H_ */
