#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>

#include "core/file_system.h"
#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"

#include "public/ap_admin.h"
#include "public/ap_character.h"
#include "public/ap_define.h"
#include "public/ap_item.h"
#include "public/ap_module.h"
#include "public/ap_packet.h"
#include "public/ap_random.h"
#include "public/ap_tick.h"

#include "utility/au_packet.h"
#include "utility/au_table.h"

#define MAX_GROW_UP_FACTOR_COUNT 20

#define INI_NAME_NAME "Name"
#define INI_NAME_BTYPE "BlockingType"
#define	INI_NAME_CHAR_TYPE "CharacterType"
#define INI_NAME_BOX_INF "BoxInf"
#define INI_NAME_BOX_SUP "BoxSup"
#define INI_NAME_SPHERE_CENTER "SphereCenter"
#define INI_NAME_SPHERE_RADIUS "SphereRadius"
#define INI_NAME_CYLINDER_CENTER "CylinderCenter"
#define INI_NAME_CYLINDER_HEIGHT "CylinderHeight"
#define INI_NAME_CYLINDER_RADIUS "CylinderRadius"
#define INI_NAME_BOX "Box"
#define INI_NAME_SPHERE "Sphere"
#define INI_NAME_CYLINDER "Cylinder"
#define INI_NAME_NONE "None"
#define INI_NAME_TID "TID"
#define INI_NAME_SCALE "Scale"
#define INI_NAME_POSITION "Position"
#define INI_NAME_AXIS "Axis"
#define INI_NAME_DEGREE "Degree"
#define	INI_NAME_DIRECTION "Direction"
#define INI_NAME_MINIMAP "Minimap"
#define INI_NAME_NAMEBOARD "NameBoard"
#define	INI_NAME_UNDEAD "Undead"
#define	INI_NAME_FACE_NUM "FaceNum"
#define	INI_NAME_HAIR_NUM "HairNum"
#define INI_NAME_SELF_DESTRUCTION_ATTACK_TYPE "SELF_DEST"
#define INI_NAME_SWCO_BOX "SIEGEWAR_COLL_BOX"
#define INI_NAME_SWCO_SPHERE "SIEGEWAR_COLL_SPHERE"
#define INI_NAME_SWCO_OFFSET "SIEGEWAR_COLL_OFFSET"

struct grow_up_factors {
	uint32_t tid;
	struct ap_factor factors[AP_CHARACTER_MAX_LEVEL + 2];
};

struct ap_character_module {
	struct ap_module_instance instance;
	struct ap_factors_module * ap_factors;
	struct ap_packet_module * ap_packet;
	struct ap_random_module * ap_random;
	struct ap_tick_module * ap_tick;
	struct au_packet packet;
	struct au_packet packet_move;
	struct au_packet packet_action;
	struct au_packet packet_client;
	struct ap_admin template_admin;
	uint32_t grow_up_factor_count;
	struct grow_up_factors grow_up_factors[MAX_GROW_UP_FACTOR_COUNT];
	uint64_t level_up_exp[AP_CHARACTER_MAX_LEVEL + 1];
};

static boolean readgrowthfloat(float * f)
{
	char * token = strtok(NULL, "\t");
	if (!token)
		return FALSE;
	*f = strtof(token, NULL);
	return TRUE;
}

static boolean skipgrowth()
{
	return (strtok(NULL, "\t") != NULL);
}

boolean apchar_template_read(
	struct ap_character_module * mod,
	void * data, 
	struct ap_module_stream * stream)
{
	struct ap_character_template * temp = data;
	while (ap_module_stream_read_next_value(stream)) {
		const char * value_name = 
			ap_module_stream_get_value_name(stream);
		const char * value = ap_module_stream_get_value(stream);
		if (strcmp(value_name, AP_FACTORS_INI_START) == 0) {
			int32_t hitrange;
			if (!ap_factors_stream_read(mod->ap_factors, &temp->factor, stream))
				return FALSE;
			/* Reset most of the factors.
			 * These are largely erroneous and the correct 
			 * values will be read from import data. */
			hitrange = temp->factor.attack.hit_range;
			memset(&temp->factor, 0, sizeof(temp->factor));
			temp->factor.attack.hit_range = hitrange;
		}
		else if (strcmp(value_name, INI_NAME_NAME) == 0) {
			strlcpy(temp->name, value, sizeof(temp->name));
		}
		else if (strcmp(value_name, INI_NAME_SWCO_BOX) == 0) {
			if (sscanf(value, "%f:%f", 
					&temp->siege_war_coll_box_width,
					&temp->siege_war_coll_box_height) != 2) {
				return FALSE;
			}
		}
		else if (strcmp(value_name, INI_NAME_SWCO_SPHERE) == 0) {
			temp->siege_war_coll_sphere_radius = 
				strtof(value, NULL);
		}
		else if (strcmp(value_name, INI_NAME_SWCO_OFFSET) == 0) {
			if (sscanf(value, "%f:%f", 
					&temp->siege_war_coll_obj_offset_x,
					&temp->siege_war_coll_obj_offset_z) != 2) {
				return FALSE;
			}
		}
	}
	return TRUE;
}

boolean apchar_static_read(
	struct ap_character_module * mod,
	struct ap_character * character, 
	struct ap_module_stream * stream)
{
	while (ap_module_stream_read_next_value(stream)) {
		const char * value_name = 
			ap_module_stream_get_value_name(stream);
		const char * value = ap_module_stream_get_value(stream);
		if (strcmp(value_name, INI_NAME_NAME) == 0) {
			strlcpy(character->name, value, 
				sizeof(character->name));
		}
		else if (strcmp(value_name, INI_NAME_TID) == 0) {
			uint32_t tid = strtoul(value, NULL, 10);
			struct ap_character_template * temp = 
				ap_character_get_template(mod, tid);
			if (!temp) {
				ERROR("Invalid character template id (name = %s, tid = %u).",
					character->name, tid);
				return FALSE;
			}
			ap_character_set_template(mod, character, temp);
		}
		else if (strcmp(value_name, INI_NAME_POSITION) == 0) {
			if (sscanf(value, "%f,%f,%f", 
					&character->pos.x,
					&character->pos.y,
					&character->pos.z) != 3) {
				ERROR("Invalid character position (%s).",
					character->name);
				return FALSE;
			}
		}
		else if (strcmp(value_name, INI_NAME_DEGREE) == 0) {
			if (sscanf(value, "%f,%f", 
					&character->rotation_x,
					&character->rotation_y) != 2) {
				ERROR("Invalid character degree (%s).",
					character->name);
				return FALSE;
			}
		}
		else if (strcmp(value_name, INI_NAME_MINIMAP) == 0) {
			character->npc_display_for_map = 
				strtol(value, NULL, 10);
		}
		else if (strcmp(value_name, INI_NAME_NAMEBOARD) == 0) {
			character->npc_display_for_nameboard = 
				strtol(value, NULL, 10);
		}
		else {
			assert(0);
		}
	}
	return TRUE;
}

