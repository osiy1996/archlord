#ifndef _AP_DEFINE_H_
#define _AP_DEFINE_H_

#include <math.h>

#include "core/macros.h"
#include "core/types.h"

BEGIN_DECLS

enum au_race_type {
	AU_RACE_TYPE_NONE = 0,
	AU_RACE_TYPE_HUMAN,
	AU_RACE_TYPE_ORC,
	AU_RACE_TYPE_MOONELF,
	AU_RACE_TYPE_DRAGONSCION,
	AU_RACE_TYPE_COUNT
};

enum au_char_gender_type {
	AU_CHARGENDER_TYPE_NONE = 0,
	AU_CHARGENDER_TYPE_MALE,
	AU_CHARGENDER_TYPE_FEMALE,
	AU_CHARGENDER_TYPE_COUNT
};

enum au_char_class_type {
	AU_CHARCLASS_TYPE_NONE = 0,
	AU_CHARCLASS_TYPE_KNIGHT,
	AU_CHARCLASS_TYPE_RANGER,
	AU_CHARCLASS_TYPE_SCION,
	AU_CHARCLASS_TYPE_MAGE,
	AU_CHARCLASS_TYPE_COUNT
};

enum au_blocking_type {
	AU_BLOCKING_TYPE_NONE = 0,
	AU_BLOCKING_TYPE_BOX,
	AU_BLOCKING_TYPE_SPHERE,
	AU_BLOCKING_TYPE_CYLINDER,
};

#define AU_POS_DISTANCE(pos1,pos2)		(sqrt(((pos1).x - (pos2).x) * ((pos1).x - (pos2).x) + ((pos1).y - (pos2).y) * ((pos1).y - (pos2).y) + ((pos1).z - (pos2).z) * ((pos1).z - (pos2).z)))
#define AU_POS_DISTANCE_XY(pos1,pos2)	(sqrt(((pos1).x - (pos2).x) * ((pos1).x - (pos2).x) + ((pos1).y - (pos2).y) * ((pos1).y - (pos2).y)))
#define AU_POS_DISTANCE_XZ(pos1,pos2)	(sqrt(((pos1).x - (pos2).x) * ((pos1).x - (pos2).x) + ((pos1).z - (pos2).z) * ((pos1).z - (pos2).z)))

struct au_pos {
	float x;
	float y;
	float z;
};

typedef struct au_pos au_v3d;

struct au_line {
	struct au_pos start;
	struct au_pos end;
};

struct au_pos_base_meter {
	int16_t x;
	int16_t y;
	int16_t z;
};

struct au_box {
	struct au_pos inf;
	struct au_pos sup;
};

struct au_sphere {
	struct au_pos center;
	float radius;
};

struct au_cylinder {
	struct au_pos center;
	float radius;
	float height;
};

struct au_matrix {
	struct au_pos right;
	uint32_t flags;
	struct au_pos up;
	uint32_t pad1;
	struct au_pos at;
	uint32_t pad2;
	struct au_pos pos;
	uint32_t pad3;
};

struct au_blocking {
	enum au_blocking_type type;
	union {
		struct au_box box;
		struct au_sphere sphere;
		struct au_cylinder cylinder;
	} data;
};


#define MIN3(a,b,c)						(MIN(MIN(a,b),c))
#define MAX3(a,b,c)						(MAX(MAX(a,b),c))
#define SWAP(a,b)						{(a)^=(b)^=(a)^=(b);}

#define AUPOS_EQUAL(pos1,pos2)			((pos1).x == (pos2).x && (pos1).y == (pos2).y && (pos1).z == (pos2).z)

#define	AUTEST_POS_IN_SPHERE(pos,sphere)		(AUPOS_DISTANCE((pos),(sphere).center) <= (sphere).radius)
#define AUTEST_POS_IN_CYLINDER(pos,cylinder)	((pos).y >= (cylinder).center.y && (pos).y <= (cylinder).center.y + (cylinder).height && AUPOS_DISTANCE_XY(pos,(cylinder).center) <= (cylinder).radius)
#define AUTEST_POS_IN_BOX(pos,box)				((pos).x >= (box).inf.x && (pos).y >= (box).inf.y && (pos).z >= (box).inf.z && (pos).x <= (box).sup.x && (pos).y <= (box).sup.y && (pos).z <= (box).sup.z)

