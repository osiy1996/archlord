#include "public/ap_ai2.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"
#include "core/vector.h"

#include "public/ap_admin.h"
#include "public/ap_skill.h"
#include "public/ap_tick.h"

#include "utility/au_table.h"

#include "vendor/pcg/pcg_basic.h"

#include <assert.h>
#include <stdlib.h>
#include <time.h>

struct ap_ai2_module {
	struct ap_module_instance instance;
	struct ap_character_module * ap_character;
	struct ap_skill_module * ap_skill;
	struct ap_tick_module * ap_tick;
	size_t character_attachment_offset;
	pcg32_random_t rng;
	struct ap_admin template_admin;
};

static boolean cbcharctor(
	struct ap_ai2_module * mod,
	struct ap_character * character)
{
	struct ap_ai2_character_attachment * attachment = 
		ap_ai2_get_character_attachment(mod, character);
	attachment->aggro_targets = vec_new(sizeof(*attachment->aggro_targets));
	return TRUE;
}

static boolean cbchardtor(
	struct ap_ai2_module * mod,
	struct ap_character * character)
{
	struct ap_ai2_character_attachment * attachment = 
		ap_ai2_get_character_attachment(mod, character);
	vec_free(attachment->aggro_targets);
	return TRUE;
}

static boolean onregister(
	struct ap_ai2_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_character, AP_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_skill, AP_SKILL_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_tick, AP_TICK_MODULE_NAME);
	mod->character_attachment_offset = ap_character_attach_data(mod->ap_character,
		AP_CHARACTER_MDI_CHAR, sizeof(struct ap_ai2_character_attachment),
		mod, cbcharctor, cbchardtor);
	return TRUE;
}

static void onshutdown(struct ap_ai2_module * mod)
{
	ap_admin_destroy(&mod->template_admin);
}

struct ap_ai2_module * ap_ai2_create_module()
{
	struct ap_ai2_module * mod = ap_module_instance_new(AP_AI2_MODULE_NAME, 
		sizeof(*mod), onregister, NULL, NULL, onshutdown);
	pcg32_srandom_r(&mod->rng, (uint64_t)time(NULL), (uint64_t)time);
	ap_admin_init(&mod->template_admin, sizeof(struct ap_ai2_template), 128);
	return mod;
}

size_t ap_ai2_attach_data(
	struct ap_ai2_module * mod,
	enum ap_ai2_module_data_index data_index,
	size_t data_size,
	ap_module_t callback_module,
	ap_module_default_t constructor,
	ap_module_default_t destructor)
{
	return ap_module_attach_data(mod, data_index, data_size, 
		callback_module, constructor, destructor);
}

enum data_column_id {
	DCID_TID,
	DCID_NAME,
	DCID_USENORMALATTACK,
	DCID_HATERACE,
	DCID_HATERACEPERCENT,
	DCID_HATECLASS,
	DCID_HATECLASSPERCENT,
	DCID_FOLLOWERTID,
	DCID_FOLLOWERNUM,
	DCID_MAX_CUMULATIVEFOLLOWER,
	DCID_MIN_UPKEEPFOLLOWER,
	DCID_SKILL1,
	DCID_SKILLCONDITION,
	DCID_SKILLCOUNT,
	DCID_SKILL2,
	DCID_SKILL3,
	DCID_SKILL4,
	DCID_SKILL5,
	DCID_ITEM1,
	DCID_ITEMUSEHPLIMIT,
	DCID_ITEMUSENUMBER,
	DCID_ITEMUSEINTERVAL,
	DCID_GUARD,
	DCID_GUARDOBJECT,
	DCID_PREEMPTIVETYPE,
	DCID_PREEMPTIVECONDITION1,
	DCID_PREEMPTIVECONDITION2,
	DCID_PREEMPTIVECONDITION3,
	DCID_PREEMPTIVERANGE,
	DCID_IMMUNITY,
	DCID_IMMUNPERCENT,
	DCID_ATTRIBUTE,
	DCID_SUMMONTID,
	DCID_SUMMONMAXCOUNT,
	DCID_SEARCHENEMY,
	DCID_STOPFIGHT,
	DCID_ESCAPE,
	DCID_ESCAPTEHP,
	DCID_ESCAPETIME,
	DCID_ESCAPENUMBER,
	DCID_LINKMONSTERTID,
	DCID_LINKMONSTERSIGHT,
	DCID_HIDEMONSTER,
	DCID_HIDEMONSTERHP,
	DCID_HIDEMONSTERTIME,
	DCID_MAINTENANCERANGE,
	DCID_HELPER,
	DCID_LOOTITEM,
	DCID_LOOTITEMTIME,
	DCID_RANDOMTARGET,
	DCID_PINCHMONSTER,
	DCID_VISIBILITY,
};