boolean apchar_read_import(
	struct ap_character_module * mod, 
	const char * file_path, 
	boolean decrypt)
{
	struct au_table * table = au_table_open(file_path, decrypt);
	boolean r = TRUE;
	if (!table) {
		ERROR("Failed to open file (%s).", file_path);
		return FALSE;
	}
	r &= au_table_set_column(table,
		"TID", AP_CHARACTER_IMPORT_DCID_TID);
	r &= au_table_set_column(table,
		"Name", AP_CHARACTER_IMPORT_DCID_NAME);
	r &= au_table_set_column(table,
		"Type", AP_CHARACTER_IMPORT_DCID_TYPE);
	r &= au_table_set_column(table,
		"Race", AP_CHARACTER_IMPORT_DCID_RACE);
	r &= au_table_set_column(table,
		"Gender", AP_CHARACTER_IMPORT_DCID_GENDER);
	r &= au_table_set_column(table,
		"Class", AP_CHARACTER_IMPORT_DCID_CLASS);
	r &= au_table_set_column(table,
		"LVL", AP_CHARACTER_IMPORT_DCID_LVL);
	r &= au_table_set_column(table,
		"STR", AP_CHARACTER_IMPORT_DCID_STR);
	r &= au_table_set_column(table,
		"DEX", AP_CHARACTER_IMPORT_DCID_DEX);
	r &= au_table_set_column(table,
		"INT", AP_CHARACTER_IMPORT_DCID_INT);
	r &= au_table_set_column(table,
		"CON", AP_CHARACTER_IMPORT_DCID_CON);
	r &= au_table_set_column(table,
		"CHA", AP_CHARACTER_IMPORT_DCID_CHA);
	r &= au_table_set_column(table,
		"Wis", AP_CHARACTER_IMPORT_DCID_WIS);
	r &= au_table_set_column(table,
		"AC", AP_CHARACTER_IMPORT_DCID_AC);
	r &= au_table_set_column(table,
		"MAX_HP", AP_CHARACTER_IMPORT_DCID_MAX_HP);
	r &= au_table_set_column(table,
		"MAX_SP", AP_CHARACTER_IMPORT_DCID_MAX_SP);
	r &= au_table_set_column(table,
		"MAX_MP", AP_CHARACTER_IMPORT_DCID_MAX_MP);
	r &= au_table_set_column(table,
		"CUR_HP", AP_CHARACTER_IMPORT_DCID_CUR_HP);
	r &= au_table_set_column(table,
		"CUR_SP", AP_CHARACTER_IMPORT_DCID_CUR_SP);
	r &= au_table_set_column(table,
		"CUR_MP", AP_CHARACTER_IMPORT_DCID_CUR_MP);
	r &= au_table_set_column(table,
		"Movement Walk", AP_CHARACTER_IMPORT_DCID_MOVEMENT_WALK);
	r &= au_table_set_column(table,
		"Movement Run", AP_CHARACTER_IMPORT_DCID_MOVEMENT_RUN);
	r &= au_table_set_column(table,
		"AR", AP_CHARACTER_IMPORT_DCID_AR);
	r &= au_table_set_column(table,
		"DR", AP_CHARACTER_IMPORT_DCID_DR);
	r &= au_table_set_column(table,
		"MAR", AP_CHARACTER_IMPORT_DCID_MAR);
	r &= au_table_set_column(table,
		"MDR", AP_CHARACTER_IMPORT_DCID_MDR);
	r &= au_table_set_column(table,
		"Min_Damage", AP_CHARACTER_IMPORT_DCID_MIN_DAMAGE);
	r &= au_table_set_column(table,
		"Max_Damage", AP_CHARACTER_IMPORT_DCID_MAX_DAMAGE);
	r &= au_table_set_column(table,
		"ATK Speed", AP_CHARACTER_IMPORT_DCID_ATK_SPEED);
	r &= au_table_set_column(table,
		"Range", AP_CHARACTER_IMPORT_DCID_RANGE);
	r &= au_table_set_column(table,
		"Fire Res.", AP_CHARACTER_IMPORT_DCID_FIRE_RES);
	r &= au_table_set_column(table,
		"Water Res.", AP_CHARACTER_IMPORT_DCID_WATER_RES);
	r &= au_table_set_column(table,
		"Air Res.", AP_CHARACTER_IMPORT_DCID_AIR_RES);
	r &= au_table_set_column(table,
		"Earth Res.", AP_CHARACTER_IMPORT_DCID_EARTH_RES);
	r &= au_table_set_column(table,
		"Magic Res.", AP_CHARACTER_IMPORT_DCID_MAGIC_RES);
	r &= au_table_set_column(table,
		"Poison Res.", AP_CHARACTER_IMPORT_DCID_POISON_RES);
	r &= au_table_set_column(table,
		"Ice Res.", AP_CHARACTER_IMPORT_DCID_ICE_RES);
	r &= au_table_set_column(table,
		"Thunder Res.", AP_CHARACTER_IMPORT_DCID_THUNDER_RES);
	r &= au_table_set_column(table,
		"Magic History", AP_CHARACTER_IMPORT_DCID_MAGIC_HISTORY);
	r &= au_table_set_column(table,
		"Murder Point", AP_CHARACTER_IMPORT_DCID_MURDER_POINT);
	r &= au_table_set_column(table,
		"Exp", AP_CHARACTER_IMPORT_DCID_EXP);
	r &= au_table_set_column(table,
		"Fire Min_Damage", AP_CHARACTER_IMPORT_DCID_FIRE_MIN_DAMAGE);
	r &= au_table_set_column(table,
		"Fire Max_Damage", AP_CHARACTER_IMPORT_DCID_FIRE_MAX_DAMAGE);
	r &= au_table_set_column(table,
		"Water Min_Damage", AP_CHARACTER_IMPORT_DCID_WATER_MIN_DAMAGE);
	r &= au_table_set_column(table,
		"Water Max_Damage", AP_CHARACTER_IMPORT_DCID_WATER_MAX_DAMAGE);
	r &= au_table_set_column(table,
		"Earth Min_Damage", AP_CHARACTER_IMPORT_DCID_EARTH_MIN_DAMAGE);
	r &= au_table_set_column(table,
		"Earth Max_Damage", AP_CHARACTER_IMPORT_DCID_EARTH_MAX_DAMAGE);
	r &= au_table_set_column(table,
		"Air Min_Damage", AP_CHARACTER_IMPORT_DCID_AIR_MIN_DAMAGE);
	r &= au_table_set_column(table,
		"Air Max_Damage", AP_CHARACTER_IMPORT_DCID_AIR_MAX_DAMAGE);
	r &= au_table_set_column(table,
		"Magic Min_Damage", AP_CHARACTER_IMPORT_DCID_MAGIC_MIN_DAMAGE);
	r &= au_table_set_column(table,
		"Magic Max_Damage", AP_CHARACTER_IMPORT_DCID_MAGIC_MAX_DAMAGE);
	r &= au_table_set_column(table,
		"Poison Min_Damage", AP_CHARACTER_IMPORT_DCID_POISON_MIN_DAMAGE);
	r &= au_table_set_column(table,
		"Poison Max_Damage", AP_CHARACTER_IMPORT_DCID_POISON_MAX_DAMAGE);
	r &= au_table_set_column(table,
		"Ice Min_Damage", AP_CHARACTER_IMPORT_DCID_ICE_MIN_DAMAGE);
	r &= au_table_set_column(table,
		"Ice Max_Damage", AP_CHARACTER_IMPORT_DCID_ICE_MAX_DAMAGE);
	r &= au_table_set_column(table,
		"Thunder Min_Damage", AP_CHARACTER_IMPORT_DCID_THUNDER_MIN_DAMAGE);
	r &= au_table_set_column(table,
		"Thunder Max_Damage", AP_CHARACTER_IMPORT_DCID_THUNDER_MAX_DAMAGE);
	r &= au_table_set_column(table,
		"Physical_Resistance", AP_CHARACTER_IMPORT_DCID_PHYSICAL_RESISTANCE);
	r &= au_table_set_column(table,
		"Skill_Block", AP_CHARACTER_IMPORT_DCID_SKILL_BLOCK);
	r &= au_table_set_column(table,
		"Sight", AP_CHARACTER_IMPORT_DCID_SIGHT);
	r &= au_table_set_column(table,
		"Badge1_Rank Point", AP_CHARACTER_IMPORT_DCID_BADGE1_RANK_POINT);
	r &= au_table_set_column(table,
		"Badge2_Rank Point", AP_CHARACTER_IMPORT_DCID_BADGE2_RANK_POINT);
	r &= au_table_set_column(table,
		"Badge3_Rank Point", AP_CHARACTER_IMPORT_DCID_BADGE3_RANK_POINT);
	r &= au_table_set_column(table,
		"Rank", AP_CHARACTER_IMPORT_DCID_RANK);
	r &= au_table_set_column(table,
		"Item Point", AP_CHARACTER_IMPORT_DCID_ITEM_POINT);
	r &= au_table_set_column(table,
		"Guild", AP_CHARACTER_IMPORT_DCID_GUILD);
	r &= au_table_set_column(table,
		"Guild Rank", AP_CHARACTER_IMPORT_DCID_GUILD_RANK);
	r &= au_table_set_column(table,
		"Guild Score", AP_CHARACTER_IMPORT_DCID_GUILD_SCORE);
	r &= au_table_set_column(table,
		"Lost Exp", AP_CHARACTER_IMPORT_DCID_LOST_EXP);
	r &= au_table_set_column(table,
		"Weapon", AP_CHARACTER_IMPORT_DCID_WEAPON);
	r &= au_table_set_column(table,
		"Item1", AP_CHARACTER_IMPORT_DCID_ITEM1);
	r &= au_table_set_column(table,
		"Item2", AP_CHARACTER_IMPORT_DCID_ITEM2);
	r &= au_table_set_column(table,
		"Item3", AP_CHARACTER_IMPORT_DCID_ITEM3);
	r &= au_table_set_column(table,
		"Item4", AP_CHARACTER_IMPORT_DCID_ITEM4);
	r &= au_table_set_column(table,
		"Armor_Head", AP_CHARACTER_IMPORT_DCID_ARMOR_HEAD);
	r &= au_table_set_column(table,
		"Armor_Body", AP_CHARACTER_IMPORT_DCID_ARMOR_BODY);
	r &= au_table_set_column(table,
		"Armor_Arm", AP_CHARACTER_IMPORT_DCID_ARMOR_ARM);
	r &= au_table_set_column(table,
		"Armor_Hand", AP_CHARACTER_IMPORT_DCID_ARMOR_HAND);
	r &= au_table_set_column(table,
		"Armor_Leg", AP_CHARACTER_IMPORT_DCID_ARMOR_LEG);
	r &= au_table_set_column(table,
		"Armor_Foot", AP_CHARACTER_IMPORT_DCID_ARMOR_FOOT);
	r &= au_table_set_column(table,
		"Armor_Mantle", AP_CHARACTER_IMPORT_DCID_ARMOR_MANTLE);
	r &= au_table_set_column(table,
		"Money", AP_CHARACTER_IMPORT_DCID_MONEY);
	r &= au_table_set_column(table,
		"Geld_Min", AP_CHARACTER_IMPORT_DCID_GELD_MIN);
	r &= au_table_set_column(table,
		"Geld_Max", AP_CHARACTER_IMPORT_DCID_GELD_MAX);
	r &= au_table_set_column(table,
		"Drop_TID", AP_CHARACTER_IMPORT_DCID_DROP_TID);
	r &= au_table_set_column(table,
		"slow_percent", AP_CHARACTER_IMPORT_DCID_SLOW_PERCENT);
	r &= au_table_set_column(table,
		"slow_time", AP_CHARACTER_IMPORT_DCID_SLOW_TIME);
	r &= au_table_set_column(table,
		"fast_percent", AP_CHARACTER_IMPORT_DCID_FAST_PERCENT);
	r &= au_table_set_column(table,
		"fast_time", AP_CHARACTER_IMPORT_DCID_FAST_TIME);
	r &= au_table_set_column(table,
		"Product_Skin_ItemName", AP_CHARACTER_IMPORT_DCID_PRODUCT_SKIN_ITEMNAME);
	r &= au_table_set_column(table,
		"Product_Meat", AP_CHARACTER_IMPORT_DCID_PRODUCT_MEAT);
	r &= au_table_set_column(table,
		"Product_Meat_ItemName", AP_CHARACTER_IMPORT_DCID_PRODUCT_MEAT_ITEMNAME);
	r &= au_table_set_column(table,
		"Product_Garbage", AP_CHARACTER_IMPORT_DCID_PRODUCT_GARBAGE);
	r &= au_table_set_column(table,
		"Product_Garbage_ItmeName", AP_CHARACTER_IMPORT_DCID_PRODUCT_GARBAGE_ITMENAME);
	r &= au_table_set_column(table,
		"attribute_type", AP_CHARACTER_IMPORT_DCID_ATTRIBUTE_TYPE);
	r &= au_table_set_column(table,
		"name_color", AP_CHARACTER_IMPORT_DCID_NAME_COLOR);
	r &= au_table_set_column(table,
		"DropRank", AP_CHARACTER_IMPORT_DCID_DROPRANK);
	r &= au_table_set_column(table,
		"DropMeditation", AP_CHARACTER_IMPORT_DCID_DROPMEDITATION);
	r &= au_table_set_column(table,
		"Spirit Type", AP_CHARACTER_IMPORT_DCID_SPIRIT_TYPE);
	r &= au_table_set_column(table,
		"LandAttachType", AP_CHARACTER_IMPORT_DCID_LANDATTACHTYPE);
	r &= au_table_set_column(table,
		"Tamable", AP_CHARACTER_IMPORT_DCID_TAMABLE);
	r &= au_table_set_column(table,
		"Run Correct", AP_CHARACTER_IMPORT_DCID_RUN_CORRECT);
	r &= au_table_set_column(table,
		"Attack Correct", AP_CHARACTER_IMPORT_DCID_ATTACK_CORRECT);
	r &= au_table_set_column(table,
		"SiegeWarCharType", AP_CHARACTER_IMPORT_DCID_SIEGEWARCHARTYPE);
	r &= au_table_set_column(table,
		"RangeType", AP_CHARACTER_IMPORT_DCID_RANGETYPE);
	r &= au_table_set_column(table,
		"StaminaPoint", AP_CHARACTER_IMPORT_DCID_STAMINAPOINT);
	r &= au_table_set_column(table,
		"PetType", AP_CHARACTER_IMPORT_DCID_PETTYPE);
	r &= au_table_set_column(table,
		"StartStamina", AP_CHARACTER_IMPORT_DCID_STARTSTAMINA);
	r &= au_table_set_column(table,
		"Heroic_Min_Damage", AP_CHARACTER_IMPORT_DCID_HEROIC_MIN_DAMAGE);
	r &= au_table_set_column(table,
		"Heroic_Max_Damage", AP_CHARACTER_IMPORT_DCID_HEROIC_MAX_DAMAGE);
	r &= au_table_set_column(table,
		"Heroic_Defense_Point", AP_CHARACTER_IMPORT_DCID_HEROIC_DEFENSE_POINT);
	if (!r) {
		ERROR("Failed to setup columns.");
		au_table_destroy(table);
		return FALSE;
	}
	while (au_table_read_next_line(table)) {
		struct ap_character_template * temp = NULL;
		struct ap_character_cb_end_read_import cb = { 0 };
		while (au_table_read_next_column(table)) {
			enum data_col_id id = au_table_get_column(table);
			const char * value = au_table_get_value(table);
			if (id == AP_CHARACTER_IMPORT_DCID_TID) {
				uint32_t tid = strtoul(value, NULL, 10);
				temp = ap_character_get_template(mod, tid);
				if (!temp) {
					ERROR("Invalid character template id (%u).",
						tid);
					assert(0);
					return FALSE;
				}
				continue;
			}
			if (!temp) {
				assert(0);
				continue;
			}
			switch (id) {
			case AP_CHARACTER_IMPORT_DCID_NAME:
				break;
			case AP_CHARACTER_IMPORT_DCID_TYPE:
				temp->char_type = au_table_get_i32(table);
				break;
			case AP_CHARACTER_IMPORT_DCID_RACE:
				ap_factors_set_value(&temp->factor, 
					AP_FACTORS_TYPE_CHAR_TYPE, 
					AP_FACTORS_CHAR_TYPE_TYPE_RACE, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_GENDER:
				ap_factors_set_value(&temp->factor, 
					AP_FACTORS_TYPE_CHAR_TYPE, 
					AP_FACTORS_CHAR_TYPE_TYPE_GENDER, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_CLASS:
				ap_factors_set_value(&temp->factor, 
					AP_FACTORS_TYPE_CHAR_TYPE, 
					AP_FACTORS_CHAR_TYPE_TYPE_CLASS, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_LVL:
				ap_factors_set_value(&temp->factor, 
					AP_FACTORS_TYPE_CHAR_STATUS, 
					AP_FACTORS_CHAR_STATUS_TYPE_LEVEL, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_STR:
				ap_factors_set_value(&temp->factor, 
					AP_FACTORS_TYPE_CHAR_STATUS, 
					AP_FACTORS_CHAR_STATUS_TYPE_STR, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_DEX:
				ap_factors_set_value(&temp->factor, 
					AP_FACTORS_TYPE_CHAR_STATUS, 
					AP_FACTORS_CHAR_STATUS_TYPE_DEX, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_INT:
				ap_factors_set_value(&temp->factor, 
					AP_FACTORS_TYPE_CHAR_STATUS, 
					AP_FACTORS_CHAR_STATUS_TYPE_INT, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_CON:
				ap_factors_set_value(&temp->factor, 
					AP_FACTORS_TYPE_CHAR_STATUS, 
					AP_FACTORS_CHAR_STATUS_TYPE_CON, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_CHA:
				ap_factors_set_value(&temp->factor, 
					AP_FACTORS_TYPE_CHAR_STATUS, 
					AP_FACTORS_CHAR_STATUS_TYPE_CHA, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_WIS:
				ap_factors_set_value(&temp->factor, 
					AP_FACTORS_TYPE_CHAR_STATUS, 
					AP_FACTORS_CHAR_STATUS_TYPE_WIS, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_AC:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DEFENSE,
					AP_FACTORS_DEFENSE_TYPE_DEFENSE_POINT,
					AP_FACTORS_ATTRIBUTE_TYPE_PHYSICAL, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_MAX_HP:
				ap_factors_set_value(&temp->factor, 
					AP_FACTORS_TYPE_CHAR_POINT_MAX, 
					AP_FACTORS_CHAR_POINT_MAX_TYPE_HP, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_MAX_SP:
				ap_factors_set_value(&temp->factor, 
					AP_FACTORS_TYPE_CHAR_POINT_MAX, 
					AP_FACTORS_CHAR_POINT_MAX_TYPE_SP, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_MAX_MP:
				ap_factors_set_value(&temp->factor, 
					AP_FACTORS_TYPE_CHAR_POINT_MAX, 
					AP_FACTORS_CHAR_POINT_MAX_TYPE_MP, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_CUR_HP:
				break;
			case AP_CHARACTER_IMPORT_DCID_CUR_SP:
				break;
			case AP_CHARACTER_IMPORT_DCID_CUR_MP:
				break;
			case AP_CHARACTER_IMPORT_DCID_MOVEMENT_WALK:
				ap_factors_set_value(&temp->factor, 
					AP_FACTORS_TYPE_CHAR_STATUS, 
					AP_FACTORS_CHAR_STATUS_TYPE_MOVEMENT, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_MOVEMENT_RUN:
				ap_factors_set_value(&temp->factor, 
					AP_FACTORS_TYPE_CHAR_STATUS, 
					AP_FACTORS_CHAR_STATUS_TYPE_MOVEMENT_FAST, 
					value);
				break;
			case AP_CHARACTER_IMPORT_DCID_AR:
				ap_factors_set_value(&temp->factor, 
					AP_FACTORS_TYPE_CHAR_POINT_MAX, 
					AP_FACTORS_CHAR_POINT_MAX_TYPE_AR, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_DR:
				ap_factors_set_value(&temp->factor, 
					AP_FACTORS_TYPE_CHAR_POINT_MAX, 
					AP_FACTORS_CHAR_POINT_MAX_TYPE_DR, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_MAR:
				ap_factors_set_value(&temp->factor, 
					AP_FACTORS_TYPE_CHAR_POINT_MAX, 
					AP_FACTORS_CHAR_POINT_MAX_TYPE_MAR, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_MDR:
				ap_factors_set_value(&temp->factor, 
					AP_FACTORS_TYPE_CHAR_POINT_MAX, 
					AP_FACTORS_CHAR_POINT_MAX_TYPE_MDR, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_MIN_DAMAGE:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MIN,
					AP_FACTORS_ATTRIBUTE_TYPE_PHYSICAL, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_MAX_DAMAGE:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MAX,
					AP_FACTORS_ATTRIBUTE_TYPE_PHYSICAL, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_ATK_SPEED:
				ap_factors_set_value(&temp->factor, 
					AP_FACTORS_TYPE_ATTACK, 
					AP_FACTORS_ATTACK_TYPE_SPEED, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_RANGE:
				ap_factors_set_value(&temp->factor, 
					AP_FACTORS_TYPE_ATTACK, 
					AP_FACTORS_ATTACK_TYPE_ATTACKRANGE, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_FIRE_RES:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DEFENSE,
					AP_FACTORS_DEFENSE_TYPE_DEFENSE_POINT,
					AP_FACTORS_ATTRIBUTE_TYPE_FIRE, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_WATER_RES:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DEFENSE,
					AP_FACTORS_DEFENSE_TYPE_DEFENSE_POINT,
					AP_FACTORS_ATTRIBUTE_TYPE_WATER, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_AIR_RES:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DEFENSE,
					AP_FACTORS_DEFENSE_TYPE_DEFENSE_POINT,
					AP_FACTORS_ATTRIBUTE_TYPE_AIR, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_EARTH_RES:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DEFENSE,
					AP_FACTORS_DEFENSE_TYPE_DEFENSE_POINT,
					AP_FACTORS_ATTRIBUTE_TYPE_EARTH, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_MAGIC_RES:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DEFENSE,
					AP_FACTORS_DEFENSE_TYPE_DEFENSE_POINT,
					AP_FACTORS_ATTRIBUTE_TYPE_MAGIC, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_POISON_RES:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DEFENSE,
					AP_FACTORS_DEFENSE_TYPE_DEFENSE_POINT,
					AP_FACTORS_ATTRIBUTE_TYPE_POISON, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_ICE_RES:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DEFENSE,
					AP_FACTORS_DEFENSE_TYPE_DEFENSE_POINT,
					AP_FACTORS_ATTRIBUTE_TYPE_ICE, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_THUNDER_RES:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DEFENSE,
					AP_FACTORS_DEFENSE_TYPE_DEFENSE_POINT,
					AP_FACTORS_ATTRIBUTE_TYPE_LIGHTENING, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_MAGIC_HISTORY:
				break;
			case AP_CHARACTER_IMPORT_DCID_MURDER_POINT:
				break;
			case AP_CHARACTER_IMPORT_DCID_EXP:
				ap_factors_set_value(&temp->factor, 
					AP_FACTORS_TYPE_CHAR_POINT_MAX, 
					AP_FACTORS_CHAR_POINT_MAX_TYPE_BASE_EXP, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_FIRE_MIN_DAMAGE:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MIN,
					AP_FACTORS_ATTRIBUTE_TYPE_FIRE, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_FIRE_MAX_DAMAGE:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MAX,
					AP_FACTORS_ATTRIBUTE_TYPE_FIRE, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_WATER_MIN_DAMAGE:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MIN,
					AP_FACTORS_ATTRIBUTE_TYPE_WATER, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_WATER_MAX_DAMAGE:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MAX,
					AP_FACTORS_ATTRIBUTE_TYPE_WATER, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_EARTH_MIN_DAMAGE:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MIN,
					AP_FACTORS_ATTRIBUTE_TYPE_EARTH, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_EARTH_MAX_DAMAGE:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MAX,
					AP_FACTORS_ATTRIBUTE_TYPE_EARTH, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_AIR_MIN_DAMAGE:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MIN,
					AP_FACTORS_ATTRIBUTE_TYPE_AIR, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_AIR_MAX_DAMAGE:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MAX,
					AP_FACTORS_ATTRIBUTE_TYPE_AIR, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_MAGIC_MIN_DAMAGE:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MIN,
					AP_FACTORS_ATTRIBUTE_TYPE_MAGIC, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_MAGIC_MAX_DAMAGE:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MAX,
					AP_FACTORS_ATTRIBUTE_TYPE_MAGIC, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_POISON_MIN_DAMAGE:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MIN,
					AP_FACTORS_ATTRIBUTE_TYPE_POISON, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_POISON_MAX_DAMAGE:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MAX,
					AP_FACTORS_ATTRIBUTE_TYPE_POISON, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_ICE_MIN_DAMAGE:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MIN,
					AP_FACTORS_ATTRIBUTE_TYPE_ICE, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_ICE_MAX_DAMAGE:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MAX,
					AP_FACTORS_ATTRIBUTE_TYPE_ICE, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_THUNDER_MIN_DAMAGE:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MIN,
					AP_FACTORS_ATTRIBUTE_TYPE_LIGHTENING, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_THUNDER_MAX_DAMAGE:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MAX,
					AP_FACTORS_ATTRIBUTE_TYPE_LIGHTENING, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_PHYSICAL_RESISTANCE:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DEFENSE,
					AP_FACTORS_DEFENSE_TYPE_DEFENSE_RATE,
					AP_FACTORS_ATTRIBUTE_TYPE_PHYSICAL, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_SKILL_BLOCK:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DEFENSE,
					AP_FACTORS_DEFENSE_TYPE_DEFENSE_POINT,
					AP_FACTORS_ATTRIBUTE_TYPE_SKILL_BLOCK, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_SIGHT:
				break;
			case AP_CHARACTER_IMPORT_DCID_BADGE1_RANK_POINT:
				break;
			case AP_CHARACTER_IMPORT_DCID_BADGE2_RANK_POINT:
				break;
			case AP_CHARACTER_IMPORT_DCID_BADGE3_RANK_POINT:
				break;
			case AP_CHARACTER_IMPORT_DCID_RANK:
				break;
			case AP_CHARACTER_IMPORT_DCID_ITEM_POINT:
				break;
			case AP_CHARACTER_IMPORT_DCID_GUILD:
				break;
			case AP_CHARACTER_IMPORT_DCID_GUILD_RANK:
				break;
			case AP_CHARACTER_IMPORT_DCID_GUILD_SCORE:
				break;
			case AP_CHARACTER_IMPORT_DCID_LOST_EXP:
				break;
			case AP_CHARACTER_IMPORT_DCID_WEAPON:
				break;
			case AP_CHARACTER_IMPORT_DCID_ITEM1:
				break;
			case AP_CHARACTER_IMPORT_DCID_ITEM2:
				break;
			case AP_CHARACTER_IMPORT_DCID_ITEM3:
				break;
			case AP_CHARACTER_IMPORT_DCID_ITEM4:
				break;
			case AP_CHARACTER_IMPORT_DCID_ARMOR_HEAD:
				break;
			case AP_CHARACTER_IMPORT_DCID_ARMOR_BODY:
				break;
			case AP_CHARACTER_IMPORT_DCID_ARMOR_ARM:
				break;
			case AP_CHARACTER_IMPORT_DCID_ARMOR_HAND:
				break;
			case AP_CHARACTER_IMPORT_DCID_ARMOR_LEG:
				break;
			case AP_CHARACTER_IMPORT_DCID_ARMOR_FOOT:
				break;
			case AP_CHARACTER_IMPORT_DCID_ARMOR_MANTLE:
				break;
			case AP_CHARACTER_IMPORT_DCID_MONEY:
				break;
			case AP_CHARACTER_IMPORT_DCID_GELD_MIN:
				temp->gold_min = au_table_get_i32(table);
				break;
			case AP_CHARACTER_IMPORT_DCID_GELD_MAX:
				temp->gold_max = au_table_get_i32(table);
				break;
			case AP_CHARACTER_IMPORT_DCID_DROP_TID:
				break;
			case AP_CHARACTER_IMPORT_DCID_SLOW_PERCENT:
				break;
			case AP_CHARACTER_IMPORT_DCID_SLOW_TIME:
				break;
			case AP_CHARACTER_IMPORT_DCID_FAST_PERCENT:
				break;
			case AP_CHARACTER_IMPORT_DCID_FAST_TIME:
				break;
			case AP_CHARACTER_IMPORT_DCID_PRODUCT_SKIN_ITEMNAME:
				break;
			case AP_CHARACTER_IMPORT_DCID_PRODUCT_MEAT:
				break;
			case AP_CHARACTER_IMPORT_DCID_PRODUCT_MEAT_ITEMNAME:
				break;
			case AP_CHARACTER_IMPORT_DCID_PRODUCT_GARBAGE:
				break;
			case AP_CHARACTER_IMPORT_DCID_PRODUCT_GARBAGE_ITMENAME:
				break;
			case AP_CHARACTER_IMPORT_DCID_ATTRIBUTE_TYPE:
				temp->attribute_type = au_table_get_i32(table);
				break;
			case AP_CHARACTER_IMPORT_DCID_NAME_COLOR:
				temp->name_color = strtoul(value, NULL, 10);
				break;
			case AP_CHARACTER_IMPORT_DCID_SPIRIT_TYPE:
				break;
			case AP_CHARACTER_IMPORT_DCID_LANDATTACHTYPE:
				temp->land_attach_type = au_table_get_i32(table);
				break;
			case AP_CHARACTER_IMPORT_DCID_TAMABLE:
				temp->tamable_type = au_table_get_i32(table);
				break;
			case AP_CHARACTER_IMPORT_DCID_RUN_CORRECT:
				break;
			case AP_CHARACTER_IMPORT_DCID_ATTACK_CORRECT:
				break;
			case AP_CHARACTER_IMPORT_DCID_SIEGEWARCHARTYPE:
				break;
			case AP_CHARACTER_IMPORT_DCID_RANGETYPE:
				temp->range_type = au_table_get_i32(table);
				break;
			case AP_CHARACTER_IMPORT_DCID_STAMINAPOINT:
				temp->stamina_point = au_table_get_i32(table);
				break;
			case AP_CHARACTER_IMPORT_DCID_PETTYPE:
				temp->pet_type = au_table_get_i32(table);
				break;
			case AP_CHARACTER_IMPORT_DCID_STARTSTAMINA:
				temp->start_stamina_point = 
					au_table_get_i32(table);
				break;
			case AP_CHARACTER_IMPORT_DCID_HEROIC_MIN_DAMAGE:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MIN,
					AP_FACTORS_ATTRIBUTE_TYPE_HEROIC, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_HEROIC_MAX_DAMAGE:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MAX,
					AP_FACTORS_ATTRIBUTE_TYPE_HEROIC, value);
				break;
			case AP_CHARACTER_IMPORT_DCID_HEROIC_DEFENSE_POINT:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DEFENSE,
					AP_FACTORS_DEFENSE_TYPE_DEFENSE_POINT,
					AP_FACTORS_ATTRIBUTE_TYPE_HEROIC, value);
				break;
			default: {
				struct ap_character_cb_read_import cb = { 0 };
				cb.temp = temp;
				cb.id = id;
				cb.value = value;
				if (ap_module_enum_callback(mod, AP_CHARACTER_CB_READ_IMPORT, &cb)) {
					ERROR("Invalid column id (%u).", id);
					au_table_destroy(table);
					return FALSE;
				}
				break;
			}
			}
		}
		assert(temp != NULL);
		cb.temp = temp;
		if (!ap_module_enum_callback(mod, AP_CHARACTER_CB_END_READ_IMPORT, &cb)) {
			ERROR("Read import callback failed (%u).", temp->tid);
			au_table_destroy(table);
			return FALSE;
		}
	}
	au_table_destroy(table);
	return TRUE;
}

boolean apchar_read_growth(
	struct ap_character_module * mod, 
	const char * file_path, 
	boolean decrypt)
{
	file f = open_file(file_path, FILE_ACCESS_READ);
	char line[1024];
	boolean readnext = TRUE;
	if (!f) {
		ERROR("Failed to open file (%s).", file_path);
		return FALSE;
	}
	while (!readnext || read_line(f, line, sizeof(line))) {
		uint32_t i = mod->grow_up_factor_count;
		struct grow_up_factors * g = &mod->grow_up_factors[i];
		uint32_t level = 1;
		if (i >= MAX_GROW_UP_FACTOR_COUNT) {
			ERROR("Too many grow up factors.");
			close_file(f);
			return FALSE;
		}
		g->tid = strtoul(line, NULL, 10);
		if (!read_line(f, line, sizeof(line))) {
			ERROR("Invalid file format (missing header).");
			close_file(f);
			return FALSE;
		}
		readnext = TRUE;
		while (read_line(f, line, sizeof(line))) {
			char * token = NULL;
			boolean r = TRUE;
			struct ap_factor * fac = &g->factors[level];
			token = strtok(line, "\t");
			if (strtoul(token, NULL, 10) != level) {
				readnext = FALSE;
				break;
			}
			if (level > AP_CHARACTER_MAX_LEVEL) {
				level++;
				continue;
			}
			r &= readgrowthfloat(&fac->char_status.str);
			r &= readgrowthfloat(&fac->char_status.agi);
			r &= readgrowthfloat(&fac->char_status.con);
			r &= readgrowthfloat(&fac->char_status.wis);
			r &= readgrowthfloat(&fac->char_status.intel);
			r &= skipgrowth(); /* Charisma */
			r &= skipgrowth(); /* HP */
			r &= skipgrowth(); /* MP */
			r &= skipgrowth(); /* SP */
			r &= readgrowthfloat(&fac->defense.point.physical);
			if (!r) {
				ERROR("Invalid file format.");
				close_file(f);
				return FALSE;
			}
			level++;
		}
		mod->grow_up_factor_count++;
	}
	close_file(f);
	INFO("Loaded %u grow up factors.", mod->grow_up_factor_count);
	return TRUE;
}

boolean apchar_read_level(
	struct ap_character_module * mod, 
	const char * file_path, 
	boolean decrypt)
{
	struct au_table * table = au_table_open(file_path, decrypt);
	boolean r = TRUE;
	if (!table) {
		ERROR("Failed to open file (%s).", file_path);
		return FALSE;
	}
	r &= au_table_set_column(table,
		"level", 0);
	r &= au_table_set_column(table,
		"exp", 1);
	if (!r) {
		ERROR("Failed to setup columns.");
		au_table_destroy(table);
		return FALSE;
	}
	while (au_table_read_next_line(table)) {
		uint32_t level = 0;
		while (au_table_read_next_column(table)) {
			enum data_col_id id = au_table_get_column(table);
			const char * value = au_table_get_value(table);
			switch (id) {
			case 0:
				level = strtoul(value, NULL, 10);
				break;
			case 1:
				if (!level-- || level > AP_CHARACTER_MAX_LEVEL) {
					WARN("Invalid level (%u).", level);
					break;
				}
				mod->level_up_exp[level] = 
					strtoull(value, NULL, 10);
				break;
			default:
				ERROR("Invalid column id (%u).", id);
				au_table_destroy(table);
				return FALSE;
			}
		}
	}
	au_table_destroy(table);
	return TRUE;
}

static void setgrowupfactor(
	struct ap_character * c,
	const struct grow_up_factors * factors,
	uint32_t level)
{
	const struct ap_factor * f = &factors->factors[level];
	c->factor.char_status.con = f->char_status.con;
	c->factor.char_status.str = f->char_status.str;
	c->factor.char_status.intel = f->char_status.intel;
	c->factor.char_status.agi = f->char_status.agi;
	c->factor.char_status.wis = f->char_status.wis;
	c->factor.defense.point.physical = f->defense.point.physical;
	c->update_flags |= AP_FACTORS_BIT_CHAR_STATUS |
		AP_FACTORS_BIT_MAX_HP | AP_FACTORS_BIT_MAX_MP | 
		AP_FACTORS_BIT_DEFENSE;
}

static void applygrowupfactor(
	struct ap_character * c,
	const struct grow_up_factors * factors,
	uint32_t level,
	int modifier)
{
	const struct ap_factor * f = &factors->factors[level];
	c->factor.char_status.con += f->char_status.con * modifier;
	c->factor.char_status.str += f->char_status.str * modifier;
	c->factor.char_status.intel += f->char_status.intel * modifier;
	c->factor.char_status.agi += f->char_status.agi * modifier;
	c->factor.char_status.wis += f->char_status.wis * modifier;
	c->factor.defense.point.physical += 
		f->defense.point.physical * modifier;
	c->update_flags |= AP_FACTORS_BIT_CHAR_STATUS |
		AP_FACTORS_BIT_MAX_HP | AP_FACTORS_BIT_MAX_MP | 
		AP_FACTORS_BIT_DEFENSE;
}

static boolean charctor(
	struct ap_character_module * mod,
	struct ap_character * c)
{
	c->base_type = AP_BASE_TYPE_CHARACTER;
	c->last_process_tick = ap_tick_get(mod->ap_tick);
	return TRUE;
}

static boolean onregister(
	struct ap_character_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_factors, AP_FACTORS_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_packet, AP_PACKET_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_random, AP_RANDOM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_tick, AP_TICK_MODULE_NAME);
	return TRUE;
}

static void onclose(struct ap_character_module * mod)
{
	size_t index = 0;
	struct ap_character_template ** obj = NULL;
	while (ap_admin_iterate_id(&mod->template_admin, 
			&index, (void **)&obj)) {
		struct ap_character_template * t = *obj;
		ap_module_destroy_module_data(&mod->instance.context, 
			AP_CHARACTER_MDI_TEMPLATE, t);
	}
}

static void onshutdown(struct ap_character_module * mod)
{
	ap_admin_destroy(&mod->template_admin);
}

struct ap_character_module * ap_character_create_module()
{
	struct ap_character_module * ctx = ap_module_instance_new("AgpmCharacter", 
		sizeof(*ctx), onregister, NULL, onclose, onshutdown);
	au_packet_init(&ctx->packet, sizeof(uint32_t),
		AU_PACKET_TYPE_UINT8, 1, /* Packet Type */ 
		AU_PACKET_TYPE_INT32, 1, /* Character ID */
		AU_PACKET_TYPE_INT32, 1, /* Character Template ID */
		AU_PACKET_TYPE_MEMORY_BLOCK, 1, /* Character Name */
		AU_PACKET_TYPE_INT8, 1, /* Character Status */
		AU_PACKET_TYPE_PACKET, 1, /* Move Packet */
		AU_PACKET_TYPE_PACKET, 1, /* Action Packet */
		AU_PACKET_TYPE_PACKET, 1, /* Factor Packet */
		AU_PACKET_TYPE_INT64, 1, /* Inventory money */
		AU_PACKET_TYPE_INT64, 1, /* Bank money */
		AU_PACKET_TYPE_INT64, 1, /* Cash */
		AU_PACKET_TYPE_INT8, 1, /* Character action status */
		AU_PACKET_TYPE_INT8, 1, /* Character criminal status */
		AU_PACKET_TYPE_INT32, 1, /* Attacker id */
		AU_PACKET_TYPE_INT8, 1, /*  */
		AU_PACKET_TYPE_UINT8, 1, /* Region index */
		AU_PACKET_TYPE_UINT8, 1, /* Social action index */
		AU_PACKET_TYPE_UINT64, 1, /* Special status */
		AU_PACKET_TYPE_INT8, 1, /* Is transform status */
		/* Skill initialization text */
		AU_PACKET_TYPE_CHAR, AP_CHARACTER_MAX_CHARACTER_SKILL_INIT,
		AU_PACKET_TYPE_INT8, 1, /* Face index */
		AU_PACKET_TYPE_INT8, 1, /* Hair index */
		AU_PACKET_TYPE_INT32, 1, /* Option Flag */
		AU_PACKET_TYPE_INT8, 1, /* Bank size */
		AU_PACKET_TYPE_UINT16, 1, /* Event status flag */
		AU_PACKET_TYPE_INT32, 1, /* Remained criminal status time */
		AU_PACKET_TYPE_INT32, 1, /* Remained murderer point time */
		/* Nick name */
		AU_PACKET_TYPE_CHAR, AP_CHARACTER_MAX_NICKNAME_SIZE, 
		AU_PACKET_TYPE_MEMORY_BLOCK, 1, /* Gameguard auth data */
		/* Last killed time in battlesquare */
		AU_PACKET_TYPE_UINT32, 1, 
		AU_PACKET_TYPE_END);
	au_packet_init(&ctx->packet_move, sizeof(uint8_t),
		AU_PACKET_TYPE_POS, 1, /* Character Current Position */
		AU_PACKET_TYPE_POS, 1, /* Character Destination Position */
		AU_PACKET_TYPE_INT32, 1, /* Character Follow Target ID */
		AU_PACKET_TYPE_INT32, 1, /* Follow Distance */
		AU_PACKET_TYPE_FLOAT, 1, /* Turn Degree X */
		AU_PACKET_TYPE_FLOAT, 1, /* Turn Degree Y */
		AU_PACKET_TYPE_INT8, 1, /* Move Flag */
		AU_PACKET_TYPE_INT8, 1, /* Move Direction */
		AU_PACKET_TYPE_END);
	au_packet_init(&ctx->packet_action, sizeof(uint16_t),
		AU_PACKET_TYPE_INT8, 1, /* Action Type */
		AU_PACKET_TYPE_INT32, 1, /* Target CID */
		AU_PACKET_TYPE_INT32, 1, /* Skill TID */
		AU_PACKET_TYPE_INT8, 1, /* Action Result Type */
		AU_PACKET_TYPE_PACKET, 1, /* Target Damage Factor Packet */
		AU_PACKET_TYPE_POS, 1, /* Character Attack Position */
		AU_PACKET_TYPE_UINT8, 1, /* Combo Information */
		AU_PACKET_TYPE_INT8, 1, /* Is Force Attack */
		AU_PACKET_TYPE_UINT32, 1, /* Additional Effect. */
		AU_PACKET_TYPE_UINT8, 1, /* Hit Index */
		AU_PACKET_TYPE_END);
	au_packet_init(&ctx->packet_client, sizeof(uint8_t),
		AU_PACKET_TYPE_INT8, 1, /* Operation */
		AU_PACKET_TYPE_CHAR, 12, /* Account Name */
		AU_PACKET_TYPE_INT32, 1, /* Character Template ID */
		/* Character Name */
		AU_PACKET_TYPE_CHAR, AP_CHARACTER_MAX_NAME_LENGTH + 1,
		AU_PACKET_TYPE_INT32, 1, /* Character id */
		AU_PACKET_TYPE_POS, 1, /* Character Position */
		AU_PACKET_TYPE_INT32, 1, /* m_lReceivedAuthKey */
		AU_PACKET_TYPE_END);
	ap_admin_init(&ctx->template_admin, 
		sizeof(struct ap_character_template *), 512);
	ap_module_set_module_data(&ctx->instance.context, 
		AP_CHARACTER_MDI_CHAR,
		sizeof(struct ap_character), charctor, NULL);
	ap_module_set_module_data(&ctx->instance.context, 
		AP_CHARACTER_MDI_TEMPLATE,
		sizeof(struct ap_character_template), NULL, NULL);
	ap_character_add_stream_callback(ctx, AP_CHARACTER_MDI_TEMPLATE, 
		"AgpmCharacter", ctx, apchar_template_read, NULL);
	ap_character_add_stream_callback(ctx, AP_CHARACTER_MDI_STATIC, 
		"AgpmCharacter", ctx, apchar_static_read, NULL);
	return ctx;
}

boolean ap_character_read_templates(
	struct ap_character_module * mod,
	const char * file_path, 
	boolean decrypt)
{
	struct ap_module_stream * stream;
	uint32_t count;
	uint32_t i;
	stream = ap_module_stream_create();
	if (!ap_module_stream_open(stream, file_path, 0, decrypt)) {
		ERROR("Failed to open stream (%s).", file_path);
		ap_module_stream_destroy(stream);
		return FALSE;
	}
	count = ap_module_stream_get_section_count(stream);
	for (i = 0; i < count; i++) {
		uint32_t tid = strtoul(
			ap_module_stream_read_section_name(stream, i), NULL, 10);
		struct ap_character_template ** obj;
		struct ap_character_template * temp = 
			ap_module_create_module_data(&mod->instance.context, 
				AP_CHARACTER_MDI_TEMPLATE);
		if (!temp) {
			ERROR("Failed to add character template (tid = %u).", 
				tid);
			ap_module_stream_destroy(stream);
			return FALSE;
		}
		temp->tid = tid;
		if (!ap_module_stream_enum_read(&mod->instance.context, stream, 
				AP_CHARACTER_MDI_TEMPLATE, temp)) {
			ERROR("Failed to read character template (tid = %u).",
				tid);
			ap_module_destroy_module_data(&mod->instance.context,
				AP_CHARACTER_MDI_TEMPLATE, temp);
			ap_module_stream_destroy(stream);
			return FALSE;
		}
		obj = ap_admin_add_object_by_id(&mod->template_admin, tid);
		if (!obj) {
			ERROR("Failed to add character template (tid = %u).",
				tid);
			ap_module_destroy_module_data(&mod->instance.context,
				AP_CHARACTER_MDI_TEMPLATE, temp);
			ap_module_stream_destroy(stream);
			return FALSE;
		}
		*obj = temp;
	}
	ap_module_stream_destroy(stream);
	INFO("Loaded %u character templates.", count);
	return TRUE;
}

boolean ap_character_read_static(
	struct ap_character_module * mod,
	const char * file_path, 
	boolean decrypt)
{
	struct ap_module_stream * stream;
	uint32_t count;
	uint32_t i;
	stream = ap_module_stream_create();
	if (!ap_module_stream_open(stream, file_path, 0, decrypt)) {
		ERROR("Failed to open stream (%s).", file_path);
		ap_module_stream_destroy(stream);
		return FALSE;
	}
	if (!ap_module_stream_set_mode(stream, 
			AU_INI_MGR_MODE_NAME_OVERWRITE)) {
		ERROR("Failed to set stream mode (%s).", file_path);
		ap_module_stream_destroy(stream);
		return FALSE;
	}
	count = ap_module_stream_get_section_count(stream);
	for (i = 0; i < count; i++) {
		uint32_t id = strtoul(
			ap_module_stream_read_section_name(stream, i), NULL, 10);
		struct ap_character * c = ap_character_new(mod);
		/* Static character ids are preset but character may 
		 * have obtained a generated id when it was constructed, 
		 * in which case we need to free that id before we 
		 * can use the preset id. */
		ap_character_free_id(mod, c);
		c->id = id;
		if (!ap_module_stream_enum_read(&mod->instance.context, stream, 
				AP_CHARACTER_MDI_STATIC, c)) {
			ERROR("Failed to read static character.");
			ap_character_free(mod, c);
			ap_module_stream_destroy(stream);
			return FALSE;
		}
		assert(c->temp != NULL);
		c->login_status = AP_CHARACTER_STATUS_IN_GAME_WORLD;
		c->char_type = AP_CHARACTER_TYPE_NPC;
		c->npc_display_for_map = TRUE;
		c->npc_display_for_nameboard = TRUE;
		if (!ap_module_enum_callback(&mod->instance.context, 
				AP_CHARACTER_CB_INIT_STATIC, c)) {
			ERROR("Failed to initialize static character (%s).",
				c->name);
			ap_character_free(mod, c);
			ap_module_stream_destroy(stream);
			return FALSE;
		}
	}
	ap_module_stream_destroy(stream);
	INFO("Loaded %u static characters.", count);
	return TRUE;
}

boolean ap_character_read_import_data(
	struct ap_character_module * mod,
	const char * file_path, 
	boolean decrypt)
{
	return apchar_read_import(mod, file_path, decrypt);
}

boolean ap_character_read_grow_up_table(
	struct ap_character_module * mod,
	const char * file_path,
	boolean decrypt)
{
	return apchar_read_growth(mod, file_path, decrypt);
}

boolean ap_character_read_level_up_exp(
	struct ap_character_module * mod,
	const char * file_path,
	boolean decrypt)
{
	return apchar_read_level(mod, file_path, decrypt);
}

struct ap_character * ap_character_new(struct ap_character_module * mod)
{
	struct ap_character * c = ap_module_create_module_data(
		&mod->instance.context, AP_CHARACTER_MDI_CHAR);
	c->factor_percent.char_status.movement = 100;
	c->factor_percent.char_status.movement_fast = 100;
	c->factor_percent.char_point_max.hp = 100;
	c->factor_percent.char_point_max.mp = 100;
	c->factor_percent.damage.min.physical = 100.0f;
	c->factor_percent.damage.max.physical = 100.0f;
	c->factor_percent.attack.attack_speed = 100;
	c->npc_display_for_nameboard = TRUE;
	return c;
}

void ap_character_free(struct ap_character_module * mod, struct ap_character * character)
{
	ap_module_destroy_module_data(&mod->instance.context, 
		AP_CHARACTER_MDI_CHAR, character);
}

void ap_character_free_id(struct ap_character_module * mod, struct ap_character * character)
{
	if (character->id) {
		struct ap_character_cb_free_id cb = 
			{ character, character->id };
		character->id = 0;
		ap_module_enum_callback(&mod->instance.context, 
			AP_CHARACTER_CB_FREE_ID, &cb);
	}
}

void ap_character_add_callback(
	struct ap_character_module * mod,
	enum ap_character_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	ap_module_add_callback(mod, id, callback_module, callback);
}

size_t ap_character_attach_data(
	struct ap_character_module * mod,
	enum ap_character_module_data_index data_index,
	size_t data_size,
	ap_module_t callback_module,
	ap_module_default_t constructor,
	ap_module_default_t destructor)
{
	return ap_module_attach_data(&mod->instance.context, data_index, data_size, 
		callback_module, constructor, destructor);
}

void ap_character_add_stream_callback(
	struct ap_character_module * mod,
	enum ap_character_module_data_index data_index,
	const char * module_name,
	ap_module_t callback_module,
	ap_module_stream_read_t read_cb,
	ap_module_stream_write_t write_cb)
{
	if (data_index < 0 || data_index >= AP_CHARACTER_MDI_COUNT) {
		ERROR("Invalid stream data index: %u.");
		return;
	}
	ap_module_stream_add_callback(&mod->instance.context, data_index,
		module_name, callback_module, read_cb, write_cb);
}

void ap_character_set_template(
	struct ap_character_module * mod,
	struct ap_character * character,
	struct ap_character_template * temp)
{
	const struct ap_factor * src = &temp->factor;
	struct ap_factor * dst = &character->factor;
	struct ap_factor * dstpoint = &character->factor_point;
	character->tid = temp->tid;
	character->temp = temp;
	ap_factors_copy(&character->factor, &temp->factor);
	dstpoint->char_point_max.attack_rating = 
		src->char_point_max.attack_rating;
	dstpoint->char_point_max.defense_rating = 
		src->char_point_max.defense_rating;
	dstpoint->damage = src->damage;
}

struct ap_character_template * ap_character_get_template(
	struct ap_character_module * mod,
	uint32_t tid)
{
	struct ap_character_template ** obj = 
		ap_admin_get_object_by_id(&mod->template_admin, tid);
	if (!obj)
		return NULL;
	return *obj;
}

void ap_character_move(
	struct ap_character_module * mod,
	struct ap_character * character,
	const struct au_pos * pos)
{
	struct ap_character_cb_move cb = { character, character->pos };
	character->pos = *pos;
	ap_module_enum_callback(&mod->instance.context, AP_CHARACTER_CB_MOVE, &cb);
}

void ap_character_set_movement(
	struct ap_character_module * mod,
	struct ap_character * character,
	const struct au_pos * dst,
	boolean move_fast)
{
	struct ap_character_cb_set_movement cb = { 
		character, *dst, move_fast };
	ap_module_enum_callback(&mod->instance.context, 
		AP_CHARACTER_CB_SET_MOVEMENT, &cb);
}

void ap_character_follow(
	struct ap_character_module * mod,
	struct ap_character * character,
	struct ap_character * target,
	boolean move_fast,
	uint16_t follow_distance)
{
	struct ap_character_cb_follow cb = { 
		character, target, move_fast, follow_distance };
	ap_module_enum_callback(&mod->instance.context, 
		AP_CHARACTER_CB_FOLLOW, &cb);
}

void ap_character_stop_movement(struct ap_character_module * mod, struct ap_character * character)
{
	struct ap_character_cb_stop_movement cb = { character, 
		character->is_moving };
	character->is_moving = FALSE;
	character->is_moving_fast = FALSE;
	character->is_moving_horizontal = FALSE;
	if (character->is_following) {
		struct ap_character_cb_interrupt_follow cb = { character };
		character->is_following = FALSE;
		ap_module_enum_callback(&mod->instance.context, 
			AP_CHARACTER_CB_INTERRUPT_FOLLOW, &cb);
	}
	ap_module_enum_callback(&mod->instance.context, 
		AP_CHARACTER_CB_STOP_MOVEMENT, &cb);
}

void ap_character_stop_action(struct ap_character_module * mod, struct ap_character * character)
{
	struct ap_character_cb_stop_action cb = { character};
	ap_module_enum_callback(&mod->instance.context, 
		AP_CHARACTER_CB_STOP_ACTION, &cb);
}

void ap_character_process(
	struct ap_character_module * mod,
	struct ap_character * character,
	uint64_t tick,
	float dt)
{
	struct ap_character_cb_process cb = { character, tick, dt };
	ap_module_enum_callback(&mod->instance.context, 
		AP_CHARACTER_CB_PROCESS, &cb);
}

void ap_character_process_monster(
	struct ap_character_module * mod,
	struct ap_character * character,
	uint64_t tick,
	float dt)
{
	struct ap_character_cb_process_monster cb = { 
		character, tick, dt };
	ap_module_enum_callback(&mod->instance.context, 
		AP_CHARACTER_CB_PROCESS_MONSTER, &cb);
}

void ap_character_set_grow_up_factor(
	struct ap_character_module * mod,
	struct ap_character * character,
	uint32_t prev_level,
	uint32_t level)
{
	const struct grow_up_factors * g = NULL;
	uint32_t i;
	for (i = 0; i < mod->grow_up_factor_count; i++) {
		if (mod->grow_up_factors[i].tid == character->tid) {
			g = &mod->grow_up_factors[i];
			break;
		}
	}
	if (!g)
		return;
	if (prev_level) {
		if (prev_level <= AP_CHARACTER_MAX_LEVEL)
			applygrowupfactor(character, g, prev_level, -1);
		if (level <= AP_CHARACTER_MAX_LEVEL) {
			applygrowupfactor(character, g, level, 1);
			character->factor_growth = &g->factors[level];
		}
		else {
			character->factor_growth = NULL;
		}
	}
	else {
		if (level <= AP_CHARACTER_MAX_LEVEL) {
			setgrowupfactor(character, g, level);
			character->factor_growth = &g->factors[level];
		}
		else {
			character->factor_growth = NULL;
		}
	}
}

uint64_t ap_character_get_level_up_exp(struct ap_character_module * mod, uint32_t current_level)
{
	if (!current_level-- || current_level > AP_CHARACTER_MAX_LEVEL)
		return 0;
	return mod->level_up_exp[current_level];
}

int ap_character_calc_defense_penalty(
	struct ap_character_module * mod,
	struct ap_character * character,
	struct ap_character * target)
{
	boolean pcc = ap_character_is_pc(character);
	boolean pct = ap_character_is_pc(target);
	if (pcc || pct) {
		int diff = (int)(ap_character_get_level(character) - 
			ap_character_get_level(target));
		if (diff > 0) {
			switch (diff) {
			case 1:
			case 2:
			case 3:
				return 2;
			case 4:
			case 5:
				return 4;
			case 6:
			case 7:
				return 6;
			case 8:
			case 9:
			case 10:
			case 11:
				return 8;
			case 12:
			case 13:
			case 14:
			case 15:
				return 12;
			case 16:
			case 17:
			case 18:
			case 19:
				return 16;
			default:
				return 20;
			}
		}
		else {
			return 0;
		}
	}
	else {
		return 0;
	}
}

uint32_t ap_character_calc_physical_attack(
	struct ap_character_module * mod,
	struct ap_character * character,
	struct ap_character * target,
	struct ap_character_action_info * action)
{
	int att;
	float normal_min;
	float normal_max;
	const struct ap_factor * cf = &character->factor;
	const struct ap_factor * tf = &character->factor;
	float defense = tf->defense.point.physical;
	float resist = MIN(tf->defense.rate.physical, 80.f) - 
		action->defense_penalty;
	if (resist < 0.0f)
		resist = 0.0f;
	if (ap_character_is_pc(character)) {
		switch (character->factor.char_type.cs) {
		case AU_CHARCLASS_TYPE_RANGER: {
			float x = cf->char_status.agi * 0.222222f + 
				cf->char_status.str * 0.111111f;
			normal_min = cf->damage.min.physical + x;
			normal_max = cf->damage.max.physical + x * 1.05f;
			break;
		}
		case AU_CHARCLASS_TYPE_MAGE:
			normal_min = cf->damage.min.physical + 
				cf->char_status.intel * 0.4f;
			normal_max = cf->damage.max.physical + 
				cf->char_status.intel * 0.42f;
			break;
		default:
			normal_min = cf->damage.min.physical + 
				cf->char_status.str * 0.3333f;
			normal_max = cf->damage.max.physical + 
				cf->char_status.str * 0.35f;
			break;
		}
	}
	else {
		normal_min = cf->damage.min.physical;
		normal_max = cf->damage.max.physical;
	}
	if (normal_min > normal_max)
		normal_min = normal_max;
	if (normal_min < 0.0f)
		normal_min = 0.0f;
	if (normal_max < 0.0f)
		normal_max = 0.0f;
	/** \todo: Check if damage is minimized/maximized. */
	att = (int)(ap_random_between(mod->ap_random, (uint32_t)normal_min, 
		(uint32_t)normal_max) - defense);
	att = -1 * (int)(att * (100.0f - resist) / 100.0f);
	action->target.dmg_normal = (att < 0) ? att : -1;
	return (-1 * action->target.dmg_normal);
}

uint32_t ap_character_calc_spirit_attack(
	struct ap_character_module * mod,
	struct ap_character * character,
	struct ap_character * target,
	struct ap_character_action_info * action,
	enum ap_factors_attribute_type type)
{
	float dmin;
	float dmax;
	float dmg;
	float res;
	int * dmgtarget = NULL;
	uint32_t hitrate = 0;
	switch (type) {
	case AP_FACTORS_ATTRIBUTE_TYPE_MAGIC:
		dmin = character->factor.damage.min.magic;
		dmax = character->factor.damage.max.magic;
		res = target->factor.defense.point.magic;
		dmgtarget = &action->target.dmg_attr_magic;
		break;
	case AP_FACTORS_ATTRIBUTE_TYPE_WATER:
		dmin = character->factor.damage.min.water;
		dmax = character->factor.damage.max.water;
		res = target->factor.defense.point.water;
		dmgtarget = &action->target.dmg_attr_water;
		break;
	case AP_FACTORS_ATTRIBUTE_TYPE_FIRE:
		dmin = character->factor.damage.min.fire;
		dmax = character->factor.damage.max.fire;
		res = target->factor.defense.point.fire;
		dmgtarget = &action->target.dmg_attr_fire;
		break;
	case AP_FACTORS_ATTRIBUTE_TYPE_EARTH:
		dmin = character->factor.damage.min.earth;
		dmax = character->factor.damage.max.earth;
		res = target->factor.defense.point.earth;
		dmgtarget = &action->target.dmg_attr_earth;
		break;
	case AP_FACTORS_ATTRIBUTE_TYPE_AIR:
		dmin = character->factor.damage.min.air;
		dmax = character->factor.damage.max.air;
		res = target->factor.defense.point.air;
		dmgtarget = &action->target.dmg_attr_air;
		break;
	case AP_FACTORS_ATTRIBUTE_TYPE_POISON:
		dmin = character->factor.damage.min.poison;
		dmax = character->factor.damage.max.poison;
		res = target->factor.defense.point.poison;
		dmgtarget = &action->target.dmg_attr_poison;
		break;
	case AP_FACTORS_ATTRIBUTE_TYPE_LIGHTENING:
		dmin = character->factor.damage.min.lightning;
		dmax = character->factor.damage.max.lightning;
		res = target->factor.defense.point.lightning;
		dmgtarget = &action->target.dmg_attr_lightning;
		break;
	case AP_FACTORS_ATTRIBUTE_TYPE_ICE:
		dmin = character->factor.damage.min.ice;
		dmax = character->factor.damage.max.ice;
		res = target->factor.defense.point.ice;
		dmgtarget = &action->target.dmg_attr_ice;
		break;
	default:
		assert(0);
		return 0;
	}
	if (dmax <= 0.0f)
		return 0;
	if (ap_character_is_pc(character)) {
		hitrate = 70;
	}
	else {
		hitrate = 80 + (ap_character_get_level(character) - 
			ap_character_get_level(target));
		if (hitrate < 60)
			hitrate = 60;
	}
	if (!(ap_random_get(mod->ap_random, 100) < hitrate))
		return 0;
	if (dmin > dmax)
		dmg = dmax;
	else
		dmg = ap_random_float(mod->ap_random, dmin, dmax);
	if (res > 80.0f && ap_character_is_pc(target))
		res = 80.0f - action->defense_penalty;
	else if (res > 100.0f)
		res = 100.0f - action->defense_penalty;
	else
		res -= action->defense_penalty;
	*dmgtarget = (int)(-2.0f * dmg * (100.0f - res) / 100.0f);
	return (uint32_t)(-1 * *dmgtarget);
}

uint32_t ap_character_calc_heroic_attack(
	struct ap_character_module * mod,
	struct ap_character * character,
	struct ap_character * target,
	struct ap_character_action_info * action)
{
	float resistance = 0.0f;	
	float dmg;
	float dmgmin;
	float dmgmax;
	int att;
	if (ap_character_is_pc(character)) {
		switch (character->factor.char_type.cs) {
		case AU_CHARCLASS_TYPE_RANGER:
			resistance = 
				target->factor.defense.rate.heroic_ranged;
			break;
		case AU_CHARCLASS_TYPE_MAGE:
			resistance = 
				target->factor.defense.rate.heroic_magic;
			break;
		default:
			resistance = 
				target->factor.defense.rate.heroic_melee;
			break;
		}
		resistance = MIN(resistance, 80.f) - 
			action->defense_penalty;
		if (resistance < 0.0f)
			resistance = 0.0f;
	}
	dmgmin = character->factor.damage.min.heroic;
	dmgmax = character->factor.damage.max.heroic;
	if (dmgmin > dmgmax) {
		dmg = dmgmax;
	}
	else {
		dmg = ap_random_float(mod->ap_random, character->factor.damage.min.heroic, 
			character->factor.damage.max.heroic);
	}
	att = -(int)((dmg - target->factor.defense.point.heroic) * 
		(100.f - resistance) / 100.f);
	action->target.dmg_heroic = (att < 0) ? att : -1;
	return (-1 * action->target.dmg_heroic);
}

void ap_character_update_factor(
	struct ap_character_module * mod,
	struct ap_character * character,
	uint64_t update_flags)
{
	struct ap_factor * f = &character->factor;
	const struct ap_factor * fpt = &character->factor_point;
	const struct ap_factor * fper = &character->factor_percent;
	struct ap_character_cb_update_factor cb = { 0 };
	update_flags |= character->update_flags;
	character->update_flags = 0;
	cb.character = character;
	cb.update_flags = update_flags;
	if (CHECK_BIT(update_flags, AP_FACTORS_BIT_MOVEMENT_SPEED)) {
		int32_t percent = fper->char_status.movement;
		int32_t percentfast = fper->char_status.movement_fast;
		int32_t ms;
		int32_t msfast;
		if (!(character->special_status & AP_CHARACTER_SPECIAL_STATUS_SLOW_INVINCIBLE)) {
			percent += character->negative_percent_movement;
			percentfast += character->negative_percent_movement_fast;
		}
		ms = (int32_t)(character->temp->factor.char_status.movement * 
			percent / 100.0f) + fpt->char_status.movement;
		msfast = (int32_t)(character->temp->factor.char_status.movement_fast * 
			percentfast / 100.0f) + fpt->char_status.movement_fast;
		if (ms < 0)
			ms = 0;
		else if (ms % 100 >= 50)
			ms += 100 - (ms % 100);
		if (msfast < 0)
			msfast = 0;
		else if (msfast % 100 >= 50)
			msfast += 100 - (msfast % 100);
		f->char_status.movement = ms;
		f->char_status.movement_fast = msfast;
	}
	if (CHECK_BIT(update_flags, AP_FACTORS_BIT_MAX_HP)) {
		if (character->char_type & AP_CHARACTER_TYPE_PC) {
			f->char_point_max.hp = (uint32_t)(f->char_status.con *
				8.0f * fper->char_point_max.hp / 100.0f) + 
					fpt->char_point_max.hp;
			if ((int)f->char_point_max.hp <= 50)
				f->char_point_max.hp = 50;
			f->char_point.hp = MIN(f->char_point.hp,
				f->char_point_max.hp);
		}
	}
	if (CHECK_BIT(update_flags, AP_FACTORS_BIT_MAX_MP)) {
		if (character->char_type & AP_CHARACTER_TYPE_PC) {
			float wis = f->char_status.wis;
			if (character->factor_growth &&
				wis < character->factor_growth->char_status.wis) {
				wis = character->factor_growth->char_status.wis;
			}
			f->char_point_max.mp = (uint32_t)(wis * 5.38f * 
				fper->char_point_max.mp / 100.0f) + 
					fpt->char_point_max.mp;
			if ((int)f->char_point_max.mp <= 50)
				f->char_point_max.mp = 50;
			f->char_point.mp = MIN(f->char_point.mp,
				f->char_point_max.mp);
		}
	}
	if (CHECK_BIT(update_flags, AP_FACTORS_BIT_ATTACK_RATING)) {
		f->char_point_max.attack_rating = (int32_t)(f->char_status.agi * 0.6f) 
			+ fpt->char_point_max.attack_rating;
	}
	if (CHECK_BIT(update_flags, AP_FACTORS_BIT_DEFENSE_RATING)) {
		f->char_point_max.defense_rating = (int32_t)(f->char_status.agi * 0.4f) 
			+ fpt->char_point_max.defense_rating;
	}
	if (CHECK_BIT(update_flags, AP_FACTORS_BIT_PHYSICAL_DMG)) {
		/*
		float addmin = 0.0f;
		float addmax = 0.0f;
		switch (f->char_type.cs) {
		default:
		case AU_CHARCLASS_TYPE_KNIGHT:
			addmin = f->char_status.str * 0.3333f;
			addmax = f->char_status.str * 0.35f;
			break;
		case AU_CHARCLASS_TYPE_RANGER: {
			addmin = f->char_status.str * 0.111111f +
				f->char_status.agi * 0.222222f;
			addmax = addmin * 1.05f;
			break;
		}
		case AU_CHARCLASS_TYPE_MAGE:
			addmin = f->char_status.intel * 0.4f;
			addmax = f->char_status.intel * 0.42f;
			break;
		}
		*/
		f->damage.min.physical = fpt->damage.min.physical;
		f->damage.max.physical = fpt->damage.max.physical;
		f->damage.min.physical *= fper->damage.min.physical / 100.0f;
		f->damage.max.physical *= fper->damage.max.physical / 100.0f;
		ap_character_adjust_combat_point(character);
	}
	if (CHECK_BIT(update_flags, AP_FACTORS_BIT_ATTACK_RANGE)) {
		f->attack.attack_range = 
			character->temp->factor.attack.attack_range;
	}
	if (CHECK_BIT(update_flags, AP_FACTORS_BIT_ATTACK_SPEED)) {
		f->attack.attack_speed = (int)(character->temp->factor.attack.attack_speed * 
				fper->attack.attack_speed / 100.0f);
		ap_character_fix_attack_speed(character);
	}
	ap_module_enum_callback(&mod->instance.context, 
		AP_CHARACTER_CB_UPDATE_FACTOR, &cb);
}

void ap_character_update(
	struct ap_character_module * mod,
	struct ap_character * character,
	uint64_t update_flags,
	boolean is_private)
{
	struct ap_character_cb_update cb = { character, 
		update_flags, is_private };
	ap_module_enum_callback(&mod->instance.context, 
		AP_CHARACTER_CB_UPDATE, &cb);
}

void ap_character_set_level(
	struct ap_character_module * mod,
	struct ap_character * character,
	uint32_t level)
{
	struct ap_character_cb_set_level cb = { character, 
		character->factor.char_status.level };
	character->factor.char_status.level = level;
	ap_module_enum_callback(&mod->instance.context, 
		AP_CHARACTER_CB_SET_LEVEL, &cb);
}

boolean ap_character_is_valid_attack_target(
	struct ap_character_module * mod,
	struct ap_character * character,
	struct ap_character * target,
	boolean force_attack)
{
	struct ap_character_cb_is_valid_attack_target cb = { 
		character, target, force_attack };
	return ap_module_enum_callback(&mod->instance.context, 
		AP_CHARACTER_CB_IS_VALID_ATTACK_TARGET, &cb);
}

boolean ap_character_attempt_attack(
	struct ap_character_module * mod,
	struct ap_character * character,
	struct ap_character * target,
	struct ap_character_action_info * action)
{
	struct ap_character_cb_attempt_attack cb = { 
		character, target, action };
	return ap_module_enum_callback(&mod->instance.context, 
		AP_CHARACTER_CB_ATTEMPT_ATTACK, &cb);
}

void ap_character_kill(
	struct ap_character_module * mod,
	struct ap_character * character,
	struct ap_character * killer,
	enum ap_character_death_cause cause)
{
	struct ap_character_cb_death cb = { 
		character, killer, cause };
	ap_module_enum_callback(&mod->instance.context, 
		AP_CHARACTER_CB_DEATH, &cb);
}

void ap_character_special_status_on(
	struct ap_character_module * mod,
	struct ap_character * character,
	uint64_t special_status,
	uint64_t duration_ms)
{
	uint32_t index = ap_character_get_special_status_duration_index(special_status);
	boolean temporary = FALSE;
	if (!(character->special_status & special_status)) {
		struct ap_character_cb_special_status_on cb = { 0 };
		character->special_status |= special_status;
		ap_character_update(mod, character, AP_CHARACTER_BIT_SPECIAL_STATUS, FALSE);
		cb.character = character;
		cb.special_status = special_status;
		ap_module_enum_callback(mod, AP_CHARACTER_CB_SPECIAL_STATUS_ON, &cb);
		if (duration_ms)
			temporary = TRUE;
	}
	else if (character->special_status_end_tick[index] && duration_ms) {
		temporary = TRUE;
	}
	if (temporary) {
		/* Special status is temporary, extend the duration if necessary. */
		uint64_t endtick = ap_tick_get(mod->ap_tick) + duration_ms;
		if (endtick > character->special_status_end_tick[index])
			character->special_status_end_tick[index] = endtick;
	}
	else {
		/* Special status is permanent. */
		character->special_status_end_tick[index] = 0;
	}
}

void ap_character_special_status_off(
	struct ap_character_module * mod,
	struct ap_character * character,
	uint64_t special_status)
{
	if (character->special_status & special_status) {
		struct ap_character_cb_special_status_off cb = { 0 };
		uint32_t index = ap_character_get_special_status_duration_index(special_status);
		character->special_status &= ~special_status;
		character->special_status_end_tick[index] = 0;
		ap_character_update(mod, character, AP_CHARACTER_BIT_SPECIAL_STATUS, FALSE);
		cb.character = character;
		cb.special_status = special_status;
		ap_module_enum_callback(mod, AP_CHARACTER_CB_SPECIAL_STATUS_OFF, &cb);
	}
}

void ap_character_gain_experience(
	struct ap_character_module * mod,
	struct ap_character * character,
	uint64_t amount)
{
	struct ap_character_cb_gain_experience cb = { 0 };
	uint64_t result = ap_factors_get_current_exp(&character->factor) + amount;
	ap_factors_set_current_exp(&character->factor, result);
	cb.character = character;
	cb.amount = amount;
	ap_module_enum_callback(mod, AP_CHARACTER_CB_GAIN_EXPERIENCE, &cb);
}

boolean ap_character_on_receive(
	struct ap_character_module * mod,
	const void * data,
	uint16_t length,
	void * user_data)
{
	struct ap_character_cb_receive cb = { 0 };
	const char * skillinit = NULL;
	const char * nickname = NULL;
	if (!au_packet_get_field(&mod->packet, TRUE, data, length,
			&cb.type,
			&cb.character_id,
			&cb.character_tid,
			NULL, NULL, /* Character Name */
			&cb.character_status,
			NULL, /* Move Packet */
			NULL, /* Action Packet */
			NULL, /* Factor Packet */
			&cb.inventory_gold,
			&cb.bank_gold,
			&cb.chantra_coins,
			&cb.action_status,
			&cb.criminal_status,
			&cb.attacker_id,
			&cb.is_new_character,
			&cb.region_index,
			&cb.social_action,
			&cb.special_status,
			&cb.is_transform_status,
			&skillinit,
			&cb.face_index,
			&cb.hair_index,
			&cb.option_flags,
			&cb.extra_bank_slots,
			&cb.event_status_flags,
			&cb.criminal_duration,
			&cb.rogue_duration,
			&nickname, 
			NULL, NULL, /* Gameguard auth data */
			&cb.last_bsq_death_time)) {
		return FALSE;
	}
	if (skillinit)
		memcpy(cb.skill_init, skillinit, sizeof(cb.skill_init));
	cb.user_data = user_data;
	return ap_module_enum_callback(&mod->instance.context, 
		AP_CHARACTER_CB_RECEIVE, &cb);
}

void ap_character_make_add_packet(
	struct ap_character_module * mod, 
	struct ap_character * character)
{
	uint8_t type = AP_CHARACTER_PACKET_TYPE_ADD;
	uint16_t length = 0;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	void * movepacket = ap_packet_get_temp_buffer(mod->ap_packet);
	void * factorpacket = ap_packet_get_temp_buffer(mod->ap_packet);
	uint16_t namelen = (uint16_t)strlen(character->name);
	uint8_t moveflag = 0;
	uint8_t transformflags = 0;
	if (character->move_direction != AP_CHARACTER_MD_NONE)
		moveflag |= AP_CHARACTER_MOVE_FLAG_DIRECTION;
	if (character->is_path_finding)
		moveflag |= AP_CHARACTER_MOVE_FLAG_PATHFINDING;
	if (!character->is_moving)
		moveflag |= AP_CHARACTER_MOVE_FLAG_STOP;
	if (character->is_syncing)
		moveflag |= AP_CHARACTER_MOVE_FLAG_SYNC;
	if (character->is_moving_fast)
		moveflag |= AP_CHARACTER_MOVE_FLAG_FAST;
	if (character->is_following)
		moveflag |= AP_CHARACTER_MOVE_FLAG_FOLLOW;
	if (character->is_moving_horizontal)
		moveflag |= AP_CHARACTER_MOVE_FLAG_HORIZONTAL;
	au_packet_make_packet(
		&mod->packet_move, 
		movepacket, FALSE, NULL, 0,
		&character->pos,
		character->is_moving ? &character->dst_pos : 
			&character->pos,
		NULL, /* Follow Target ID */
		NULL, /* Follow Distance */
		&character->rotation_x,
		&character->rotation_y,
		&moveflag,
		&character->move_direction);
	ap_factors_make_char_view_packet(mod->ap_factors, factorpacket, 
		&character->factor);
	if (character->is_transformed)
		transformflags |= AP_CHARACTER_FLAG_TRANSFORM;
	if (character->is_ridable)
		transformflags |= AP_CHARACTER_FLAG_RIDABLE;
	if (character->is_evolved)
		transformflags |= AP_CHARACTER_FLAG_EVOLUTION;
	au_packet_make_packet(&mod->packet, 
		buffer, TRUE, &length,
		AP_CHARACTER_PACKET_TYPE, 
		&type,
		&character->id,
		&character->tid,
		character->name, &namelen,
		&character->login_status,
		movepacket,
		NULL, /* Action Packet */
		factorpacket, /* Factor Packet */
		&character->inventory_gold, /* Inventory money */
		&character->bank_gold, /* Bank money */
		NULL, /* Cash */
		&character->action_status,
		&character->criminal_status,
		NULL, /* Attacker id */
		NULL, /* Is New Character? */
		NULL, /* Region index */
		NULL, /* Social action index */
		&character->special_status,
		&transformflags, /* Is transform status */
		NULL, /* Skill initialization text */
		&character->face_index,
		&character->hair_index,
		&character->option_flags,
		&character->extra_bank_slots, /* Bank size */
		&character->event_status_flags,
		&character->remain_criminal_status_ms, /* Remained criminal status time */
		NULL, /* Remained murderer point time */
		character->nickname,
		NULL, /* Gameguard auth data */
		NULL); /* Last killed time in battlesquare */
	ap_packet_set_length(mod->ap_packet, length);
	ap_packet_reset_temp_buffers(mod->ap_packet);
}

void ap_character_make_event_effect_packet(
	struct ap_character_module * mod, 
	struct ap_character * character,
	uint32_t event_effect_id)
{
	uint8_t type = AP_CHARACTER_PACKET_TYPE_EVENT_EFFECT_ID;
	uint16_t length = 0;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	au_packet_make_packet(&mod->packet, 
		buffer, TRUE, &length,
		AP_CHARACTER_PACKET_TYPE, 
		&type,
		&character->id,
		NULL,
		NULL, NULL,
		NULL,
		NULL,
		NULL, /* Action Packet */
		NULL, /* Factor Packet */
		NULL, /* Inventory money */
		NULL, /* Bank money */
		NULL, /* Cash */
		NULL,
		NULL,
		NULL, /* Attacker id */
		NULL, /* Is New Character? */
		NULL, /* Region index */
		NULL, /* Social action index */
		NULL,
		NULL, /* Is transform status */
		NULL, /* Skill initialization text */
		NULL,
		NULL,
		&event_effect_id,
		NULL, /* Bank size */
		NULL,
		NULL, /* Remained criminal status time */
		NULL, /* Remained murderer point time */
		NULL,
		NULL, /* Gameguard auth data */
		NULL); /* Last killed time in battlesquare */
	ap_packet_set_length(mod->ap_packet, length);
}

void ap_character_make_packet(
	struct ap_character_module * mod, 
	enum ap_character_packet_type type,
	uint32_t character_id)
{
	uint16_t length = 0;
	ap_character_make_packet_buffer(mod, type, character_id, 
		ap_packet_get_buffer(mod->ap_packet), &length);
	ap_packet_set_length(mod->ap_packet, length);
}

void ap_character_make_packet_buffer(
	struct ap_character_module * mod, 
	enum ap_character_packet_type type,
	uint32_t character_id,
	void * buffer,
	uint16_t * length)
{
	au_packet_make_packet(&mod->packet, 
		buffer, TRUE, length,
		AP_CHARACTER_PACKET_TYPE, 
		&type,
		&character_id,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL, /* Action Packet */
		NULL, /* Factor Packet */
		NULL, /* Inventory money */
		NULL, /* Bank money */
		NULL, /* Cash */
		NULL,
		NULL,
		NULL, /* Attacker id */
		NULL, /* Is New Character? */
		NULL, /* Region index */
		NULL, /* Social action index */
		NULL,
		NULL, /* Is transform status */
		NULL, /* Skill initialization text */
		NULL,
		NULL,
		NULL,
		NULL, /* Bank size */
		NULL,
		NULL, /* Remained criminal status time */
		NULL, /* Remained murderer point time */
		NULL,
		NULL, /* Gameguard auth data */
		NULL); /* Last killed time in battlesquare */
}

void ap_character_make_update_packet(
	struct ap_character_module * mod, 
	struct ap_character * character,
	uint64_t flags)
{
	uint8_t type = AP_CHARACTER_PACKET_TYPE_UPDATE;
	uint16_t length = 0;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	void * factorpacket = NULL;
	void * movepacket = NULL;
	if (CHECK_BIT(flags, AP_CHARACTER_BIT_POSITION)) {
		uint8_t moveflag = AP_CHARACTER_MOVE_FLAG_SYNC;
		movepacket = ap_packet_get_temp_buffer(mod->ap_packet);
		au_packet_make_packet(
			&mod->packet_move, 
			movepacket, FALSE, NULL, 0,
			&character->pos, /* Character Current Position */
			&character->pos, /* Character Destination Position */
			NULL, /* Follow Target ID */
			NULL, /* Follow Distance */
			NULL, /* Turn Degree X */
			NULL, /* Turn Degree Y */
			&moveflag, /* Move Flag */
			NULL); /* Move Direction */
	}
	if (flags & AP_FACTORS_BIT_MASK) {
		factorpacket = ap_packet_get_temp_buffer(mod->ap_packet);
		ap_factors_make_result_packet(mod->ap_factors, factorpacket, 
			&character->factor, flags);
	}
	au_packet_make_packet(&mod->packet, 
		buffer, TRUE, &length,
		AP_CHARACTER_PACKET_TYPE, 
		&type, /* Packet Type */ 
		&character->id, /* Character ID */
		NULL, /* Character Template ID */
		NULL, /* Character Name */
		NULL, /* Character Status */
		movepacket, /* Move Packet */
		NULL, /* Action Packet */
		factorpacket, /* Factor Packet */
		(flags & AP_CHARACTER_BIT_INVENTORY_GOLD) ?
			&character->inventory_gold : NULL,
		(flags & AP_CHARACTER_BIT_BANK_GOLD) ?
			&character->bank_gold : NULL, /* Bank money */
		NULL, /* Cash */
		(flags & AP_CHARACTER_BIT_ACTION_STATUS) ?
			&character->action_status : NULL,
		(flags & AP_CHARACTER_BIT_CRIMINAL_STATUS) ?
			&character->criminal_status : NULL,
		NULL, /* Attacker id */
		NULL, /* Is New Character? */
		NULL, /* Region index */
		NULL, /* Social action index */
		(flags & AP_CHARACTER_BIT_SPECIAL_STATUS) ?
			&character->special_status : NULL,
		NULL, /* Is transform status */
		NULL, /* Skill initialization text */
		NULL,
		NULL,
		NULL,
		NULL, /* Bank size */
		NULL,
		NULL, /* Remained criminal status time */
		NULL, /* Remained murderer point time */
		NULL,
		NULL, /* Gameguard auth data */
		NULL); /* Last killed time in battlesquare */
	ap_packet_set_length(mod->ap_packet, length);
	ap_packet_reset_temp_buffers(mod->ap_packet);
}

void ap_character_make_update_gameplay_option_packet(
	struct ap_character_module * mod,
	struct ap_character * character)
{
	uint8_t type = AP_CHARACTER_PACKET_TYPE_UPDATE_GAMEPLAY_OPTION;
	uint16_t length = 0;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	char skillinit[AP_CHARACTER_MAX_CHARACTER_SKILL_INIT] = { 0 };
	((int *)skillinit)[0] = character->stop_experience;
	((int *)skillinit)[1] = character->auto_attack_after_skill_cast;
	au_packet_make_packet(&mod->packet, 
		buffer, TRUE, &length,
		AP_CHARACTER_PACKET_TYPE, 
		&type, /* Packet Type */ 
		&character->id, /* Character ID */
		NULL, /* Character Template ID */
		NULL, /* Character Name */
		NULL, /* Character Status */
		NULL, /* Move Packet */
		NULL, /* Action Packet */
		NULL, /* Factor Packet */
		NULL,
		NULL, /* Bank money */
		NULL, /* Cash */
		NULL,
		NULL,
		NULL, /* Attacker id */
		NULL, /* Is New Character? */
		NULL, /* Region index */
		NULL, /* Social action index */
		NULL,
		NULL, /* Is transform status */
		skillinit, /* Skill initialization text */
		NULL,
		NULL,
		NULL,
		NULL, /* Bank size */
		NULL,
		NULL, /* Remained criminal status time */
		NULL, /* Remained murderer point time */
		NULL,
		NULL, /* Gameguard auth data */
		NULL); /* Last killed time in battlesquare */
	ap_packet_set_length(mod->ap_packet, length);
}

void ap_character_make_client_packet(
	struct ap_character_module * mod, 
	enum ac_character_packet_type type,
	const char * account_id,
	uint32_t * character_tid,
	const char * character_name,
	uint32_t * character_id,
	const struct au_pos * pos,
	uint32_t * auth_key)
{
	uint16_t length = 0;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	au_packet_make_packet(&mod->packet_client, 
		buffer, TRUE, &length,
		AC_CHARACTER_PACKET_TYPE, 
		&type,
		account_id,
		character_tid,
		character_name,
		character_id,
		pos,
		auth_key);
	ap_packet_set_length(mod->ap_packet, length);
}

boolean ap_character_parse_client_packet(
	struct ap_character_module * mod, 
	const void * data, 
	uint16_t length,
	enum ac_character_packet_type * type,
	char * character_name,
	uint32_t * auth_key)
{
	const char * cname = NULL;
	if (!au_packet_get_field(&mod->packet_client, 
			TRUE, data, length,
			type,
			NULL,
			NULL,
			&cname,
			NULL,
			NULL,
			auth_key)) {
		return FALSE;
	}
	if (character_name) {
		if (cname) {
			strlcpy(character_name, cname, 
				AP_CHARACTER_MAX_NAME_LENGTH + 1);
		}
		else {
			character_name[0] = '\0';
		}
	}
	return TRUE;
}