#define AUMATRIX_EQUAL_POS(mat1,mat2)		AUPOS_EQUAL((mat1).pos,(mat2).pos)
#define AUMATRIX_EQUAL_RIGHT(mat1,mat2)		AUPOS_EQUAL((mat1).right,(mat2).right)
#define AUMATRIX_EQUAL_UP(mat1,mat2)		AUPOS_EQUAL((mat1).up,(mat2).up)
#define AUMATRIX_EQUAL_AT(mat1,mat2)		AUPOS_EQUAL((mat1).at,(mat2).at)

#define AUMATRIX_EQUAL(mat1,mat2)			(AUMATRIX_EQUAL_POS(mat1,mat2) && AUMATRIX_EQUAL_RIGHT(mat1,mat2) && AUMATRIX_EQUAL_UP(mat1,mat2) && AUMATRIX_EQUAL_AT(mat1,mat2))

#define AU_DEG2RAD							0.0174532925199f
#define AU_RAD2DEG							57.2957795132f

struct au_area {
	struct au_pos left_top;
	struct au_pos right_bottom;
};

struct au_dirt {
	int16_t	total_point;
	int16_t	duration;
	int16_t	intensity;
	int16_t	range;
	int16_t	target;
};

// packet type definition
///////////////////////////////////////////////////////////////////////////////
// 공통 모듈
enum packet_type {
	AP_CONFIG_PACKET_TYPE = 0x01,
	AP_CHARACTER_PACKET_TYPE = 0x02,
	AC_CHARACTER_PACKET_TYPE = 0x03,
	AP_ITEM_PACKET_TYPE = 0x04,
	AP_PARTY_ITEM_PACKET_TYPE = 0x29,
	AP_SKILL_PACKET_TYPE = 0x05,
	AP_SHRINE_PACKET_TYPE = 0x06,
	 /* PrivateTrade Packet Type */
	AP_PRIVATE_TRADE_PACKET_TYPE = 0x07,
	AP_PARTY_PACKET_TYPE = 0x08,         // party module packet type
	AP_PARTY_MANAGEMENT_PACKET_TYPE = 0x09,
	AP_RECRUIT_PACKET_TYPE = 0x0A,         // Recruit packet type
	AP_AUCTION_PACKET_TYPE = 0x0B,         // Auction module packet type
	AP_SYSTEMINFO_PACKET_TYPE = 0x0C,         // 시스템 정보를 Client로.....
	AP_LOGIN_PACKET_TYPE = 0x0D,         // Login정보 주고받기용
	AP_OBJECT_PACKET_TYPE = 0x0E,         // object client & server packet type
	AP_CHATTING_PACKET_TYPE = 0x0F,         // chatting packet type
	AP_AI_PACKET_TYPE = 0x10,         // ai information packet
	AP_TIMER_PACKET_TYPE = 0x11,         // Timer packet type
	AP_ADMIN_PACKET_TYPE = 0x12,         // Admin Packet Type
	AP_QUEST_PACKET_TYPE = 0x13,         // Quest Packet Type
	AP_UISTATUS_PACKET_TYPE = 0x14,         // UI Status Module Packet Type
	AP_GUILD_PACKET_TYPE = 0x15,         // Guild Packet Type
	AP_ITEM_CONVERT_PACKET_TYPE = 0x16,         // Item Convert Module Packet Type
	AP_PRODUCT_PACKET_TYPE = 0x17,         // Product Module Packet Type
	AP_WORLD_PACKET_TYPE = 0x18,         // World Module Packet Type
	AP_CASPER_PACKET_TYPE = 0x19,         // Casper Module Packet Type
	AP_PVP_PACKET_TYPE = 0x4A,         // PvP Module Packet Type
	AP_REFINERY_PACKET_TYPE = 0x4B,         // Refinery Module Packet Type
	AP_AREACHATTING_PACKET_TYPE = 0x4C,
	AP_RIDE_PACKET_TYPE = 0x4D,         // Ride Module Packet Type
	AP_BILL_INFO_PACKET_TYPE = 0x4E,         // billing information module
	AP_SUMMONS_PACKET_TYPE = 0x4F,         // Summons Module Packet Type
	AP_REMISSION_PACKET_TYPE = 0x2A,
	AP_BUDDY_PACKET_TYPE = 0x2B,         // Buddy Module Packet Type
	AP_CHANNEL_PACKET_TYPE = 0x2C,         // Channel Module Packet Type
	AP_MAILBOX_PACKET_TYPE = 0x2D,         // Mail Box Module Packet Type
	AP_CASH_MALL_PACKET_TYPE = 0x2E,
	AP_RETURNTOLOGIN_PACKET_TYPE = 0x2F,
	AP_OPTIMIZEDCHARMOVE_PACKET_TYPE = 0x40,         // AP_OptimizedPacket Module Character Move Packet Type
	AP_OPTIMIZEDCHARACTION_PACKET_TYPE = 0x41,         // AP_OptimizedPacket Module Character Action Packet Type
	AP_OPTIMIZEDVIEW_PACKET_TYPE = 0x42,         // AP_OptimizedPacket Module Character, Item View Packet Type
	AP_WANTEDCRIMINAL_PACKET_TYPE = 0x46,
	AP_NATUREEFFECT_PACKET_TYPE = 0x47,         // Nature Effect 뿌리기~
	AP_STARTUP_ENCRYPTION_PACKET_TYPE = 0x48,
	AP_SIEGEWAR_PACKET_TYPE = 0x49,         // 공성 모듈
	AP_SEARCH_PACKET_TYPE = 0x52,         // 검색 패킷
	AP_TAX_PACKET_TYPE = 0x53,
	AP_GUILDWAREHOUSE_PACKET_TYPE = 0x54,
	AP_ARCHLORD_PACKET_TYPE = 0x55,         // Archlord System Packet
	AP_GAMBLE_PACKET_TYPE = 0x56,         // gamble


