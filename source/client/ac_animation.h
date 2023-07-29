#ifndef _AC_ANIM_H_
#define _AC_ANIM_H_

#include "core/macros.h"
#include "core/types.h"
#include "client/ac_define.h"
#include "vendor/RenderWare/rwcore.h"
#include "vendor/RenderWare/rtanim.h"
#include "vendor/RenderWare/rpskin.h"

#define AC_ANIM_MAX_ANIM_POINT 10
//#define AC_ANIM_MAX_ANIM 10
#define AC_ANIM_NAME_LENGTH 64
#define AC_ANIM_DEFAULT_EXT "RWS"
#define AC_ANIM_CB_PARAMS_CUST_DATA_NUM 10

BEGIN_DECLS

enum ac_anim_flag_type {
	AC_ANIM_FLAG_TYPE_LOOP = 0,
	AC_ANIM_FLAG_TYPE_BLEND,
	//	AC_ANIM_FLAG_TYPE_END_THEN_CHANGE,
	AC_ANIM_FLAG_TYPE_LINK
};

enum ac_anim_flag_bits {
	AC_ANIM_FLAG_EMPTY = 0, // default
	AC_ANIM_FLAG_LOOP = 1 << AC_ANIM_FLAG_TYPE_LOOP,
	AC_ANIM_FLAG_BLEND = 1 << AC_ANIM_FLAG_TYPE_BLEND,
	//	AC_ANIM_FLAG_END_THEN_CHANGE = 1 << AC_ANIM_FLAG_TYPE_END_THEN_CHANGE,
	AC_ANIM_FLAG_LINK = 1 << AC_ANIM_FLAG_TYPE_LINK
};

struct ac_anim_flag {
	uint16_t flags;
	uint16_t preference;
};

struct ac_anim_rt_anim {
	RtAnimAnimation * m_pstAnimation;
	uint32_t refcount;
};

struct ac_anim_data {
	struct ac_anim_rt_anim * rt_anim;
	char rt_anim_name[AC_ANIM_NAME_LENGTH];
	void ** m_pavAttachedData;
	struct ac_anim_data * next;
};

struct ac_animation {
	struct ac_anim_data * head;
	struct ac_anim_data * tail;
};

typedef void *(*ac_anim_callback_t)(void * data);

struct ac_anim_callback_data {
	float time;
	float play_speed;
	boolean is_loop;
	boolean influence_next_anim;
	ac_anim_callback_t callback;
	void * callback_data[AC_ANIM_CB_PARAMS_CUST_DATA_NUM];
};

END_DECLS

#endif /* _AC_ANIM_H_ */
