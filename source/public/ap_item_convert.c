#include "public/ap_item_convert.h"

#include "core/file_system.h"
#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"

#include "public/ap_packet.h"
#include "public/ap_random.h"
#include "public/ap_skill.h"

#include "utility/au_packet.h"
#include "utility/au_table.h"

#include <assert.h>
#include <stdlib.h>

struct ap_item_convert_module {
	struct ap_module_instance instance;
	struct ap_item_module * ap_item;
	struct ap_packet_module * ap_packet;
	struct ap_random_module * ap_random;
	struct ap_skill_module * ap_skill;
	struct au_packet packet;
	struct au_packet packet_tid;
	size_t item_offset;
	size_t item_template_offset;
	struct ap_item_convert_physical physical[AP_ITEM_CONVERT_MAX_ITEM_RANK + 1];
	struct ap_item_convert_spirit_stone spirit_stone[AP_ITEM_CONVERT_MAX_SPIRITSTONE_RANK + 1];
	struct ap_item_convert_add_socket add_socket[AP_ITEM_CONVERT_MAX_SOCKET + 1];
	struct ap_item_convert_add_socket_fail add_socket_fail[AP_ITEM_CONVERT_MAX_SOCKET + 1];
};

static void maketidpacket(
	struct ap_item_convert_module * mod,
	void * buffer,
	const struct ap_item_convert_item * item)
{
	switch (item->convert_count) {
	case 1:
		au_packet_make_packet(&mod->packet_tid, 
			buffer, FALSE, NULL, 0, 
			&item->socket_attr[0].tid, /* socket #1*/
			NULL, /* socket #2*/
			NULL, /* socket #3*/
			NULL, /* socket #4*/
			NULL, /* socket #5*/
			NULL, /* socket #6*/
			NULL, /* socket #7*/
			NULL); /* socket #8 */
		break;
	case 2:
		au_packet_make_packet(&mod->packet_tid, 
			buffer, FALSE, NULL, 0, 
			&item->socket_attr[0].tid, /* socket #1*/
			&item->socket_attr[1].tid, /* socket #2*/
			NULL, /* socket #3*/
			NULL, /* socket #4*/
			NULL, /* socket #5*/
			NULL, /* socket #6*/
			NULL, /* socket #7*/
			NULL); /* socket #8 */
		break;
	case 3:
		au_packet_make_packet(&mod->packet_tid, 
			buffer, FALSE, NULL, 0, 
			&item->socket_attr[0].tid, /* socket #1*/
			&item->socket_attr[1].tid, /* socket #2*/
			&item->socket_attr[2].tid, /* socket #3*/
			NULL, /* socket #4*/
			NULL, /* socket #5*/
			NULL, /* socket #6*/
			NULL, /* socket #7*/
			NULL); /* socket #8 */
		break;
	case 4:
		au_packet_make_packet(&mod->packet_tid, 
			buffer, FALSE, NULL, 0, 
			&item->socket_attr[0].tid, /* socket #1*/
			&item->socket_attr[1].tid, /* socket #2*/
			&item->socket_attr[2].tid, /* socket #3*/
			&item->socket_attr[3].tid, /* socket #4*/
			NULL, /* socket #5*/
			NULL, /* socket #6*/
			NULL, /* socket #7*/
			NULL); /* socket #8 */
		break;
	case 5:
		au_packet_make_packet(&mod->packet_tid, 
			buffer, FALSE, NULL, 0, 
			&item->socket_attr[0].tid, /* socket #1*/
			&item->socket_attr[1].tid, /* socket #2*/
			&item->socket_attr[2].tid, /* socket #3*/
			&item->socket_attr[3].tid, /* socket #4*/
			&item->socket_attr[4].tid, /* socket #5*/
			NULL, /* socket #6*/
			NULL, /* socket #7*/
			NULL); /* socket #8 */
		break;
	case 6:
		au_packet_make_packet(&mod->packet_tid, 
			buffer, FALSE, NULL, 0, 
			&item->socket_attr[0].tid, /* socket #1*/
			&item->socket_attr[1].tid, /* socket #2*/
			&item->socket_attr[2].tid, /* socket #3*/
			&item->socket_attr[3].tid, /* socket #4*/
			&item->socket_attr[4].tid, /* socket #5*/
			&item->socket_attr[5].tid, /* socket #6*/
			NULL, /* socket #7*/
			NULL); /* socket #8 */
		break;
	case 7:
		au_packet_make_packet(&mod->packet_tid, 
			buffer, FALSE, NULL, 0, 
			&item->socket_attr[0].tid, /* socket #1*/
			&item->socket_attr[1].tid, /* socket #2*/
			&item->socket_attr[2].tid, /* socket #3*/
			&item->socket_attr[3].tid, /* socket #4*/
			&item->socket_attr[4].tid, /* socket #5*/
			&item->socket_attr[5].tid, /* socket #6*/
			&item->socket_attr[6].tid, /* socket #7*/
			NULL); /* socket #8 */
		break;
	case 8:
		au_packet_make_packet(&mod->packet_tid, 
			buffer, FALSE, NULL, 0, 
			&item->socket_attr[0].tid, /* socket #1*/
			&item->socket_attr[1].tid, /* socket #2*/
			&item->socket_attr[2].tid, /* socket #3*/
			&item->socket_attr[3].tid, /* socket #4*/
			&item->socket_attr[4].tid, /* socket #5*/
			&item->socket_attr[5].tid, /* socket #6*/
			&item->socket_attr[6].tid, /* socket #7*/
			&item->socket_attr[7].tid); /* socket #8 */
		break;
	}
}

static void applyconvert(
	struct ap_item_convert_module * mod,
	struct ap_character * character,
	struct ap_item * item,
	int modifier)
{
	uint32_t i;
	struct ap_item_convert_item * attachment = ap_item_convert_get_item(mod, item);
	for (i = 0; i < attachment->convert_count; i++) {
		const struct ap_item_convert_socket_attr * socket = 
			&attachment->socket_attr[i];
		uint32_t j;
		if (socket->is_spirit_stone)
			continue;
		for (j = 0; j < socket->item_template->option_count; j++) {
			ap_item_apply_option(mod->ap_item, character, 
				socket->item_template->options[j], modifier);
		}
	}
}

static boolean onequip(
	struct ap_item_convert_module * mod,
	struct ap_item_cb_equip * cb)
{
	if (!(cb->item->equip_flags & AP_ITEM_EQUIP_WITH_NO_STATS))
		applyconvert(mod, cb->character, cb->item, 1);
	return TRUE;
}

static boolean onunequip(
	struct ap_item_convert_module * mod,
	struct ap_item_cb_unequip * cb)
{
	if (!(cb->item->equip_flags & AP_ITEM_EQUIP_WITH_NO_STATS))
		applyconvert(mod, cb->character, cb->item, -1);
	return TRUE;
}

