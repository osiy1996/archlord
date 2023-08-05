#ifndef _AC_EVENT_EFFECT_H_
#define _AC_EVENT_EFFECT_H_

#include "public/ap_module_instance.h"

#include "vendor/RenderWare/rwcore.h"

#define AC_EVENT_EFFECT_MODULE_NAME "AgcmEventEffect"

#define AC_EVENT_EFFECT_MAX_USE_EFFECT_SET_DATA_LINK 5
#define AC_EVENT_EFFECT_MAX_SOUND_NAME_SIZE 64
#define AC_EVENT_EFFECT_MAX_CUSTOM_DATA_SIZE 128

BEGIN_DECLS

extern size_t g_AcEventEffectObjectTemplateAttachmentOffset;

struct ac_event_effect_use_effect_set_data_rotation {
	RwV3d right;
	RwV3d up;
	RwV3d at;
};

struct ac_event_effect_use_effect_set_data {
	uint32_t index;
	int type;
	char sound_name[AC_EVENT_EFFECT_MAX_SOUND_NAME_SIZE];
	int id;
	uint32_t eid;
	RwV3d offset;
	float scale;
	int parent_node_id;
	uint32_t start_gap;
	RwRGBA rgb_scale;
	float particle_count_scale;
	boolean is_atomic_emitter;
	boolean is_clump_emitter;
	struct ac_event_effect_use_effect_set_data_rotation * rotation;
	char custom_data[AC_EVENT_EFFECT_MAX_CUSTOM_DATA_SIZE];
	uint32_t condition_flags;
	uint32_t custom_flags;
	int target_cid;
	int sub_effect[AC_EVENT_EFFECT_MAX_USE_EFFECT_SET_DATA_LINK];
};

struct ac_event_effect_use_effect_set_list {
	struct ac_event_effect_use_effect_set_data data;
	struct ac_event_effect_use_effect_set_list * next;
};

struct ac_event_effect_use_effect_set {
	struct ac_event_effect_use_effect_set_list * list;
	uint32_t condition_flags;
	uint32_t custom_flags;
	int origin_type;		
};

struct ac_event_effect_module * ac_event_effect_create_module();

static inline struct ac_event_effect_use_effect_set * ac_event_effect_get_object_template_attachment(
	struct ap_object_template * temp)
{
	return (struct ac_event_effect_use_effect_set *)ap_module_get_attached_data(temp, 
		g_AcEventEffectObjectTemplateAttachmentOffset);
}

END_DECLS

#endif /* _AC_EVENT_EFFECT_H_ */