	AP_EVENT_NATURE_PACKET_TYPE = 0x30,         // event module은 0x30부터 시작
	AP_EVENT_TELEPORT_PACKET_TYPE = 0x31,
	AP_EVENT_NPCTRADE_PACKET_TYPE = 0x32,         // NPC Trade Event
	AP_EVENT_ITEMREPAIR_PACKET_TYPE = 0x34,         // Item repair Event
	AP_EVENT_MASTERY_SPECIALIZE_PACKET_TYPE = 0x35,         // Mastery specialize event
	AP_EVENT_BANK_PACKET_TYPE = 0x36,         // bank event
	AP_EVENT_NPCDIALOG_PACKET_TYPE = 0x37,         // NPC Dialog Event
	AP_EVENT_ITEMCONVERT_PACKET_TYPE = 0x38,         // item convert event
	AP_EVENT_GUILD_PACKET_TYPE = 0x39,         // Guild Event
	AP_EVENT_PRODUCT_PACKET_TYPE = 0x3A,         // Product Event
	AP_EVENT_SKILLMASTER_PACKET_TYPE = 0x3B,         // Skill Master Event
	AP_EVENT_MANAGER_PACKET_TYPE = 0x3C,
	AP_EVENT_QUEST_PACKET_TYPE = 0x3E,         // Quest Event
	AP_EVENT_REFINERY_PACKET_TYPE = 0x3F,         // Refinery Event
	AP_EVENT_CHARCUSTOMIZE_PACKET_TYPE = 0x43,         // character customize
	AP_EVENT_SIEGEWAR_NPC_PACKET_TYPE = 0x50,         // siegewar npc event module
	AP_EVENT_GACHA_PACKET_TYPE = 0x57,         // Gacha Event