static boolean onregister(
	struct ap_item_convert_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_item, AP_ITEM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_packet, AP_PACKET_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_random, AP_RANDOM_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_skill, AP_SKILL_MODULE_NAME);
	mod->item_offset = ap_item_attach_data(mod->ap_item, AP_ITEM_MDI_ITEM,
		sizeof(struct ap_item_convert_item), mod, NULL, NULL);
	mod->item_template_offset = ap_item_attach_data(mod->ap_item,
		AP_ITEM_MDI_TEMPLATE, sizeof(struct ap_item_convert_item_template), 
		mod, NULL, NULL);
	ap_item_add_callback(mod->ap_item, AP_ITEM_CB_EQUIP, mod, onequip);
	ap_item_add_callback(mod->ap_item, AP_ITEM_CB_UNEQUIP, mod, onunequip);
	return TRUE;
}

struct ap_item_convert_module * ap_item_convert_create_module()
{
	struct ap_item_convert_module * mod = ap_module_instance_new(
		AP_ITEM_CONVERT_MODULE_NAME, sizeof(*mod), onregister, NULL, NULL, NULL);
	au_packet_init(&mod->packet, sizeof(uint16_t),
		AU_PACKET_TYPE_UINT8, 1, /* packet type */ 
		AU_PACKET_TYPE_INT32, 1, /* character id */
		AU_PACKET_TYPE_INT32, 1, /* item id */
		AU_PACKET_TYPE_INT32, 1, /* catalyst item id */
		AU_PACKET_TYPE_INT8, 1, /* action result */
		AU_PACKET_TYPE_INT8, 1, /* # of physical convert */
		AU_PACKET_TYPE_INT8, 1, /* # of socket */
		AU_PACKET_TYPE_INT8, 1, /* # of converted socket */
		AU_PACKET_TYPE_INT32, 1, /* add convert item tid */
		AU_PACKET_TYPE_PACKET, 1, /* converted item tid list */
		AU_PACKET_TYPE_END);
	au_packet_init(&mod->packet_tid, sizeof(uint8_t),
		AU_PACKET_TYPE_INT32, 1, /* converted item tid list */
		AU_PACKET_TYPE_INT32, 1,
		AU_PACKET_TYPE_INT32, 1,
		AU_PACKET_TYPE_INT32, 1,
		AU_PACKET_TYPE_INT32, 1,
		AU_PACKET_TYPE_INT32, 1,
		AU_PACKET_TYPE_INT32, 1,
		AU_PACKET_TYPE_INT32, 1,
		AU_PACKET_TYPE_END);
	return mod;
}

static inline boolean isconvertstepseparator(const char * line)
{
	while (TRUE) {
		char c = *line++;
		if (!c)
			break;
		if (c != '\t')
			return FALSE;
	}
	return TRUE;
}

enum convertstep {
	CONVERTSTEP_PHYSICAL_RANK,
	CONVERTSTEP_PHYSICAL_SUCCESS,
	CONVERTSTEP_PHYSICAL_FAIL,
	CONVERTSTEP_PHYSICAL_INIT,
	CONVERTSTEP_PHYSICAL_DESTROY,
	CONVERTSTEP_SPIRIT_STONE,
	CONVERTSTEP_ADD_BONUS,
	CONVERTSTEP_ADD_SOCKET,
	CONVERTSTEP_ADD_SOCKET_FAIL,
};

static char * strsep(char ** stringp, const char * delim)
{
	char * start = *stringp;
	char * p;
	p = (start != NULL) ? strpbrk(start, delim) : NULL;
	if (p == NULL) {
		*stringp = NULL;
	}
	else {
		*p = '\0';
		*stringp = p + 1;
	}
	return start;
}

void ap_item_convert_add_callback(
	struct ap_item_convert_module * mod,
	enum ap_item_convert_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	ap_module_add_callback(mod, id, callback_module, callback);
}

