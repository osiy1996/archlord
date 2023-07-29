#ifndef _AC_OBJECT_H_
#define _AC_OBJECT_H_

#include "core/macros.h"
#include "core/types.h"

#include "vendor/cglm/cglm.h"
#include "vendor/RenderWare/rwplcore.h"
#include "vendor/bgfx/c99/bgfx.h"

#include "public/ap_map.h"
#include "public/ap_object.h"

#include "client/ac_animation.h"
#include "client/ac_define.h"
#include "client/ac_render.h"

#define AC_OBJECT_MODULE_NAME "AgcmObject"

#define AC_OBJECT_ANIM_NAME_LENGTH 128
#define	AC_OBJECT_DFF_NAME_LENGTH 64
#define	AC_OBJECT_CATEGORY_LENGTH 64
#define AC_OBJECT_MAX_ALLOC_ANIM_DATA 1
#define AC_OBJECT_ANIMATION_ATTACHED_DATA_KEY "AGCDOBJECT"

BEGIN_DECLS

enum ac_object_type {
	AC_OBJECT_TYPE_NONE = 0x0000,
	AC_OBJECT_TYPE_OBJECT = 0x0001,
	AC_OBJECT_TYPE_CHARACTER = 0x0002,
	AC_OBJECT_TYPE_ITEM = 0x0003,
	AC_OBJECT_TYPE_WORLDSECTOR = 0x0004,
	AC_OBJECT_TYPE_TYPEMASK = 0x000f,
	AC_OBJECT_TYPE_NO_MTEXTURE = 0x0010,
	AC_OBJECT_TYPE_SECTOR_ROUGHMAP = 0x0020,
	AC_OBJECT_TYPE_RESERVED2 = 0x0040,
	AC_OBJECT_TYPE_RESERVED3 = 0x0080,
	AC_OBJECT_TYPE_OPTIONMASK = 0x00f0,
	AC_OBJECT_TYPE_BLOCKING = 0x00000100,
	AC_OBJECT_TYPE_RIDABLE = 0x00000200,
	AC_OBJECT_TYPE_NO_CAMERA_ALPHA = 0x00000400,
	AC_OBJECT_TYPE_USE_ALPHA = 0x00000800,
	AC_OBJECT_TYPE_USE_AMBIENT = 0x00001000,
	AC_OBJECT_TYPE_USE_LIGHT = 0x00002000,
	AC_OBJECT_TYPE_USE_PRE_LIGHT = 0x00004000,
	AC_OBJECT_TYPE_USE_FADE_IN_OUT = 0x00008000,
	AC_OBJECT_TYPE_IS_SYSTEM_OBJECT = 0x00010000,
	AC_OBJECT_TYPE_MOVE_INSECTOR = 0x00020000,
	AC_OBJECT_TYPE_NO_INTERSECTION = 0x00040000,
	AC_OBJECT_TYPE_WORLDADD = 0x00080000,
	AC_OBJECT_TYPE_OBJECTSHADOW = 0x00100000,
	AC_OBJECT_TYPE_RENDER_UDA = 0x00200000,
	AC_OBJECT_TYPE_USE_ALPHAFUNC = 0x00400000,
	AC_OBJECT_TYPE_OCCLUDER = 0x00800000,
	AC_OBJECT_TYPE_DUNGEON_STRUCTURE = 0x01000000,
	AC_OBJECT_TYPE_DUNGEON_DOME = 0x02000000,
	AC_OBJECT_TYPE_CAM_ZOOM = 0x04000000,
	AC_OBJECT_TYPE_CAM_ALPHA = 0x08000000,
	AC_OBJECT_TYPE_INVISIBLE = 0x10000000,
	AC_OBJECT_TYPE_FORCED_RENDER_EFFECT = 0x20000000,
	AC_OBJECT_TYPE_DONOT_CULL = 0x40000000,
	AC_OBJECT_TYPE_PROPERTY_FILTER = 0xffffff00,
};

enum ac_object_status {
	AC_OBJECT_STATUS_INIT			= 0,
	AC_OBJECT_STATUS_LOAD_TEMPLATE	= 0x01,
	AC_OBJECT_STATUS_REMOVED		= 0x02,
};