	AP_SYSTEM_MESSAGE_PACKET_TYPE = 0x44,         // System Message, 20050906, kelovon
	AP_SCRPIT_PACKET_TYPE = 0x45,         // Script command
	AP_BATTLEGROUND_PACKET_TYPE = 0x58,         // Gacha Event
	AP_EPICZONE_PACKET_TYPE = 0x59,         // EpicZone Event
	AP_TITLE_PACKET_TYPE = 0x60,         // Title Event
	AP_ARENA_PACKET_TYPE = 0x63,
	AP_BATTLE_PACKET_TYPE = 0x64,
	AP_GAME_EVENT_PACKET_TYPE = 0x65,
	/*
	ALEF_SYSTEM_PACKET_TYPE							= 0x00,

	AGPMCONFIG_PACKET_TYPE							= 0x01,
	AGPMCHARACTER_PACKET_TYPE						= 0x02,
	AGPMITEM_PACKET_TYPE							= 0x04,
	AGPMPARTYITEM_PACKET_TYPE						= 0x29,
	AGPMSKILL_PACKET_TYPE							= 0x05,
	AGPMSHRINE_PACKET_TYPE							= 0x06,
	AGPMPRIVATETRADE_PACKET_TYPE					= 0x07,					// PrivateTrade Packet Type
	AGPMPARTY_PACKET_TYPE							= 0x08,					// party module packet type
	AGPMRECRUIT_PACKET_TYPE							= 0x0A,					// Recruit packet type
	AGPMAUCTION_PACKET_TYPE							= 0x0B,					// Auction module packet type
	AGPMSYSTEMINFO_PACKET_TYPE						= 0x0C,					// 시스템 정보를 Client로.....
	AGPMLOGIN_PACKET_TYPE							= 0x0D,					// Login정보 주고받기용
	AGPMOBJECT_PACKET_TYPE							= 0x0E,					// object client & server packet type
	AGPMCHATTING_PACKET_TYPE						= 0x0F,					// chatting packet type
	AGPMAI_PACKET_TYPE								= 0x10,					// ai information packet
	AGPMTIMER_PACKET_TYPE							= 0x11,					// Timer packet type
	AGPMADMIN_PACKET_TYPE							= 0x12,					// Admin Packet Type
	AGPMQUEST_PACKET_TYPE							= 0x13,					// Quest Packet Type
	AGPMUISTATUS_PACKET_TYPE						= 0x14,					// UI Status Module Packet Type
	AGPMGUILD_PACKET_TYPE							= 0x15,					// Guild Packet Type
	AGPMITEMCONVERT_PACKET_TYPE						= 0x16,					// Item Convert Module Packet Type
	AGPMPRODUCT_PACKET_TYPE							= 0x17,					// Product Module Packet Type
	AGPMWORLD_PACKET_TYPE							= 0x18,					// World Module Packet Type
	AGPMCASPER_PACKET_TYPE							= 0x19,					// Casper Module Packet Type
	AGPMREFINERY_PACKET_TYPE						= 0x4B,					// Refinery Module Packet Type
	AGPMPVP_PACKET_TYPE								= 0x4A,					// PvP Module Packet Type
	AGPMAREACHATTING_PACKET_TYPE					= 0x4C,
	AGPMBILLINFO_PACKET_TYPE						= 0x4E,					// billing information module
	AGPMRIDE_PACKET_TYPE							= 0x4D,					// Ride Module Packet Type
	AGPMSUMMONS_PACKET_TYPE							= 0x4F,					// Summons Module Packet Type
	AGPMREMISSION_PACKET_TYPE						= 0x2A,
	AGPMBUDDY_PACKET_TYPE							= 0x2B,					// Buddy Module Packet Type
	AGPMCHANNEL_PACKET_TYPE							= 0x2C,					// Channel Module Packet Type
	AGPMMAILBOX_PACKET_TYPE							= 0x2D,					// Mail Box Module Packet Type
	AGPMCASHMALL_PACKET_TYPE						= 0x2E,
	AGPMRETURNTOLOGIN_PACKET_TYPE					= 0x2F,
	AGPMWANTEDCRIMINAL_PACKET_TYPE					= 0x46,
	AGPMNATUREEFFECT_PACKET_TYPE					= 0x47,					// Nature Effect 뿌리기~
	AGPMSTARTUPENCRYPTION_PACKET_TYPE				= 0x48,
	AGPMSIEGEWAR_PACKET_TYPE						= 0x49,					// 공성 모듈
	AGPMSEARCH_PACKET_TYPE							= 0x52,					// 검색 패킷
	AGPMTAX_PACKET_TYPE								= 0x53,
	AGPMGUILDWAREHOUSE_PACKET_TYPE					= 0x54,
	AGPMARCHLORD_PACKET_TYPE						= 0x55,					// Archlord System Packet
	AGPMGAMBLE_PACKET_TYPE							= 0x56,					// gamble

	AGPMOPTIMIZEDCHARMOVE_PACKET_TYPE				= 0x40,					// AgpmOptimizedPacket Module Character Move Packet Type
	AGPMOPTIMIZEDCHARACTION_PACKET_TYPE				= 0x41,					// AgpmOptimizedPacket Module Character Action Packet Type
	AGPMOPTIMIZEDVIEW_PACKET_TYPE					= 0x42,					// AgpmOptimizedPacket Module Character, Item View Packet Type

	AGPMEVENT_NATURE_PACKET_TYPE					= 0x30,					// event module은 0x30부터 시작
	AGPMEVENT_TELEPORT_PACKET_TYPE					= 0x31,
	AGPMEVENT_NPCTRADE_PACKET_TYPE					= 0x32,					// NPC Trade Event
	AGPMEVENT_ITEMREPAIR_PACKET_TYPE				= 0x34,					// Item repair Event
	AGPMEVENT_MASTERY_SPECIALIZE_PACKET_TYPE		= 0x35,					// Mastery specialize event
	AGPMEVENT_BANK_PACKET_TYPE						= 0x36,					// bank event
	AGPMEVENT_NPCDIALOG_PACKET_TYPE					= 0x37,					// NPC Dialog Event
	AGPMEVENT_ITEMCONVERT_PACKET_TYPE				= 0x38,					// item convert event
	AGPMEVENT_GUILD_PACKET_TYPE						= 0x39,					// Guild Event
	AGPMEVENT_PRODUCT_PACKET_TYPE					= 0x3A,					// Product Event
	AGPMEVENT_SKILLMASTER_PACKET_TYPE				= 0x3B,					// Skill Master Event
	AGPMEVENT_MANAGER								= 0x3C,
	AGPMEVENT_QUEST_PACKET_TYPE						= 0x3E,					// Quest Event
	AGPMEVENT_REFINERY_PACKET_TYPE					= 0x3F,					// Refinery Event
	AGPMEVENT_CHARCUSTOMIZE_PACKET_TYPE				= 0x43,					// character customize
	AGPMEVENT_SIEGEWAR_NPC_PACKET_TYPE				= 0x50,					// siegewar npc event module
	AGPMEVENT_GACHA_PACKET_TYPE						= 0x57,					// Gacha Event

	AGPMSYSTEMMESSAGE_PACKET_TYPE					= 0x44,					// System Message, 20050906, kelovon
	AGPMSCRPIT_PACKET_TYPE							= 0x45,					// Script command
	AGPM_BATTLEGROUND_PACKET_TYPE					= 0x58,					// Gacha Event
	AGPM_EPICZONE_PACKET_TYPE						= 0x59,					// EpicZone Event
	AGPM_TITLE_PACKET_TYPE							= 0x60,					// Title Event
	AGPM_ARENA_PACKET_TYPE							= 0x63,
	AGPM_BATTLE_PACKET_TYPE							= 0x64,
	AGPM_GAME_EVENT_PACKET_TYPE						= 0x65,

	// 클라이언트 모듈
	AGCMCHAR_PACKET_TYPE							= 0x03,
	AGCMCONNECTMANAGER_PACKET_TYPE					= 0x1B,

	// 서버 모듈
	AGSMCHARMANAGER_PACKET_TYPE						= 0x03,
	AGSMPARTY_PACKET_TYPE							= 0x09,
	AGSMGLOBALCHATTING_PACKET_TYPE					= 0x51,

	// 서버간 통신
	AGSMSERVER_PACKET_TYPE							= 0x1A,
	AGSMCHARACTER_PACKET_TYPE						= 0x1B,					// 서버간 통신은 0x1A부터...
	AGSMACCOUNT_PACKET_TYPE							= 0x1C,					// account manager가 사용
	AGSMRECRUIT_PACKET_TYPE							= 0x1D,					// Recruit서버와 게임서버간 통신.
	AGSMAUCTION_PACKET_TYPE							= 0x1E,					// Auction서버와 게임서버간 통신.
	AGSMITEM_PACKET_TYPE							= 0x1F,
	AGSMSKILL_PACKET_TYPE							= 0x20,
	AGSMDEATH_PACKET_TYPE							= 0x21,
	AGSMLOGIN_PACKET_TYPE							= 0x22,
	AGSMZONING_PACKET_TYPE							= 0x23,
	AGSMADMIN_PACKET_TYPE							= 0x24,
	AGSMRELAY_PACKET_TYPE							= 0x25,
	AGSMITEMLOG_PACKET_TYPE							= 0x26,
	AGSMLOG_PACKET_TYPE								= 0x27,
	AGSMGUILD_PACKET_TYPE							= 0x28,
	*/
};