boolean ap_item_convert_read_convert_table(
	struct ap_item_convert_module * mod,
	const char * file_path)
{
	file file = open_file(file_path, FILE_ACCESS_READ);
	char line[1024];
	uint32_t convertstep = 0;
	uint32_t linenumber = 0;
	if (!file) {
		ERROR("Failed to open file (%s).", file_path);
		return FALSE;
	}
	if (!read_line(file, line, sizeof(line))) {
		ERROR("Failed to read file header (%s).", file_path);
		return FALSE;
	}
	while (read_line(file, line, sizeof(line))) {
		char * cursor = line;
		linenumber++;
		if (isconvertstepseparator(line)) {
			convertstep++;
			/* Skip header. */
			if (!read_line(file, line, sizeof(line)))
				break;
			linenumber++;
			continue;
		}
		switch (convertstep) {
		case CONVERTSTEP_PHYSICAL_RANK: {
			uint32_t i;
			char * token = strsep(&cursor, "\t");
			uint32_t rank = strtoul(token, NULL, 10);
			struct ap_item_convert_physical * physical = &mod->physical[rank];
			assert(rank <= AP_ITEM_CONVERT_MAX_ITEM_RANK);
			physical->weapon_add_value = strtoul(strsep(&cursor, "\t"), NULL, 10);
			physical->armor_add_value = strtoul(strsep(&cursor, "\t"), NULL, 10);
			strlcpy(physical->rank, strsep(&cursor, "\t"), sizeof(physical->rank));
			for (i = 0; i < AP_ITEM_CONVERT_MAX_SPIRITSTONE_RANK; i++) {
				physical->is_convertable_spirit[i + 1] = 
					!strisempty(strsep(&cursor, "\t"));
			}
			break;
		}
		case CONVERTSTEP_PHYSICAL_SUCCESS: {
			uint32_t i;
			char * token = strsep(&cursor, "\t");
			uint32_t rank = strtoul(token, NULL, 10);
			struct ap_item_convert_physical * physical = &mod->physical[rank];
			assert(rank <= AP_ITEM_CONVERT_MAX_ITEM_RANK);
			for (i = 0; i < AP_ITEM_CONVERT_MAX_ITEM_RANK; i++)
				physical->success_rate[i + 1] = strtoul(strsep(&cursor, "\t"), NULL, 10);
			break;
		}
		case CONVERTSTEP_PHYSICAL_FAIL: {
			uint32_t i;
			char * token = strsep(&cursor, "\t");
			uint32_t rank = strtoul(token, NULL, 10);
			struct ap_item_convert_physical * physical = &mod->physical[rank];
			assert(rank <= AP_ITEM_CONVERT_MAX_ITEM_RANK);
			for (i = 0; i < AP_ITEM_CONVERT_MAX_ITEM_RANK; i++)
				physical->fail_rate[i + 1] = strtoul(strsep(&cursor, "\t"), NULL, 10);
			break;
		}
		case CONVERTSTEP_PHYSICAL_INIT: {
			uint32_t i;
			char * token = strsep(&cursor, "\t");
			uint32_t rank = strtoul(token, NULL, 10);
			struct ap_item_convert_physical * physical = &mod->physical[rank];
			assert(rank <= AP_ITEM_CONVERT_MAX_ITEM_RANK);
			for (i = 0; i < AP_ITEM_CONVERT_MAX_ITEM_RANK; i++)
				physical->init_rate[i + 1] = strtoul(strsep(&cursor, "\t"), NULL, 10);
			break;
		}
		case CONVERTSTEP_PHYSICAL_DESTROY: {
			uint32_t i;
			char * token = strsep(&cursor, "\t");
			uint32_t rank = strtoul(token, NULL, 10);
			struct ap_item_convert_physical * physical = &mod->physical[rank];
			assert(rank <= AP_ITEM_CONVERT_MAX_ITEM_RANK);
			for (i = 0; i < AP_ITEM_CONVERT_MAX_ITEM_RANK; i++) {
				physical->destroy_rate[i + 1] = strtoul(strsep(&cursor, "\t"), NULL, 10);
				if (!physical->first_destroy_rank && physical->destroy_rate[i + 1])
					physical->first_destroy_rank = i + 1;
			}
			break;
		}
		case CONVERTSTEP_SPIRIT_STONE: {
			char * token = strsep(&cursor, "\t");
			uint32_t rank = strtoul(token, NULL, 10);
			struct ap_item_convert_spirit_stone * spirit = &mod->spirit_stone[rank];
			assert(rank <= AP_ITEM_CONVERT_MAX_SPIRITSTONE_RANK);
			spirit->weapon_add_value = strtoul(strsep(&cursor, "\t"), NULL, 10);
			spirit->weapon_rate = strtoul(strsep(&cursor, "\t"), NULL, 10);
			spirit->armor_add_value = strtoul(strsep(&cursor, "\t"), NULL, 10);
			spirit->armor_rate = strtoul(strsep(&cursor, "\t"), NULL, 10);
			break;
		}
		case CONVERTSTEP_ADD_SOCKET_FAIL: {
			char * token = strsep(&cursor, "\t");
			uint32_t rank = strtoul(token, NULL, 10);
			struct ap_item_convert_add_socket_fail * fail = &mod->add_socket_fail[rank];
			assert(rank <= AP_ITEM_CONVERT_MAX_SOCKET);
			fail->weapon_keep_current = strtoul(strsep(&cursor, "\t"), NULL, 10);
			fail->weapon_initialize_same = strtoul(strsep(&cursor, "\t"), NULL, 10);
			fail->weapon_initialize = strtoul(strsep(&cursor, "\t"), NULL, 10);
			fail->weapon_destroy = strtoul(strsep(&cursor, "\t"), NULL, 10);
			fail->armor_keep_current = strtoul(strsep(&cursor, "\t"), NULL, 10);
			fail->armor_initialize_same = strtoul(strsep(&cursor, "\t"), NULL, 10);
			fail->armor_initialize = strtoul(strsep(&cursor, "\t"), NULL, 10);
			fail->armor_destroy = strtoul(strsep(&cursor, "\t"), NULL, 10);
			fail->etc_keep_current = strtoul(strsep(&cursor, "\t"), NULL, 10);
			fail->etc_initialize_same = strtoul(strsep(&cursor, "\t"), NULL, 10);
			fail->etc_initialize = strtoul(strsep(&cursor, "\t"), NULL, 10);
			fail->etc_destroy = strtoul(strsep(&cursor, "\t"), NULL, 10);
			break;
		}
		}
	}
	close_file(file);
	return TRUE;
}

enum rune_attribute_column_id {
	RUNE_COLUMN_ITEM_NAME,
	RUNE_COLUMN_TID,
	RUNE_COLUMN_ANTI_NUMBER,
	RUNE_COLUMN_SECONDCATEGORY,
	RUNE_COLUMN_SKILL_NAME,
	RUNE_COLUMN_SKILL_LEVEL,
	RUNE_COLUMN_SKILL_RATE,
	RUNE_COLUMN_UPPER_BODY,
	RUNE_COLUMN_LOWER_BODY,
	RUNE_COLUMN_WEAPON,
	RUNE_COLUMN_SHIELD,
	RUNE_COLUMN_HEAD,
	RUNE_COLUMN_RING,
	RUNE_COLUMN_NECKLACE,
	RUNE_COLUMN_SHOES,
	RUNE_COLUMN_GLOVES,
	RUNE_COLUMN_SUCCESS_RATE,
	RUNE_COLUMN_RANK,
	RUNE_COLUMN_MOUNT_RESTRICTION_LEVEL,
	RUNE_COLUMN_DESCRIPTION,
};