boolean ap_ai2_read_data_table(
	struct ap_ai2_module * mod,
	const char * file_path, 
	boolean decrypt)
{
	struct au_table * table = au_table_open(file_path, decrypt);
	boolean r = TRUE;
	uint32_t count = 0;
	if (!table) {
		ERROR("Failed to open file (%s).", file_path);
		return FALSE;
	}
	r &= au_table_set_column(table, "TID", DCID_TID);
	r &= au_table_set_column(table, "NAME", DCID_NAME);
	r &= au_table_set_column(table, "UseNormalAttack", DCID_USENORMALATTACK);
	r &= au_table_set_column(table, "HateRace", DCID_HATERACE);
	r &= au_table_set_column(table, "HateRacePercent", DCID_HATERACEPERCENT);
	r &= au_table_set_column(table, "HateClass", DCID_HATECLASS);
	r &= au_table_set_column(table, "HateClassPercent", DCID_HATECLASSPERCENT);
	r &= au_table_set_column(table, "FollowerTID", DCID_FOLLOWERTID);
	r &= au_table_set_column(table, "FollowerNUM", DCID_FOLLOWERNUM);
	r &= au_table_set_column(table, "Max_CumulativeFollower", DCID_MAX_CUMULATIVEFOLLOWER);
	r &= au_table_set_column(table, "Min_UpkeepFollower", DCID_MIN_UPKEEPFOLLOWER);
	r &= au_table_set_column(table, "Skill1", DCID_SKILL1);
	r &= au_table_set_column(table, "SkillCondition", DCID_SKILLCONDITION);
	r &= au_table_set_column(table, "SkillCount", DCID_SKILLCOUNT);
	r &= au_table_set_column(table, "Skill2", DCID_SKILL2);
	r &= au_table_set_column(table, "Skill3", DCID_SKILL3);
	r &= au_table_set_column(table, "Skill4", DCID_SKILL4);
	r &= au_table_set_column(table, "Skill5", DCID_SKILL5);
	r &= au_table_set_column(table, "Item1", DCID_ITEM1);
	r &= au_table_set_column(table, "ItemUseHPLimit", DCID_ITEMUSEHPLIMIT);
	r &= au_table_set_column(table, "ItemUseNumber", DCID_ITEMUSENUMBER);
	r &= au_table_set_column(table, "ItemUseInterval", DCID_ITEMUSEINTERVAL);
	r &= au_table_set_column(table, "Guard", DCID_GUARD);
	r &= au_table_set_column(table, "GuardObject", DCID_GUARDOBJECT);
	r &= au_table_set_column(table, "PreemptiveType", DCID_PREEMPTIVETYPE);
	r &= au_table_set_column(table, "PreemptiveCondition1", DCID_PREEMPTIVECONDITION1);
	r &= au_table_set_column(table, "PreemptiveCondition2", DCID_PREEMPTIVECONDITION2);
	r &= au_table_set_column(table, "PreemptiveCondition3", DCID_PREEMPTIVECONDITION3);
	r &= au_table_set_column(table, "PreemptiveRange", DCID_PREEMPTIVERANGE);
	r &= au_table_set_column(table, "Immunity", DCID_IMMUNITY);
	r &= au_table_set_column(table, "ImmunPercent", DCID_IMMUNPERCENT);
	r &= au_table_set_column(table, "Attribute", DCID_ATTRIBUTE);
	r &= au_table_set_column(table, "SummonTID", DCID_SUMMONTID);
	r &= au_table_set_column(table, "SummonMaxCount", DCID_SUMMONMAXCOUNT);
	r &= au_table_set_column(table, "SearchEnemy", DCID_SEARCHENEMY);
	r &= au_table_set_column(table, "StopFight", DCID_STOPFIGHT);
	r &= au_table_set_column(table, "Escape", DCID_ESCAPE);
	r &= au_table_set_column(table, "EscapteHP", DCID_ESCAPTEHP);
	r &= au_table_set_column(table, "EscapeTime", DCID_ESCAPETIME);
	r &= au_table_set_column(table, "EscapeNumber", DCID_ESCAPENUMBER);
	r &= au_table_set_column(table, "LinkMonsterTID", DCID_LINKMONSTERTID);
	r &= au_table_set_column(table, "LinkMonsterSight", DCID_LINKMONSTERSIGHT);
	r &= au_table_set_column(table, "HideMonster", DCID_HIDEMONSTER);
	r &= au_table_set_column(table, "HideMonsterHP", DCID_HIDEMONSTERHP);
	r &= au_table_set_column(table, "HideMonsterTime", DCID_HIDEMONSTERTIME);
	r &= au_table_set_column(table, "MaintenanceRange", DCID_MAINTENANCERANGE);
	r &= au_table_set_column(table, "Helper", DCID_HELPER);
	r &= au_table_set_column(table, "LootItem", DCID_LOOTITEM);
	r &= au_table_set_column(table, "LootItemTime", DCID_LOOTITEMTIME);
	r &= au_table_set_column(table, "RandomTarget", DCID_RANDOMTARGET);
	r &= au_table_set_column(table, "PinchMonster", DCID_PINCHMONSTER);
	r &= au_table_set_column(table, "Visibility", DCID_VISIBILITY);
	if (!r) {
		ERROR("Failed to setup columns.");
		au_table_destroy(table);
		return FALSE;
	}
	if (!au_table_read_next_line(table)) {
		ERROR("Invalid file format.");
		au_table_destroy(table);
		return FALSE;
	}
	while (au_table_read_next_line(table)) {
		struct ap_ai2_template * temp = NULL;
		while (au_table_read_next_column(table)) {
			enum data_column_id id = au_table_get_column(table);
			char * value = au_table_get_value(table);
			if (id == DCID_TID) {
				uint32_t tid = strtoul(value, NULL, 10);
				assert(temp == NULL);
				temp = ap_admin_add_object_by_id(&mod->template_admin, tid);
				if (!temp) {
					ERROR("Failed to add AI template (%u).", tid);
					break;
				}
				temp->tid = tid;
				count++;
			}
			if (!temp) {
				assert(0);
				continue;
			}
			switch (id) {
			case DCID_NAME:
				break;
			case DCID_USENORMALATTACK:
				temp->use_normal_attack = strtol(value, NULL, 10) != 0;
				break;
			case DCID_HATERACE:
				break;
			case DCID_HATERACEPERCENT:
				break;
			case DCID_HATECLASS:
				break;
			case DCID_HATECLASSPERCENT:
				break;
			case DCID_FOLLOWERTID:
				break;
			case DCID_FOLLOWERNUM:
				break;
			case DCID_MAX_CUMULATIVEFOLLOWER:
				break;
			case DCID_MIN_UPKEEPFOLLOWER:
				break;
			case DCID_SKILL1:
			case DCID_SKILL2:
			case DCID_SKILL3:
			case DCID_SKILL4:
			case DCID_SKILL5: {
				uint32_t tid;
				struct ap_ai2_use_skill * useskill;
				if (temp->use_skill_count >= AP_AI2_MAX_USABLE_SKILL_COUNT) {
					assert(0);
					continue;
				}
				tid = strtoul(value, NULL, 10);
				useskill = &temp->use_skill[temp->use_skill_count];
				useskill->skill_tid = tid;
				useskill->skill_level = 1;
				useskill->skill_template = ap_skill_get_template(mod->ap_skill, tid);
				if (!useskill->skill_template) {
					ERROR("Failed to retrieve skill template (%u).", tid);
					continue;
				}
				temp->use_skill_count++;
				break;
			}
			case DCID_SKILLCONDITION:
				break;
			case DCID_SKILLCOUNT:
				break;
			case DCID_ITEM1:
				break;
			case DCID_ITEMUSEHPLIMIT:
				break;
			case DCID_ITEMUSENUMBER:
				break;
			case DCID_ITEMUSEINTERVAL:
				break;
			case DCID_GUARD:
				break;
			case DCID_GUARDOBJECT:
				break;
			case DCID_PREEMPTIVETYPE:
				break;
			case DCID_PREEMPTIVECONDITION1:
				break;
			case DCID_PREEMPTIVECONDITION2:
				break;
			case DCID_PREEMPTIVECONDITION3:
				break;
			case DCID_PREEMPTIVERANGE:
				break;
			case DCID_IMMUNITY:
				break;
			case DCID_IMMUNPERCENT:
				break;
			case DCID_ATTRIBUTE:
				break;
			case DCID_SUMMONTID:
				break;
			case DCID_SUMMONMAXCOUNT:
				break;
			case DCID_SEARCHENEMY:
				break;
			case DCID_STOPFIGHT:
				break;
			case DCID_ESCAPE:
				break;
			case DCID_ESCAPTEHP:
				break;
			case DCID_ESCAPETIME:
				break;
			case DCID_ESCAPENUMBER:
				break;
			case DCID_LINKMONSTERTID:
				break;
			case DCID_LINKMONSTERSIGHT:
				break;
			case DCID_HIDEMONSTER:
				break;
			case DCID_HIDEMONSTERHP:
				break;
			case DCID_HIDEMONSTERTIME:
				break;
			case DCID_MAINTENANCERANGE:
				break;
			case DCID_HELPER:
				break;
			case DCID_LOOTITEM:
				break;
			case DCID_LOOTITEMTIME:
				break;
			case DCID_RANDOMTARGET:
				break;
			case DCID_PINCHMONSTER:
				break;
			case DCID_VISIBILITY:
				break;
			}
		}
	}
	au_table_destroy(table);
	INFO("Loaded %u AI templates.", count);
	return TRUE;
}