// const variable definition
///////////////////////////////////////////////////////////////////////////////
#define AP_INVALID_AID 0
#define AP_INVALID_CID 0
#define AP_INVALID_IID 0
#define AP_INVALID_PARTYID 0
#define AP_INVALID_SERVERID 0
#define AP_INVALID_SKILLID 0
#define AP_INVALID_SHRINEID 0


// AuBplusTree
///////////////////////////////////////////////////////////////////////////////
#define AUBTREE_FLAG_KEY_NUMBER 0
#define AUBTREE_FLAG_KEY_STRING 1

#define AUBTREE_MAX_KEY_STRING 40

// LOD (Bob님 작업, 122902)
///////////////////////////////////////////////////////////////////////////////
#define AP_LOD_MAX_NUM 5

// TEMPORARY TEMPLATE
///////////////////////////////////////////////////////////////////////////////
#define AGPD_TEMPORARY_TEMPLATE_INDEX 44444

// AgpmItem (public item module)
///////////////////////////////////////////////////////////////////////////////
#define AGPMITEM_MAX_MONEY 999999999999
#define	AGPM_TRADE_MAX_MONEY 999999999

// AgpmShrine (public shrine module)
///////////////////////////////////////////////////////////////////////////////
#define AGPMSHRINE_MAX_SHRINE_NAME 40
#define AGPMSHRINE_MAX_GUARDIAN 3
#define AGPMSHRINE_MAX_SHRINE_DATA_COUNT 40
#define AGPMSHRINE_MAX_SHRINE_POSITION 4