enum ac_object_sector_flag_bits {
	AC_OBJECT_SECTOR_NONE = 0,
	AC_OBJECT_SECTOR_IS_LOADED = 1u << 0,
	AC_OBJECT_SECTOR_HAS_CHANGES = 1u << 1,
};

struct ac_object_template_group {
	uint32_t index;
	char dff_name[AC_OBJECT_DFF_NAME_LENGTH];
	RwSphere bsphere;
	struct ac_mesh_clump * clump;
	/* TODO: Free memory */
	struct ac_animation * animation;
	float anim_speed;
	struct ac_render_crt clump_render_type;
	struct ac_object_template_group * next;
};

struct ac_object_template {
	uint32_t tid;
	char category[AC_OBJECT_CATEGORY_LENGTH];
	char collision_dff_name[AC_OBJECT_DFF_NAME_LENGTH];
	char picking_dff_name[AC_OBJECT_DFF_NAME_LENGTH];
	//struct ac_mesh_geometry * collision_geometry;
	//struct ac_mesh_geometry * picking_geometry;
	//OcTreeRenderData			m_stOcTreeData;
	RwRGBA pre_light;
	uint32_t object_type;
	struct ac_lod lod;
	struct ac_object_template_group * group_list;
	uint32_t refcount;
	enum ac_object_status status;
	enum ap_map_material_type ridable_material_type;
	uint32_t dnf;
};

struct ac_object_group {
	uint32_t index;
	boolean added_to_world;
	//RpHAnimHierarchy			*m_pstInHierarchy;
	boolean stop_anim;
	uint32_t prev_tick;
	struct ac_object_template_group_data * temp;
};

struct ac_object {
	struct ap_object * obj;
	struct ac_object_group * groups;
	//OcTreeRenderData	m_stOcTreeData;
	//RpAtomic			*m_pstCollisionAtomic;
	//RpAtomic			*m_pstPickAtomic;
	boolean is_group_child;
	//GroupChildInfo		*m_pInfoGroup;
	uint32_t object_type;
	struct ac_object_template * temp;
	uint32_t status;
	struct ac_object_sector * sector;
	struct ac_object * next;
};

struct ac_object_sector {
	uint32_t index_x;
	uint32_t index_z;
	vec2 extent_start;
	vec2 extent_end;
	uint32_t flags;
	struct ap_object ** objects;
	struct ap_octree_root * octree_roots;
};

struct ac_object_render_data {
	uint64_t state;
	bgfx_program_handle_t program;
	boolean no_texture;
};

struct ac_object_module * ac_object_create_module();

struct ac_object_template * ac_object_get_template(
	struct ac_object_module * mod, 
	uint32_t tid);

struct ac_object_template_group * ac_object_get_group(
	struct ac_object_module * mod, 
	struct ac_object_template * t,
	uint32_t index,
	boolean add);

struct ac_object * ac_object_get_object(
	struct ac_object_module * mod, 
	struct ap_object * obj);

struct ap_object ** ac_object_get_objects(
	struct ac_object_module * mod, 
	uint32_t x, 
	uint32_t z);

struct ac_object_sector * ac_object_get_sector(
	struct ac_object_module * mod, 
	const struct au_pos * pos);

/*
 * Inserts visible sectors into provided vector.
 */
void ac_object_query_visible_sectors(
	struct ac_object_module * mod, 
	struct ac_object_sector *** sectors);

void ac_object_query_visible_objects(
	struct ac_object_module * mod, 
	struct ap_object *** objects);

void ac_object_sync(struct ac_object_module * mod, const vec3 pos, boolean force);

void ac_object_update(struct ac_object_module * mod, float dt);

void ac_object_render(struct ac_object_module * mod);

struct ap_object * ac_object_pick(struct ac_object_module * mod, vec3 origin, vec3 dir);

void ac_object_render_object(
	struct ac_object_module * mod, 
	struct ap_object * obj, 
	struct ac_object_render_data * render_data);

/*
 * Finds the lowest vertex in object.
 */
boolean ac_object_get_min_height(
	struct ac_object_module * mod, 
	const struct ac_object_template * temp,
	float * height);

void ac_object_commit_changes(struct ac_object_module * mod);

END_DECLS

#endif /* _AC_OBJECT_H_ */