struct ap_ai2_character_attachment * ap_ai2_get_character_attachment(
	struct ap_ai2_module * mod,
	struct ap_character * character)
{
	return ap_module_get_attached_data(character, mod->character_attachment_offset);
}

void ap_ai2_add_aggro_damage(
	struct ap_ai2_module * mod, 
	struct ap_character * character,
	uint32_t aggro_character_id,
	uint32_t damage)
{
	uint32_t i;
	uint32_t count;
	struct ap_ai2_character_attachment * attachment;
	struct ap_ai2_aggro_target * aggro;
	if (ap_character_is_pc(character))
		return;
	attachment = ap_ai2_get_character_attachment(mod, character);
	count = vec_count(attachment->aggro_targets);
	for (i = 0; i < count; i++) {
		aggro = &attachment->aggro_targets[i];
		if (aggro->character_id == aggro_character_id) {
			aggro->damage += damage;
			aggro->end_aggro_tick = ap_tick_get(mod->ap_tick) + 30000;
			return;
		}
	}
	aggro = vec_add_empty(&attachment->aggro_targets);
	aggro->character_id = aggro_character_id;
	aggro->damage = damage;
	aggro->begin_aggro_tick = ap_tick_get(mod->ap_tick);
	aggro->end_aggro_tick = aggro->begin_aggro_tick + 30000;
}