#define AGPMSHRINE_TYPE_ALWAYS 0x001
#define AGPMSHRINE_TYPE_ACTIVE_BY_COND 0x002
#define AGPMSHRINE_TYPE_RANDOM_POS 0x004

#define AGPMSHRINE_TYPE_COND_NONE 0x010
#define AGPMSHRINE_TYPE_KILL_GUARD 0x020
#define AGPMSHRINE_TYPE_HAVE_RUNE 0x040
#define AGPMSHRINE_TYPE_WITH_PARTY 0x080

#define AGPMSHRINE_TYPE_ACTIVE 1
#define AGPMSHRINE_TYPE_INACTIVE 2
#define AGPMSHRINE_TYPE_VANISH 3

inline boolean au_is_in_bbox(struct au_box box, struct au_pos pos)
{
	if (box.inf.x <= pos.x && box.sup.x > pos.x &&
		box.inf.y <= pos.y && box.sup.y > pos.y &&
		box.inf.z <= pos.z && box.sup.z > pos.z) {
		return TRUE;
	}
	else {
		return FALSE;
	}
}

/*
 * Do not check the Y factor.
 * Only determine whether position is in bbox x-y plane.
 */
inline boolean au_is_in_bbox_plane(struct au_box box, struct au_pos pos)
{
	if (box.inf.x <= pos.x && box.sup.x > pos.x &&
		//box.inf.y <= pos.y && box.sup.y > pos.y &&
		box.inf.z <= pos.z && box.sup.z > pos.z) {
		return TRUE;
	}
	else {
		return FALSE;
	}
}

/**
 * Distance between two points projected on X-Z plane.
 * \param[in] p1 First point.
 * \param[in] p2 Second point.
 * 
 * \return Distance between points.
 */
inline float au_distance2d(
	const struct au_pos * p1, 
	const struct au_pos * p2)
{
	float dx = p2->x - p1->x;
	float dz = p2->z - p1->z;
	return sqrtf(dx * dx + dz * dz);
}

enum ap_account_level {
	AP_ACCOUNT_LEVEL_NONE = 0,
	AP_ACCOUNT_LEVEL_CLOSE_BETA = 5,
	AP_ACCOUNT_LEVEL_DEVELOPER = 70,
	AP_ACCOUNT_LEVEL_ADMIN = 88,
};

// -----------------------------------------------------------------------------
// /////////////////////////////////////////////////////////////////////////////
// macro
// /////////////////////////////////////////////////////////////////////////////
// *macro constants
#define DEF_PI		3.141592653589793f
#define DEF_2PI		6.283185307179587f
#define DEF_DTOR	0.017453292519943f			// DEF_PI/180
#define DEF_RTOD	57.29577951308232f			// 180/DEF_PI
// *macro func
#define DEF_D2R(deg)	( (deg) * DEF_DTOR )	// degree -> radian
#define DEF_R2D(rad)	( (rad) * DEF_RTOD )	// radian -> degree

