#ifndef _AC_DEFINE_H_
#define _AC_DEFINE_H_

#include "core/macros.h"
#include "core/types.h"
#include "public/ap_define.h"
#include "vendor/RenderWare/rwcore.h"
#include "vendor/RenderWare/rpworld.h"

BEGIN_DECLS

///////////////////////////////////////////////////////////////////////////////
// Export data header(prefix) - (AC_EXPORT_HD)_(DIRECTORY)_(OBJECT)
// clump
#define AC_EXPORT_HD_CHAR_CHARACTER			"A"
#define AC_EXPORT_HD_CHAR_DEF_ARMOUR		"B"
#define AC_EXPORT_HD_CHAR_CHAR_PICKING_DATA	"C"
#define AC_EXPORT_HD_CHAR_BASE_ITEM			"D"
#define AC_EXPORT_HD_CHAR_SECOND_ITEM		"E"
#define AC_EXPORT_HD_CHAR_FIELD_ITEM		"F"
#define AC_EXPORT_HD_CHAR_ITEM_PICKING_DATA	"G"

#define AC_EXPORT_HD_OBJ_OBJECT				"H"
#define AC_EXPORT_HD_OBJ_COLLISION_DATA		"I"
#define AC_EXPORT_HD_OBJ_PICKING_DATA		"J"

#define AC_EXPORT_HD_EFF_CLUMP_BASE			"K"

// animation
#define AC_EXPORT_HD_CHAR_ANIM_ANIMATION	"A"
#define AC_EXPORT_HD_CHAR_ANIM_BLEND_ANIM	"B"
#define AC_EXPORT_HD_CHAR_ANIM_SUB_ANIM		"C"
#define AC_EXPORT_HD_CHAR_ANIM_SKILL_ANIM	"D"

#define AC_EXPORT_HD_OBJ_ANIM_ANIMATION		"E"

#define AC_EXPORT_HD_EFF_ANIM_ANIMATION		"F"
#define AC_EXPORT_HD_EFF_ANIM_SPLINE		"G"

// texture
#define AC_EXPORT_HD_TEX_ITEM_ITEM			"L"
#define AC_EXPORT_HD_TEX_ITEM_SMALL			"M"
#define AC_EXPORT_HD_TEX_ITEM_DUR_ZERO		"N"
#define AC_EXPORT_HD_TEX_ITEM_DUR_5_UNDER	"O"

#define AC_EXPORT_HD_TEX_SKILL_SKILL		"P"
#define AC_EXPORT_HD_TEX_SKILL_SMALL		"Q"
#define AC_EXPORT_HD_TEX_SKILL_UNABLE		"R"

#define AC_EXPORT_HD_TEX_EFFECT_EFFECT		"S"

#define AC_EXPORT_HD_TEX_UI_UI				"T"
#define AC_EXPORT_HD_TEX_UI_CONTROL			"U"

//customize
#define AC_EXPORT_HD_CHAR_CUSTOMIZE			"V"
///////////////////////////////////////////////////////////////////////////////

#define AC_EXPORT_EXT_CLUMP					"ECL"
#define AC_EXPORT_EXT_ANIMATION				"EAN"
#define AC_EXPORT_EXT_SPLINE				"ESP"
#define AC_EXPORT_EXT_TEXTURE_DDS			"DDS"

#define AC_EXPORT_ID_LENGTH					7

#define AC_EXPORT_TRACE_FILE				"ERROR_EXPORT_RESOURCE.TXT"

struct ac_lod;

struct ac_lod_data {
	uint32_t index;
	boolean has_bill_count;
	uint32_t bill_count[AP_LOD_MAX_NUM];
	uint32_t max_lod_level;
	uint32_t lod_distance[AP_LOD_MAX_NUM];
	uint32_t boundary;
	uint32_t max_distance_ratio;
	struct ac_lod * lod;
};

struct ac_lod_list {
	struct ac_lod_data data;
	struct ac_lod_list * next;
};

struct ac_lod {
	uint32_t count;
	uint32_t distance_type;
	struct ac_lod_list * list;
};

#define ARGB32_TO_UINT(a,r,g,b)	((r & 0xff) + ((g & 0xff) << 8) + ((b & 0xff) << 16) + ((a & 0xff) << 24))
#define ARGB_TO_ABGR(color)	( ((color << 8) >> 24) | ((color << 24) >> 8) | ((color & 0xff00ff00)) )

//----> kday 20040916
enum		// Effect Process Type 
{
	AGCMEFF2_PROCESS_TYPE_NONE				= 0	,
	AGCMEFF2_PROCESS_TYPE_EFFECT_START		,// Effect 시작
	AGCMEFF2_PROCESS_TYPE_EFFECT_END		,// Effect 종료  
	AGCMEFF2_PROCESS_TYPE_EFFECT_MISSLEEND	,// 발사체가 도달! - Effect 가 끝난것은 아님 
};
enum		// Effect Process Type 
{
	AGCMEFFECT_EFFECT_PROCESS_TYPE_NONE				= 0	,
	AGCMEFFECT_EFFECT_PROCESS_TYPE_EFFECT_START			,// Effect 시작
	AGCMEFFECT_EFFECT_PROCESS_TYPE_EFFECT_END			,// Effect 종료  
	AGCMEFFECT_EFFECT_PROCESS_TYPE_EFFECT_MISSLEEND		 // 발사체가 도달! - Effect 가 끝난것은 아님 
};

enum
{
	RWFLAG_RENDER_ADD		= 0x01,
	RWFLAG_DONOT_CULL		= 0x02
};
///////////////////////////////////////////////////////////////////////////////
// struct
///////////////////////////////////////////////////////////////////////////////
struct ac_effect_notice_process_data {
	uint32_t effect_process_type;
	uint32_t owner_cid;
	uint32_t target_cid;
	boolean	is_missile;
	uint32_t cust_data;
	uint32_t cust_id;
};

///////////////////////////////////////////////////////////////////////////////
// fptr
///////////////////////////////////////////////////////////////////////////////
// CALLBACK FUNCTION
typedef boolean (*ac_effect_notice_process_t)(
	struct ac_effect_notice_process_data process_data,
	void * user_data);
//<----


static inline uint32_t RwRGBAToUInt(RwRGBA rgba)
{
	return DEF_ARGB32(rgba.alpha, rgba.red, rgba.green, rgba.blue);
}

END_DECLS

#endif /* _AC_DEFINE_H_ */