boolean ap_item_convert_read_rune_attribute_table(
	struct ap_item_convert_module * mod,
	const char * file_path,
	boolean decrypt)
{
	struct au_table * table = au_table_open(file_path, decrypt);
	boolean r = TRUE;
	if (!table) {
		ERROR("Failed to open file (%s).", file_path);
		return FALSE;
	}
	r &= au_table_set_column(table, "Item_Name", RUNE_COLUMN_ITEM_NAME);
	r &= au_table_set_column(table, "TID", RUNE_COLUMN_TID);
	r &= au_table_set_column(table, "ANTI Number", RUNE_COLUMN_ANTI_NUMBER);
	r &= au_table_set_column(table, "SecondCategory", RUNE_COLUMN_SECONDCATEGORY);
	r &= au_table_set_column(table, "Skill_Name", RUNE_COLUMN_SKILL_NAME);
	r &= au_table_set_column(table, "Skill_Level", RUNE_COLUMN_SKILL_LEVEL);
	r &= au_table_set_column(table, "Skill_Rate", RUNE_COLUMN_SKILL_RATE);
	r &= au_table_set_column(table, "Upper_Body", RUNE_COLUMN_UPPER_BODY);
	r &= au_table_set_column(table, "Lower_Body", RUNE_COLUMN_LOWER_BODY);
	r &= au_table_set_column(table, "Weapon", RUNE_COLUMN_WEAPON);
	r &= au_table_set_column(table, "Shield", RUNE_COLUMN_SHIELD);
	r &= au_table_set_column(table, "Head", RUNE_COLUMN_HEAD);
	r &= au_table_set_column(table, "Ring", RUNE_COLUMN_RING);
	r &= au_table_set_column(table, "Necklace", RUNE_COLUMN_NECKLACE);
	r &= au_table_set_column(table, "Shoes", RUNE_COLUMN_SHOES);
	r &= au_table_set_column(table, "Gloves", RUNE_COLUMN_GLOVES);
	r &= au_table_set_column(table, "Success_Rate", RUNE_COLUMN_SUCCESS_RATE);
	r &= au_table_set_column(table, "Rank", RUNE_COLUMN_RANK);
	r &= au_table_set_column(table, "Mount_Restriction_Level", RUNE_COLUMN_MOUNT_RESTRICTION_LEVEL);
	r &= au_table_set_column(table, "Description", RUNE_COLUMN_DESCRIPTION);
	if (!r) {
		ERROR("Failed to setup columns.");
		au_table_destroy(table);
		return FALSE;
	}
	while (au_table_read_next_line(table)) {
		struct ap_item_convert_item_template * rune = NULL;
		char name[AP_ITEM_MAX_NAME_LENGTH + 1] = "";
		while (au_table_read_next_column(table)) {
			enum ap_item_option_data_column_id id = 
				au_table_get_column(table);
			char * value = au_table_get_value(table);
			switch (id) {
			case  RUNE_COLUMN_TID: {
				uint32_t tid = strtoul(value, NULL, 10);
				struct ap_item_template * temp = 
					ap_item_get_template(mod->ap_item, tid);
				if (!temp) {
					ERROR("Failed to retrieve item rune attribute template (%u).", tid);
					au_table_destroy(table);
					return FALSE;
				}
				assert(temp->type == AP_ITEM_TYPE_USABLE);
				assert(temp->usable.usable_type == AP_ITEM_USABLE_TYPE_RUNE);
				rune = ap_item_convert_get_item_template_attachment(mod, temp);
				continue;
			}
			case RUNE_COLUMN_ITEM_NAME:
				strlcpy(name, value, sizeof(name));
				continue;
			default:
				if (!rune) {
					assert(0);
					continue;
				}
				break;
			}
			switch (id) {
			case RUNE_COLUMN_ANTI_NUMBER:
				rune->anti_type_number = strtoul(value, NULL, 10);
				break;
			case RUNE_COLUMN_SECONDCATEGORY:
				break;
			case RUNE_COLUMN_SKILL_NAME:
				rune->skill = ap_skill_get_template_by_name(mod->ap_skill, value);
				assert(rune->skill != NULL);
				break;
			case RUNE_COLUMN_SKILL_LEVEL:
				rune->skill_level = strtoul(value, NULL, 10);
				break;
			case RUNE_COLUMN_SKILL_RATE:
				rune->skill_rate = strtoul(value, NULL, 10);
				break;
			case RUNE_COLUMN_UPPER_BODY:
				rune->rune_convertable_equip_kind[AP_ITEM_EQUIP_KIND_ARMOR_INDEX] = TRUE;
				rune->rune_convertable_equip_part[AP_ITEM_PART_BODY] = TRUE;
				break;
			case RUNE_COLUMN_LOWER_BODY:
				rune->rune_convertable_equip_kind[AP_ITEM_EQUIP_KIND_ARMOR_INDEX] = TRUE;
				rune->rune_convertable_equip_part[AP_ITEM_PART_LEGS] = TRUE;
				break;
			case RUNE_COLUMN_WEAPON:
				rune->rune_convertable_equip_kind[AP_ITEM_EQUIP_KIND_WEAPON_INDEX] = TRUE;
				break;
			case RUNE_COLUMN_SHIELD:
				rune->rune_convertable_equip_kind[AP_ITEM_EQUIP_KIND_SHIELD_INDEX] = TRUE;
				break;
			case RUNE_COLUMN_HEAD:
				rune->rune_convertable_equip_kind[AP_ITEM_EQUIP_KIND_ARMOR_INDEX] = TRUE;
				rune->rune_convertable_equip_part[AP_ITEM_PART_HEAD] = TRUE;
				break;
			case RUNE_COLUMN_RING:
				rune->rune_convertable_equip_kind[AP_ITEM_EQUIP_KIND_RING_INDEX] = TRUE;
				rune->rune_convertable_equip_part[AP_ITEM_PART_ACCESSORY_RING1] = TRUE;
				break;
			case RUNE_COLUMN_NECKLACE:
				rune->rune_convertable_equip_kind[AP_ITEM_EQUIP_KIND_NECKLACE_INDEX] = TRUE;
				rune->rune_convertable_equip_part[AP_ITEM_PART_ACCESSORY_NECKLACE] = TRUE;
				break;
			case RUNE_COLUMN_SHOES:
				rune->rune_convertable_equip_kind[AP_ITEM_EQUIP_KIND_ARMOR_INDEX] = TRUE;
				rune->rune_convertable_equip_part[AP_ITEM_PART_FOOT] = TRUE;
				break;
			case RUNE_COLUMN_GLOVES:
				rune->rune_convertable_equip_kind[AP_ITEM_EQUIP_KIND_ARMOR_INDEX] = TRUE;
				rune->rune_convertable_equip_part[AP_ITEM_PART_HANDS] = TRUE;
				break;
			case RUNE_COLUMN_SUCCESS_RATE:
				rune->rune_success_rate = strtoul(value, NULL, 10);
				break;
			case RUNE_COLUMN_RANK:
				break;
			case RUNE_COLUMN_MOUNT_RESTRICTION_LEVEL:
				rune->rune_restrict_level = strtoul(value, NULL, 10);
				break;
			case RUNE_COLUMN_DESCRIPTION:
				strlcpy(rune->description, value, sizeof(rune->description));
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

struct ap_item_convert_item_template * ap_item_convert_get_item_template_attachment(
	struct ap_item_convert_module * mod,
	struct ap_item_template * temp)
{
	return ap_module_get_attached_data(temp, mod->item_template_offset);
}

struct ap_item_convert_item * ap_item_convert_get_item(
	struct ap_item_convert_module * mod,
	struct ap_item * item)
{
	return ap_module_get_attached_data(item, mod->item_offset);
}

static void applyphysicalconvertbonus(
	struct ap_item_convert_module * mod,
	struct ap_item * item,
	uint32_t rank,
	float modifier)
{
	assert(rank <= AP_ITEM_CONVERT_MAX_ITEM_RANK);
	switch (item->temp->equip.kind) {
	case AP_ITEM_EQUIP_KIND_WEAPON: {
		uint32_t i;
		for (i = 1; i <= rank; i++) {
			const struct ap_item_convert_physical * physical = &mod->physical[i];
			item->factor.damage.min.physical += modifier * physical->weapon_add_value;
			item->factor.damage.max.physical += modifier * physical->weapon_add_value;
		}
		break;
	}
	case AP_ITEM_EQUIP_KIND_ARMOR:
	case AP_ITEM_EQUIP_KIND_SHIELD: {
		uint32_t i;
		for (i = 1; i <= rank; i++) {
			const struct ap_item_convert_physical * physical = &mod->physical[i];
			item->factor.defense.point.physical += modifier * physical->armor_add_value;
		}
		break;
	}
	}
}

void ap_item_convert_set_physical_convert_bonus(
	struct ap_item_convert_module * mod,
	struct ap_item * item,
	struct ap_item_convert_item * attachment,
	uint32_t previous_physical_convert_level)
{
	if (previous_physical_convert_level != UINT32_MAX)
		applyphysicalconvertbonus(mod, item, previous_physical_convert_level, -1.0f);
	applyphysicalconvertbonus(mod, item, attachment->physical_convert_level, 1.0f);
}

boolean ap_item_convert_is_suitable_for_physical_convert(
	struct ap_item_convert_module * mod,
	struct ap_item * item,
	struct ap_item_convert_item * attachment)
{
	uint32_t maxrank = item->temp->factor.item.physical_rank;
	assert(maxrank <= AP_ITEM_CONVERT_MAX_ITEM_RANK);
	if (attachment->physical_convert_level >= maxrank)
		return FALSE;
	switch (item->temp->equip.part) {
	case AP_ITEM_PART_HAND_LEFT:
	case AP_ITEM_PART_HAND_RIGHT:
	case AP_ITEM_PART_BODY:
	case AP_ITEM_PART_LEGS:
		return TRUE;
	default:
		return FALSE;
	}
}

enum ap_item_convert_result ap_item_convert_attempt_physical_convert(
	struct ap_item_convert_module * mod,
	struct ap_item * item,
	struct ap_item_convert_item * attachment,
	uint32_t rate)
{
	uint32_t rank = item->temp->factor.item.physical_rank;
	uint32_t nextrank = attachment->physical_convert_level + 1;
	const struct ap_item_convert_physical * physical = &mod->physical[rank];
	uint32_t totalrate = 0;
	assert(rank <= AP_ITEM_CONVERT_MAX_ITEM_RANK);
	assert(nextrank <= AP_ITEM_CONVERT_MAX_ITEM_RANK);
	if (rate < physical->success_rate[nextrank]) {
		attachment->physical_convert_level++;
		attachment->update_flags |= AP_ITEM_CONVERT_UPDATE_PHYSICAL_CONVERT_LEVEL;
		ap_item_convert_set_physical_convert_bonus(mod, item, attachment, 
			attachment->physical_convert_level - 1);
		return AP_ITEM_CONVERT_RESULT_SUCCESS;
	}
	rate = ap_random_get(mod->ap_random, 100);
	totalrate += physical->fail_rate[nextrank];
	if (rate < totalrate)
		return AP_ITEM_CONVERT_RESULT_FAILED;
	totalrate += physical->init_rate[nextrank];
	if (rate < totalrate) {
		if (attachment->physical_convert_level) {
			attachment->physical_convert_level--;
			attachment->update_flags |= AP_ITEM_CONVERT_UPDATE_PHYSICAL_CONVERT_LEVEL;
			ap_item_convert_set_physical_convert_bonus(mod, item, attachment, 
				attachment->physical_convert_level + 1);
		}
		return AP_ITEM_CONVERT_RESULT_FAILED_AND_INIT;
	}
	totalrate += physical->destroy_rate[nextrank];
	if (rate < totalrate)
		return AP_ITEM_CONVERT_RESULT_FAILED_AND_DESTROY;
	/* Unreachable!*/
	assert(0);
	return AP_ITEM_CONVERT_RESULT_FAILED;
}

void ap_item_convert_init_socket(
	struct ap_item_convert_module * mod,
	struct ap_item * item,
	struct ap_item_convert_item * attachment)
{
	uint32_t i;
	for (i = 0; i < attachment->convert_count; i++) {
		struct ap_item_convert_socket_attr * socket = &attachment->socket_attr[i];
		if (socket->is_spirit_stone) {
			ap_item_convert_apply_spirit_stone(mod, item, attachment, 
				socket->item_template, -1);
		}
	}
	attachment->convert_count = 0;
	attachment->update_flags |= AP_ITEM_CONVERT_UPDATE_CONVERT_TID;
}

void ap_item_convert_apply_spirit_stone(
	struct ap_item_convert_module * mod,
	struct ap_item * item,
	struct ap_item_convert_item * attachment,
	struct ap_item_template * spirit_stone,
	int modifier)
{
	struct ap_item_convert_spirit_stone * spirit = 
		&mod->spirit_stone[spirit_stone->factor.item.rank];
	assert(spirit_stone->factor.item.rank <= AP_ITEM_CONVERT_MAX_SPIRITSTONE_RANK);
	if (item->temp->equip.kind == AP_ITEM_EQUIP_KIND_WEAPON) {
		switch (spirit_stone->usable.spirit_stone_type) {
		case AP_ITEM_USABLE_SS_TYPE_MAGIC:
			item->factor.damage.min.magic += modifier * spirit->weapon_add_value;
			item->factor.damage.max.magic += modifier * spirit->weapon_add_value;
			break;
		case AP_ITEM_USABLE_SS_TYPE_WATER:
			item->factor.damage.min.water += modifier * spirit->weapon_add_value;
			item->factor.damage.max.water += modifier * spirit->weapon_add_value;
			break;
		case AP_ITEM_USABLE_SS_TYPE_FIRE:
			item->factor.damage.min.fire += modifier * spirit->weapon_add_value;
			item->factor.damage.max.fire += modifier * spirit->weapon_add_value;
			break;
		case AP_ITEM_USABLE_SS_TYPE_EARTH:
			item->factor.damage.min.earth += modifier * spirit->weapon_add_value;
			item->factor.damage.max.earth += modifier * spirit->weapon_add_value;
			break;
		case AP_ITEM_USABLE_SS_TYPE_AIR:
			item->factor.damage.min.air += modifier * spirit->weapon_add_value;
			item->factor.damage.max.air += modifier * spirit->weapon_add_value;
			break;
		}
	}
	else {
		switch (spirit_stone->usable.spirit_stone_type) {
		case AP_ITEM_USABLE_SS_TYPE_MAGIC:
			item->factor.defense.point.magic += modifier * spirit->armor_add_value;
			break;
		case AP_ITEM_USABLE_SS_TYPE_WATER:
			item->factor.defense.point.water += modifier * spirit->armor_add_value;
			break;
		case AP_ITEM_USABLE_SS_TYPE_FIRE:
			item->factor.defense.point.fire += modifier * spirit->armor_add_value;
			break;
		case AP_ITEM_USABLE_SS_TYPE_EARTH:
			item->factor.defense.point.earth += modifier * spirit->armor_add_value;
			break;
		case AP_ITEM_USABLE_SS_TYPE_AIR:
			item->factor.defense.point.air += modifier * spirit->armor_add_value;
			break;
		}
	}
}

enum ap_item_convert_spirit_stone_result ap_item_convert_is_suitable_for_spirit_stone(
	struct ap_item_convert_module * mod,
	struct ap_item * item,
	struct ap_item_convert_item * attachment,
	struct ap_item * spirit_stone)
{
	struct ap_item_convert_physical * physical;
	assert(item->temp->type == AP_ITEM_TYPE_EQUIP);
	assert(spirit_stone->temp->type == AP_ITEM_TYPE_USABLE);
	if (spirit_stone->temp->usable.usable_type != AP_ITEM_USABLE_TYPE_SPIRIT_STONE)
		return AP_ITEM_CONVERT_SPIRIT_STONE_RESULT_INVALID_SPIRITSTONE;
	if (attachment->convert_count >= attachment->socket_count)
		return AP_ITEM_CONVERT_SPIRIT_STONE_RESULT_IS_ALREADY_FULL;
	if (item->temp->equip.kind != AP_ITEM_EQUIP_KIND_WEAPON &&
		item->temp->equip.kind != AP_ITEM_EQUIP_KIND_ARMOR) {
		return AP_ITEM_CONVERT_SPIRIT_STONE_RESULT_IMPROPER_ITEM;
	}
	switch (spirit_stone->temp->usable.spirit_stone_type) {
	case AP_ITEM_USABLE_SS_TYPE_MAGIC:
	case AP_ITEM_USABLE_SS_TYPE_WATER:
	case AP_ITEM_USABLE_SS_TYPE_FIRE:
	case AP_ITEM_USABLE_SS_TYPE_EARTH:
	case AP_ITEM_USABLE_SS_TYPE_AIR:
		break;
	default:
		return AP_ITEM_CONVERT_SPIRIT_STONE_RESULT_INVALID_SPIRITSTONE;
	}
	if (spirit_stone->factor.item.rank < 1 || spirit_stone->factor.item.rank > 5)
		return AP_ITEM_CONVERT_SPIRIT_STONE_RESULT_INVALID_SPIRITSTONE;
	assert(item->factor.item.physical_rank <= AP_ITEM_CONVERT_MAX_ITEM_RANK);
	physical = &mod->physical[item->factor.item.physical_rank];
	if (!physical->is_convertable_spirit[spirit_stone->factor.item.rank])
		return AP_ITEM_CONVERT_SPIRIT_STONE_RESULT_INVALID_RANK;
	return AP_ITEM_CONVERT_SPIRIT_STONE_RESULT_SUCCESS;
}

enum ap_item_convert_spirit_stone_result ap_item_convert_attempt_spirit_stone(
	struct ap_item_convert_module * mod,
	struct ap_item * item,
	struct ap_item_convert_item * attachment,
	struct ap_item * spirit_stone)
{
	struct ap_item_convert_spirit_stone * spirit;
	uint32_t rate = ap_random_get(mod->ap_random, 100);
	uint32_t successrate;
	uint32_t index;
	assert(item->temp->type == AP_ITEM_TYPE_EQUIP);
	assert(spirit_stone->temp->type == AP_ITEM_TYPE_USABLE);
	assert(spirit_stone->temp->usable.usable_type == AP_ITEM_USABLE_TYPE_SPIRIT_STONE);
	assert(spirit_stone->factor.item.rank <= AP_ITEM_CONVERT_MAX_SPIRITSTONE_RANK);
	assert(attachment->convert_count < attachment->socket_count);
	spirit = &mod->spirit_stone[spirit_stone->factor.item.rank];
	if (item->temp->equip.kind == AP_ITEM_EQUIP_KIND_WEAPON)
		successrate = spirit->weapon_rate;
	else
		successrate = spirit->armor_rate;
	if (!(rate < successrate))
		return AP_ITEM_CONVERT_SPIRIT_STONE_RESULT_FAILED;
	index = attachment->convert_count++;
	attachment->socket_attr[index].is_spirit_stone = TRUE;
	attachment->socket_attr[index].tid = spirit_stone->tid;
	attachment->socket_attr[index].item_template = spirit_stone->temp;
	attachment->update_flags |= AP_ITEM_CONVERT_UPDATE_CONVERT_TID;
	ap_item_convert_apply_spirit_stone(mod, item, attachment, spirit_stone->temp, 1);
	return AP_ITEM_CONVERT_SPIRIT_STONE_RESULT_SUCCESS;
}

enum ap_item_convert_rune_result ap_item_convert_is_suitable_for_rune_convert(
	struct ap_item_convert_module * mod,
	struct ap_item * item,
	struct ap_item_convert_item * attachment,
	struct ap_item * rune)
{
	struct ap_item_convert_item_template * runeattachment = 
		ap_item_convert_get_item_template_attachment(mod, rune->temp);
	assert(item->temp->type == AP_ITEM_TYPE_EQUIP);
	assert(rune->temp->type == AP_ITEM_TYPE_USABLE);
	if (rune->temp->usable.usable_type != AP_ITEM_USABLE_TYPE_RUNE)
		return AP_ITEM_CONVERT_RUNE_RESULT_INVALID_RUNE_ITEM;
	switch (item->temp->equip.kind) {
	case AP_ITEM_EQUIP_KIND_ARMOR:
		if (!runeattachment->rune_convertable_equip_kind[AP_ITEM_EQUIP_KIND_ARMOR_INDEX] ||
			!runeattachment->rune_convertable_equip_part[item->temp->equip.part]) {
			return AP_ITEM_CONVERT_RUNE_RESULT_IS_IMPROPER_PART;
		}
		break;
	case AP_ITEM_EQUIP_KIND_WEAPON:
		if (!runeattachment->rune_convertable_equip_kind[AP_ITEM_EQUIP_KIND_WEAPON_INDEX])
			return AP_ITEM_CONVERT_RUNE_RESULT_IS_IMPROPER_PART;
		break;
	case AP_ITEM_EQUIP_KIND_SHIELD:
		if (!runeattachment->rune_convertable_equip_kind[AP_ITEM_EQUIP_KIND_SHIELD_INDEX])
			return AP_ITEM_CONVERT_RUNE_RESULT_IS_IMPROPER_PART;
		break;
	case AP_ITEM_EQUIP_KIND_RING:
		if (!runeattachment->rune_convertable_equip_kind[AP_ITEM_EQUIP_KIND_RING_INDEX])
			return AP_ITEM_CONVERT_RUNE_RESULT_IS_IMPROPER_PART;
		break;
	case AP_ITEM_EQUIP_KIND_NECKLACE:
		if (!runeattachment->rune_convertable_equip_kind[AP_ITEM_EQUIP_KIND_NECKLACE_INDEX])
			return AP_ITEM_CONVERT_RUNE_RESULT_IS_IMPROPER_PART;
		break;
	default:
		return AP_ITEM_CONVERT_RUNE_RESULT_IS_IMPROPER_PART;
	}
	if (attachment->convert_count >= attachment->socket_count)
		return AP_ITEM_CONVERT_RUNE_RESULT_IS_ALREADY_FULL;
	if (rune->temp->usable.rune_attribute_type == AP_ITEM_RUNE_ATTR_CONVERT) {
		uint32_t physicalrank = item->temp->factor.item.physical_rank;
		const struct ap_item_convert_physical * physical = &mod->physical[physicalrank];
		assert(physicalrank <= AP_ITEM_CONVERT_MAX_ITEM_RANK);
		if (attachment->physical_convert_level >= physical->first_destroy_rank ||
			attachment->convert_count ||
			item->option_count) {
			return AP_ITEM_CONVERT_RUNE_RESULT_INVALID_ITEM;
		}
		return AP_ITEM_CONVERT_RUNE_RESULT_SUCCESS;
	}
	if (runeattachment->anti_type_number) {
		uint32_t i;
		for (i = 0; i < attachment->convert_count; i++) {
			struct ap_item_convert_socket_attr * socket = 
				&attachment->socket_attr[i];
			struct ap_item_convert_item_template * socketattachment;
			if (socket->is_spirit_stone)
				continue;
			socketattachment = ap_item_convert_get_item_template_attachment(mod,
				socket->item_template);
			if (socketattachment->anti_type_number == runeattachment->anti_type_number)
				return AP_ITEM_CONVERT_RUNE_RESULT_IS_ALREADY_ANTI_CONVERT;
		}
	}
	return AP_ITEM_CONVERT_RUNE_RESULT_SUCCESS;
}

enum ap_item_convert_rune_result ap_item_convert_attempt_rune_convert(
	struct ap_item_convert_module * mod,
	struct ap_item * item,
	struct ap_item_convert_item * attachment,
	struct ap_item * rune)
{
	const struct ap_item_convert_item_template * runeattr = 
		ap_item_convert_get_item_template_attachment(mod, rune->temp);
	struct ap_item_convert_add_socket_fail * fail;
	uint32_t rate = ap_random_get(mod->ap_random, 100);
	uint32_t totalrate = 0;
	uint32_t keepcurrent;
	uint32_t initsame;
	uint32_t init;
	uint32_t destroy;
	assert(attachment->convert_count < attachment->socket_count);
	if (rate < runeattr->rune_success_rate) {
		uint32_t index = attachment->convert_count++;
		attachment->socket_attr[index].is_spirit_stone = FALSE;
		attachment->socket_attr[index].tid = rune->tid;
		attachment->socket_attr[index].item_template = rune->temp;
		attachment->update_flags |= AP_ITEM_CONVERT_UPDATE_CONVERT_TID;
		return AP_ITEM_CONVERT_RUNE_RESULT_SUCCESS;
	}
	assert(attachment->convert_count + 1 < COUNT_OF(mod->add_socket_fail));
	fail = &mod->add_socket_fail[attachment->convert_count + 1];
	rate = ap_random_get(mod->ap_random, 100);
	switch (item->temp->equip.kind) {
	case AP_ITEM_EQUIP_KIND_WEAPON:
		keepcurrent = fail->weapon_keep_current;
		initsame = fail->weapon_initialize_same;
		init = fail->weapon_initialize;
		destroy = fail->weapon_destroy;
		break;
	case AP_ITEM_EQUIP_KIND_ARMOR:
		keepcurrent = fail->armor_keep_current;
		initsame = fail->armor_initialize_same;
		init = fail->armor_initialize;
		destroy = fail->armor_destroy;
		break;
	default:
		keepcurrent = fail->etc_keep_current;
		initsame = fail->etc_initialize_same;
		init = fail->etc_initialize;
		destroy = fail->etc_destroy;
		break;
	}
	totalrate = keepcurrent;
	if (rate < totalrate)
		return AP_ITEM_CONVERT_RUNE_RESULT_FAILED;
	totalrate += initsame;
	if (rate < totalrate) {
		ap_item_convert_init_socket(mod, item, attachment);
		attachment->update_flags |= AP_ITEM_CONVERT_UPDATE_CONVERT_TID;
		return AP_ITEM_CONVERT_RUNE_RESULT_FAILED_AND_INIT_SAME;
	}
	totalrate += init;
	if (rate < totalrate) {
		attachment->physical_convert_level = 0;
		attachment->update_flags |= AP_ITEM_CONVERT_UPDATE_PHYSICAL_CONVERT_LEVEL;
		ap_item_convert_init_socket(mod, item, attachment);
		return AP_ITEM_CONVERT_RUNE_RESULT_FAILED_AND_INIT;
	}
	totalrate += destroy;
	if (rate < totalrate)
		return AP_ITEM_CONVERT_RUNE_RESULT_FAILED_AND_DESTROY;
	/* Unreachable! */
	assert(0);
	return AP_ITEM_CONVERT_RUNE_RESULT_FAILED;
}

boolean ap_item_convert_on_receive(
	struct ap_item_convert_module * mod,
	const void * data,
	uint16_t length,
	void * user_data)
{
	struct ap_item_convert_cb_receive cb = { 0 };
	if (!au_packet_get_field(&mod->packet, TRUE, data, length,
			&cb.type, /* packet type */ 
			NULL, /* character id */
			&cb.item_id, /* item id */
			&cb.convert_id, /* catalyst item id */
			NULL, /* action result */
			NULL, /* # of physical convert */
			NULL, /* # of socket */
			NULL, /* # of converted socket */
			NULL, /* add convert item tid */
			NULL, /* converted item tid list */
			NULL)) {
		return FALSE;
	}
	cb.user_data = user_data;
	return ap_module_enum_callback(mod, AP_ITEM_CONVERT_CB_RECEIVE, &cb);
}

void ap_item_convert_make_add_packet(
	struct ap_item_convert_module * mod,
	struct ap_item * item)
{
	enum ap_item_convert_packet_type type = 
		AP_ITEM_CONVERT_PACKET_TYPE_ADD;
	uint16_t length = 0;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	struct ap_item_convert_item * iconv = 
		ap_item_convert_get_item(mod, item);
	void * tidpacket = NULL;
	if (iconv->convert_count) {
		tidpacket = ap_packet_get_temp_buffer(mod->ap_packet);
		maketidpacket(mod, tidpacket, iconv);
	}
	assert(item->character_id != AP_INVALID_CID);
	au_packet_make_packet(&mod->packet, 
		buffer, TRUE, &length,
		AP_ITEM_CONVERT_PACKET_TYPE, 
		&type, /* packet type */
		&item->character_id, /* character id */
		&item->id, /* item id */
		NULL, /* catalyst item id */
		NULL, /* action result */
		&iconv->physical_convert_level, /* # of physical convert */
		&iconv->socket_count, /* # of socket */
		&iconv->convert_count, /* # of converted socket */
		NULL, /* add convert item tid */
		tidpacket); /* converted item tid list */
	ap_packet_set_length(mod->ap_packet, length);
	if (tidpacket)
		ap_packet_reset_temp_buffers(mod->ap_packet);
}

void ap_item_convert_make_add_packet_buffer(
	struct ap_item_convert_module * mod,
	struct ap_item * item,
	void * buffer,
	uint16_t * length)
{
	enum ap_item_convert_packet_type type = 
		AP_ITEM_CONVERT_PACKET_TYPE_ADD;
	struct ap_item_convert_item * iconv = 
		ap_item_convert_get_item(mod, item);
	void * tidpacket = NULL;
	if (iconv->convert_count) {
		tidpacket = ap_packet_get_temp_buffer(mod->ap_packet);
		maketidpacket(mod, tidpacket, iconv);
	}
	assert(item->character_id != AP_INVALID_CID);
	au_packet_make_packet(&mod->packet, 
		buffer, TRUE, length,
		AP_ITEM_CONVERT_PACKET_TYPE, 
		&type, /* packet type */
		&item->character_id, /* character id */
		&item->id, /* item id */
		NULL, /* catalyst item id */
		NULL, /* action result */
		&iconv->physical_convert_level, /* # of physical convert */
		&iconv->socket_count, /* # of socket */
		&iconv->convert_count, /* # of converted socket */
		NULL, /* add convert item tid */
		tidpacket); /* converted item tid list */
	if (tidpacket)
		ap_packet_pop_temp_buffers(mod->ap_packet, 1);
}

void ap_item_convert_make_response_physical_convert_packet(
	struct ap_item_convert_module * mod,
	struct ap_item * item,
	enum ap_item_convert_result result)
{
	enum ap_item_convert_packet_type type = 
		AP_ITEM_CONVERT_PACKET_TYPE_RESPONSE_PHYSICAL_CONVERT;
	uint16_t length = 0;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	struct ap_item_convert_item * convert = ap_item_convert_get_item(mod, item);
	assert(item->character_id != AP_INVALID_CID);
	au_packet_make_packet(&mod->packet, 
		buffer, TRUE, &length,
		AP_ITEM_CONVERT_PACKET_TYPE, 
		&type, /* packet type */
		&item->character_id, /* character id */
		&item->id, /* item id */
		NULL, /* catalyst item id */
		&result, /* action result */
		&convert->physical_convert_level, /* # of physical convert */
		&convert->socket_count, /* # of socket */
		&convert->convert_count, /* # of converted socket */
		NULL, /* add convert item tid */
		NULL); /* converted item tid list */
	ap_packet_set_length(mod->ap_packet, length);
}

void ap_item_convert_make_response_spirit_stone_packet(
	struct ap_item_convert_module * mod,
	struct ap_item * item,
	enum ap_item_convert_spirit_stone_result result,
	uint32_t add_spirit_stone_tid)
{
	enum ap_item_convert_packet_type type = 
		AP_ITEM_CONVERT_PACKET_TYPE_RESPONSE_SPIRITSTONE_CONVERT;
	uint16_t length = 0;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	struct ap_item_convert_item * convert = ap_item_convert_get_item(mod, item);
	assert(item->character_id != AP_INVALID_CID);
	au_packet_make_packet(&mod->packet, 
		buffer, TRUE, &length,
		AP_ITEM_CONVERT_PACKET_TYPE, 
		&type, /* packet type */
		&item->character_id, /* character id */
		&item->id, /* item id */
		NULL, /* catalyst item id */
		&result, /* action result */
		&convert->physical_convert_level, /* # of physical convert */
		&convert->socket_count, /* # of socket */
		&convert->convert_count, /* # of converted socket */
		&add_spirit_stone_tid, /* add convert item tid */
		NULL); /* converted item tid list */
	ap_packet_set_length(mod->ap_packet, length);
}

void ap_item_convert_make_response_rune_convert_packet(
	struct ap_item_convert_module * mod,
	struct ap_item * item,
	enum ap_item_convert_rune_result result,
	uint32_t add_convert_item_tid)
{
	enum ap_item_convert_packet_type type = 
		AP_ITEM_CONVERT_PACKET_TYPE_RESPONSE_RUNE_CONVERT;
	uint16_t length = 0;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	struct ap_item_convert_item * convert = ap_item_convert_get_item(mod, item);
	assert(item->character_id != AP_INVALID_CID);
	au_packet_make_packet(&mod->packet, 
		buffer, TRUE, &length,
		AP_ITEM_CONVERT_PACKET_TYPE, 
		&type, /* packet type */
		&item->character_id, /* character id */
		&item->id, /* item id */
		NULL, /* catalyst item id */
		&result, /* action result */
		&convert->physical_convert_level, /* # of physical convert */
		&convert->socket_count, /* # of socket */
		&convert->convert_count, /* # of converted socket */
		&add_convert_item_tid, /* add convert item tid */
		NULL); /* converted item tid list */
	ap_packet_set_length(mod->ap_packet, length);
}

void ap_item_convert_make_response_spirit_stone_check_result_packet(
	struct ap_item_convert_module * mod,
	uint32_t character_id,
	uint32_t item_id,
	uint32_t spirit_stone_id,
	enum ap_item_convert_spirit_stone_result result)
{
	enum ap_item_convert_packet_type type = 
		AP_ITEM_CONVERT_PACKET_TYPE_RESPONSE_SPIRITSTONE_CHECK_RESULT;
	uint16_t length = 0;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	au_packet_make_packet(&mod->packet, 
		buffer, TRUE, &length,
		AP_ITEM_CONVERT_PACKET_TYPE, 
		&type, /* packet type */
		&character_id, /* character id */
		&item_id, /* item id */
		&spirit_stone_id, /* catalyst item id */
		&result, /* action result */
		NULL, /* # of physical convert */
		NULL, /* # of socket */
		NULL, /* # of converted socket */
		NULL, /* add convert item tid */
		NULL); /* converted item tid list */
	ap_packet_set_length(mod->ap_packet, length);
}

void ap_item_convert_make_response_rune_check_result_packet(
	struct ap_item_convert_module * mod,
	uint32_t character_id,
	uint32_t item_id,
	uint32_t convert_id,
	enum ap_item_convert_rune_result result)
{
	enum ap_item_convert_packet_type type = 
		AP_ITEM_CONVERT_PACKET_TYPE_RESPONSE_RUNE_CHECK_RESULT;
	uint16_t length = 0;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	au_packet_make_packet(&mod->packet, 
		buffer, TRUE, &length,
		AP_ITEM_CONVERT_PACKET_TYPE, 
		&type, /* packet type */
		&character_id, /* character id */
		&item_id, /* item id */
		&convert_id, /* catalyst item id */
		&result, /* action result */
		NULL, /* # of physical convert */
		NULL, /* # of socket */
		NULL, /* # of converted socket */
		NULL, /* add convert item tid */
		NULL); /* converted item tid list */
	ap_packet_set_length(mod->ap_packet, length);
}