#define DEF_ALIGN32(a)	( ((a)+31) & (~31) )	// b = DEF_ALIGN32(a) -> (b >= a) AND ((b % 32) == 0)
#define DEF_ALIGN16(a)	( ((a)+15) & (~15) )	// b = DEF_ALIGN16(a) -> (b >= a) AND ((b % 16) == 0)
#define DEF_ALIGN8(a)	( ((a)+7 ) & (~7 ) )	// b = DEF_ALIGN8(a)  -> (b >= a) AND ((b % 8 ) == 0)
#define DEF_ALIGN4(a)	( ((a)+3 ) & (~3 ) )	// b = DEF_ALIGN4(a)  -> (b >= a) AND ((b % 4 ) == 0)
#define DEF_ALIGN2(a)	( ((a)+1 ) & (~1 ) )	// b = DEF_ALIGN2(a)  -> (b >= a) AND ((b % 2 ) == 0)

#define DEF_ISODD(a)	( (a) & 0x1 )			// if( DEF_XXX(a) == TRUE	) --> err
#define DEF_ISMINUS4(a)	( (a) & 0x80000000 )	// if( DEF_XXX(a) == FALSE	) --> err
#define DEF_ISMINUS2(a)	( (a) & 0x8000     )	// if( DEF_XXX(a) )			  --> only use in this form
#define DEF_ISMINUS1(a)	( (a) & 0x80       )

#define DEF_CLAMP(val, min, max)	( ((val)<(min)) ? ((val)=(min)) : ( ((val)>(max)) ? ((val)=(max)) : (val) ) )

#define DEF_FLAG_CHK(val, flag)		( (val) & (flag) )
#define DEF_FLAG_ON(val, flag)		( (val) |= (flag) )
#define DEF_FLAG_OFF(val, flag)		( (val) &= ~(flag) )

#define DEF_RGB32(r,g,b)			( (uint8_t)(b) + ((uint8_t)(g)<<8) + ((uint8_t)(r)<<16) )
#define DEF_ARGB32(a,r,g,b)			( (uint8_t)(b) + ((uint8_t)(g)<<8) + ((uint8_t)(r)<<16) + ((uint8_t)(a)<<24) )

#define DEF_SET_ALPHA(color, alpha)	( ( (color) &= 0x00ffffff ) |= ( (uint8_t)(alpha) << 24 ) )
#define DEF_SET_RED(color, red)		( ( (color) &= 0xff00ffff ) |= ( (uint8_t)(red  ) << 16 ) )
#define DEF_SET_GREEN(color, green)	( ( (color) &= 0xffff00ff ) |= ( (uint8_t)(green) << 8  ) )
#define DEF_SET_BLUE(color, blue)	( ( (color) &= 0xffffff00 ) |= ( (uint8_t)(blue )       ) )

#define DEF_GET_ALPHA(color)		(UINT8)( ( (color) & 0xff000000 ) >> 24 )
#define DEF_GET_RED(color)			(UINT8)( ( (color) & 0x00ff0000 ) >> 16 )
#define DEF_GET_GREEN(color)		(UINT8)( ( (color) & 0x0000ff00 ) >> 8  )
#define DEF_GET_BLUE(color)			(UINT8)( ( (color) & 0x000000ff )		)

#define	DEF_SAFEDELETE(a)			if(a){ delete (a); (a)=NULL;}
#define DEF_SAFEDELETEARRAY(a)		if(a){ delete [] (a); (a)=NULL;}
#define DEF_SAFERELEASE(a)			if(a){ (a)->Release(); (a)=NULL;}
#define DEF_SAFEFCLOSE(fp)			if(fp){ fclose(fp); (fp)=NULL;}

#define Eff2Ut_ZEROBLOCK(t)			{ ZeroMemory( &t,sizeof(t) ); }


enum ap_service_area {
	AP_SERVICE_AREA_KOREA	= 0,
	AP_SERVICE_AREA_CHINA	= 1,
	AP_SERVICE_AREA_TAIWAN	= 2,
	AP_SERVICE_AREA_JAPAN	= 3,
	AP_SERVICE_AREA_GLOBAL	= 4,
} ;

extern const enum ap_service_area g_ServiceArea;

#define AP_GETSERVICEAREAFLAG(eArea) (0x0001 << (eArea))

#define SET_BIT(BITFLAGS, BIT) (BITFLAGS) |= (BIT)

#define CHECK_BIT(BITFLAGS, BIT) \
	(((BITFLAGS) & (BIT)) == (BIT))

#define CLEAR_BIT(BITFLAGS, BIT) (BITFLAGS) &= ~(BIT)

END_DECLS

#endif /* _AP_DEFINE_H_ */
