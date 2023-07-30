#include "public/ap_item.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"
#include "core/vector.h"

#include "public/ap_admin.h"
#include "public/ap_character.h"
#include "public/ap_define.h"
#include "public/ap_module.h"
#include "public/ap_packet.h"
#include "public/ap_skill.h"
#include "public/ap_tick.h"

#include "utility/au_packet.h"
#include "utility/au_table.h"

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>

enum lottery_data_column_id {
	LOTTERY_DCID_LOTTERYBOXTID,
	LOTTERY_DCID_LOTTERYBOXNAME,
	LOTTERY_DCID_POTITEMTID,
	LOTTERY_DCID_POTITEMNAME,
	LOTTERY_DCID_MINSTACK,
	LOTTERY_DCID_MAXSTACK,
	LOTTERY_DCID_RATE,
};

#define LINK_ID_CAP 600

struct ap_item_module {
	struct ap_module_instance instance;
	struct ap_character_module * ap_character;
	struct ap_factors_module * ap_factors;
	struct ap_packet_module * ap_packet;
	struct ap_skill_module * ap_skill;
	struct ap_tick_module * ap_tick;
	size_t character_item_offset;
	size_t skill_buff_offset;
	struct au_packet packet;
	struct au_packet packet_field;
	struct au_packet packet_inventory;
	struct au_packet packet_bank;
	struct au_packet packet_equip;
	struct au_packet packet_convert;
	struct au_packet packet_restrict;
	struct au_packet packet_ego;
	struct au_packet packet_quest;
	struct au_packet packet_option;
	struct au_packet packet_skill_plus;
	struct au_packet packet_cash;
	struct au_packet packet_extra;
	struct au_packet packet_auto_pick;
	struct ap_admin template_admin;
	struct ap_admin option_template_admin;
	struct ap_item_option_link_pool option_link_pools[LINK_ID_CAP];
	uint32_t link_option_pool_count;
	uint32_t money_tid;
	uint32_t arrow_tid;
	uint32_t bolt_tid;
	uint32_t human_skull_tid;
	uint32_t orc_skull_tid;
	uint32_t skull_tid[AP_ITEM_MAX_SKULL_LEVEL];
	uint32_t catalyst_tid;
	uint32_t lucky_scroll_tid;
	uint32_t reverse_orb_tid;
	uint32_t chatting_emphasis_tid;
};

static void * makeoptionpacket(
	struct ap_item_module * mod,
	const struct ap_item * item)
{
	void * buffer = NULL;
	switch (item->option_count) {
	case 1:
		buffer = ap_packet_get_temp_buffer(mod->ap_packet);
		au_packet_make_packet(&mod->packet_option, 
			buffer, FALSE, NULL, 0, 
			&item->option_tid[0],
			NULL,
			NULL,
			NULL,
			NULL);
		break;
	case 2:
		buffer = ap_packet_get_temp_buffer(mod->ap_packet);
		au_packet_make_packet(&mod->packet_option, 
			buffer, FALSE, NULL, 0, 
			&item->option_tid[0],
			&item->option_tid[1],
			NULL,
			NULL,
			NULL);
		break;
	case 3:
		buffer = ap_packet_get_temp_buffer(mod->ap_packet);
		au_packet_make_packet(&mod->packet_option, 
			buffer, FALSE, NULL, 0, 
			&item->option_tid[0],
			&item->option_tid[1],
			&item->option_tid[2],
			NULL,
			NULL);
		break;
	case 4:
		buffer = ap_packet_get_temp_buffer(mod->ap_packet);
		au_packet_make_packet(&mod->packet_option, 
			buffer, FALSE, NULL, 0, 
			&item->option_tid[0],
			&item->option_tid[1],
			&item->option_tid[2],
			&item->option_tid[3],
			NULL);
		break;
	case 5:
		buffer = ap_packet_get_temp_buffer(mod->ap_packet);
		au_packet_make_packet(&mod->packet_option, 
			buffer, FALSE, NULL, 0, 
			&item->option_tid[0],
			&item->option_tid[1],
			&item->option_tid[2],
			&item->option_tid[3],
			&item->option_tid[4]);
		break;
	}
	return buffer;
}

static struct ap_item * getprimaryweapon(
	struct ap_item_module * mod, 
	struct ap_character * c)
{
	struct ap_item * weapon = ap_item_get_equipment(mod, c,
		AP_ITEM_PART_HAND_RIGHT);
	if (weapon && 
		!(weapon->equip_flags & AP_ITEM_EQUIP_WITH_NO_STATS) && 
		weapon->temp->equip.kind == AP_ITEM_EQUIP_KIND_WEAPON) {
		return weapon;
	}
	weapon = ap_item_get_equipment(mod, c, AP_ITEM_PART_HAND_LEFT);
	if (weapon && 
		!(weapon->equip_flags & AP_ITEM_EQUIP_WITH_NO_STATS) && 
		weapon->temp->equip.kind == AP_ITEM_EQUIP_KIND_WEAPON &&
		weapon->temp->equip.weapon_type == AP_ITEM_EQUIP_WEAPON_TYPE_TWO_HAND_BOW) {
		return weapon;
	}
	return NULL;
}

static void adjustfactor(struct ap_item_module * mod, struct ap_character * c)
{
	struct ap_item * mount = ap_item_get_equipment(mod, c, AP_ITEM_PART_RIDE);
	struct ap_item * mountweap = ap_item_get_equipment(mod, c, AP_ITEM_PART_LANCER);
	struct ap_item * primaryweap = getprimaryweapon(mod, c);
	struct ap_item_character * ic = ap_item_get_character(mod, c);
	if (mount && !CHECK_BIT(mount->equip_flags, AP_ITEM_EQUIP_WITH_NO_STATS)) {
		/* Character is mounted, movement speed needs to 
		 * be adjusted. */
		ic->factor_adjust_movement = AP_ITEM_CHARACTER_FACTOR_ADJUST_MOUNT;
		ic->mount_movement = mount->temp->run_buff;
		ic->mount_movement_fast = mount->temp->run_buff;
		c->update_flags |= AP_FACTORS_BIT_MOVEMENT_SPEED;
		if (mountweap && !(mountweap->equip_flags & AP_ITEM_EQUIP_WITH_NO_STATS)) {
			/* Character is mounted, attack stats need to 
			 * be adjusted. */
			ic->factor_adjust_attack = 
				AP_ITEM_CHARACTER_FACTOR_ADJUST_COMBAT;
			ic->mount_weapon_min_damage = mountweap->factor.damage.min.physical;
			ic->mount_weapon_max_damage = mountweap->factor.damage.max.physical;
			ic->mount_weapon_attack_speed = mountweap->factor.attack.attack_speed;
			ic->mount_weapon_attack_range = mountweap->factor.attack.attack_range;
			c->update_flags |= 
				AP_FACTORS_BIT_PHYSICAL_DMG |
				AP_FACTORS_BIT_ATTACK_RANGE |
				AP_FACTORS_BIT_ATTACK_SPEED;
		}
		else {
			/* Character is mounted but does not have a 
			 * mount weapon, attack stats need to be 
			 * calculated as if character does not have 
			 * any weapon. */
			ic->factor_adjust_attack = AP_ITEM_CHARACTER_FACTOR_ADJUST_NONE;
			c->update_flags |= 
				AP_FACTORS_BIT_PHYSICAL_DMG |
				AP_FACTORS_BIT_ATTACK_RANGE |
				AP_FACTORS_BIT_ATTACK_SPEED;
		}
	}
	else if (primaryweap && !(primaryweap->equip_flags & AP_ITEM_EQUIP_WITH_NO_STATS)) {
		/* Character is not mounted but has primary weapon
		 * equiped, movement and attack stats need to 
		 * be adjusted. */
		ic->factor_adjust_attack = AP_ITEM_CHARACTER_FACTOR_ADJUST_COMBAT;
		ic->factor_adjust_movement = AP_ITEM_CHARACTER_FACTOR_ADJUST_NONE;
		ic->combat_weapon_min_damage = primaryweap->factor.damage.min.physical;
		ic->combat_weapon_max_damage = primaryweap->factor.damage.max.physical;
		ic->combat_weapon_attack_speed = primaryweap->factor.attack.attack_speed;
		ic->combat_weapon_attack_range = primaryweap->factor.attack.attack_range;
		c->update_flags |= 
			AP_FACTORS_BIT_MOVEMENT_SPEED |
			AP_FACTORS_BIT_PHYSICAL_DMG |
			AP_FACTORS_BIT_ATTACK_RANGE |
			AP_FACTORS_BIT_ATTACK_SPEED;
	}
	else {
		ic->factor_adjust_attack = AP_ITEM_CHARACTER_FACTOR_ADJUST_NONE;
		ic->factor_adjust_movement = AP_ITEM_CHARACTER_FACTOR_ADJUST_NONE;
		c->update_flags |= 
			AP_FACTORS_BIT_MOVEMENT_SPEED |
			AP_FACTORS_BIT_PHYSICAL_DMG |
			AP_FACTORS_BIT_ATTACK_RANGE |
			AP_FACTORS_BIT_ATTACK_SPEED;
	}
}

static void applylinkoptions(
	struct ap_item_module * mod,
	struct ap_character * c,
	struct ap_item * item,
	uint32_t level,
	int modifier)
{
	uint32_t i;
	for (i = 0; i < item->temp->link_count; i++) {
		const struct ap_item_option_link_pool * pool = item->temp->link_pools[i];
		uint32_t j;
		for (j = 0; j < pool->count; j++) {
			const struct ap_item_option_template * option = pool->options[j];
			if (level >= option->level_min && level <= option->level_max) {
				ap_item_apply_option(mod, c, option, modifier);
				break;
			}
		}
	}
}

static void applystats(
	struct ap_item_module * mod,
	struct ap_character * character, 
	struct ap_item * item,
	int modifier)
{
	struct ap_item_cb_apply_stats cb = { 0 };
	uint32_t i;
	if (item->temp->equip.kind == AP_ITEM_EQUIP_KIND_WEAPON ||
		item->temp->equip.part == AP_ITEM_PART_RIDE) {
		adjustfactor(mod, character);
	}
	/** \todo
	 * When a weapon is equiped/unequiped, we need to 
	 * go through passive/active skill effects and 
	 * check weapon requirements. */
	if (item->factor.damage.min.magic) {
		character->factor.damage.min.magic += modifier * item->factor.damage.min.magic;
		character->factor.damage.max.magic += modifier * item->factor.damage.max.magic;
		character->update_flags |= AP_FACTORS_BIT_DAMAGE;
	}
	if (item->factor.damage.min.water) {
		character->factor.damage.min.water += modifier * item->factor.damage.min.water;
		character->factor.damage.max.water += modifier * item->factor.damage.max.water;
		character->update_flags |= AP_FACTORS_BIT_DAMAGE;
	}
	if (item->factor.damage.min.fire) {
		character->factor.damage.min.fire += modifier * item->factor.damage.min.fire;
		character->factor.damage.max.fire += modifier * item->factor.damage.max.fire;
		character->update_flags |= AP_FACTORS_BIT_DAMAGE;
	}
	if (item->factor.damage.min.earth) {
		character->factor.damage.min.earth += modifier * item->factor.damage.min.earth;
		character->factor.damage.max.earth += modifier * item->factor.damage.max.earth;
		character->update_flags |= AP_FACTORS_BIT_DAMAGE;
	}
	if (item->factor.damage.min.air) {
		character->factor.damage.min.air += modifier * item->factor.damage.min.air;
		character->factor.damage.max.air += modifier * item->factor.damage.max.air;
		character->update_flags |= AP_FACTORS_BIT_DAMAGE;
	}
	if (item->factor.damage.min.poison) {
		character->factor.damage.min.poison += modifier * item->factor.damage.min.poison;
		character->factor.damage.max.poison += modifier * item->factor.damage.max.poison;
		character->update_flags |= AP_FACTORS_BIT_DAMAGE;
	}
	if (item->factor.damage.min.lightning) {
		character->factor.damage.min.lightning += modifier * item->factor.damage.min.lightning;
		character->factor.damage.max.lightning += modifier * item->factor.damage.max.lightning;
		character->update_flags |= AP_FACTORS_BIT_DAMAGE;
	}
	if (item->factor.damage.min.ice) {
		character->factor.damage.min.ice += modifier * item->factor.damage.min.ice;
		character->factor.damage.max.ice += modifier * item->factor.damage.max.ice;
		character->update_flags |= AP_FACTORS_BIT_DAMAGE;
	}
	if (item->factor.defense.rate.physical_block) {
		character->factor.defense.rate.physical_block +=
			modifier * item->factor.defense.rate.physical_block;
		character->update_flags |= AP_FACTORS_BIT_DEFENSE;
	}
	if (item->factor.defense.point.physical) {
		character->factor.defense.point.physical +=
			modifier * item->factor.defense.point.physical;
		character->update_flags |= AP_FACTORS_BIT_DEFENSE;
	}
	if (item->factor.defense.point.magic) {
		character->factor.defense.point.magic +=
			modifier * item->factor.defense.point.magic;
		character->update_flags |= AP_FACTORS_BIT_DEFENSE;
	}
	if (item->factor.defense.point.water) {
		character->factor.defense.point.water +=
			modifier * item->factor.defense.point.water;
		character->update_flags |= AP_FACTORS_BIT_DEFENSE;
	}
	if (item->factor.defense.point.fire) {
		character->factor.defense.point.fire +=
			modifier * item->factor.defense.point.fire;
		character->update_flags |= AP_FACTORS_BIT_DEFENSE;
	}
	if (item->factor.defense.point.earth) {
		character->factor.defense.point.earth +=
			modifier * item->factor.defense.point.earth;
		character->update_flags |= AP_FACTORS_BIT_DEFENSE;
	}
	if (item->factor.defense.point.air) {
		character->factor.defense.point.air +=
			modifier * item->factor.defense.point.air;
		character->update_flags |= AP_FACTORS_BIT_DEFENSE;
	}
	if (item->factor.defense.point.poison) {
		character->factor.defense.point.poison +=
			modifier * item->factor.defense.point.poison;
		character->update_flags |= AP_FACTORS_BIT_DEFENSE;
	}
	if (item->factor.defense.point.lightning) {
		character->factor.defense.point.lightning +=
			modifier * item->factor.defense.point.lightning;
		character->update_flags |= AP_FACTORS_BIT_DEFENSE;
	}
	if (item->factor.defense.point.ice) {
		character->factor.defense.point.ice +=
			modifier * item->factor.defense.point.ice;
		character->update_flags |= AP_FACTORS_BIT_DEFENSE;
	}
	if (item->factor.char_status.con) {
		character->factor.char_status.con += modifier * item->factor.char_status.con;
		character->update_flags |= AP_FACTORS_BIT_CHAR_STATUS;
	}
	if (item->factor.char_status.str) {
		character->factor.char_status.str += modifier * item->factor.char_status.str;
		character->update_flags |= AP_FACTORS_BIT_CHAR_STATUS;
	}
	if (item->factor.char_status.intel) {
		character->factor.char_status.intel += modifier * item->factor.char_status.intel;
		character->update_flags |= AP_FACTORS_BIT_CHAR_STATUS;
	}
	if (item->factor.char_status.agi) {
		character->factor.char_status.agi += modifier * item->factor.char_status.agi;
		character->update_flags |= AP_FACTORS_BIT_CHAR_STATUS;
	}
	if (item->factor.char_status.wis) {
		character->factor.char_status.wis += modifier * item->factor.char_status.wis;
		character->update_flags |= AP_FACTORS_BIT_CHAR_STATUS;
	}
	for (i = 0; i < item->option_count; i++)
		ap_item_apply_option(mod, character, item->options[i], modifier);
	applylinkoptions(mod, character, item, item->current_link_level, modifier);
	cb.character = character;
	cb.item = item;
	cb.modifier = modifier;
	ap_module_enum_callback(mod, AP_ITEM_CB_APPLY_STATS, &cb);
}

static void addequipstats(
	struct ap_item_module * mod,
	struct ap_character * c,
	struct ap_item * item)
{
	/** \todo
	 * Add skill plus. */
}

static void removeequipstats(
	struct ap_item_module * mod,
	struct ap_character * c,
	struct ap_item * item)
{
	/** \todo
	 * Remove skill plus. */
}

static boolean charctor(struct ap_item_module * mod, struct ap_character * c)
{
	struct ap_item_character * ci = ap_item_get_character(mod, c);
	ci->inventory = ap_grid_new(AP_ITEM_INVENTORY_LAYER, 
		AP_ITEM_INVENTORY_ROW, AP_ITEM_INVENTORY_COLUMN, 
		AP_GRID_ITEM_TYPE_ITEM);
	ci->equipment = ap_grid_new(AP_ITEM_EQUIP_LAYER, 
		AP_ITEM_EQUIP_ROW, AP_ITEM_EQUIP_COLUMN, 
		AP_GRID_ITEM_TYPE_ITEM);
	ci->cash_inventory = ap_grid_new(AP_ITEM_CASH_INVENTORY_LAYER, 
		AP_ITEM_CASH_INVENTORY_ROW, AP_ITEM_CASH_INVENTORY_COLUMN, 
		AP_GRID_ITEM_TYPE_ITEM);
	ci->sub_inventory = ap_grid_new(AP_ITEM_SUB_LAYER, 
		AP_ITEM_SUB_ROW, AP_ITEM_SUB_COLUMN, 
		AP_GRID_ITEM_TYPE_ITEM);
	ci->bank = ap_grid_new(AP_ITEM_BANK_MAX_LAYER, 
		AP_ITEM_BANK_ROW, AP_ITEM_BANK_COLUMN, 
		AP_GRID_ITEM_TYPE_ITEM);
	ci->recent_inventory_process_tick = ap_tick_get(mod->ap_tick);
	return TRUE;
}

static boolean chardtor(struct ap_item_module * mod, struct ap_character * c)
{
	uint32_t i;
	struct ap_item_character * ci = ap_item_get_character(mod, c);
	struct ap_grid ** grids[] = { 
		&ci->inventory,
		&ci->equipment,
		&ci->cash_inventory,
		&ci->sub_inventory,
		&ci->bank };
	for (i = 0; i < COUNT_OF(grids); i++) {
		struct ap_grid * g = *grids[i];
		uint32_t j;
		for (j = 0; j < g->grid_count; j++) {
			struct ap_item * item = ap_grid_get_object_by_index(g, j);
			if (item)
				ap_item_free(mod, item);
		}
		ap_grid_clear(g);
		ap_grid_free(g);
	}
	return TRUE;
}

static boolean cbtempdtor(struct ap_item_module * mod, struct ap_item_template * temp)
{
	if (temp->type == AP_ITEM_TYPE_USABLE) {
		if (temp->usable.usable_type == AP_ITEM_USABLE_TYPE_LOTTERY_BOX)
			vec_free(temp->usable.lottery_box.items);
	}
	return TRUE;
}

static boolean onregister(
	struct ap_item_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_character, AP_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_factors, AP_FACTORS_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_packet, AP_PACKET_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_skill, AP_SKILL_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_tick, AP_TICK_MODULE_NAME);
	mod->character_item_offset = ap_character_attach_data(mod->ap_character,
		AP_CHARACTER_MDI_CHAR, sizeof(struct ap_item_character),
		mod, charctor, chardtor);
	mod->skill_buff_offset = ap_skill_attach_data(mod->ap_skill,
		AP_SKILL_MDI_BUFF, sizeof(struct ap_item_skill_buff_attachment),
		mod, NULL, NULL);
	return TRUE;
}

static void onshutdown(struct ap_item_module * mod)
{
	size_t index = 0;
	struct ap_item_template ** obj = NULL;
	while (ap_admin_iterate_id(&mod->template_admin, 
			&index, (void **)&obj)) {
		struct ap_item_template * t = *obj;
		ap_module_destroy_module_data(mod, 
			AP_ITEM_MDI_TEMPLATE, t);
	}
	ap_admin_destroy(&mod->template_admin);
}

struct ap_item_module * ap_item_create_module()
{
	struct ap_item_module * mod = ap_module_instance_new(AP_ITEM_MODULE_NAME,
		sizeof(*mod), onregister, NULL, NULL, onshutdown);
	au_packet_init(&mod->packet, sizeof(uint32_t),
		AU_PACKET_TYPE_UINT8, 1, /* Packet Type */ 
		AU_PACKET_TYPE_INT8, 1, /* Status */
		AU_PACKET_TYPE_INT32, 1, /* Item ID */
		AU_PACKET_TYPE_INT32, 1, /* Item Template ID */
		AU_PACKET_TYPE_INT32, 1, /* Item Owner CID */
		AU_PACKET_TYPE_INT32, 1, /* Item Count */
		AU_PACKET_TYPE_PACKET, 1, /* Field */
		AU_PACKET_TYPE_PACKET, 1, /* Inventory */
		AU_PACKET_TYPE_PACKET, 1, /* Bank */
		AU_PACKET_TYPE_PACKET, 1, /* Equip */
		AU_PACKET_TYPE_PACKET, 1, /* Factors */
		AU_PACKET_TYPE_PACKET, 1, /* Percent Factors */
		/* target character id (case usable (scroll) item) */
		AU_PACKET_TYPE_INT32, 1,
		AU_PACKET_TYPE_PACKET, 1, /* Convert packet */
		AU_PACKET_TYPE_PACKET, 1, /* restrict factor packet */
		AU_PACKET_TYPE_PACKET, 1, /* Ego Item packet */
		AU_PACKET_TYPE_PACKET, 1, /* Quest Item packet */
		AU_PACKET_TYPE_INT32, 1, /* skill template id */
		AU_PACKET_TYPE_UINT32, 1, /* reuse time for reverse orb */
		AU_PACKET_TYPE_INT32, 1, /* status flag */
		AU_PACKET_TYPE_PACKET, 1, /* option packet */
		AU_PACKET_TYPE_PACKET, 1, /* skill plus packet */
		AU_PACKET_TYPE_UINT32, 1, /* reuse time for transform */
		AU_PACKET_TYPE_PACKET, 1, /* CashItem adding information */
		AU_PACKET_TYPE_PACKET, 1, /* Extra information */
		AU_PACKET_TYPE_END);
	au_packet_init(&mod->packet_field, sizeof(uint8_t),
		AU_PACKET_TYPE_POS, 1, /* Position */
		AU_PACKET_TYPE_END);
	au_packet_init(&mod->packet_inventory, sizeof(uint8_t),
		AU_PACKET_TYPE_INT16, 1, /* Tab */
		AU_PACKET_TYPE_INT16, 1, /* Row */
		AU_PACKET_TYPE_INT16, 1, /* Column */
		AU_PACKET_TYPE_END);
	au_packet_init(&mod->packet_bank, sizeof(uint8_t),
		AU_PACKET_TYPE_INT16, 1, /* Tab */
		AU_PACKET_TYPE_INT16, 1, /* Row */
		AU_PACKET_TYPE_INT16, 1, /* Column */
		AU_PACKET_TYPE_END);
	au_packet_init(&mod->packet_equip, sizeof(uint8_t),
		AU_PACKET_TYPE_END);
	au_packet_init(&mod->packet_convert, sizeof(uint16_t),
		AU_PACKET_TYPE_INT8, 1, /* convert operation & result */
		AU_PACKET_TYPE_INT32, 1, /* spirit stone id */
		/* WHOLE HISTORY BELOW */
		AU_PACKET_TYPE_INT8, 1, /* total convert level */
		AU_PACKET_TYPE_FLOAT, 1, /* convert constant */
		/* bUseSpiritStone */
		AU_PACKET_TYPE_INT8, AP_ITEM_MAX_CONVERT, 
		/* lSpiritStoneTID */
		AU_PACKET_TYPE_INT32, AP_ITEM_MAX_CONVERT, 
		/* lRuneAttributeType */
		AU_PACKET_TYPE_INT8, AP_ITEM_MAX_CONVERT, 
		/* lRuneAttributeValue */
		AU_PACKET_TYPE_INT8, AP_ITEM_MAX_CONVERT, 
		/* converted spirit stone factor point */
		AU_PACKET_TYPE_PACKET, AP_ITEM_MAX_CONVERT, 
		/* converted spirit stone factor percent */
		AU_PACKET_TYPE_PACKET, AP_ITEM_MAX_CONVERT, 
		/* ADD HISTORY BELOW */
		AU_PACKET_TYPE_INT8, 1, /* bUseSpiritStone */
		AU_PACKET_TYPE_INT32, 1, /* lSpiritStoneTID */
		AU_PACKET_TYPE_INT8, 1, /* lRuneAttributeType */
		AU_PACKET_TYPE_INT8, 1, /* lRuneAttributeValue */
		AU_PACKET_TYPE_PACKET, 1, /* converted spirit stone factor point */
		AU_PACKET_TYPE_PACKET, 1, /* converted spirit stone factor percent */
		AU_PACKET_TYPE_END);
	au_packet_init(&mod->packet_restrict, sizeof(uint8_t),
		AU_PACKET_TYPE_END);
	au_packet_init(&mod->packet_ego, sizeof(uint8_t),
		AU_PACKET_TYPE_INT8, 1, /* ego item operation & result */
		AU_PACKET_TYPE_INT32, 1, /* soul cube id */
		AU_PACKET_TYPE_INT32, 1, /* soul character id */
		AU_PACKET_TYPE_INT32, 1, /* target item id */
		AU_PACKET_TYPE_END);
	au_packet_init(&mod->packet_quest, sizeof(uint8_t),
		AU_PACKET_TYPE_INT16, 1, /* Tab */
		AU_PACKET_TYPE_INT16, 1, /* Row */
		AU_PACKET_TYPE_INT16, 1, /* Column */
		AU_PACKET_TYPE_END);
	au_packet_init(&mod->packet_option, sizeof(uint8_t),
		AU_PACKET_TYPE_UINT16, 1, /* option tid 1 */
		AU_PACKET_TYPE_UINT16, 1, /* option tid 2 */
		AU_PACKET_TYPE_UINT16, 1, /* option tid 3 */
		AU_PACKET_TYPE_UINT16, 1, /* option tid 4 */
		AU_PACKET_TYPE_UINT16, 1, /* option tid 5 */
		AU_PACKET_TYPE_END);
	au_packet_init(&mod->packet_skill_plus, sizeof(uint8_t),
		AU_PACKET_TYPE_UINT16, 1, /* skill tid 1 */
		AU_PACKET_TYPE_UINT16, 1, /* skill tid 2 */
		AU_PACKET_TYPE_UINT16, 1, /* skill tid 3 */
		AU_PACKET_TYPE_END);
	au_packet_init(&mod->packet_cash, sizeof(uint8_t),
		AU_PACKET_TYPE_INT8, 1, /*	inuse */
		AU_PACKET_TYPE_INT64, 1, /*	RemainTime */
		AU_PACKET_TYPE_UINT32, 1, /* ExpireTime */
		AU_PACKET_TYPE_INT32, 1, /*	EnableTrade */
		AU_PACKET_TYPE_INT64, 1, /*	StaminaRemainTime */
		AU_PACKET_TYPE_END);
	au_packet_init(&mod->packet_extra, sizeof(uint8_t),
		AU_PACKET_TYPE_END);
	au_packet_init(&mod->packet_auto_pick, sizeof(uint8_t),
		AU_PACKET_TYPE_UINT16, 1, /* On/Off */
		AU_PACKET_TYPE_END);
	ap_admin_init(&mod->template_admin, 
		sizeof(struct ap_item_template *), 4096);
	ap_admin_init(&mod->option_template_admin, 
		sizeof(struct ap_item_option_template *), 4096);
	ap_module_set_module_data(mod, AP_ITEM_MDI_ITEM,
		sizeof(struct ap_item), NULL, NULL);
	ap_module_set_module_data(mod, AP_ITEM_MDI_TEMPLATE,
		sizeof(struct ap_item_template), NULL, cbtempdtor);
	ap_module_set_module_data(mod, AP_ITEM_MDI_OPTION_TEMPLATE,
		sizeof(struct ap_item_option_template), NULL, NULL);
	return mod;
}

boolean ap_item_read_import_data(
	struct ap_item_module * mod,
	const char * file_path, 
	boolean decrypt)
{
	struct au_table * table = au_table_open(file_path, decrypt);
	boolean r = TRUE;
	size_t index;
	void * object;
	if (!table) {
		ERROR("Failed to open file (%s).", file_path);
		return FALSE;
	}
	r &= au_table_set_column(table,
		"TID", AP_ITEM_DCID_TID);
	r &= au_table_set_column(table,
		"ItemName", AP_ITEM_DCID_ITEMNAME);
	r &= au_table_set_column(table,
		"Level", AP_ITEM_DCID_LEVEL);
	r &= au_table_set_column(table,
		"SuitableLevelMin", AP_ITEM_DCID_SUITABLELEVELMIN);
	r &= au_table_set_column(table,
		"SuitableLevelMax", AP_ITEM_DCID_SUITABLELEVELMAX);
	r &= au_table_set_column(table,
		"Class", AP_ITEM_DCID_CLASS);
	r &= au_table_set_column(table,
		"Race", AP_ITEM_DCID_RACE);
	r &= au_table_set_column(table,
		"Gender", AP_ITEM_DCID_GENDER);
	r &= au_table_set_column(table,
		"Part", AP_ITEM_DCID_PART);
	r &= au_table_set_column(table,
		"Kind", AP_ITEM_DCID_KIND);
	r &= au_table_set_column(table,
		"ItemType", AP_ITEM_DCID_ITEMTYPE);
	r &= au_table_set_column(table,
		"Type", AP_ITEM_DCID_TYPE);
	r &= au_table_set_column(table,
		"SubType", AP_ITEM_DCID_SUBTYPE);
	r &= au_table_set_column(table,
		"ExtraType", AP_ITEM_DCID_EXTRATYPE);
	r &= au_table_set_column(table,
		"BoundType", AP_ITEM_DCID_BOUNDTYPE);
	r &= au_table_set_column(table,
		"EventItem", AP_ITEM_DCID_EVENTITEM);
	r &= au_table_set_column(table,
		"PCBang Only", AP_ITEM_DCID_PCBANG_ONLY);
	r &= au_table_set_column(table,
		"Villain Only", AP_ITEM_DCID_VILLAIN_ONLY);
	r &= au_table_set_column(table,
		"GroupID", AP_ITEM_DCID_GROUPID);
	r &= au_table_set_column(table,
		"DropRate", AP_ITEM_DCID_DROPRATE);
	r &= au_table_set_column(table,
		"DropRank", AP_ITEM_DCID_DROPRANK);
	r &= au_table_set_column(table,
		"font color", AP_ITEM_DCID_FONT_COLOR);
	r &= au_table_set_column(table,
		"Spirit Type", AP_ITEM_DCID_SPIRIT_TYPE);
	r &= au_table_set_column(table,
		"NPC Price", AP_ITEM_DCID_NPC_PRICE);
	r &= au_table_set_column(table,
		"PC Price", AP_ITEM_DCID_PC_PRICE);
	r &= au_table_set_column(table,
		"NPC Charisma Price", AP_ITEM_DCID_NPC_CHARISMA_PRICE);
	r &= au_table_set_column(table,
		"NPC Shrine Coin", AP_ITEM_DCID_NPC_SHRINE_COIN);
	r &= au_table_set_column(table,
		"PC Charisma Price", AP_ITEM_DCID_PC_CHARISMA_PRICE);
	r &= au_table_set_column(table,
		"PC Shrine Coin", AP_ITEM_DCID_PC_SHRINE_COIN);
	r &= au_table_set_column(table,
		"NPC Arena Coin", AP_ITEM_DCID_NPC_ARENA_COIN);
	r &= au_table_set_column(table,
		"Stack", AP_ITEM_DCID_STACK);
	r &= au_table_set_column(table,
		"Hand", AP_ITEM_DCID_HAND);
	r &= au_table_set_column(table,
		"Rank", AP_ITEM_DCID_RANK);
	r &= au_table_set_column(table,
		"DUR", AP_ITEM_DCID_DUR);
	r &= au_table_set_column(table,
		"MinSocket", AP_ITEM_DCID_MINSOCKET);
	r &= au_table_set_column(table,
		"MaxSocket", AP_ITEM_DCID_MAXSOCKET);
	r &= au_table_set_column(table,
		"MinOption", AP_ITEM_DCID_MINOPTION);
	r &= au_table_set_column(table,
		"MaxOption", AP_ITEM_DCID_MAXOPTION);
	r &= au_table_set_column(table,
		"ATK Range", AP_ITEM_DCID_ATK_RANGE);
	r &= au_table_set_column(table,
		"ATK Speed", AP_ITEM_DCID_ATK_SPEED);
	r &= au_table_set_column(table,
		"Physical_Min_Damage", AP_ITEM_DCID_PHYSICAL_MIN_DAMAGE);
	r &= au_table_set_column(table,
		"Physical_Max_Damage", AP_ITEM_DCID_PHYSICAL_MAX_DAMAGE);
	r &= au_table_set_column(table,
		"Magic_Min_Damage", AP_ITEM_DCID_MAGIC_MIN_DAMAGE);
	r &= au_table_set_column(table,
		"Water_Min_Damage", AP_ITEM_DCID_WATER_MIN_DAMAGE);
	r &= au_table_set_column(table,
		"Fire_Min_Damage", AP_ITEM_DCID_FIRE_MIN_DAMAGE);
	r &= au_table_set_column(table,
		"Earth_Min_Damage", AP_ITEM_DCID_EARTH_MIN_DAMAGE);
	r &= au_table_set_column(table,
		"Air_Min_Damage", AP_ITEM_DCID_AIR_MIN_DAMAGE);
	r &= au_table_set_column(table,
		"Poison_Min_Damage", AP_ITEM_DCID_POISON_MIN_DAMAGE);
	r &= au_table_set_column(table,
		"Ice_Min_Damage", AP_ITEM_DCID_ICE_MIN_DAMAGE);
	r &= au_table_set_column(table,
		"Linghtning_Min_Damage", AP_ITEM_DCID_LINGHTNING_MIN_DAMAGE);
	r &= au_table_set_column(table,
		"Magic_Max_Damage", AP_ITEM_DCID_MAGIC_MAX_DAMAGE);
	r &= au_table_set_column(table,
		"Water_Max_Damage", AP_ITEM_DCID_WATER_MAX_DAMAGE);
	r &= au_table_set_column(table,
		"Fire_Max_Damage", AP_ITEM_DCID_FIRE_MAX_DAMAGE);
	r &= au_table_set_column(table,
		"Earth_Max_Damage", AP_ITEM_DCID_EARTH_MAX_DAMAGE);
	r &= au_table_set_column(table,
		"Air_Max_Damage", AP_ITEM_DCID_AIR_MAX_DAMAGE);
	r &= au_table_set_column(table,
		"Poison_Max_Damage", AP_ITEM_DCID_POISON_MAX_DAMAGE);
	r &= au_table_set_column(table,
		"Ice_Max_Damage", AP_ITEM_DCID_ICE_MAX_DAMAGE);
	r &= au_table_set_column(table,
		"Linghtning_Max_Damage", AP_ITEM_DCID_LINGHTNING_MAX_DAMAGE);
	r &= au_table_set_column(table,
		"Physical_Defense_Point", AP_ITEM_DCID_PHYSICAL_DEFENSE_POINT);
	r &= au_table_set_column(table,
		"Physical_ATKRate", AP_ITEM_DCID_PHYSICAL_ATKRATE);
	r &= au_table_set_column(table,
		"Magic_ATKRate ", AP_ITEM_DCID_MAGIC_ATKRATE_);
	r &= au_table_set_column(table,
		"PDR", AP_ITEM_DCID_PDR);
	r &= au_table_set_column(table,
		"BlockRate", AP_ITEM_DCID_BLOCKRATE);
	r &= au_table_set_column(table,
		"Magic_Defense_Point", AP_ITEM_DCID_MAGIC_DEFENSE_POINT);
	r &= au_table_set_column(table,
		"Water_Defense_Point", AP_ITEM_DCID_WATER_DEFENSE_POINT);
	r &= au_table_set_column(table,
		"Fire_Defense_Point", AP_ITEM_DCID_FIRE_DEFENSE_POINT);
	r &= au_table_set_column(table,
		"Earth_Defense_Point", AP_ITEM_DCID_EARTH_DEFENSE_POINT);
	r &= au_table_set_column(table,
		"Air_Defense_Point", AP_ITEM_DCID_AIR_DEFENSE_POINT);
	r &= au_table_set_column(table,
		"Poison_Defense_Point", AP_ITEM_DCID_POISON_DEFENSE_POINT);
	r &= au_table_set_column(table,
		"Ice_Defense_Point", AP_ITEM_DCID_ICE_DEFENSE_POINT);
	r &= au_table_set_column(table,
		"Linghtning_Defense_Point", AP_ITEM_DCID_LINGHTNING_DEFENSE_POINT);
	r &= au_table_set_column(table,
		"Magic_Defense_Rate", AP_ITEM_DCID_MAGIC_DEFENSE_RATE);
	r &= au_table_set_column(table,
		"Water_Defense_Rate", AP_ITEM_DCID_WATER_DEFENSE_RATE);
	r &= au_table_set_column(table,
		"Fire_Defense_Rate", AP_ITEM_DCID_FIRE_DEFENSE_RATE);
	r &= au_table_set_column(table,
		"Earth_Defense_Rate", AP_ITEM_DCID_EARTH_DEFENSE_RATE);
	r &= au_table_set_column(table,
		"Air_Defense_Rate", AP_ITEM_DCID_AIR_DEFENSE_RATE);
	r &= au_table_set_column(table,
		"Poison_Defense_Rate", AP_ITEM_DCID_POISON_DEFENSE_RATE);
	r &= au_table_set_column(table,
		"Ice_Defense_Rate", AP_ITEM_DCID_ICE_DEFENSE_RATE);
	r &= au_table_set_column(table,
		"Linghtning_Defense_Rate", AP_ITEM_DCID_LINGHTNING_DEFENSE_RATE);
	r &= au_table_set_column(table,
		"CON_Bonus_Point", AP_ITEM_DCID_CON_BONUS_POINT);
	r &= au_table_set_column(table,
		"STR_Bonus_Point", AP_ITEM_DCID_STR_BONUS_POINT);
	r &= au_table_set_column(table,
		"DEX_Bonus_Point", AP_ITEM_DCID_DEX_BONUS_POINT);
	r &= au_table_set_column(table,
		"INT_Bonus_Point", AP_ITEM_DCID_INT_BONUS_POINT);
	r &= au_table_set_column(table,
		"Wis_Bonus_Point", AP_ITEM_DCID_WIS_BONUS_POINT);
	r &= au_table_set_column(table,
		"Apply Effect Count", AP_ITEM_DCID_APPLY_EFFECT_COUNT);
	r &= au_table_set_column(table,
		"Apply Effect Time", AP_ITEM_DCID_APPLY_EFFECT_TIME);
	r &= au_table_set_column(table,
		"Use Interval", AP_ITEM_DCID_USE_INTERVAL);
	r &= au_table_set_column(table,
		"HP", AP_ITEM_DCID_HP);
	r &= au_table_set_column(table,
		"SP", AP_ITEM_DCID_SP);
	r &= au_table_set_column(table,
		"MP", AP_ITEM_DCID_MP);
	r &= au_table_set_column(table,
		"Rune Attribute", AP_ITEM_DCID_RUNE_ATTRIBUTE);
	r &= au_table_set_column(table,
		"Use skill id", AP_ITEM_DCID_USE_SKILL_ID);
	r &= au_table_set_column(table,
		"Use skill level", AP_ITEM_DCID_USE_SKILL_LEVEL);
	r &= au_table_set_column(table,
		"Polymorph Id", AP_ITEM_DCID_POLYMORPH_ID);
	r &= au_table_set_column(table,
		"Polymorph Dur", AP_ITEM_DCID_POLYMORPH_DUR);
	r &= au_table_set_column(table,
		"FirstCategory", AP_ITEM_DCID_FIRSTCATEGORY);
	r &= au_table_set_column(table,
		"FirstCategoryName", AP_ITEM_DCID_FIRSTCATEGORYNAME);
	r &= au_table_set_column(table,
		"SecondCategory", AP_ITEM_DCID_SECONDCATEGORY);
	r &= au_table_set_column(table,
		"SecondCategoryName", AP_ITEM_DCID_SECONDCATEGORYNAME);
	r &= au_table_set_column(table,
		"PhysicalRank", AP_ITEM_DCID_PHYSICALRANK);
	r &= au_table_set_column(table,
		"Hp_Buff", AP_ITEM_DCID_HP_BUFF);
	r &= au_table_set_column(table,
		"Mp_Buff", AP_ITEM_DCID_MP_BUFF);
	r &= au_table_set_column(table,
		"Attack_Buff", AP_ITEM_DCID_ATTACK_BUFF);
	r &= au_table_set_column(table,
		"Defense_Buff", AP_ITEM_DCID_DEFENSE_BUFF);
	r &= au_table_set_column(table,
		"Run_Buff", AP_ITEM_DCID_RUN_BUFF);
	r &= au_table_set_column(table,
		"Cash", AP_ITEM_DCID_CASH);
	r &= au_table_set_column(table,
		"Destination", AP_ITEM_DCID_DESTINATION);
	r &= au_table_set_column(table,
		"RemainTime", AP_ITEM_DCID_REMAINTIME);
	r &= au_table_set_column(table,
		"ExpireTime", AP_ITEM_DCID_EXPIRETIME);
	r &= au_table_set_column(table,
		"classify_id", AP_ITEM_DCID_CLASSIFY_ID);
	r &= au_table_set_column(table,
		"CanStopUsingItem", AP_ITEM_DCID_CANSTOPUSINGITEM);
	r &= au_table_set_column(table,
		"CashItemUseType", AP_ITEM_DCID_CASHITEMUSETYPE);
	r &= au_table_set_column(table,
		"EnableOnRide", AP_ITEM_DCID_ENABLEONRIDE);
	r &= au_table_set_column(table,
		"Buyer_Type", AP_ITEM_DCID_BUYER_TYPE);
	r &= au_table_set_column(table,
		"Using_Type", AP_ITEM_DCID_USING_TYPE);
	r &= au_table_set_column(table,
		"OptionTID", AP_ITEM_DCID_OPTIONTID);
	r &= au_table_set_column(table,
		"PotionType2", AP_ITEM_DCID_POTIONTYPE2);
	r &= au_table_set_column(table,
		"Link_id", AP_ITEM_DCID_LINK_ID);
	r &= au_table_set_column(table,
		"Skill_plus", AP_ITEM_DCID_SKILL_PLUS);
	r &= au_table_set_column(table,
		"Gambling", AP_ITEM_DCID_GAMBLING);
	r &= au_table_set_column(table,
		"QuestItem", AP_ITEM_DCID_QUESTITEM);
	r &= au_table_set_column(table,
		"Gacha_Type_Numer", AP_ITEM_DCID_GACHA_TYPE_NUMER);
	r &= au_table_set_column(table,
		"GachaRank", AP_ITEM_DCID_GACHARANK);
	r &= au_table_set_column(table,
		"Gacha_Point_Type", AP_ITEM_DCID_GACHA_POINT_TYPE);
	r &= au_table_set_column(table,
		"StaminaCure", AP_ITEM_DCID_STAMINACURE);
	r &= au_table_set_column(table,
		"RemainPetStamina", AP_ITEM_DCID_REMAINPETSTAMINA);
	r &= au_table_set_column(table,
		"GachaMinLv", AP_ITEM_DCID_GACHAMINLV);
	r &= au_table_set_column(table,
		"GachaMaxLv", AP_ITEM_DCID_GACHAMAXLV);
	r &= au_table_set_column(table,
		"Item_section_num", AP_ITEM_DCID_ITEM_SECTION_NUM);
	r &= au_table_set_column(table,
		"Heroic_Min_Damage", AP_ITEM_DCID_HEROIC_MIN_DAMAGE);
	r &= au_table_set_column(table,
		"Heroic_Max_Damage", AP_ITEM_DCID_HEROIC_MAX_DAMAGE);
	r &= au_table_set_column(table,
		"Heroic_Defense_Point", AP_ITEM_DCID_HEROIC_DEFENSE_POINT);
	if (!r) {
		ERROR("Failed to setup columns.");
		au_table_destroy(table);
		return FALSE;
	}
	while (au_table_read_next_line(table)) {
		struct ap_item_template * temp = NULL;
		while (au_table_read_next_column(table)) {
			enum ap_item_data_column_id id = au_table_get_column(table);
			char * value = au_table_get_value(table);
			if (id == AP_ITEM_DCID_TID) {
				uint32_t tid = strtoul(value, NULL, 10);
				struct ap_item_template ** t = 
					ap_admin_add_object_by_id(&mod->template_admin,
						tid);
				if (!t) {
					ERROR("Multiple item templates with same id (%u).",
						tid);
					au_table_destroy(table);
					return FALSE;
				}
				temp = ap_module_create_module_data(mod, 
					AP_ITEM_MDI_TEMPLATE);
				temp->tid = tid;
				*t = temp;
				continue;
			}
			if (!temp) {
				assert(0);
				continue;
			}
			switch (id) {
			case AP_ITEM_DCID_ITEMNAME:
				strlcpy(temp->name, value, sizeof(temp->name));
				break;
			case AP_ITEM_DCID_LEVEL: {
				char * token = strtok(value, ";");
				if (token) {
					ap_factors_set_value(&temp->factor_restrict,
						AP_FACTORS_TYPE_CHAR_STATUS,
						AP_FACTORS_CHAR_STATUS_TYPE_LEVEL,
						token);
					token = strtok(NULL, ";");
					if (token) {
						ap_factors_set_value(&temp->factor_restrict,
							AP_FACTORS_TYPE_CHAR_STATUS,
							AP_FACTORS_CHAR_STATUS_TYPE_LIMITEDLEVEL,
							token);
					}
				}
				break;
			}
			case AP_ITEM_DCID_CLASS: {
				char * token = strtok(value, ";");
				uint32_t mask = 0;
				char buf[32];
				while (token) {
					int32_t cs = strtol(token, NULL, 10);
					if (cs > AU_CHARCLASS_TYPE_NONE &&
						cs < AU_CHARCLASS_TYPE_COUNT) {
						mask |= 1u << cs;
					}
					token = strtok(NULL, ";");
				}
				sprintf(buf, "%d", mask);
				ap_factors_set_value(&temp->factor_restrict,
					AP_FACTORS_TYPE_CHAR_TYPE,
					AP_FACTORS_CHAR_TYPE_TYPE_CLASS,
					buf);
				break;
			}
			case AP_ITEM_DCID_RACE: {
				char * token = strtok(value, ";");
				uint32_t mask = 0;
				char buf[32];
				while (token) {
					int32_t race = strtol(token, NULL, 10);
					if (race > AU_RACE_TYPE_NONE &&
						race < AU_RACE_TYPE_COUNT) {
						mask |= 1u << race;
					}
					token = strtok(NULL, ";");
				}
				sprintf(buf, "%d", mask);
				ap_factors_set_value(&temp->factor_restrict,
					AP_FACTORS_TYPE_CHAR_TYPE,
					AP_FACTORS_CHAR_TYPE_TYPE_RACE,
					buf);
				break;
			}
			case AP_ITEM_DCID_GENDER:
				ap_factors_set_value(&temp->factor_restrict,
					AP_FACTORS_TYPE_CHAR_TYPE,
					AP_FACTORS_CHAR_TYPE_TYPE_GENDER,
					value);
				break;
			case AP_ITEM_DCID_PART:
				assert(temp->type == AP_ITEM_TYPE_EQUIP);
				temp->equip.part = strtol(value, NULL, 10);
				break;
			case AP_ITEM_DCID_KIND:
				assert(temp->type == AP_ITEM_TYPE_EQUIP);
				temp->equip.kind = strtol(value, NULL, 10);
				break;
			case AP_ITEM_DCID_ITEMTYPE:
				temp->type = strtol(value, NULL, 10);
				break;
			case AP_ITEM_DCID_TYPE:
				switch (temp->type) {
				case AP_ITEM_TYPE_EQUIP:
					if (temp->equip.kind == AP_ITEM_EQUIP_KIND_WEAPON) {
						temp->equip.weapon_type = 
							strtol(value, NULL, 10);
					}
					break;
				case AP_ITEM_TYPE_USABLE:
					temp->usable.usable_type = 
						strtol(value, NULL, 10);
					switch (temp->usable.usable_type) {
					case AP_ITEM_USABLE_TYPE_CONVERT_CATALYST:
						mod->catalyst_tid = temp->tid;
						break;
					case AP_ITEM_USABLE_TYPE_LOTTERY_BOX:
						temp->usable.lottery_box.items = 
							vec_new(sizeof(struct ap_item_lottery_item));
						break;
					case AP_ITEM_USABLE_TYPE_LUCKY_SCROLL:
						mod->lucky_scroll_tid = temp->tid;
						break;
					}
					break;
				case AP_ITEM_TYPE_OTHER:
					temp->other.other_type = 
						strtol(value, NULL, 10);
					break;
				default:
					WARN("Invalid item type (tid = %u).",
						temp->tid);
					break;
				}
				break;
			case AP_ITEM_DCID_SUBTYPE:
				switch (temp->type) {
				case AP_ITEM_TYPE_EQUIP:
					temp->subtype = strtol(value, NULL, 10);
					break;
				case AP_ITEM_TYPE_USABLE:
					switch (temp->usable.usable_type) {
					case AP_ITEM_USABLE_TYPE_TELEPORT_SCROLL:
						temp->usable.teleport_scroll_type = 
							strtol(value, NULL, 10);
						break;
					case AP_ITEM_USABLE_TYPE_RUNE:
						temp->usable.rune_attribute_type = 
							strtol(value, NULL, 10);
						break;
					case AP_ITEM_USABLE_TYPE_POTION:
						temp->usable.is_percent_potion = TRUE;
						break;
					case AP_ITEM_USABLE_TYPE_SKILL_SCROLL:
						temp->usable.scroll_subtype = 
							strtol(value, NULL, 10);
						break;
					case AP_ITEM_USABLE_TYPE_AREA_CHATTING:
						temp->usable.area_chatting_type = 
							strtol(value, NULL, 10);
						break;
					default:
						temp->subtype = strtol(value, NULL, 10);
						if (temp->usable.usable_type == AP_ITEM_USABLE_TYPE_CHATTING &&
							temp->subtype == AP_ITEM_USABLE_CHATTING_TYPE_EMPHASIS) {
							mod->chatting_emphasis_tid = temp->tid;
						}
						break;
					}
					break;
				case AP_ITEM_TYPE_OTHER:
					/* TODO: Read skull tids. */
					break;
				default:
					WARN("Invalid item type (tid = %u).",
						temp->tid);
					break;
				}
				break;
			case AP_ITEM_DCID_EXTRATYPE:
				temp->extra_type = strtol(value, NULL, 10);
				break;
			case AP_ITEM_DCID_BOUNDTYPE:
				temp->status_flags &= ~AP_ITEM_STATUS_FLAG_BIND_ON_ACQUIRE;
				temp->status_flags &= ~AP_ITEM_STATUS_FLAG_BIND_ON_EQUIP;
				temp->status_flags &= ~AP_ITEM_STATUS_FLAG_BIND_ON_USE;
				switch (strtoul(value, NULL, 10)) {
				case AP_ITEM_BIND_ON_ACQUIRE:
					temp->status_flags |= AP_ITEM_STATUS_FLAG_BIND_ON_ACQUIRE;
					break;
				case AP_ITEM_BIND_ON_EQUIP:
					temp->status_flags |= AP_ITEM_STATUS_FLAG_BIND_ON_EQUIP;
					break;
				case AP_ITEM_BIND_ON_USE:
					temp->status_flags |= AP_ITEM_STATUS_FLAG_BIND_ON_USE;
					break;
				}
				break;
			case AP_ITEM_DCID_EVENTITEM:
				temp->is_event_item = strtol(value, NULL, 10);
				break;
			case AP_ITEM_DCID_PCBANG_ONLY:
				temp->is_use_only_pc_bang = strtol(value, NULL, 10);
				break;
			case AP_ITEM_DCID_VILLAIN_ONLY:
				temp->is_villain_only = strtol(value, NULL, 10);
				break;
			case AP_ITEM_DCID_FONT_COLOR:
				temp->title_font_color = strtol(value, NULL, 10);
				break;
			case AP_ITEM_DCID_SPIRIT_TYPE:
				if (temp->type != AP_ITEM_TYPE_USABLE ||
					temp->usable.usable_type != AP_ITEM_USABLE_TYPE_SPIRIT_STONE) {
					ERROR("Item has spirit stone type despite not being a spirit stone (%u).",
						temp->tid);
					au_table_destroy(table);
					return FALSE;
				}
				temp->usable.spirit_stone_type = 
					strtol(value, NULL, 10);
				break;
			case AP_ITEM_DCID_NPC_PRICE:
				ap_factors_set_value(&temp->factor,
					AP_FACTORS_TYPE_PRICE,
					AP_FACTORS_PRICE_TYPE_NPC_PRICE,
					value);
				break;
			case AP_ITEM_DCID_PC_PRICE:
				ap_factors_set_value(&temp->factor,
					AP_FACTORS_TYPE_PRICE,
					AP_FACTORS_PRICE_TYPE_PC_PRICE,
					value);
				break;
			case AP_ITEM_DCID_NPC_CHARISMA_PRICE:
				temp->npc_arena_coin = strtoul(value, NULL, 10);
				break;
			case AP_ITEM_DCID_NPC_SHRINE_COIN:
				break;
			case AP_ITEM_DCID_PC_CHARISMA_PRICE:
				break;
			case AP_ITEM_DCID_PC_SHRINE_COIN:
				break;
			case AP_ITEM_DCID_NPC_ARENA_COIN:
				break;
			case AP_ITEM_DCID_STACK:
				temp->max_stackable_count = 
					strtol(value, NULL, 10);
				if (temp->max_stackable_count)
					temp->is_stackable = TRUE;
				break;
			case AP_ITEM_DCID_HAND:
				ap_factors_set_value(&temp->factor,
					AP_FACTORS_TYPE_ITEM,
					AP_FACTORS_ITEM_TYPE_HAND,
					value);
				break;
			case AP_ITEM_DCID_RANK:
				ap_factors_set_value(&temp->factor,
					AP_FACTORS_TYPE_ITEM,
					AP_FACTORS_ITEM_TYPE_RANK,
					value);
				break;
			case AP_ITEM_DCID_DUR:
				ap_factors_set_value(&temp->factor,
					AP_FACTORS_TYPE_ITEM,
					AP_FACTORS_ITEM_TYPE_DURABILITY,
					value);
				break;
			case AP_ITEM_DCID_MINSOCKET:
				temp->min_socket_count = strtol(value, NULL, 10);
				break;
			case AP_ITEM_DCID_MAXSOCKET:
				temp->max_socket_count = strtol(value, NULL, 10);
				break;
			case AP_ITEM_DCID_MINOPTION:
				temp->min_option_count = strtol(value, NULL, 10);
				break;
			case AP_ITEM_DCID_MAXOPTION:
				temp->max_option_count = strtol(value, NULL, 10);
				break;
			case AP_ITEM_DCID_ATK_RANGE:
				ap_factors_set_value(&temp->factor,
					AP_FACTORS_TYPE_ATTACK,
					AP_FACTORS_ATTACK_TYPE_ATTACKRANGE,
					value);
				break;
			case AP_ITEM_DCID_ATK_SPEED:
				ap_factors_set_value(&temp->factor,
					AP_FACTORS_TYPE_ATTACK,
					AP_FACTORS_ATTACK_TYPE_SPEED,
					value);
				break;
			case AP_ITEM_DCID_PHYSICAL_MIN_DAMAGE:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MIN,
					AP_FACTORS_ATTRIBUTE_TYPE_PHYSICAL,
					value);
				break;
			case AP_ITEM_DCID_PHYSICAL_MAX_DAMAGE:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MAX,
					AP_FACTORS_ATTRIBUTE_TYPE_PHYSICAL,
					value);
				break;
			case AP_ITEM_DCID_MAGIC_MIN_DAMAGE:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MIN,
					AP_FACTORS_ATTRIBUTE_TYPE_MAGIC,
					value);
				break;
			case AP_ITEM_DCID_WATER_MIN_DAMAGE:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MIN,
					AP_FACTORS_ATTRIBUTE_TYPE_WATER,
					value);
				break;
			case AP_ITEM_DCID_FIRE_MIN_DAMAGE:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MIN,
					AP_FACTORS_ATTRIBUTE_TYPE_FIRE,
					value);
				break;
			case AP_ITEM_DCID_EARTH_MIN_DAMAGE:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MIN,
					AP_FACTORS_ATTRIBUTE_TYPE_EARTH,
					value);
				break;
			case AP_ITEM_DCID_AIR_MIN_DAMAGE:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MIN,
					AP_FACTORS_ATTRIBUTE_TYPE_AIR,
					value);
				break;
			case AP_ITEM_DCID_POISON_MIN_DAMAGE:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MIN,
					AP_FACTORS_ATTRIBUTE_TYPE_POISON,
					value);
				break;
			case AP_ITEM_DCID_ICE_MIN_DAMAGE:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MIN,
					AP_FACTORS_ATTRIBUTE_TYPE_ICE,
					value);
				break;
			case AP_ITEM_DCID_LINGHTNING_MIN_DAMAGE:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MIN,
					AP_FACTORS_ATTRIBUTE_TYPE_LIGHTENING,
					value);
				break;
			case AP_ITEM_DCID_MAGIC_MAX_DAMAGE:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MAX,
					AP_FACTORS_ATTRIBUTE_TYPE_MAGIC,
					value);
				break;
			case AP_ITEM_DCID_WATER_MAX_DAMAGE:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MAX,
					AP_FACTORS_ATTRIBUTE_TYPE_WATER,
					value);
				break;
			case AP_ITEM_DCID_FIRE_MAX_DAMAGE:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MAX,
					AP_FACTORS_ATTRIBUTE_TYPE_FIRE,
					value);
				break;
			case AP_ITEM_DCID_EARTH_MAX_DAMAGE:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MAX,
					AP_FACTORS_ATTRIBUTE_TYPE_EARTH,
					value);
				break;
			case AP_ITEM_DCID_AIR_MAX_DAMAGE:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MAX,
					AP_FACTORS_ATTRIBUTE_TYPE_AIR,
					value);
				break;
			case AP_ITEM_DCID_POISON_MAX_DAMAGE:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MAX,
					AP_FACTORS_ATTRIBUTE_TYPE_POISON,
					value);
				break;
			case AP_ITEM_DCID_ICE_MAX_DAMAGE:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MAX,
					AP_FACTORS_ATTRIBUTE_TYPE_ICE,
					value);
				break;
			case AP_ITEM_DCID_LINGHTNING_MAX_DAMAGE:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MAX,
					AP_FACTORS_ATTRIBUTE_TYPE_LIGHTENING,
					value);
				break;
			case AP_ITEM_DCID_PHYSICAL_DEFENSE_POINT:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DEFENSE,
					AP_FACTORS_DEFENSE_TYPE_DEFENSE_POINT,
					AP_FACTORS_ATTRIBUTE_TYPE_PHYSICAL,
					value);
				break;
			case AP_ITEM_DCID_PHYSICAL_ATKRATE:
				ap_factors_set_value(&temp->factor,
					AP_FACTORS_TYPE_CHAR_POINT_MAX,
					AP_FACTORS_CHAR_POINT_MAX_TYPE_AR,
					value);
				break;
			case AP_ITEM_DCID_MAGIC_ATKRATE_:
				break;
			case AP_ITEM_DCID_PDR:
				ap_factors_set_value(&temp->factor,
					AP_FACTORS_TYPE_CHAR_POINT_MAX,
					AP_FACTORS_CHAR_POINT_MAX_TYPE_DR,
					value);
				break;
			case AP_ITEM_DCID_BLOCKRATE:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DEFENSE,
					AP_FACTORS_DEFENSE_TYPE_DEFENSE_RATE,
					AP_FACTORS_ATTRIBUTE_TYPE_PHYSICAL_BLOCK,
					value);
				break;
			case AP_ITEM_DCID_MAGIC_DEFENSE_POINT:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DEFENSE,
					AP_FACTORS_DEFENSE_TYPE_DEFENSE_POINT,
					AP_FACTORS_ATTRIBUTE_TYPE_MAGIC,
					value);
				break;
			case AP_ITEM_DCID_WATER_DEFENSE_POINT:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DEFENSE,
					AP_FACTORS_DEFENSE_TYPE_DEFENSE_POINT,
					AP_FACTORS_ATTRIBUTE_TYPE_WATER,
					value);
				break;
			case AP_ITEM_DCID_FIRE_DEFENSE_POINT:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DEFENSE,
					AP_FACTORS_DEFENSE_TYPE_DEFENSE_POINT,
					AP_FACTORS_ATTRIBUTE_TYPE_FIRE,
					value);
				break;
			case AP_ITEM_DCID_EARTH_DEFENSE_POINT:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DEFENSE,
					AP_FACTORS_DEFENSE_TYPE_DEFENSE_POINT,
					AP_FACTORS_ATTRIBUTE_TYPE_EARTH,
					value);
				break;
			case AP_ITEM_DCID_AIR_DEFENSE_POINT:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DEFENSE,
					AP_FACTORS_DEFENSE_TYPE_DEFENSE_POINT,
					AP_FACTORS_ATTRIBUTE_TYPE_AIR,
					value);
				break;
			case AP_ITEM_DCID_POISON_DEFENSE_POINT:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DEFENSE,
					AP_FACTORS_DEFENSE_TYPE_DEFENSE_POINT,
					AP_FACTORS_ATTRIBUTE_TYPE_POISON,
					value);
				break;
			case AP_ITEM_DCID_ICE_DEFENSE_POINT:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DEFENSE,
					AP_FACTORS_DEFENSE_TYPE_DEFENSE_POINT,
					AP_FACTORS_ATTRIBUTE_TYPE_ICE,
					value);
				break;
			case AP_ITEM_DCID_LINGHTNING_DEFENSE_POINT:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DEFENSE,
					AP_FACTORS_DEFENSE_TYPE_DEFENSE_POINT,
					AP_FACTORS_ATTRIBUTE_TYPE_LIGHTENING,
					value);
				break;
			case AP_ITEM_DCID_MAGIC_DEFENSE_RATE:
				break;
			case AP_ITEM_DCID_WATER_DEFENSE_RATE:
				break;
			case AP_ITEM_DCID_FIRE_DEFENSE_RATE:
				break;
			case AP_ITEM_DCID_EARTH_DEFENSE_RATE:
				break;
			case AP_ITEM_DCID_AIR_DEFENSE_RATE:
				break;
			case AP_ITEM_DCID_POISON_DEFENSE_RATE:
				break;
			case AP_ITEM_DCID_ICE_DEFENSE_RATE:
				break;
			case AP_ITEM_DCID_LINGHTNING_DEFENSE_RATE:
				break;
			case AP_ITEM_DCID_CON_BONUS_POINT:
				ap_factors_set_value(&temp->factor,
					AP_FACTORS_TYPE_CHAR_STATUS,
					AP_FACTORS_CHAR_STATUS_TYPE_CON,
					value);
				break;
			case AP_ITEM_DCID_STR_BONUS_POINT:
				ap_factors_set_value(&temp->factor,
					AP_FACTORS_TYPE_CHAR_STATUS,
					AP_FACTORS_CHAR_STATUS_TYPE_STR,
					value);
				break;
			case AP_ITEM_DCID_DEX_BONUS_POINT:
				ap_factors_set_value(&temp->factor,
					AP_FACTORS_TYPE_CHAR_STATUS,
					AP_FACTORS_CHAR_STATUS_TYPE_DEX,
					value);
				break;
			case AP_ITEM_DCID_INT_BONUS_POINT:
				ap_factors_set_value(&temp->factor,
					AP_FACTORS_TYPE_CHAR_STATUS,
					AP_FACTORS_CHAR_STATUS_TYPE_INT,
					value);
				break;
			case AP_ITEM_DCID_WIS_BONUS_POINT:
				ap_factors_set_value(&temp->factor,
					AP_FACTORS_TYPE_CHAR_STATUS,
					AP_FACTORS_CHAR_STATUS_TYPE_WIS,
					value);
				break;
			case AP_ITEM_DCID_APPLY_EFFECT_COUNT:
				temp->usable.effect_apply_count = 
					strtol(value, NULL, 10);
				break;
			case AP_ITEM_DCID_APPLY_EFFECT_TIME:
				temp->usable.effect_apply_interval = 
					strtol(value, NULL, 10);
				break;
			case AP_ITEM_DCID_USE_INTERVAL:
				temp->usable.use_interval = 
					strtol(value, NULL, 10);
				break;
			case AP_ITEM_DCID_HP:
				ap_factors_set_value(&temp->factor,
					AP_FACTORS_TYPE_CHAR_POINT,
					AP_FACTORS_CHAR_POINT_TYPE_HP,
					value);
				temp->usable.potion_type = 
					AP_ITEM_USABLE_POTION_TYPE_HP;
				break;
			case AP_ITEM_DCID_SP:
				break;
			case AP_ITEM_DCID_MP:
				ap_factors_set_value(&temp->factor,
					AP_FACTORS_TYPE_CHAR_POINT,
					AP_FACTORS_CHAR_POINT_TYPE_MP,
					value);
				if (temp->usable.potion_type == AP_ITEM_USABLE_POTION_TYPE_HP) {
					temp->usable.potion_type = 
						AP_ITEM_USABLE_POTION_TYPE_BOTH;
				}
				else {
					temp->usable.potion_type = 
						AP_ITEM_USABLE_POTION_TYPE_MP;
				}
				break;
			case AP_ITEM_DCID_RUNE_ATTRIBUTE:
				break;
			case AP_ITEM_DCID_USE_SKILL_ID:
				temp->usable.use_skill_tid = 
					strtol(value, NULL, 10);
				break;
			case AP_ITEM_DCID_USE_SKILL_LEVEL:
				temp->usable.use_skill_level = 
					strtol(value, NULL, 10);
				break;
			case AP_ITEM_DCID_POLYMORPH_ID:
				if (temp->type != AP_ITEM_TYPE_USABLE ||
					temp->usable.usable_type != AP_ITEM_USABLE_TYPE_TRANSFORM) {
					ERROR("Item has polymorph tid despite not being polymorph potion (%u).",
						temp->tid);
					au_table_destroy(table);
					return FALSE;
				}
				temp->usable.transform_tid = 
					strtol(value, NULL, 10);
				break;
			case AP_ITEM_DCID_POLYMORPH_DUR:
				if (temp->type != AP_ITEM_TYPE_USABLE ||
					temp->usable.usable_type != AP_ITEM_USABLE_TYPE_TRANSFORM) {
					ERROR("Item has polymorph duration despite not being polymorph potion (%u).",
						temp->tid);
					au_table_destroy(table);
					return FALSE;
				}
				temp->usable.transform_duration = 
					strtol(value, NULL, 10);
				break;
			case AP_ITEM_DCID_FIRSTCATEGORY:
				temp->first_category = strtol(value, NULL, 10);
				break;
			case AP_ITEM_DCID_FIRSTCATEGORYNAME:
				strlcpy(temp->first_category_name, value,
					sizeof(temp->first_category_name));
				break;
			case AP_ITEM_DCID_SECONDCATEGORY:
				temp->second_category = strtol(value, NULL, 10);
				break;
			case AP_ITEM_DCID_SECONDCATEGORYNAME:
				strlcpy(temp->second_category_name, value,
					sizeof(temp->second_category_name));
				break;
			case AP_ITEM_DCID_PHYSICALRANK:
				ap_factors_set_value(&temp->factor,
					AP_FACTORS_TYPE_ITEM,
					AP_FACTORS_ITEM_TYPE_PHYSICAL_RANK,
					value);
				break;
			case AP_ITEM_DCID_HP_BUFF:
				temp->hp_buff = strtol(value, NULL, 10);
				break;
			case AP_ITEM_DCID_MP_BUFF:
				temp->mp_buff = strtol(value, NULL, 10);
				break;
			case AP_ITEM_DCID_ATTACK_BUFF:
				temp->attack_buff = strtol(value, NULL, 10);
				break;
			case AP_ITEM_DCID_DEFENSE_BUFF:
				temp->defense_buff = strtol(value, NULL, 10);
				break;
			case AP_ITEM_DCID_RUN_BUFF:
				temp->run_buff = strtol(value, NULL, 10);
				break;
			case AP_ITEM_DCID_CASH: {
				int d = strtol(value, NULL, 10);
				if (d >= 100) {
					temp->not_continuous = TRUE;
					d -= 100;
				}
				temp->cash_item_type = d;
				break;
			}
			case AP_ITEM_DCID_DESTINATION:
				if (sscanf(value, "%f;%f;%f", 
						&temp->usable.destination.x,
						&temp->usable.destination.y,
						&temp->usable.destination.z) != 3) {
					ERROR("Invalid usable item destination (%u).",
						temp->tid);
					au_table_destroy(table);
					return FALSE;
				}
				break;
			case AP_ITEM_DCID_REMAINTIME:
				temp->remain_time = 
					(uint64_t)strtol(value, NULL, 10) * 60 * 1000;
				break;
			case AP_ITEM_DCID_EXPIRETIME:
				temp->expire_time = strtol(value, NULL, 10) * 60;
				break;
			case AP_ITEM_DCID_CLASSIFY_ID:
				temp->classify_id = strtol(value, NULL, 10);
				break;
			case AP_ITEM_DCID_CANSTOPUSINGITEM:
				temp->can_stop_using = strtol(value, NULL, 10);
				break;
			case AP_ITEM_DCID_CASHITEMUSETYPE:
				temp->cash_item_use_type = strtol(value, NULL, 10);
				break;
			case AP_ITEM_DCID_ENABLEONRIDE:
				temp->enable_on_ride = strtol(value, NULL, 10);
				break;
			case AP_ITEM_DCID_BUYER_TYPE:
				temp->buyer_type = strtol(value, NULL, 10);
				break;
			case AP_ITEM_DCID_USING_TYPE:
				temp->using_type = strtol(value, NULL, 10);
				break;
			case AP_ITEM_DCID_OPTIONTID: {
				char * token = strtok(value, ";");
				while (token) {
					uint32_t tid = strtol(token, NULL, 10);
					struct ap_item_option_template * option = 
						ap_item_get_option_template(mod, tid);
					if (!option) {
						WARN("Invalid option template id (item = [%u] %s, option_tid = %u).",
							temp->tid, temp->name, tid);
						continue;
					}
					temp->options[temp->option_count] = option;
					temp->option_tid[temp->option_count++] = tid;
					if (temp->option_count == AP_ITEM_OPTION_MAX_COUNT)
						break;
					token = strtok(NULL, ";");
				}
				break;
			}
			case AP_ITEM_DCID_POTIONTYPE2: {
				int32_t d = strtol(value, NULL, 10);
				if (d != 0)
					temp->usable.potion_type2 = d;
				break;
			}
			case AP_ITEM_DCID_LINK_ID: {
				char * token = strtok(value, ";");
				while (token) {
					uint32_t index = temp->link_count++;
					uint32_t linkid = strtoul(token, NULL, 10);
					assert(linkid < LINK_ID_CAP);
					temp->link_pools[index] = &mod->option_link_pools[linkid];
					if (temp->option_count == AP_ITEM_OPTION_MAX_LINK_COUNT)
						break;
					token = strtok(NULL, ";");
				}
				break;
			}
			case AP_ITEM_DCID_SKILL_PLUS: {
				char * token = strtok(value, ";");
				while (token) {
					temp->skill_plus_tid[temp->skill_plus_count++] =
						(uint16_t)strtol(value, NULL, 10);
					if (temp->skill_plus_count == AP_ITEM_MAX_SKILL_PLUS)
						break;
					token = strtok(NULL, ";");
				}
				break;
			}
			case AP_ITEM_DCID_GAMBLING:
				temp->enable_gamble = strtol(value, NULL, 10);
				break;
			case AP_ITEM_DCID_QUESTITEM:
				temp->quest_group = strtol(value, NULL, 10);
				break;
			case AP_ITEM_DCID_GACHA_TYPE_NUMER:
				temp->gacha_type = strtol(value, NULL, 10);
				break;
			case AP_ITEM_DCID_GACHARANK:
				ap_factors_set_value(&temp->factor,
					AP_FACTORS_TYPE_ITEM,
					AP_FACTORS_ITEM_TYPE_GACHA,
					value);
				break;
			case AP_ITEM_DCID_GACHA_POINT_TYPE:
				break;
			case AP_ITEM_DCID_STAMINACURE:
				temp->stamina_cure = 
					(uint64_t)strtol(value, NULL, 10) * 1000;
				break;
			case AP_ITEM_DCID_REMAINPETSTAMINA:
				temp->stamina_remain_time = 
					(uint64_t)strtol(value, NULL, 10) * 1000;
				break;
			case AP_ITEM_DCID_ITEM_SECTION_NUM:
				temp->section_type = strtol(value, NULL, 10);
				break;
			case AP_ITEM_DCID_HEROIC_MIN_DAMAGE:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MIN,
					AP_FACTORS_ATTRIBUTE_TYPE_HEROIC,
					value);
				break;
			case AP_ITEM_DCID_HEROIC_MAX_DAMAGE:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MAX,
					AP_FACTORS_ATTRIBUTE_TYPE_HEROIC,
					value);
				break;
			case AP_ITEM_DCID_HEROIC_DEFENSE_POINT:
				ap_factors_set_attribute(&temp->factor,
					AP_FACTORS_TYPE_DEFENSE,
					AP_FACTORS_DEFENSE_TYPE_DEFENSE_POINT,
					AP_FACTORS_ATTRIBUTE_TYPE_HEROIC,
					value);
				break;
			default: {
				struct ap_item_cb_read_import cb = { 0 };
				cb.temp = temp;
				cb.column_id = id;
				cb.value = value;
				if (!ap_module_enum_callback(mod, AP_ITEM_CB_READ_IMPORT, &cb))
					break;
				ERROR("Invalid column id (%u).", id);
				au_table_destroy(table);
				return FALSE;
			}
			}
		}
	}
	au_table_destroy(table);
	INFO("Loaded %u item templates.", 
		ap_admin_get_object_count(&mod->template_admin));
	index = 0;
	while (ap_admin_iterate_id(&mod->template_admin, &index, &object)) {
		struct ap_item_cb_end_read_import cb = { 0 };
		cb.temp = *(struct ap_item_template **)object;
		if (!ap_module_enum_callback(mod, AP_ITEM_CB_END_READ_IMPORT, &cb)) {
			ERROR("Completion callback failed.");
			return FALSE;
		}
	}
	return TRUE;
}

boolean ap_item_read_option_data(
	struct ap_item_module * mod,
	const char * file_path, 
	boolean decrypt)
{
	struct au_table * table = au_table_open(file_path, decrypt);
	boolean r = TRUE;
	size_t index;
	void * object;
	if (!table) {
		ERROR("Failed to open file (%s).", file_path);
		return FALSE;
	}
	r &= au_table_set_column(table,
		"tid", 
		AP_ITEM_OPTION_DCID_TID);
	r &= au_table_set_column(table,
		"Description", 
		AP_ITEM_OPTION_DCID_DESCRIPTION);
	r &= au_table_set_column(table,
		"Upper_Body", 
		AP_ITEM_OPTION_DCID_UPPER_BODY);
	r &= au_table_set_column(table,
		"Lower_Body", 
		AP_ITEM_OPTION_DCID_LOWER_BODY);
	r &= au_table_set_column(table,
		"Weapon", 
		AP_ITEM_OPTION_DCID_WEAPON);
	r &= au_table_set_column(table,
		"Shield", 
		AP_ITEM_OPTION_DCID_SHIELD);
	r &= au_table_set_column(table,
		"Head", 
		AP_ITEM_OPTION_DCID_HEAD);
	r &= au_table_set_column(table,
		"Ring", 
		AP_ITEM_OPTION_DCID_RING);
	r &= au_table_set_column(table,
		"Necklace", 
		AP_ITEM_OPTION_DCID_NECKLACE);
	r &= au_table_set_column(table,
		"Shoes", 
		AP_ITEM_OPTION_DCID_SHOES);
	r &= au_table_set_column(table,
		"Gloves", 
		AP_ITEM_OPTION_DCID_GLOVES);
	r &= au_table_set_column(table,
		"Refined", 
		AP_ITEM_OPTION_DCID_REFINED);
	r &= au_table_set_column(table,
		"Gacha", 
		AP_ITEM_OPTION_DCID_GACHA);
	r &= au_table_set_column(table,
		"Type", 
		AP_ITEM_OPTION_DCID_TYPE);
	r &= au_table_set_column(table,
		"PointType", 
		AP_ITEM_OPTION_DCID_POINTTYPE);
	r &= au_table_set_column(table,
		"Item_Level_Min", 
		AP_ITEM_OPTION_DCID_ITEM_LEVEL_MIN);
	r &= au_table_set_column(table,
		"Item_Level_Max", 
		AP_ITEM_OPTION_DCID_ITEM_LEVEL_MAX);
	r &= au_table_set_column(table,
		"Level_limit", 
		AP_ITEM_OPTION_DCID_LEVEL_LIMIT);
	r &= au_table_set_column(table,
		"Rank_Min", 
		AP_ITEM_OPTION_DCID_RANK_MIN);
	r &= au_table_set_column(table,
		"Rank_Max", 
		AP_ITEM_OPTION_DCID_RANK_MAX);
	r &= au_table_set_column(table,
		"Probability", 
		AP_ITEM_OPTION_DCID_PROBABILITY);
	r &= au_table_set_column(table,
		"Skill_level", 
		AP_ITEM_OPTION_DCID_SKILL_LEVEL);
	r &= au_table_set_column(table,
		"Skill_id", 
		AP_ITEM_OPTION_DCID_SKILL_ID);
	r &= au_table_set_column(table,
		"ATK_Range_bonus", 
		AP_ITEM_OPTION_DCID_ATK_RANGE_BONUS);
	r &= au_table_set_column(table,
		"ATK_Speed_bonus", 
		AP_ITEM_OPTION_DCID_ATK_SPEED_BONUS);
	r &= au_table_set_column(table,
		"Movement_Run_bonus", 
		AP_ITEM_OPTION_DCID_MOVEMENT_RUN_BONUS);
	r &= au_table_set_column(table,
		"PhysicalDamage", 
		AP_ITEM_OPTION_DCID_PHYSICALDAMAGE);
	r &= au_table_set_column(table,
		"PhysicalAttackResist", 
		AP_ITEM_OPTION_DCID_PHYSICALATTACKRESIST);
	r &= au_table_set_column(table,
		"SkillBlockRate", 
		AP_ITEM_OPTION_DCID_SKILLBLOCKRATE);
	r &= au_table_set_column(table,
		"MagicDamage", 
		AP_ITEM_OPTION_DCID_MAGICDAMAGE);
	r &= au_table_set_column(table,
		"WaterDamage", 
		AP_ITEM_OPTION_DCID_WATERDAMAGE);
	r &= au_table_set_column(table,
		"FireDamage", 
		AP_ITEM_OPTION_DCID_FIREDAMAGE);
	r &= au_table_set_column(table,
		"EarthDamage", 
		AP_ITEM_OPTION_DCID_EARTHDAMAGE);
	r &= au_table_set_column(table,
		"AirDamage", 
		AP_ITEM_OPTION_DCID_AIRDAMAGE);
	r &= au_table_set_column(table,
		"PoisonDamage", 
		AP_ITEM_OPTION_DCID_POISONDAMAGE);
	r &= au_table_set_column(table,
		"IceDamage", 
		AP_ITEM_OPTION_DCID_ICEDAMAGE);
	r &= au_table_set_column(table,
		"LinghtningDamage", 
		AP_ITEM_OPTION_DCID_LINGHTNINGDAMAGE);
	r &= au_table_set_column(table,
		"Fire Res.", 
		AP_ITEM_OPTION_DCID_FIRE_RES);
	r &= au_table_set_column(table,
		"Water Res.", 
		AP_ITEM_OPTION_DCID_WATER_RES);
	r &= au_table_set_column(table,
		"Air Res.", 
		AP_ITEM_OPTION_DCID_AIR_RES);
	r &= au_table_set_column(table,
		"Earth Res.", 
		AP_ITEM_OPTION_DCID_EARTH_RES);
	r &= au_table_set_column(table,
		"Magic Res.", 
		AP_ITEM_OPTION_DCID_MAGIC_RES);
	r &= au_table_set_column(table,
		"Poison Res.", 
		AP_ITEM_OPTION_DCID_POISON_RES);
	r &= au_table_set_column(table,
		"Ice Res.", 
		AP_ITEM_OPTION_DCID_ICE_RES);
	r &= au_table_set_column(table,
		"Thunder Res.", 
		AP_ITEM_OPTION_DCID_THUNDER_RES);
	r &= au_table_set_column(table,
		"Physical_Defense_Point", 
		AP_ITEM_OPTION_DCID_PHYSICAL_DEFENSE_POINT);
	r &= au_table_set_column(table,
		"Physical_ATKRate", 
		AP_ITEM_OPTION_DCID_PHYSICAL_ATKRATE);
	r &= au_table_set_column(table,
		"Magic_ATKRate ", 
		AP_ITEM_OPTION_DCID_MAGIC_ATKRATE_);
	r &= au_table_set_column(table,
		"BlockRate", 
		AP_ITEM_OPTION_DCID_BLOCKRATE);
	r &= au_table_set_column(table,
		"Magic_Defense_Point", 
		AP_ITEM_OPTION_DCID_MAGIC_DEFENSE_POINT);
	r &= au_table_set_column(table,
		"Water_Defense_Point", 
		AP_ITEM_OPTION_DCID_WATER_DEFENSE_POINT);
	r &= au_table_set_column(table,
		"Fire_Defense_Point", 
		AP_ITEM_OPTION_DCID_FIRE_DEFENSE_POINT);
	r &= au_table_set_column(table,
		"Earth_Defense_Point", 
		AP_ITEM_OPTION_DCID_EARTH_DEFENSE_POINT);
	r &= au_table_set_column(table,
		"Air_Defense_Point", 
		AP_ITEM_OPTION_DCID_AIR_DEFENSE_POINT);
	r &= au_table_set_column(table,
		"Poison_Defense_Point", 
		AP_ITEM_OPTION_DCID_POISON_DEFENSE_POINT);
	r &= au_table_set_column(table,
		"Ice_Defense_Point", 
		AP_ITEM_OPTION_DCID_ICE_DEFENSE_POINT);
	r &= au_table_set_column(table,
		"Linghtning_Defense_Point", 
		AP_ITEM_OPTION_DCID_LINGHTNING_DEFENSE_POINT);
	r &= au_table_set_column(table,
		"Hp_Bonus_Point", 
		AP_ITEM_OPTION_DCID_HP_BONUS_POINT);
	r &= au_table_set_column(table,
		"Mp_Bonus_Point", 
		AP_ITEM_OPTION_DCID_MP_BONUS_POINT);
	r &= au_table_set_column(table,
		"Hp_Bonus_Percent", 
		AP_ITEM_OPTION_DCID_HP_BONUS_PERCENT);
	r &= au_table_set_column(table,
		"Mp_Bonus_Percent", 
		AP_ITEM_OPTION_DCID_MP_BONUS_PERCENT);
	r &= au_table_set_column(table,
		"Str_Bonus_Point", 
		AP_ITEM_OPTION_DCID_STR_BONUS_POINT);
	r &= au_table_set_column(table,
		"Dex_Bonus_Point", 
		AP_ITEM_OPTION_DCID_DEX_BONUS_POINT);
	r &= au_table_set_column(table,
		"Int_Bonus_Point", 
		AP_ITEM_OPTION_DCID_INT_BONUS_POINT);
	r &= au_table_set_column(table,
		"Con_Bonus_Point", 
		AP_ITEM_OPTION_DCID_CON_BONUS_POINT);
	r &= au_table_set_column(table,
		"Wis_Bonus_Point", 
		AP_ITEM_OPTION_DCID_WIS_BONUS_POINT);
	r &= au_table_set_column(table,
		"PhysicalRank", 
		AP_ITEM_OPTION_DCID_PHYSICALRANK);
	r &= au_table_set_column(table,
		"skill_type", 
		AP_ITEM_OPTION_DCID_SKILL_TYPE);
	r &= au_table_set_column(table,
		"duration", 
		AP_ITEM_OPTION_DCID_DURATION);
	r &= au_table_set_column(table,
		"skill_rate", 
		AP_ITEM_OPTION_DCID_SKILL_RATE);
	r &= au_table_set_column(table,
		"Critical", 
		AP_ITEM_OPTION_DCID_CRITICAL);
	r &= au_table_set_column(table,
		"stun_time", 
		AP_ITEM_OPTION_DCID_STUN_TIME);
	r &= au_table_set_column(table,
		"damage_convert_hp", 
		AP_ITEM_OPTION_DCID_DAMAGE_CONVERT_HP);
	r &= au_table_set_column(table,
		"regen_hp", 
		AP_ITEM_OPTION_DCID_REGEN_HP);
	r &= au_table_set_column(table,
		"regen_mp", 
		AP_ITEM_OPTION_DCID_REGEN_MP);
	r &= au_table_set_column(table,
		"dot_damage_time", 
		AP_ITEM_OPTION_DCID_DOT_DAMAGE_TIME);
	r &= au_table_set_column(table,
		"skill_cast", 
		AP_ITEM_OPTION_DCID_SKILL_CAST);
	r &= au_table_set_column(table,
		"skill_delay", 
		AP_ITEM_OPTION_DCID_SKILL_DELAY);
	r &= au_table_set_column(table,
		"skill_levelup", 
		AP_ITEM_OPTION_DCID_SKILL_LEVELUP);
	r &= au_table_set_column(table,
		"bonus_exp", 
		AP_ITEM_OPTION_DCID_BONUS_EXP);
	r &= au_table_set_column(table,
		"bonus_money", 
		AP_ITEM_OPTION_DCID_BONUS_MONEY);
	r &= au_table_set_column(table,
		"bonus_drop_rate", 
		AP_ITEM_OPTION_DCID_BONUS_DROP_RATE);
	r &= au_table_set_column(table,
		"evade_rate", 
		AP_ITEM_OPTION_DCID_EVADE_RATE);
	r &= au_table_set_column(table,
		"dodge_rate", 
		AP_ITEM_OPTION_DCID_DODGE_RATE);
	r &= au_table_set_column(table,
		"visible", 
		AP_ITEM_OPTION_DCID_VISIBLE);
	r &= au_table_set_column(table,
		"Level_min", 
		AP_ITEM_OPTION_DCID_LEVEL_MIN);
	r &= au_table_set_column(table,
		"Level_max", 
		AP_ITEM_OPTION_DCID_LEVEL_MAX);
	r &= au_table_set_column(table,
		"Link_id", 
		AP_ITEM_OPTION_DCID_LINK_ID);
	r &= au_table_set_column(table,
		"Heroic_Min_Damage", 
		AP_ITEM_OPTION_DCID_HEROIC_MIN_DAMAGE);
	r &= au_table_set_column(table,
		"Heroic_Max_Damage", 
		AP_ITEM_OPTION_DCID_HEROIC_MAX_DAMAGE);
	r &= au_table_set_column(table,
		"Heroic_Defense_Point", 
		AP_ITEM_OPTION_DCID_HEROIC_DEFENSE_POINT);
	r &= au_table_set_column(table,
		"Ignore_Physical_Defense_Rate", 
		AP_ITEM_OPTION_DCID_IGNORE_PHYSICAL_DEFENSE_RATE);
	r &= au_table_set_column(table,
		"Ignore_Attribute_Defense_Rate", 
		AP_ITEM_OPTION_DCID_IGNORE_ATTRIBUTE_DEFENSE_RATE);
	r &= au_table_set_column(table,
		"Critical_Defense_Rate", 
		AP_ITEM_OPTION_DCID_CRITICAL_DEFENSE_RATE);
	r &= au_table_set_column(table,
		"ReinforcementRate", 
		AP_ITEM_OPTION_DCID_REINFORCEMENTRATE);
	r &= au_table_set_column(table,
		"Ignore_PhysicalAttackResist", 
		AP_ITEM_OPTION_DCID_IGNORE_PHYSICALATTACKRESIST);
	r &= au_table_set_column(table,
		"Ignore_BlockRate", 
		AP_ITEM_OPTION_DCID_IGNORE_BLOCKRATE);
	r &= au_table_set_column(table,
		"Ignore_SkillBlockRate", 
		AP_ITEM_OPTION_DCID_IGNORE_SKILLBLOCKRATE);
	r &= au_table_set_column(table,
		"Ignore_EvadeRate", 
		AP_ITEM_OPTION_DCID_IGNORE_EVADERATE);
	r &= au_table_set_column(table,
		"Ignore_DodgeRate", 
		AP_ITEM_OPTION_DCID_IGNORE_DODGERATE);
	r &= au_table_set_column(table,
		"Ignore_CriticalDefenseRate", 
		AP_ITEM_OPTION_DCID_IGNORE_CRITICALDEFENSERATE);
	r &= au_table_set_column(table,
		"Ignore_StunDefenseRate", 
		AP_ITEM_OPTION_DCID_IGNORE_STUNDEFENSERATE);
	r &= au_table_set_column(table,
		"Add_Debuff_Duration", 
		AP_ITEM_OPTION_DCID_ADD_DEBUFF_DURATION);
	if (!r) {
		ERROR("Failed to setup columns.");
		au_table_destroy(table);
		return FALSE;
	}
	while (au_table_read_next_line(table)) {
		struct ap_item_option_template * temp = NULL;
		struct ap_factor * factor = NULL;
		while (au_table_read_next_column(table)) {
			enum ap_item_option_data_column_id id = 
				au_table_get_column(table);
			char * value = au_table_get_value(table);
			if (id == AP_ITEM_OPTION_DCID_TID) {
				uint32_t tid = strtoul(value, NULL, 10);
				struct ap_item_option_template ** t = 
					ap_admin_add_object_by_id(
						&mod->option_template_admin, tid);
				if (!t) {
					ERROR("Multiple item option templates with same id (%u).",
						tid);
					au_table_destroy(table);
					return FALSE;
				}
				temp = ap_module_create_module_data(mod, 
					AP_ITEM_MDI_OPTION_TEMPLATE);
				temp->id = tid;
				*t = temp;
				factor = &temp->factor;
				continue;
			}
			if (!temp) {
				assert(0);
				continue;
			}
			switch (id) {
			case AP_ITEM_OPTION_DCID_DESCRIPTION:
				strlcpy(temp->description, value, 
					sizeof(temp->description));
				break;
			case AP_ITEM_OPTION_DCID_UPPER_BODY:
				temp->set_part |= AP_ITEM_OPTION_SET_TYPE_BODY;
				break;
			case AP_ITEM_OPTION_DCID_LOWER_BODY:
				temp->set_part |= AP_ITEM_OPTION_SET_TYPE_LEGS;
				break;
			case AP_ITEM_OPTION_DCID_WEAPON:
				temp->set_part |= AP_ITEM_OPTION_SET_TYPE_WEAPON;
				break;
			case AP_ITEM_OPTION_DCID_SHIELD:
				temp->set_part |= AP_ITEM_OPTION_SET_TYPE_SHIELD;
				break;
			case AP_ITEM_OPTION_DCID_HEAD:
				temp->set_part |= AP_ITEM_OPTION_SET_TYPE_HEAD;
				break;
			case AP_ITEM_OPTION_DCID_RING:
				temp->set_part |= AP_ITEM_OPTION_SET_TYPE_RING;
				break;
			case AP_ITEM_OPTION_DCID_NECKLACE:
				temp->set_part |= AP_ITEM_OPTION_SET_TYPE_NECKLACE;
				break;
			case AP_ITEM_OPTION_DCID_SHOES:
				temp->set_part |= AP_ITEM_OPTION_SET_TYPE_FOOTS;
				break;
			case AP_ITEM_OPTION_DCID_GLOVES:
				temp->set_part |= AP_ITEM_OPTION_SET_TYPE_HANDS;
				break;
			case AP_ITEM_OPTION_DCID_REFINED:
				temp->set_part |= AP_ITEM_OPTION_SET_TYPE_REFINERY;
				break;
			case AP_ITEM_OPTION_DCID_GACHA:
				temp->set_part |= AP_ITEM_OPTION_SET_TYPE_GACHA;
				break;
			case AP_ITEM_OPTION_DCID_TYPE:
				temp->type = au_table_get_i32(table);
				break;
			case AP_ITEM_OPTION_DCID_POINTTYPE:
				temp->point_type = au_table_get_i32(table);
				if (temp->point_type)
					factor = &temp->factor_percent;
				break;
			case AP_ITEM_OPTION_DCID_ITEM_LEVEL_MIN:
				break;
			case AP_ITEM_OPTION_DCID_ITEM_LEVEL_MAX:
				break;
			case AP_ITEM_OPTION_DCID_LEVEL_LIMIT:
				temp->level_limit = au_table_get_i32(table);
				break;
			case AP_ITEM_OPTION_DCID_RANK_MIN:
				temp->rank_min = au_table_get_i32(table);
				break;
			case AP_ITEM_OPTION_DCID_RANK_MAX:
				temp->rank_max = au_table_get_i32(table);
				break;
			case AP_ITEM_OPTION_DCID_PROBABILITY:
				temp->probability = au_table_get_i32(table);
				break;
			case AP_ITEM_OPTION_DCID_SKILL_LEVEL:
				temp->skill_level = au_table_get_i32(table);
				break;
			case AP_ITEM_OPTION_DCID_SKILL_ID:
				temp->skill_tid = au_table_get_i32(table);
				break;
			case AP_ITEM_OPTION_DCID_ATK_RANGE_BONUS:
				ap_factors_set_value(factor, 
					AP_FACTORS_TYPE_ATTACK,
					AP_FACTORS_ATTACK_TYPE_ATTACKRANGE, value);
				break;
			case AP_ITEM_OPTION_DCID_ATK_SPEED_BONUS:
				ap_factors_set_value(factor, 
					AP_FACTORS_TYPE_ATTACK,
					AP_FACTORS_ATTACK_TYPE_SPEED, value);
				break;
			case AP_ITEM_OPTION_DCID_MOVEMENT_RUN_BONUS:
				ap_factors_set_value(factor, 
					AP_FACTORS_TYPE_CHAR_STATUS,
					AP_FACTORS_CHAR_STATUS_TYPE_MOVEMENT, 
					value);
				ap_factors_set_value(factor, 
					AP_FACTORS_TYPE_CHAR_STATUS,
					AP_FACTORS_CHAR_STATUS_TYPE_MOVEMENT_FAST, 
					value);
				break;
			case AP_ITEM_OPTION_DCID_PHYSICALDAMAGE:
				ap_factors_set_attribute(factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MIN,
					AP_FACTORS_ATTRIBUTE_TYPE_PHYSICAL, value);
				ap_factors_set_attribute(factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MAX,
					AP_FACTORS_ATTRIBUTE_TYPE_PHYSICAL, value);
				break;
			case AP_ITEM_OPTION_DCID_PHYSICALATTACKRESIST:
				ap_factors_set_attribute(factor,
					AP_FACTORS_TYPE_DEFENSE,
					AP_FACTORS_DEFENSE_TYPE_DEFENSE_RATE,
					AP_FACTORS_ATTRIBUTE_TYPE_PHYSICAL, value);
				break;
			case AP_ITEM_OPTION_DCID_SKILLBLOCKRATE:
				ap_factors_set_attribute(factor,
					AP_FACTORS_TYPE_DEFENSE,
					AP_FACTORS_DEFENSE_TYPE_DEFENSE_RATE,
					AP_FACTORS_ATTRIBUTE_TYPE_SKILL_BLOCK, value);
				break;
			case AP_ITEM_OPTION_DCID_MAGICDAMAGE:
				ap_factors_set_attribute(factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MIN,
					AP_FACTORS_ATTRIBUTE_TYPE_MAGIC, value);
				ap_factors_set_attribute(factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MAX,
					AP_FACTORS_ATTRIBUTE_TYPE_MAGIC, value);
				break;
			case AP_ITEM_OPTION_DCID_WATERDAMAGE:
				ap_factors_set_attribute(factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MIN,
					AP_FACTORS_ATTRIBUTE_TYPE_WATER, value);
				ap_factors_set_attribute(factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MAX,
					AP_FACTORS_ATTRIBUTE_TYPE_WATER, value);
				break;
			case AP_ITEM_OPTION_DCID_FIREDAMAGE:
				ap_factors_set_attribute(factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MIN,
					AP_FACTORS_ATTRIBUTE_TYPE_FIRE, value);
				ap_factors_set_attribute(factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MAX,
					AP_FACTORS_ATTRIBUTE_TYPE_FIRE, value);
				break;
			case AP_ITEM_OPTION_DCID_EARTHDAMAGE:
				ap_factors_set_attribute(factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MIN,
					AP_FACTORS_ATTRIBUTE_TYPE_EARTH, value);
				ap_factors_set_attribute(factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MAX,
					AP_FACTORS_ATTRIBUTE_TYPE_EARTH, value);
				break;
			case AP_ITEM_OPTION_DCID_AIRDAMAGE:
				ap_factors_set_attribute(factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MIN,
					AP_FACTORS_ATTRIBUTE_TYPE_AIR, value);
				ap_factors_set_attribute(factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MAX,
					AP_FACTORS_ATTRIBUTE_TYPE_AIR, value);
				break;
			case AP_ITEM_OPTION_DCID_POISONDAMAGE:
				ap_factors_set_attribute(factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MIN,
					AP_FACTORS_ATTRIBUTE_TYPE_POISON, value);
				ap_factors_set_attribute(factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MAX,
					AP_FACTORS_ATTRIBUTE_TYPE_POISON, value);
				break;
			case AP_ITEM_OPTION_DCID_ICEDAMAGE:
				ap_factors_set_attribute(factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MIN,
					AP_FACTORS_ATTRIBUTE_TYPE_ICE, value);
				ap_factors_set_attribute(factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MAX,
					AP_FACTORS_ATTRIBUTE_TYPE_ICE, value);
				break;
			case AP_ITEM_OPTION_DCID_LINGHTNINGDAMAGE:
				ap_factors_set_attribute(factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MIN,
					AP_FACTORS_ATTRIBUTE_TYPE_LIGHTENING, value);
				ap_factors_set_attribute(factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MAX,
					AP_FACTORS_ATTRIBUTE_TYPE_LIGHTENING, value);
				break;
			case AP_ITEM_OPTION_DCID_FIRE_RES:
				ap_factors_set_attribute(factor,
					AP_FACTORS_TYPE_DEFENSE,
					AP_FACTORS_DEFENSE_TYPE_DEFENSE_RATE,
					AP_FACTORS_ATTRIBUTE_TYPE_FIRE, value);
				break;
			case AP_ITEM_OPTION_DCID_WATER_RES:
				ap_factors_set_attribute(factor,
					AP_FACTORS_TYPE_DEFENSE,
					AP_FACTORS_DEFENSE_TYPE_DEFENSE_RATE,
					AP_FACTORS_ATTRIBUTE_TYPE_WATER, value);
				break;
			case AP_ITEM_OPTION_DCID_AIR_RES:
				ap_factors_set_attribute(factor,
					AP_FACTORS_TYPE_DEFENSE,
					AP_FACTORS_DEFENSE_TYPE_DEFENSE_RATE,
					AP_FACTORS_ATTRIBUTE_TYPE_AIR, value);
				break;
			case AP_ITEM_OPTION_DCID_EARTH_RES:
				ap_factors_set_attribute(factor,
					AP_FACTORS_TYPE_DEFENSE,
					AP_FACTORS_DEFENSE_TYPE_DEFENSE_RATE,
					AP_FACTORS_ATTRIBUTE_TYPE_EARTH, value);
				break;
			case AP_ITEM_OPTION_DCID_MAGIC_RES:
				ap_factors_set_attribute(factor,
					AP_FACTORS_TYPE_DEFENSE,
					AP_FACTORS_DEFENSE_TYPE_DEFENSE_RATE,
					AP_FACTORS_ATTRIBUTE_TYPE_MAGIC, value);
				break;
			case AP_ITEM_OPTION_DCID_POISON_RES:
				ap_factors_set_attribute(factor,
					AP_FACTORS_TYPE_DEFENSE,
					AP_FACTORS_DEFENSE_TYPE_DEFENSE_RATE,
					AP_FACTORS_ATTRIBUTE_TYPE_POISON, value);
				break;
			case AP_ITEM_OPTION_DCID_ICE_RES:
				ap_factors_set_attribute(factor,
					AP_FACTORS_TYPE_DEFENSE,
					AP_FACTORS_DEFENSE_TYPE_DEFENSE_RATE,
					AP_FACTORS_ATTRIBUTE_TYPE_ICE, value);
				break;
			case AP_ITEM_OPTION_DCID_THUNDER_RES:
				ap_factors_set_attribute(factor,
					AP_FACTORS_TYPE_DEFENSE,
					AP_FACTORS_DEFENSE_TYPE_DEFENSE_RATE,
					AP_FACTORS_ATTRIBUTE_TYPE_LIGHTENING, value);
				break;
			case AP_ITEM_OPTION_DCID_PHYSICAL_DEFENSE_POINT:
				ap_factors_set_attribute(factor,
					AP_FACTORS_TYPE_DEFENSE,
					AP_FACTORS_DEFENSE_TYPE_DEFENSE_POINT,
					AP_FACTORS_ATTRIBUTE_TYPE_PHYSICAL, value);
				break;
			case AP_ITEM_OPTION_DCID_PHYSICAL_ATKRATE:
				ap_factors_set_value(factor,
					AP_FACTORS_TYPE_CHAR_POINT_MAX,
					AP_FACTORS_CHAR_POINT_MAX_TYPE_AR, value);
				break;
			case AP_ITEM_OPTION_DCID_MAGIC_ATKRATE_:
				ap_factors_set_value(factor,
					AP_FACTORS_TYPE_CHAR_POINT_MAX,
					AP_FACTORS_CHAR_POINT_MAX_TYPE_MAR, value);
				break;
			case AP_ITEM_OPTION_DCID_BLOCKRATE:
				ap_factors_set_attribute(factor,
					AP_FACTORS_TYPE_DEFENSE,
					AP_FACTORS_DEFENSE_TYPE_DEFENSE_RATE,
					AP_FACTORS_ATTRIBUTE_TYPE_PHYSICAL_BLOCK, 
					value);
				break;
			case AP_ITEM_OPTION_DCID_MAGIC_DEFENSE_POINT:
				ap_factors_set_attribute(factor,
					AP_FACTORS_TYPE_DEFENSE,
					AP_FACTORS_DEFENSE_TYPE_DEFENSE_POINT,
					AP_FACTORS_ATTRIBUTE_TYPE_MAGIC, value);
				break;
			case AP_ITEM_OPTION_DCID_WATER_DEFENSE_POINT:
				ap_factors_set_attribute(factor,
					AP_FACTORS_TYPE_DEFENSE,
					AP_FACTORS_DEFENSE_TYPE_DEFENSE_POINT,
					AP_FACTORS_ATTRIBUTE_TYPE_WATER, value);
				break;
			case AP_ITEM_OPTION_DCID_FIRE_DEFENSE_POINT:
				ap_factors_set_attribute(factor,
					AP_FACTORS_TYPE_DEFENSE,
					AP_FACTORS_DEFENSE_TYPE_DEFENSE_POINT,
					AP_FACTORS_ATTRIBUTE_TYPE_FIRE, value);
				break;
			case AP_ITEM_OPTION_DCID_EARTH_DEFENSE_POINT:
				ap_factors_set_attribute(factor,
					AP_FACTORS_TYPE_DEFENSE,
					AP_FACTORS_DEFENSE_TYPE_DEFENSE_POINT,
					AP_FACTORS_ATTRIBUTE_TYPE_EARTH, value);
				break;
			case AP_ITEM_OPTION_DCID_AIR_DEFENSE_POINT:
				ap_factors_set_attribute(factor,
					AP_FACTORS_TYPE_DEFENSE,
					AP_FACTORS_DEFENSE_TYPE_DEFENSE_POINT,
					AP_FACTORS_ATTRIBUTE_TYPE_AIR, value);
				break;
			case AP_ITEM_OPTION_DCID_POISON_DEFENSE_POINT:
				ap_factors_set_attribute(factor,
					AP_FACTORS_TYPE_DEFENSE,
					AP_FACTORS_DEFENSE_TYPE_DEFENSE_POINT,
					AP_FACTORS_ATTRIBUTE_TYPE_POISON, value);
				break;
			case AP_ITEM_OPTION_DCID_ICE_DEFENSE_POINT:
				ap_factors_set_attribute(factor,
					AP_FACTORS_TYPE_DEFENSE,
					AP_FACTORS_DEFENSE_TYPE_DEFENSE_POINT,
					AP_FACTORS_ATTRIBUTE_TYPE_ICE, value);
				break;
			case AP_ITEM_OPTION_DCID_LINGHTNING_DEFENSE_POINT:
				ap_factors_set_attribute(factor,
					AP_FACTORS_TYPE_DEFENSE,
					AP_FACTORS_DEFENSE_TYPE_DEFENSE_POINT,
					AP_FACTORS_ATTRIBUTE_TYPE_LIGHTENING, value);
				break;
			case AP_ITEM_OPTION_DCID_HP_BONUS_POINT:
				ap_factors_set_value(&temp->factor,
					AP_FACTORS_TYPE_CHAR_POINT_MAX,
					AP_FACTORS_CHAR_POINT_MAX_TYPE_HP, value);
				break;
			case AP_ITEM_OPTION_DCID_MP_BONUS_POINT:
				ap_factors_set_value(&temp->factor,
					AP_FACTORS_TYPE_CHAR_POINT_MAX,
					AP_FACTORS_CHAR_POINT_MAX_TYPE_MP, value);
				break;
			case AP_ITEM_OPTION_DCID_HP_BONUS_PERCENT:
				ap_factors_set_value(&temp->factor_percent,
					AP_FACTORS_TYPE_CHAR_POINT_MAX,
					AP_FACTORS_CHAR_POINT_MAX_TYPE_HP, value);
				break;
			case AP_ITEM_OPTION_DCID_MP_BONUS_PERCENT:
				ap_factors_set_value(&temp->factor_percent,
					AP_FACTORS_TYPE_CHAR_POINT_MAX,
					AP_FACTORS_CHAR_POINT_MAX_TYPE_MP, value);
				break;
			case AP_ITEM_OPTION_DCID_STR_BONUS_POINT:
				ap_factors_set_value(&temp->factor,
					AP_FACTORS_TYPE_CHAR_STATUS,
					AP_FACTORS_CHAR_STATUS_TYPE_STR, value);
				break;
			case AP_ITEM_OPTION_DCID_DEX_BONUS_POINT:
				ap_factors_set_value(&temp->factor,
					AP_FACTORS_TYPE_CHAR_STATUS,
					AP_FACTORS_CHAR_STATUS_TYPE_DEX, value);
				break;
			case AP_ITEM_OPTION_DCID_INT_BONUS_POINT:
				ap_factors_set_value(&temp->factor,
					AP_FACTORS_TYPE_CHAR_STATUS,
					AP_FACTORS_CHAR_STATUS_TYPE_INT, value);
				break;
			case AP_ITEM_OPTION_DCID_CON_BONUS_POINT:
				ap_factors_set_value(&temp->factor,
					AP_FACTORS_TYPE_CHAR_STATUS,
					AP_FACTORS_CHAR_STATUS_TYPE_CON, value);
				break;
			case AP_ITEM_OPTION_DCID_WIS_BONUS_POINT:
				ap_factors_set_value(&temp->factor,
					AP_FACTORS_TYPE_CHAR_STATUS,
					AP_FACTORS_CHAR_STATUS_TYPE_WIS, value);
				break;
			case AP_ITEM_OPTION_DCID_PHYSICALRANK:
				ap_factors_set_value(&temp->factor,
					AP_FACTORS_TYPE_ITEM,
					AP_FACTORS_ITEM_TYPE_PHYSICAL_RANK, value);
				break;
			case AP_ITEM_OPTION_DCID_SKILL_TYPE:
				temp->skill_type = au_table_get_i32(table);
				break;
			case AP_ITEM_OPTION_DCID_DURATION:
				switch (temp->skill_type) {
				case AP_ITEM_OPTION_SKILL_TYPE_STUN:
					temp->skill_data.stun_time = 
						au_table_get_i32(table);
					break;
				case AP_ITEM_OPTION_SKILL_TYPE_DOT:
					temp->skill_data.dot_time = 
						au_table_get_i32(table);
					break;
				}
				break;
			case AP_ITEM_OPTION_DCID_SKILL_RATE:
				switch (temp->skill_type) {
				case AP_ITEM_OPTION_SKILL_TYPE_CRITICAL:
					temp->skill_data.critical_rate = 
						au_table_get_i32(table);
					break;
				case AP_ITEM_OPTION_SKILL_TYPE_STUN:
					temp->skill_data.stun_rate = 
						au_table_get_i32(table);
					break;
				case AP_ITEM_OPTION_SKILL_TYPE_DMG_CONVERT_HP:
					temp->skill_data.damage_convert_hp_rate = 
						au_table_get_i32(table);
					break;
				case AP_ITEM_OPTION_SKILL_TYPE_DOT:
					temp->skill_data.dot_rate = 
						au_table_get_i32(table);
					break;
				}
				break;
			case AP_ITEM_OPTION_DCID_CRITICAL:
				temp->skill_data.critical_damage = 
					au_table_get_i32(table);
				break;
			case AP_ITEM_OPTION_DCID_STUN_TIME:
				temp->skill_data.stun_time = 
					au_table_get_i32(table);
				break;
			case AP_ITEM_OPTION_DCID_DAMAGE_CONVERT_HP:
				temp->skill_data.damage_convert_hp = 
					au_table_get_i32(table);
				break;
			case AP_ITEM_OPTION_DCID_REGEN_HP:
				temp->skill_data.regen_hp = 
					au_table_get_i32(table);
				break;
			case AP_ITEM_OPTION_DCID_REGEN_MP:
				temp->skill_data.regen_mp = 
					au_table_get_i32(table);
				break;
			case AP_ITEM_OPTION_DCID_DOT_DAMAGE_TIME:
				temp->skill_data.dot_time = 
					au_table_get_i32(table);
				break;
			case AP_ITEM_OPTION_DCID_SKILL_CAST:
				ap_factors_set_value(factor,
					AP_FACTORS_TYPE_ATTACK,
					AP_FACTORS_ATTACK_TYPE_SKILL_CAST, value);
				break;
			case AP_ITEM_OPTION_DCID_SKILL_DELAY:
				ap_factors_set_value(factor,
					AP_FACTORS_TYPE_ATTACK,
					AP_FACTORS_ATTACK_TYPE_SKILL_DELAY, value);
				break;
			case AP_ITEM_OPTION_DCID_SKILL_LEVELUP:
				temp->skill_data.skill_level_up = 
					au_table_get_i32(table);
				break;
			case AP_ITEM_OPTION_DCID_BONUS_EXP:
				temp->skill_data.bonus_exp = 
					au_table_get_i32(table);
				break;
			case AP_ITEM_OPTION_DCID_BONUS_MONEY:
				temp->skill_data.bonus_gold = 
					au_table_get_i32(table);
				break;
			case AP_ITEM_OPTION_DCID_BONUS_DROP_RATE:
				temp->skill_data.bonus_drop_rate = 
					au_table_get_i32(table);
				break;
			case AP_ITEM_OPTION_DCID_EVADE_RATE:
				ap_factors_set_value(factor,
					AP_FACTORS_TYPE_ATTACK,
					AP_FACTORS_ATTACK_TYPE_EVADE_RATE, value);
				break;
			case AP_ITEM_OPTION_DCID_DODGE_RATE:
				ap_factors_set_value(factor,
					AP_FACTORS_TYPE_ATTACK,
					AP_FACTORS_ATTACK_TYPE_DODGE_RATE, value);
				break;
			case AP_ITEM_OPTION_DCID_VISIBLE:
				temp->is_visible = TRUE;
				break;
			case AP_ITEM_OPTION_DCID_LEVEL_MIN:
				temp->level_min = au_table_get_i32(table);
				break;
			case AP_ITEM_OPTION_DCID_LEVEL_MAX:
				temp->level_max = au_table_get_i32(table);
				break;
			case AP_ITEM_OPTION_DCID_LINK_ID: {
				struct ap_item_option_link_pool * pool;
				temp->link_id = au_table_get_i32(table);
				assert(temp->link_id < LINK_ID_CAP);
				pool = &mod->option_link_pools[temp->link_id];
				assert(pool->count < AP_ITEM_OPTION_LINK_POOL_SIZE);
				pool->options[pool->count++] = temp;
				break;
			}
			case AP_ITEM_OPTION_DCID_HEROIC_MIN_DAMAGE:
				ap_factors_set_attribute(factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MIN,
					AP_FACTORS_ATTRIBUTE_TYPE_HEROIC, value);
				break;
			case AP_ITEM_OPTION_DCID_HEROIC_MAX_DAMAGE:
				ap_factors_set_attribute(factor,
					AP_FACTORS_TYPE_DAMAGE,
					AP_FACTORS_DAMAGE_TYPE_MAX,
					AP_FACTORS_ATTRIBUTE_TYPE_HEROIC, value);
				break;
			case AP_ITEM_OPTION_DCID_HEROIC_DEFENSE_POINT:
				if (temp->point_type) {
					ap_factors_set_attribute(factor,
						AP_FACTORS_TYPE_DEFENSE,
						AP_FACTORS_DEFENSE_TYPE_DEFENSE_RATE,
						AP_FACTORS_ATTRIBUTE_TYPE_HEROIC, value);
				}
				else {
					ap_factors_set_attribute(factor,
						AP_FACTORS_TYPE_DEFENSE,
						AP_FACTORS_DEFENSE_TYPE_DEFENSE_POINT,
						AP_FACTORS_ATTRIBUTE_TYPE_HEROIC, value);
				}
				break;
			case AP_ITEM_OPTION_DCID_IGNORE_PHYSICAL_DEFENSE_RATE:
				temp->skill_data.ignore_defense = 
					au_table_get_i32(table);
				break;
			case AP_ITEM_OPTION_DCID_IGNORE_ATTRIBUTE_DEFENSE_RATE:
				temp->skill_data.ignore_elemental_resistance = 
					au_table_get_i32(table);
				break;
			case AP_ITEM_OPTION_DCID_CRITICAL_DEFENSE_RATE:
				temp->skill_data.critical_resistance = 
					au_table_get_i32(table);
				break;
			case AP_ITEM_OPTION_DCID_REINFORCEMENTRATE:
				temp->skill_data.reinforcement_rate = 
					au_table_get_i32(table);
				break;
			case AP_ITEM_OPTION_DCID_IGNORE_PHYSICALATTACKRESIST:
				break;
			case AP_ITEM_OPTION_DCID_IGNORE_BLOCKRATE:
				break;
			case AP_ITEM_OPTION_DCID_IGNORE_SKILLBLOCKRATE:
				break;
			case AP_ITEM_OPTION_DCID_IGNORE_EVADERATE:
				break;
			case AP_ITEM_OPTION_DCID_IGNORE_DODGERATE:
				break;
			case AP_ITEM_OPTION_DCID_IGNORE_CRITICALDEFENSERATE:
				break;
			case AP_ITEM_OPTION_DCID_IGNORE_STUNDEFENSERATE:
				break;
			case AP_ITEM_OPTION_DCID_ADD_DEBUFF_DURATION:
				break;
			default:
				ERROR("Invalid column id (%u).", id);
				au_table_destroy(table);
				return FALSE;
			}
		}
	}
	au_table_destroy(table);
	INFO("Loaded %u item option templates.", 
		ap_admin_get_object_count(&mod->option_template_admin));
	index = 0;
	while (ap_admin_iterate_id(&mod->option_template_admin, &index, &object)) {
		struct ap_item_cb_end_read_option cb = { 0 };
		cb.temp = *(struct ap_item_option_template **)object;
		if (!ap_module_enum_callback(mod, AP_ITEM_CB_END_READ_OPTION, &cb)) {
			ERROR("Completion callback failed.");
			return FALSE;
		}
	}
	return TRUE;
}

boolean ap_item_read_lottery_box(
	struct ap_item_module * mod,
	const char * file_path, 
	boolean decrypt)
{
	struct au_table * table = au_table_open(file_path, decrypt);
	boolean r = TRUE;
	if (!table) {
		ERROR("Failed to open file (%s).", file_path);
		return FALSE;
	}
	r &= au_table_set_column(table, "LotteryBoxTID", LOTTERY_DCID_LOTTERYBOXTID);
	r &= au_table_set_column(table, "LotteryBoxName", LOTTERY_DCID_LOTTERYBOXNAME);
	r &= au_table_set_column(table, "PotItemTID", LOTTERY_DCID_POTITEMTID);
	r &= au_table_set_column(table, "PotItemName", LOTTERY_DCID_POTITEMNAME);
	r &= au_table_set_column(table, "MinStack", LOTTERY_DCID_MINSTACK);
	r &= au_table_set_column(table, "MaxStack", LOTTERY_DCID_MAXSTACK);
	r &= au_table_set_column(table, "Rate", LOTTERY_DCID_RATE);
	if (!r) {
		ERROR("Failed to setup columns.");
		au_table_destroy(table);
		return FALSE;
	}
	while (au_table_read_next_line(table)) {
		struct ap_item_lottery_box * box = NULL;
		struct ap_item_lottery_item * item = NULL;
		while (au_table_read_next_column(table)) {
			enum ap_item_option_data_column_id id = 
				au_table_get_column(table);
			char * value = au_table_get_value(table);
			if (id == LOTTERY_DCID_LOTTERYBOXTID) {
				uint32_t tid = strtoul(value, NULL, 10);
				struct ap_item_template * temp = ap_item_get_template(mod, tid);
				if (!temp) {
					ERROR("Failed to retrieve lottery box template (%u).", tid);
					au_table_destroy(table);
					return FALSE;
				}
				assert(temp->type == AP_ITEM_TYPE_USABLE);
				assert(temp->usable.usable_type == AP_ITEM_USABLE_TYPE_LOTTERY_BOX);
				box = &temp->usable.lottery_box;
				continue;
			}
			if (!box) {
				assert(0);
				continue;
			}
			switch (id) {
			case LOTTERY_DCID_LOTTERYBOXNAME:
				strlcpy(box->name, value, sizeof(box->name));
				break;
			case LOTTERY_DCID_POTITEMTID:
				assert(item == NULL);
				item = vec_add_empty(&box->items);
				item->item_tid = strtoul(value, NULL, 10);
				break;
			case LOTTERY_DCID_POTITEMNAME:
				assert(item != NULL);
				strlcpy(item->name, value, sizeof(item->name));
				break;
			case LOTTERY_DCID_MINSTACK:
				assert(item != NULL);
				item->min_stack_count = strtoul(value, NULL, 10);
				break;
			case LOTTERY_DCID_MAXSTACK:
				assert(item != NULL);
				item->max_stack_count = strtoul(value, NULL, 10);
				assert(item->max_stack_count >= item->min_stack_count);
				break;
			case LOTTERY_DCID_RATE:
				assert(item != NULL);
				item->rate = strtoul(value, NULL, 10);
				box->total_rate += item->rate;
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

boolean ap_item_read_avatar_set(
	struct ap_item_module * mod,
	const char * file_path, 
	boolean decrypt)
{
	struct au_ini_mgr_ctx * ini = au_ini_mgr_create();
	uint32_t sectioncount;
	uint32_t i;
	au_ini_mgr_set_mode(ini, AU_INI_MGR_MODE_NAME_OVERWRITE);
	au_ini_mgr_set_path(ini, file_path);
	if (!au_ini_mgr_read_file(ini, 0, decrypt)) {
		ERROR("Failed to read file (%s).", file_path);
		au_ini_mgr_destroy(ini);
		return FALSE;
	}
	sectioncount = au_ini_mgr_get_section_count(ini);
	for (i = 0; i < sectioncount; i++) {
		uint32_t tid = strtoul(au_ini_mgr_get_section_name(ini, i), NULL, 10);
		uint32_t keycount = au_ini_mgr_get_key_count(ini, i);
		uint32_t j;
		struct ap_item_template * temp = NULL;
		struct ap_item_avatar_set * set = NULL;
		for (j = 0; j < keycount; j++) {
			const char * key = au_ini_mgr_get_key_name(ini, i, j);
			if (strcmp(key, "name") == 0) {
			}
			else if (strcmp(key, "tid0") == 0) {
				uint32_t index = au_ini_mgr_get_key_index(ini, key);
				uint32_t tid = strtoul(au_ini_mgr_get_value(ini, i, index), 
					NULL, 10);
				temp = ap_item_get_template(mod, tid);
				if (!temp) {
					ERROR("Invalid avatar set template id (%u).", tid);
					break;
				}
				assert(temp->type == AP_ITEM_TYPE_USABLE);
				assert(temp->usable.usable_type == AP_ITEM_USABLE_TYPE_AVATAR);
				set = &temp->usable.avatar_set;
				set->count = 0;
			}
			else if (strncmp(key, "tid", 3) == 0) {
				uint32_t parttid;
				struct ap_item_template * part;
				uint32_t index = au_ini_mgr_get_key_index(ini, key);
				assert(temp != NULL);
				if (set->count >= COUNT_OF(set->temp)) {
					ERROR("Avatar set has too many parts (%u).", tid);
					continue;
				}
				parttid = strtoul(au_ini_mgr_get_value(ini, i, index), NULL, 10);
				part = ap_item_get_template(mod, parttid);
				if (!part) {
					ERROR("Invalid avatar set part (set = %u, part = %u).", 
						tid, parttid);
					continue;
				}
				set->temp[set->count++] = part;
			}
		}
	}
	au_ini_mgr_destroy(ini);
	return TRUE;
}

void ap_item_add_callback(
	struct ap_item_module * mod,
	enum ap_item_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	ap_module_add_callback(mod, id, callback_module, callback);
}

size_t ap_item_attach_data(
	struct ap_item_module * mod,
	enum ap_item_module_data_index data_index,
	size_t data_size,
	ap_module_t callback_module,
	ap_module_default_t constructor,
	ap_module_default_t destructor)
{
	return ap_module_attach_data(mod, data_index, data_size, 
		callback_module, constructor, destructor);
}

struct ap_item_character * ap_item_get_character(
	struct ap_item_module * mod,
	struct ap_character * character)
{
	return ap_module_get_attached_data(character, mod->character_item_offset);
}

struct ap_item_skill_buff_attachment * ap_item_get_skill_buff_attachment(
	struct ap_item_module * mod,
	struct ap_skill_buff_list * buff)
{
	return ap_module_get_attached_data(buff, mod->skill_buff_offset);
}

void ap_item_add_stream_callback(
	struct ap_item_module * mod,
	enum ap_item_module_data_index data_index,
	const char * module_name,
	ap_module_t callback_module,
	ap_module_stream_read_t read_cb,
	ap_module_stream_write_t write_cb)
{
	if (data_index < 0 || data_index >= AP_ITEM_MDI_COUNT) {
		ERROR("Invalid stream data index: %u.");
		return;
	}
	ap_module_stream_add_callback(mod, data_index,
		module_name, callback_module, read_cb, write_cb);
}

uint8_t ap_item_get_equip_index(enum ap_item_part part)
{
	switch (part) {
	case AP_ITEM_PART_ACCESSORY_NECKLACE:
		return 0;
	case AP_ITEM_PART_HEAD:
		return 1;
	case AP_ITEM_PART_HAND_RIGHT:
		return 2;
	case AP_ITEM_PART_ACCESSORY_RING1:
		return 3;
	case AP_ITEM_PART_HANDS:
		return 4;
	case AP_ITEM_PART_BODY:
		return 5;
	case AP_ITEM_PART_HAND_LEFT:
		return 6;
	case AP_ITEM_PART_ACCESSORY_RING2:
		return 7;
	case AP_ITEM_PART_FOOT:
		return 8;
	case AP_ITEM_PART_LEGS:
		return 9;
	case AP_ITEM_PART_LANCER:
		return 10;
	case AP_ITEM_PART_RIDE:
		return 11;
	case AP_ITEM_PART_ARMS:
		return 12;
	case AP_ITEM_PART_ARMS2:
		return 13;
	case AP_ITEM_PART_V_ACCESSORY_NECKLACE:
		return 14;
	case AP_ITEM_PART_V_HEAD:
		return 15;
	case AP_ITEM_PART_V_HAND_RIGHT:
		return 16;
	case AP_ITEM_PART_V_ACCESSORY_RING1:
		return 17;
	case AP_ITEM_PART_V_HANDS:
		return 18;
	case AP_ITEM_PART_V_BODY:
		return 19;
	case AP_ITEM_PART_V_HAND_LEFT:
		return 20;
	case AP_ITEM_PART_V_ACCESSORY_RING2:
		return 21;
	case AP_ITEM_PART_V_FOOT:
		return 22;
	case AP_ITEM_PART_V_LEGS:
		return 23;
	case AP_ITEM_PART_V_LANCER:
		return 24;
	case AP_ITEM_PART_V_RIDE:
		return 25;
	case AP_ITEM_PART_V_ARMS:
		return 26;
	case AP_ITEM_PART_V_ARMS2:
		return 27;
	default:
		return UINT8_MAX;
	}
}

struct ap_item * ap_item_get_equipment(
	struct ap_item_module * mod,
	struct ap_character * character,
	enum ap_item_part part)
{
	uint8_t index = ap_item_get_equip_index(part);
	if (index == UINT8_MAX) {
		assert(0);
		return NULL;
	}
	return ap_grid_get_object_by_index(
		ap_item_get_character_grid(mod, character, AP_ITEM_STATUS_EQUIP),
		index);
}

struct ap_item_template * ap_item_get_template(
	struct ap_item_module * mod,
	uint32_t tid)
{
	struct ap_item_template ** obj = 
		ap_admin_get_object_by_id(&mod->template_admin, tid);
	if (!obj)
		return NULL;
	return *obj;
}

struct ap_item_option_template * ap_item_get_option_template(
	struct ap_item_module * mod,
	uint16_t tid)
{
	struct ap_item_option_template ** obj = 
		ap_admin_get_object_by_id(&mod->option_template_admin, tid);
	if (!obj)
		return NULL;
	return *obj;
}

struct ap_item * ap_item_create(struct ap_item_module * mod, uint32_t tid)
{
	struct ap_item_template * temp = ap_item_get_template(mod, tid);
	struct ap_item * item;
	if (!temp)
		return NULL;
	/** \todo Use a freelist instead of allocating/deallocating
	 *        every time an item is created/destroyed. */
	item = ap_module_create_module_data(mod, 
		AP_ITEM_MDI_ITEM);
	item->tid = tid;
	item->temp = temp;
	ap_factors_copy(&item->factor, &temp->factor);
	return item;
}

void ap_item_free(struct ap_item_module * mod, struct ap_item * item)
{
	ap_module_destroy_module_data(mod, AP_ITEM_MDI_ITEM, item);
}

struct ap_item * ap_item_find(
	struct ap_item_module * mod,
	struct ap_character * character,
	uint32_t item_id,
	uint32_t count, ...)
{
	va_list args;
	uint32_t i;
	va_start(args, count);
	for (i = 0; i < count; i++) {
		enum ap_item_status st = va_arg(args, enum ap_item_status);
		struct ap_grid * grid = ap_item_get_character_grid(mod,
			character, st);
		struct ap_item * item = NULL;
		if (!grid) {
			assert(0);
			continue;
		}
		item = ap_grid_get_object_by_id(grid, item_id);
		if (item) {
			va_end(args);
			return item;
		}
	}
	va_end(args);
	return NULL;
}

struct ap_item * ap_item_find_by_cash_item_type(
	struct ap_item_module * mod,
	struct ap_character * character,
	enum ap_item_cash_item_type type,
	uint32_t count, ...)
{
	va_list args;
	uint32_t i;
	va_start(args, count);
	for (i = 0; i < count; i++) {
		enum ap_item_status st = va_arg(args, enum ap_item_status);
		struct ap_grid * grid = ap_item_get_character_grid(mod, character, st);
		uint32_t j;
		if (!grid) {
			assert(0);
			continue;
		}
		for (j = 0; j < grid->grid_count; j++) {
			struct ap_item * item = ap_grid_get_object_by_index(grid, j);
			if (item && item->temp->cash_item_type == type) {
				va_end(args);
				return item;
			}
		}
	}
	va_end(args);
	return NULL;
}

struct ap_item * ap_item_find_item_in_use_by_cash_item_type(
	struct ap_item_module * mod,
	struct ap_character * character,
	enum ap_item_cash_item_type type,
	uint32_t count, ...)
{
	va_list args;
	uint32_t i;
	va_start(args, count);
	for (i = 0; i < count; i++) {
		enum ap_item_status st = va_arg(args, enum ap_item_status);
		struct ap_grid * grid = ap_item_get_character_grid(mod, character, st);
		uint32_t j;
		if (!grid) {
			assert(0);
			continue;
		}
		for (j = 0; j < grid->grid_count; j++) {
			struct ap_item * item = ap_grid_get_object_by_index(grid, j);
			if (item && item->in_use && item->temp->cash_item_type == type) {
				va_end(args);
				return item;
			}
		}
	}
	va_end(args);
	return NULL;
}

struct ap_item * ap_item_find_item_in_use_by_usable_type(
	struct ap_item_module * mod,
	struct ap_character * character,
	enum ap_item_usable_type type,
	uint32_t count, ...)
{
	va_list args;
	uint32_t i;
	va_start(args, count);
	for (i = 0; i < count; i++) {
		enum ap_item_status st = va_arg(args, enum ap_item_status);
		struct ap_grid * grid = ap_item_get_character_grid(mod, character, st);
		uint32_t j;
		if (!grid) {
			assert(0);
			continue;
		}
		for (j = 0; j < grid->grid_count; j++) {
			struct ap_item * item = ap_grid_get_object_by_index(grid, j);
			if (item && 
				item->in_use && 
				item->temp->type == AP_ITEM_TYPE_USABLE &&
				item->temp->usable.usable_type == type) {
				va_end(args);
				return item;
			}
		}
	}
	va_end(args);
	return NULL;
}

struct ap_item * ap_item_find_usable_item_without_status_flag(
	struct ap_item_module * mod,
	struct ap_character * character,
	uint32_t item_tid,
	uint64_t status_flag,
	uint32_t count, ...)
{
	va_list args;
	uint32_t i;
	va_start(args, count);
	for (i = 0; i < count; i++) {
		enum ap_item_status st = va_arg(args, enum ap_item_status);
		struct ap_grid * grid = ap_item_get_character_grid(mod, character, st);
		uint32_t j;
		if (!grid) {
			assert(0);
			continue;
		}
		for (j = 0; j < grid->grid_count; j++) {
			struct ap_item * item = ap_grid_get_object_by_index(grid, j);
			if (item && 
				item->tid == item_tid &&
				item->temp->type == AP_ITEM_TYPE_USABLE && 
				!CHECK_BIT(item->status_flags, status_flag)) {
				va_end(args);
				return item;
			}
		}
	}
	va_end(args);
	return NULL;
}

void ap_item_remove_from_grid(
	struct ap_item_module * mod,
	struct ap_character * character,
	struct ap_item * item)
{
	struct ap_grid * grid = 
		ap_item_get_character_grid(mod, character, item->status);
	assert(grid != NULL);
	ap_grid_set_empty(grid, 
		item->grid_pos[AP_ITEM_GRID_POS_TAB],
		item->grid_pos[AP_ITEM_GRID_POS_ROW],
		item->grid_pos[AP_ITEM_GRID_POS_COLUMN]);
}

uint32_t ap_item_get_free_slot_count(
	struct ap_item_module * mod,
	struct ap_character * character,
	enum ap_item_status item_status,
	uint32_t item_tid)
{
	uint32_t i;
	struct ap_grid * grid = ap_item_get_character_grid(mod, character, item_status);
	uint32_t count = 0;
	for (i = 0; i < grid->grid_count; i++) {
		struct ap_item * item = ap_grid_get_object_by_index(grid, i);
		if (item &&
			item->tid == item_tid &&
			item->temp->max_stackable_count > item->stack_count) {
			count += item->temp->max_stackable_count - item->stack_count;
		}
	}
	return count;
}

boolean ap_item_check_use_restriction(
	struct ap_item_module * mod,
	struct ap_item * item,
	struct ap_character * character)
{
	const struct ap_item_template * temp = item->temp;
	const struct ap_factor * r = &temp->factor_restrict;
	const struct ap_factor * c = &character->factor;
	if (item->in_use)
		return FALSE;
	if (temp->type != AP_ITEM_TYPE_USABLE)
		return FALSE;
	if (ap_character_get_level(character) < r->char_status.level) {
		/* Character level is too low. */
		return FALSE;
	}
	if (r->char_status.limited_level &&
		ap_character_get_level(character) > r->char_status.limited_level) {
		/* Character level is too high. */
		return FALSE;
	}
	if (r->char_type.race &&
		!(r->char_type.race & (1u << c->char_type.race))) {
		/* Character race is does not meet requirements. */
		return FALSE;
	}
	if (r->char_type.gender && 
		c->char_type.gender != r->char_type.gender) {
		/* Character gender is does not meet requirements. */
		return FALSE;
	}
	if (r->char_type.cs &&
		!(r->char_type.cs & (1u << c->char_type.cs))) {
		/* Character class is does not meet requirements. */
		return FALSE;
	}
	return TRUE;
}

boolean ap_item_check_equip_restriction(
	struct ap_item_module * mod,
	struct ap_item * item,
	struct ap_character * character)
{
	const struct ap_item_template * temp = item->temp;
	const struct ap_factor * r = &temp->factor_restrict;
	const struct ap_factor * c = &character->factor;
	if (temp->type != AP_ITEM_TYPE_EQUIP) {
		/* Not an equipment. */
		return FALSE;
	}
	if (ap_character_get_level(character) < r->char_status.level) {
		/* Character level is too low. */
		return FALSE;
	}
	if (r->char_status.limited_level &&
		ap_character_get_level(character) > r->char_status.limited_level) {
		/* Character level is too high. */
		return FALSE;
	}
	if (r->char_type.race &&
		!(r->char_type.race & (1u << c->char_type.race))) {
		/* Character race is does not meet requirements. */
		return FALSE;
	}
	if (r->char_type.gender && 
		c->char_type.gender != r->char_type.gender) {
		/* Character gender is does not meet requirements. */
		return FALSE;
	}
	if (r->char_type.cs &&
		!(r->char_type.cs & (1u << c->char_type.cs))) {
		/* Character class is does not meet requirements. */
		return FALSE;
	}
	/** \todo Add a callback for equipment restrictions so that 
	 *        other modules can apply custom restrictions. 
	 *        For example, certain items are not allowed in HH. */
	/** \todo Extra restrictions for mounts:
	 *        - Combat status.
	 *        - Character transform status.
	 *        - Whether character is in dungeon.
	 *        - Whether character is ArchLord. */
	return TRUE;
}

struct ap_item * ap_item_to_be_unequiped_together(
	struct ap_item_module * mod,
	struct ap_character * character,
	struct ap_item * item)
{
	if (item->temp->equip.kind == AP_ITEM_EQUIP_KIND_WEAPON &&
		item->temp->equip.weapon_type == AP_ITEM_EQUIP_WEAPON_TYPE_ONE_HAND_RAPIER) {
		/* If a rapier is being unequiped, dagger needs to 
		 * be unequiped first. */
		return ap_item_get_equipment(mod, character, AP_ITEM_PART_HAND_LEFT);
	}
	return NULL;
}

uint32_t ap_item_to_be_unequiped(
	struct ap_item_module * mod,
	struct ap_character * character,
	struct ap_item * item,
	struct ap_item ** list)
{
	uint8_t index = 
		ap_item_get_equip_index(item->temp->equip.part);
	struct ap_item * prev;
	uint32_t count = 0;
	if (index == UINT8_MAX) {
		assert(0);
		return 0;
	}
	prev = ap_grid_get_object_by_index(
		ap_item_get_character_grid(mod, character, AP_ITEM_STATUS_EQUIP),
		index);
	if (prev) {
		switch (prev->temp->equip.part) {
		case AP_ITEM_PART_HAND_RIGHT: {
			struct ap_item * current = ap_item_get_equipment(mod,
				character, AP_ITEM_PART_HAND_LEFT);
			if (current) {
				if (item->temp->factor.item.hand == 2 ||
					current->temp->factor.item.hand == 2) {
					/* If item to be equiped is two-handed,
					 * or item on left hand is two-handed,
					 * remove item on left hand. */
					list[count++] = current;
				}
				else if (current->temp->equip.kind == AP_ITEM_EQUIP_KIND_WEAPON &&
					current->temp->equip.weapon_type == AP_ITEM_EQUIP_WEAPON_TYPE_ONE_HAND_DAGGER &&
					item->temp->equip.weapon_type != AP_ITEM_EQUIP_WEAPON_TYPE_ONE_HAND_RAPIER) {
					/* If item on left hand is a dagger and 
					 * what is being equiped is not a rapier,
					 * remove dagger on left hand. */
					list[count++] = current;
				}
			}
			list[count++] = prev;
			break;
		}
		case AP_ITEM_PART_HAND_LEFT: {
			struct ap_item * current;
			/* Unequip left hand first, this is important 
			 * when rapier and dagger are unequiped together. */
			list[count++] = prev;
			current = ap_item_get_equipment(mod,
				character, AP_ITEM_PART_HAND_RIGHT);
			if (current) {
				if (item->temp->factor.item.hand == 2 ||
					current->temp->factor.item.hand == 2) {
					/* If item to be equiped is two-handed,
					 * or item on right hand is two-handed,
					 * remove item on right hand. */
					list[count++] = current;
				}
			}
			break;
		}
		case AP_ITEM_PART_ACCESSORY_RING1: {
			struct ap_item * other = ap_item_get_equipment(mod,
				character, AP_ITEM_PART_ACCESSORY_RING2);
			if (other) {
				/* If both ring slots are occupied, unequip 
				 * the ring that was equiped earlier. */
				assert(prev != other);
				if (prev->equip_tick < other->equip_tick)
					list[count++] = prev;
				else
					list[count++] = other;
			}
			break;
		}
		default:
			list[count++] = prev;
			break;
		}
	}
	else {
		switch (item->temp->equip.part) {
		case AP_ITEM_PART_HAND_RIGHT: {
			struct ap_item * current = ap_item_get_equipment(mod,
				character, AP_ITEM_PART_HAND_LEFT);
			if (current) {
				if (item->temp->factor.item.hand == 2 ||
					current->temp->factor.item.hand == 2) {
					/* If item to be equiped is two-handed,
					 * or item on left hand is two-handed,
					 * remove item on left hand. */
					list[count++] = current;
				}
				else if (current->temp->equip.kind == AP_ITEM_EQUIP_KIND_WEAPON &&
					current->temp->equip.weapon_type == AP_ITEM_EQUIP_WEAPON_TYPE_ONE_HAND_DAGGER &&
					item->temp->equip.weapon_type != AP_ITEM_EQUIP_WEAPON_TYPE_ONE_HAND_RAPIER) {
					/* If item on left hand is a dagger and 
					 * what is being equiped is not a rapier,
					 * remove dagger on left hand. */
					list[count++] = current;
				}
			}
			break;
		}
		case AP_ITEM_PART_HAND_LEFT: {
			struct ap_item * current = ap_item_get_equipment(mod,
				character, AP_ITEM_PART_HAND_RIGHT);
			if (current) {
				if (item->temp->factor.item.hand == 2 ||
					current->temp->factor.item.hand == 2) {
					/* If item to be equiped is two-handed,
					 * or item on right hand is two-handed,
					 * remove item on right hand. */
					list[count++] = current;
				}
				else if (item->temp->equip.kind == AP_ITEM_EQUIP_KIND_WEAPON &&
					item->temp->equip.weapon_type == AP_ITEM_EQUIP_WEAPON_TYPE_ONE_HAND_DAGGER &&
					current->temp->equip.weapon_type != AP_ITEM_EQUIP_WEAPON_TYPE_ONE_HAND_RAPIER) {
					/* If item that is being equiped is a dagger 
					 * and item on right hand is not a rapier, 
					 * remove item on right hand. */
					list[count++] = current;
				}
			}
			break;
		}
		}
	}
	return count;
}

boolean ap_item_equip(
	struct ap_item_module * mod,
	struct ap_character * character,
	struct ap_item * item,
	uint32_t equip_flags)
{
	struct ap_item_cb_equip cb = { 0 };
	struct ap_grid * grid = ap_item_get_character_grid(mod, character,
		AP_ITEM_STATUS_EQUIP);
	uint8_t index = ap_item_get_equip_index(item->temp->equip.part);
	if (index == UINT8_MAX)
		return FALSE;
	/* Check that target equipment slot is not occupied. */
	if (ap_grid_get_object_by_index_secure(grid, index)) {
		/* If first ring slot is occupied, check second slot. */
		if (item->temp->equip.kind == AP_ITEM_EQUIP_KIND_RING) {
			index = ap_item_get_equip_index(
				AP_ITEM_PART_ACCESSORY_RING2);
			if (index == UINT8_MAX)
				return FALSE;
			if (ap_grid_get_object_by_index_secure(grid, index))
				return FALSE;
		}
		else {
			return FALSE;
		}
	}
	ap_grid_add_item_by_index(grid, index, 
		&item->grid_pos[AP_ITEM_GRID_POS_TAB],
		&item->grid_pos[AP_ITEM_GRID_POS_ROW], 
		&item->grid_pos[AP_ITEM_GRID_POS_COLUMN], 
		AP_GRID_ITEM_TYPE_ITEM, item->id, item->tid, item);
	ap_item_set_status(item, AP_ITEM_STATUS_EQUIP);
	ap_item_set_grid_pos(item,
		item->grid_pos[AP_ITEM_GRID_POS_TAB],
		item->grid_pos[AP_ITEM_GRID_POS_ROW], 
		item->grid_pos[AP_ITEM_GRID_POS_COLUMN]);
	item->equip_flags = equip_flags;
	item->equip_tick = ap_tick_get(mod->ap_tick);
	if (!(equip_flags & AP_ITEM_EQUIP_WITH_NO_STATS)) {
		item->current_link_level = ap_character_get_level(character);
		applystats(mod, character, item, 1);
		addequipstats(mod, character, item);
	}
	cb.character = character;
	cb.item = item;
	ap_module_enum_callback(mod, AP_ITEM_CB_EQUIP, &cb);
	return TRUE;
}

boolean ap_item_unequip(
	struct ap_item_module * mod,
	struct ap_character * character, 
	struct ap_item * item)
{
	struct ap_item_cb_unequip cb = { 0 };
	if (!(item->equip_flags & AP_ITEM_EQUIP_BY_ITEM_IN_USE)) {
		struct ap_grid * grid = ap_item_get_character_grid(mod, character,
			AP_ITEM_STATUS_INVENTORY);
		uint16_t gridpos[3] = { 0 };
		uint32_t index = ap_grid_get_empty(grid, 
			&gridpos[AP_ITEM_GRID_POS_TAB],
			&gridpos[AP_ITEM_GRID_POS_ROW],
			&gridpos[AP_ITEM_GRID_POS_COLUMN]);
		if (index == AP_GRID_INVALID_INDEX)
			return FALSE;
		ap_item_remove_from_grid(mod, character, item);
		ap_grid_add_item(grid, 
			gridpos[AP_ITEM_GRID_POS_TAB],
			gridpos[AP_ITEM_GRID_POS_ROW], 
			gridpos[AP_ITEM_GRID_POS_COLUMN], 
			AP_GRID_ITEM_TYPE_ITEM, item->id, item->tid, item);
		ap_item_set_status(item, AP_ITEM_STATUS_INVENTORY);
		ap_item_set_grid_pos(item,
			gridpos[AP_ITEM_GRID_POS_TAB],
			gridpos[AP_ITEM_GRID_POS_ROW], 
			gridpos[AP_ITEM_GRID_POS_COLUMN]);
	}
	else {
		ap_item_remove_from_grid(mod, character, item);
	}
	if (!(item->equip_flags & AP_ITEM_EQUIP_WITH_NO_STATS)) {
		applystats(mod, character, item, -1);
		removeequipstats(mod, character, item);
	}
	cb.character = character;
	cb.item = item;
	ap_module_enum_callback(mod, AP_ITEM_CB_UNEQUIP, &cb);
	return TRUE;
}

void ap_item_apply_option(
	struct ap_item_module * mod,
	struct ap_character * c, 
	const struct ap_item_option_template * option,
	int modifier)
{
	struct ap_factor * f = &c->factor;
	struct ap_factor * fpoint = &c->factor_point;
	struct ap_factor * fper = &c->factor_percent;
	const struct ap_factor * optionf = &option->factor;
	const struct ap_factor * optionfper = &option->factor_percent;
	const struct ap_item_option_skill_data * skilldata = &option->skill_data;
	struct ap_skill_character * skillattachment = 
		ap_skill_get_character(mod->ap_skill, c);
	struct ap_skill_buffed_combat_effect_arg  * combateffect = 
		&skillattachment->combat_effect_arg;
	struct ap_skill_buffed_factor_effect_arg  * factoreffect = 
		&skillattachment->factor_effect_arg;
	if (optionfper->char_status.movement) {
		fper->char_status.movement += modifier * optionfper->char_status.movement;
		c->update_flags |= AP_FACTORS_BIT_MOVEMENT_SPEED;
	}
	if (optionfper->char_status.movement_fast) {
		fper->char_status.movement_fast += modifier * 
			optionfper->char_status.movement_fast;
		c->update_flags |= AP_FACTORS_BIT_MOVEMENT_SPEED;
	}
	if (optionf->char_point_max.hp || optionfper->char_point_max.hp) {
		fpoint->char_point_max.hp += modifier * optionf->char_point_max.hp;
		fper->char_point_max.hp += modifier * optionfper->char_point_max.hp;
		c->update_flags |= AP_FACTORS_BIT_MAX_HP;
	}
	if (optionf->char_point_max.mp || optionfper->char_point_max.mp) {
		fpoint->char_point_max.mp += modifier * optionf->char_point_max.mp;
		fper->char_point_max.mp += modifier * optionfper->char_point_max.mp;
		c->update_flags |= AP_FACTORS_BIT_MAX_MP;
	}
	if (optionf->char_status.con) {
		f->char_status.con += modifier * optionf->char_status.con;
		c->update_flags |= AP_FACTORS_BIT_CHAR_STATUS | AP_FACTORS_BIT_MAX_HP;
	}
	if (optionf->char_status.str) {
		f->char_status.str += modifier * optionf->char_status.str;
		c->update_flags |= AP_FACTORS_BIT_CHAR_STATUS;
	}
	if (optionf->char_status.intel) {
		f->char_status.intel += modifier * optionf->char_status.intel;
		c->update_flags |= AP_FACTORS_BIT_CHAR_STATUS;
	}
	if (optionf->char_status.agi) {
		f->char_status.agi += modifier * optionf->char_status.agi;
		c->update_flags |= AP_FACTORS_BIT_CHAR_STATUS;
	}
	if (optionf->char_status.wis) {
		f->char_status.wis += modifier * optionf->char_status.str;
		c->update_flags |= AP_FACTORS_BIT_CHAR_STATUS | AP_FACTORS_BIT_MAX_MP;
	}
	if (optionf->char_point_max.attack_rating) {
		fpoint->char_point_max.attack_rating += modifier * optionf->char_point_max.attack_rating;
		c->update_flags |= AP_FACTORS_BIT_ATTACK_RATING;
	}
	if (optionf->char_point_max.defense_rating) {
		fpoint->char_point_max.defense_rating += modifier * optionf->char_point_max.defense_rating;
		c->update_flags |= AP_FACTORS_BIT_DEFENSE_RATING;
	}
	if (optionf->damage.min.physical || optionf->damage.max.physical) {
		fpoint->damage.min.physical += modifier * optionf->damage.min.physical;
		fpoint->damage.max.physical += modifier * optionf->damage.max.physical;
		c->update_flags |= AP_FACTORS_BIT_PHYSICAL_DMG;
	}
	if (optionf->damage.min.magic || optionf->damage.max.magic) {
		f->damage.min.magic += modifier * optionf->damage.min.magic;
		f->damage.max.magic += modifier * optionf->damage.max.magic;
		c->update_flags |= AP_FACTORS_BIT_DAMAGE;
	}
	if (optionf->damage.min.water || optionf->damage.max.water) {
		f->damage.min.water += modifier * optionf->damage.min.water;
		f->damage.max.water += modifier * optionf->damage.max.water;
		c->update_flags |= AP_FACTORS_BIT_DAMAGE;
	}
	if (optionf->damage.min.fire || optionf->damage.max.fire) {
		f->damage.min.fire += modifier * optionf->damage.min.fire;
		f->damage.max.fire += modifier * optionf->damage.max.fire;
		c->update_flags |= AP_FACTORS_BIT_DAMAGE;
	}
	if (optionf->damage.min.earth || optionf->damage.max.earth) {
		f->damage.min.earth += modifier * optionf->damage.min.earth;
		f->damage.max.earth += modifier * optionf->damage.max.earth;
		c->update_flags |= AP_FACTORS_BIT_DAMAGE;
	}
	if (optionf->damage.min.air || optionf->damage.max.air) {
		f->damage.min.air += modifier * optionf->damage.min.air;
		f->damage.max.air += modifier * optionf->damage.max.air;
		c->update_flags |= AP_FACTORS_BIT_DAMAGE;
	}
	if (optionf->damage.min.poison || optionf->damage.max.poison) {
		f->damage.min.poison += modifier * optionf->damage.min.poison;
		f->damage.max.poison += modifier * optionf->damage.max.poison;
		c->update_flags |= AP_FACTORS_BIT_DAMAGE;
	}
	if (optionf->damage.min.lightning || optionf->damage.max.lightning) {
		f->damage.min.lightning += modifier * optionf->damage.min.lightning;
		f->damage.max.lightning += modifier * optionf->damage.max.lightning;
		c->update_flags |= AP_FACTORS_BIT_DAMAGE;
	}
	if (optionf->damage.min.ice || optionf->damage.max.ice) {
		f->damage.min.ice += modifier * optionf->damage.min.ice;
		f->damage.max.ice += modifier * optionf->damage.max.ice;
		c->update_flags |= AP_FACTORS_BIT_DAMAGE;
	}
	if (optionf->damage.min.heroic || optionf->damage.max.heroic) {
		f->damage.min.heroic += modifier * optionf->damage.min.heroic;
		f->damage.max.heroic += modifier * optionf->damage.max.heroic;
		c->update_flags |= AP_FACTORS_BIT_HEROIC_DMG;
	}
	if (optionf->damage.min.heroic_melee || optionf->damage.max.heroic_melee) {
		f->damage.min.heroic_melee += modifier * optionf->damage.min.heroic_melee;
		f->damage.max.heroic_melee += modifier * optionf->damage.max.heroic_melee;
		c->update_flags |= AP_FACTORS_BIT_HEROIC_DMG;
	}
	if (optionf->damage.min.heroic_ranged || optionf->damage.max.heroic_ranged) {
		f->damage.min.heroic_ranged += modifier * optionf->damage.min.heroic_ranged;
		f->damage.max.heroic_ranged += modifier * optionf->damage.max.heroic_ranged;
		c->update_flags |= AP_FACTORS_BIT_HEROIC_DMG;
	}
	if (optionf->damage.min.heroic_magic || optionf->damage.max.heroic_magic) {
		f->damage.min.heroic_magic += modifier * optionf->damage.min.heroic_magic;
		f->damage.max.heroic_magic += modifier * optionf->damage.max.heroic_magic;
		c->update_flags |= AP_FACTORS_BIT_HEROIC_DMG;
	}
	if (optionf->defense.point.physical) {
		f->defense.point.physical += modifier * optionf->defense.point.physical;
		c->update_flags |= AP_FACTORS_BIT_DEFENSE;
	}
	if (optionf->defense.point.magic) {
		f->defense.point.magic += modifier * optionf->defense.point.magic;
		c->update_flags |= AP_FACTORS_BIT_DEFENSE;
	}
	if (optionf->defense.point.water) {
		f->defense.point.water += modifier * optionf->defense.point.water;
		c->update_flags |= AP_FACTORS_BIT_DEFENSE;
	}
	if (optionf->defense.point.fire) {
		f->defense.point.fire += modifier * optionf->defense.point.fire;
		c->update_flags |= AP_FACTORS_BIT_DEFENSE;
	}
	if (optionf->defense.point.earth) {
		f->defense.point.earth += modifier * optionf->defense.point.earth;
		c->update_flags |= AP_FACTORS_BIT_DEFENSE;
	}
	if (optionf->defense.point.air) {
		f->defense.point.air += modifier * optionf->defense.point.air;
		c->update_flags |= AP_FACTORS_BIT_DEFENSE;
	}
	if (optionf->defense.point.poison) {
		f->defense.point.poison += modifier * optionf->defense.point.poison;
		c->update_flags |= AP_FACTORS_BIT_DEFENSE;
	}
	if (optionf->defense.point.lightning) {
		f->defense.point.lightning += modifier * optionf->defense.point.lightning;
		c->update_flags |= AP_FACTORS_BIT_DEFENSE;
	}
	if (optionf->defense.point.ice) {
		f->defense.point.ice += modifier * optionf->defense.point.ice;
		c->update_flags |= AP_FACTORS_BIT_DEFENSE;
	}
	if (optionf->defense.point.heroic) {
		f->defense.point.heroic += modifier * optionf->defense.point.heroic;
		c->update_flags |= AP_FACTORS_BIT_HEROIC_DMG;
	}
	if (optionf->defense.rate.physical) {
		f->defense.rate.physical += modifier * optionf->defense.rate.physical;
		c->update_flags |= AP_FACTORS_BIT_DEFENSE;
	}
	if (optionf->defense.rate.physical_block) {
		f->defense.rate.physical_block += modifier * optionf->defense.rate.physical_block;
		c->update_flags |= AP_FACTORS_BIT_DEFENSE;
	}
	if (optionf->defense.rate.skill_block) {
		f->defense.rate.skill_block += modifier * optionf->defense.rate.skill_block;
		c->update_flags |= AP_FACTORS_BIT_DEFENSE;
	}
	if (optionf->attack.skill_cast) {
		f->attack.skill_cast += modifier * optionf->attack.skill_cast;
		c->update_flags |= AP_FACTORS_BIT_ATTACK;
	}
	if (optionf->attack.skill_cooldown) {
		f->attack.skill_cooldown += modifier * optionf->attack.skill_cooldown;
		c->update_flags |= AP_FACTORS_BIT_ATTACK;
	}
	if (optionf->attack.accuracy) {
		f->attack.accuracy += modifier * optionf->attack.accuracy;
		c->update_flags |= AP_FACTORS_BIT_ATTACK;
	}
	if (optionf->attack.evade_rate) {
		f->attack.evade_rate += modifier * optionf->attack.evade_rate;
		c->update_flags |= AP_FACTORS_BIT_ATTACK;
	}
	if (optionf->attack.dodge_rate) {
		f->attack.dodge_rate += modifier * optionf->attack.dodge_rate;
		c->update_flags |= AP_FACTORS_BIT_ATTACK;
	}
	if (optionfper->attack.attack_speed) {
		fper->attack.attack_speed += modifier * optionfper->attack.attack_speed;
		c->update_flags |= AP_FACTORS_BIT_ATTACK_SPEED;
	}
	switch (option->skill_type) {
	case AP_ITEM_OPTION_SKILL_TYPE_CRITICAL:
		combateffect->melee_critical_rate += modifier * skilldata->critical_rate;
		combateffect->melee_critical_damage_adjust_rate += modifier * skilldata->critical_damage;
		break;
	case AP_ITEM_OPTION_SKILL_TYPE_STUN:
		/** \todo On-hit stun rate/duration. */
		break;
	case AP_ITEM_OPTION_SKILL_TYPE_DMG_CONVERT_HP:
		combateffect->damage_to_hp_rate[0] += modifier * skilldata->damage_convert_hp_rate;
		combateffect->damage_to_hp_amount[0] += modifier * skilldata->damage_convert_hp;
		break;
	case AP_ITEM_OPTION_SKILL_TYPE_REGEN_HP:
		c->factor.char_point_recovery_rate.hp += modifier * skilldata->regen_hp;
		c->factor.char_point_recovery_rate.mp += modifier * skilldata->regen_mp;
		break;
	}
	if (skilldata->skill_level_up)
		combateffect->skill_level_up_point += modifier * skilldata->skill_level_up;
	if (skilldata->bonus_exp)
		factoreffect->bonus_exp_rate += modifier * skilldata->bonus_exp;
	if (skilldata->bonus_gold)
		factoreffect->bonus_gold_rate += modifier * skilldata->bonus_gold;
	if (skilldata->bonus_drop_rate)
		factoreffect->bonus_drop_rate += modifier * skilldata->bonus_drop_rate;
	if (skilldata->bonus_rare_drop_rate)
		factoreffect->bonus_rare_drop_rate += modifier * skilldata->bonus_rare_drop_rate;
	if (skilldata->bonus_charisma_rate)
		factoreffect->bonus_charisma_rate += modifier * skilldata->bonus_charisma_rate;
	if (skilldata->ignore_defense)
		combateffect->ignore_physical_defense += modifier * skilldata->ignore_defense;
	if (skilldata->ignore_elemental_resistance)
		combateffect->ignore_attribute_defense += modifier * skilldata->ignore_elemental_resistance;
	if (skilldata->critical_resistance)
		combateffect->critical_hit_resistance += modifier * skilldata->critical_resistance;
	skillattachment->sync_combat_effect_arg = TRUE;
}

void ap_item_sync_with_character_level(
	struct ap_item_module * mod,
	struct ap_character * character)
{
	struct ap_grid * grid = ap_item_get_character_grid(mod, character, 
		AP_ITEM_STATUS_EQUIP);
	uint32_t i;
	uint32_t level = ap_character_get_level(character);
	for (i = 0; i < grid->grid_count; i++) {
		struct ap_item * item = ap_grid_get_object_by_index(grid, i);
		boolean canequip;
		if (!item)
			continue;
		canequip = ap_item_check_equip_restriction(mod, item, character);
		if (CHECK_BIT(item->equip_flags, AP_ITEM_EQUIP_WITH_NO_STATS)) {
			if (canequip) {
				CLEAR_BIT(item->equip_flags, AP_ITEM_EQUIP_WITH_NO_STATS);
				item->current_link_level = level;
				applystats(mod, character, item, 1);
				addequipstats(mod, character, item);
			}
		}
		else if (!canequip) {
			SET_BIT(item->equip_flags, AP_ITEM_EQUIP_WITH_NO_STATS);
			applystats(mod, character, item, -1);
			removeequipstats(mod, character, item);
		}
		else if (item->current_link_level != level) {
			applylinkoptions(mod, character, item, item->current_link_level, -1);
			item->current_link_level = level;
			applylinkoptions(mod, character, item, item->current_link_level, 1);
		}
	}
	ap_character_update_factor(mod->ap_character, character, 0);
}

struct ap_grid * ap_item_get_character_grid(
	struct ap_item_module * mod,
	struct ap_character * character,
	enum ap_item_status item_status)
{
	struct ap_item_character * ic = ap_item_get_character(mod, character);
	switch (item_status) {
	case AP_ITEM_STATUS_INVENTORY:
		return ic->inventory;
	case AP_ITEM_STATUS_EQUIP:
		return ic->equipment;
	case AP_ITEM_STATUS_SUB_INVENTORY:
		return ic->sub_inventory;
	case AP_ITEM_STATUS_CASH_INVENTORY:
		return ic->cash_inventory;
	case AP_ITEM_STATUS_BANK:
		return ic->bank;
	default:
		return NULL;
	}
}

boolean ap_item_on_receive(
	struct ap_item_module * mod,
	const void * data,
	uint16_t length,
	void * user_data)
{
	struct ap_item_cb_receive cb = { 0 };
	const char * skillinit = NULL;
	const char * nickname = NULL;
	struct ap_item_grid_pos_in_packet inventory = { 0 };
	struct ap_item_grid_pos_in_packet bank = { 0 };
	const void * invenpacket = NULL;
	const void * bankpacket = NULL;
	if (!au_packet_get_field(&mod->packet, TRUE, data, length,
			&cb.type,
			&cb.status, /* Status */
			&cb.item_id, /* Item ID */
			NULL, /* Item Template ID */
			NULL, /* Item Owner CID */
			&cb.stack_count, /* Item Count */
			NULL, /* Field */
			&invenpacket, /* Inventory */
			&bankpacket, /* Bank */
			NULL, /* Equip */
			NULL, /* Factors */
			NULL, /* Percent Factors */
			NULL, /* target character id (case usable (scroll) item) */
			NULL, /* Convert packet */
			NULL, /* restrict factor packet */
			NULL, /* Ego Item packet */
			NULL, /* Quest Item packet */
			NULL, /* skill template id */
			NULL, /* reuse time for reverse orb */
			NULL, /* status flag */
			NULL, /* option packet */
			NULL, /* skill plus packet */
			NULL, /* reuse time for transform */
			NULL, /* CashItem adding information */
			NULL)) { /* Extra information */
		return FALSE;
	}
	if (invenpacket) {
		if (!au_packet_get_field(&mod->packet_inventory, FALSE,
				invenpacket, 0,
				&inventory.layer,
				&inventory.row,
				&inventory.column)) {
			return FALSE;
		}
		cb.inventory = &inventory;
	}
	if (bankpacket) {
		if (!au_packet_get_field(&mod->packet_bank, FALSE,
				bankpacket, 0,
				&bank.layer,
				&bank.row,
				&bank.column)) {
			return FALSE;
		}
		cb.bank = &bank;
	}
	cb.user_data = user_data;
	return ap_module_enum_callback(mod, 
		AP_ITEM_CB_RECEIVE, &cb);
}

static inline void makeaddpacket(
	struct ap_item_module * mod,
	const struct ap_item * item,
	void * buffer,
	uint16_t * length)
{
	uint8_t type = AP_ITEM_PACKET_TYPE_ADD;
	void * fieldpacket = NULL;
	void * invenpacket = NULL;
	void * bankpacket = NULL;
	void * questpacket = NULL;
	void * factorpacket = NULL;
	void * optionpacket = NULL;
	void * skillpacket = NULL;
	void * cashpacket = NULL;
	uint32_t bufcount = 0;
	switch (item->status) {
	case AP_ITEM_STATUS_FIELD:
		fieldpacket = ap_packet_get_temp_buffer(mod->ap_packet);
		au_packet_make_packet(
			&mod->packet_field, 
			fieldpacket, FALSE, NULL, 0,
			&item->position);
		bufcount++;
		break;
	case AP_ITEM_STATUS_INVENTORY:
	case AP_ITEM_STATUS_TRADE_GRID:
	case AP_ITEM_STATUS_CLIENT_TRADE_GRID:
	case AP_ITEM_STATUS_NPC_TRADE:
	case AP_ITEM_STATUS_SALESBOX_GRID:
	case AP_ITEM_STATUS_GUILD_WAREHOUSE:
	case AP_ITEM_STATUS_SUB_INVENTORY:
		invenpacket = ap_packet_get_temp_buffer(mod->ap_packet);
		au_packet_make_packet(
			&mod->packet_inventory, 
			invenpacket, FALSE, NULL, 0,
			&item->grid_pos[AP_ITEM_GRID_POS_TAB],
			&item->grid_pos[AP_ITEM_GRID_POS_ROW],
			&item->grid_pos[AP_ITEM_GRID_POS_COLUMN]);
		bufcount++;
		break;
	case AP_ITEM_STATUS_BANK:
		bankpacket = ap_packet_get_temp_buffer(mod->ap_packet);
		au_packet_make_packet(
			&mod->packet_bank, 
			bankpacket, FALSE, NULL, 0,
			&item->grid_pos[AP_ITEM_GRID_POS_TAB],
			&item->grid_pos[AP_ITEM_GRID_POS_ROW],
			&item->grid_pos[AP_ITEM_GRID_POS_COLUMN]);
		bufcount++;
		break;
	case AP_ITEM_STATUS_QUEST:
		questpacket = ap_packet_get_temp_buffer(mod->ap_packet);
		au_packet_make_packet(
			&mod->packet_quest, 
			questpacket, FALSE, NULL, 0,
			&item->grid_pos[AP_ITEM_GRID_POS_TAB],
			&item->grid_pos[AP_ITEM_GRID_POS_ROW],
			&item->grid_pos[AP_ITEM_GRID_POS_COLUMN]);
		bufcount++;
		break;
	}
	if (item->status == AP_ITEM_STATUS_CASH_INVENTORY || item->expire_time) {
		cashpacket = ap_packet_get_temp_buffer(mod->ap_packet);
		au_packet_make_packet(
			&mod->packet_cash, 
			cashpacket, FALSE, NULL, 0,
			&item->in_use, /*	inuse */
			&item->remain_time, /*	RemainTime */
			&item->expire_time, /* ExpireTime */
			&item->cash_item_use_count, /*	EnableTrade */
			&item->stamina_remain_time); /*	StaminaRemainTime */
		bufcount++;
	}
	else if (item->temp->equip.kind == AP_ITEM_EQUIP_KIND_WEAPON) {
		factorpacket = ap_packet_get_temp_buffer(mod->ap_packet);
		ap_factors_make_packet(mod->ap_factors, factorpacket, &item->factor,
			AP_FACTORS_BIT_DAMAGE);
		bufcount++;
	}
	else if (item->temp->equip.kind == AP_ITEM_EQUIP_KIND_SHIELD ||
		item->temp->equip.part == AP_ITEM_PART_BODY || 
		item->temp->equip.part == AP_ITEM_PART_LEGS) {
		factorpacket = ap_packet_get_temp_buffer(mod->ap_packet);
		ap_factors_make_packet(mod->ap_factors, factorpacket, &item->factor,
			AP_FACTORS_BIT_DEFENSE);
		bufcount++;
	}
	optionpacket = makeoptionpacket(mod, item);
	if (optionpacket)
		bufcount++;
	au_packet_make_packet(&mod->packet, 
		buffer, TRUE, length,
		AP_ITEM_PACKET_TYPE, 
		&type,
		&item->status, /* Status */
		&item->id, /* Item ID */
		&item->tid, /* Item Template ID */
		(item->character_id != AP_INVALID_CID) ? 
			&item->character_id : NULL, /* Item Owner CID */
		&item->stack_count, /* Item Count */
		fieldpacket, /* Field */
		invenpacket, /* Inventory */
		bankpacket, /* Bank */
		NULL, /* Equip */
		factorpacket, /* Factors */
		NULL, /* Percent Factors */
		NULL, /* target character id (case usable (scroll) item) */
		NULL, /* Convert packet */
		NULL, /* restrict factor packet */
		NULL, /* Ego Item packet */
		NULL, /* Quest Item packet */
		NULL, /* skill template id */
		NULL, /* reuse time for reverse orb */
		&item->status_flags, /* status flag */
		optionpacket, /* option packet */
		skillpacket, /* skill plus packet */
		NULL, /* reuse time for transform */
		cashpacket, /* CashItem adding information */
		NULL); /* Extra information */
	ap_packet_pop_temp_buffers(mod->ap_packet, bufcount);
}

void ap_item_make_add_packet(
	struct ap_item_module * mod,
	const struct ap_item * item)
{
	uint16_t length = 0;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	makeaddpacket(mod, item, buffer, &length);
	ap_packet_set_length(mod->ap_packet, length);
}

void ap_item_make_add_packet_buffer(
	struct ap_item_module * mod,
	const struct ap_item * item,
	void * buffer,
	uint16_t * length)
{
	makeaddpacket(mod, item, buffer, length);
}

void ap_item_make_remove_packet(
	struct ap_item_module * mod,
	const struct ap_item * item)
{
	uint8_t type = AP_ITEM_PACKET_TYPE_REMOVE;
	uint16_t length = 0;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	au_packet_make_packet(&mod->packet, 
		buffer, TRUE, &length,
		AP_ITEM_PACKET_TYPE, 
		&type,
		NULL, /* Status */
		&item->id, /* Item ID */
		NULL, /* Item Template ID */
		(item->character_id != AP_INVALID_CID) ? 
			&item->character_id : NULL, /* Item Owner CID */
		NULL, /* Item Count */
		NULL, /* Field */
		NULL, /* Inventory */
		NULL, /* Bank */
		NULL, /* Equip */
		NULL, /* Factors */
		NULL, /* Percent Factors */
		NULL, /* target character id (case usable (scroll) item) */
		NULL, /* Convert packet */
		NULL, /* restrict factor packet */
		NULL, /* Ego Item packet */
		NULL, /* Quest Item packet */
		NULL, /* skill template id */
		NULL, /* reuse time for reverse orb */
		NULL, /* status flag */
		NULL, /* option packet */
		NULL, /* skill plus packet */
		NULL, /* reuse time for transform */
		NULL, /* CashItem adding information */
		NULL); /* Extra information */
	ap_packet_set_length(mod->ap_packet, length);
}

void ap_item_make_update_packet(
	struct ap_item_module * mod,
	const struct ap_item * item,
	uint32_t update_flags)
{
	uint8_t type = AP_ITEM_PACKET_TYPE_UPDATE;
	uint16_t length = 0;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	void * fieldpacket = NULL;
	void * invenpacket = NULL;
	void * bankpacket = NULL;
	void * questpacket = NULL;
	void * factorpacket = NULL;
	void * optionpacket = NULL;
	void * skillpacket = NULL;
	void * cashpacket = NULL;
	if (update_flags & AP_ITEM_UPDATE_GRID_POS) {
		switch (item->status) {
		case AP_ITEM_STATUS_FIELD:
			fieldpacket = ap_packet_get_temp_buffer(mod->ap_packet);
			au_packet_make_packet(
				&mod->packet_field, 
				fieldpacket, FALSE, NULL, 0,
				&item->position);
			break;
		case AP_ITEM_STATUS_INVENTORY:
		case AP_ITEM_STATUS_TRADE_GRID:
		case AP_ITEM_STATUS_CLIENT_TRADE_GRID:
		case AP_ITEM_STATUS_NPC_TRADE:
		case AP_ITEM_STATUS_SALESBOX_GRID:
		case AP_ITEM_STATUS_GUILD_WAREHOUSE:
		case AP_ITEM_STATUS_SUB_INVENTORY:
			invenpacket = ap_packet_get_temp_buffer(mod->ap_packet);
			au_packet_make_packet(
				&mod->packet_inventory, 
				invenpacket, FALSE, NULL, 0,
				&item->grid_pos[AP_ITEM_GRID_POS_TAB],
				&item->grid_pos[AP_ITEM_GRID_POS_ROW],
				&item->grid_pos[AP_ITEM_GRID_POS_COLUMN]);
			break;
		case AP_ITEM_STATUS_BANK:
			bankpacket = ap_packet_get_temp_buffer(mod->ap_packet);
			au_packet_make_packet(
				&mod->packet_bank, 
				bankpacket, FALSE, NULL, 0,
				&item->grid_pos[AP_ITEM_GRID_POS_TAB],
				&item->grid_pos[AP_ITEM_GRID_POS_ROW],
				&item->grid_pos[AP_ITEM_GRID_POS_COLUMN]);
			break;
		case AP_ITEM_STATUS_QUEST:
			questpacket = ap_packet_get_temp_buffer(mod->ap_packet);
			au_packet_make_packet(
				&mod->packet_quest, 
				questpacket, FALSE, NULL, 0,
				&item->grid_pos[AP_ITEM_GRID_POS_TAB],
				&item->grid_pos[AP_ITEM_GRID_POS_ROW],
				&item->grid_pos[AP_ITEM_GRID_POS_COLUMN]);
			break;
		}
	}
	if (update_flags & AP_ITEM_UPDATE_FACTORS) {
		if (item->temp->equip.kind == AP_ITEM_EQUIP_KIND_WEAPON) {
			factorpacket = ap_packet_get_temp_buffer(mod->ap_packet);
			ap_factors_make_packet(mod->ap_factors, factorpacket, &item->factor,
				AP_FACTORS_BIT_DAMAGE);
		}
		else if (item->temp->equip.kind == AP_ITEM_EQUIP_KIND_SHIELD ||
			item->temp->equip.part == AP_ITEM_PART_BODY || 
			item->temp->equip.part == AP_ITEM_PART_LEGS) {
			factorpacket = ap_packet_get_temp_buffer(mod->ap_packet);
			ap_factors_make_packet(mod->ap_factors, factorpacket, &item->factor,
				AP_FACTORS_BIT_DEFENSE);
		}
	}
	au_packet_make_packet(&mod->packet, 
		buffer, TRUE, &length,
		AP_ITEM_PACKET_TYPE, 
		&type,
		&item->status, /* Status */
		&item->id, /* Item ID */
		&item->tid, /* Item Template ID */
		(item->character_id != AP_INVALID_CID) ? 
			&item->character_id : NULL, /* Item Owner CID */
		&item->stack_count, /* Item Count */
		fieldpacket, /* Field */
		invenpacket, /* Inventory */
		bankpacket, /* Bank */
		NULL, /* Equip */
		factorpacket, /* Factors */
		NULL, /* Percent Factors */
		NULL, /* target character id (case usable (scroll) item) */
		NULL, /* Convert packet */
		NULL, /* restrict factor packet */
		NULL, /* Ego Item packet */
		NULL, /* Quest Item packet */
		NULL, /* skill template id */
		NULL, /* reuse time for reverse orb */
		&item->status_flags, /* status flag */
		optionpacket, /* option packet */
		skillpacket, /* skill plus packet */
		NULL, /* reuse time for transform */
		cashpacket, /* CashItem adding information */
		NULL); /* Extra information */
	ap_packet_set_length(mod->ap_packet, length);
	ap_packet_reset_temp_buffers(mod->ap_packet);
}

void ap_item_make_use_item_by_tid_packet(
	struct ap_item_module * mod,
	uint32_t item_tid,
	uint32_t character_id,
	uint32_t target_id,
	uint32_t reuse_ms)
{
	uint8_t type = AP_ITEM_PACKET_TYPE_USE_ITEM_BY_TID;
	uint16_t length = 0;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	au_packet_make_packet(&mod->packet, 
		buffer, TRUE, &length,
		AP_ITEM_PACKET_TYPE, 
		&type,
		NULL, /* Status */
		NULL, /* Item ID */
		&item_tid, /* Item Template ID */
		&character_id, /* Item Owner CID */
		NULL, /* Item Count */
		NULL, /* Field */
		NULL, /* Inventory */
		NULL, /* Bank */
		NULL, /* Equip */
		NULL, /* Factors */
		NULL, /* Percent Factors */
		&target_id, /* target character id (case usable (scroll) item) */
		NULL, /* Convert packet */
		NULL, /* restrict factor packet */
		NULL, /* Ego Item packet */
		NULL, /* Quest Item packet */
		NULL, /* skill template id */
		&reuse_ms, /* reuse time for reverse orb */
		NULL, /* status flag */
		NULL, /* option packet */
		NULL, /* skill plus packet */
		NULL, /* reuse time for transform */
		NULL, /* CashItem adding information */
		NULL); /* Extra information */
	ap_packet_set_length(mod->ap_packet, length);
}

void ap_item_make_enable_return_scroll_packet(
	struct ap_item_module * mod,
	uint32_t character_id)
{
	uint8_t type = AP_ITEM_PACKET_TYPE_ENABLE_RETURN_SCROLL;
	uint16_t length = 0;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	au_packet_make_packet(&mod->packet, 
		buffer, TRUE, &length,
		AP_ITEM_PACKET_TYPE, 
		&type,
		NULL, /* Status */
		NULL, /* Item ID */
		NULL, /* Item Template ID */
		&character_id, /* Item Owner CID */
		NULL, /* Item Count */
		NULL, /* Field */
		NULL, /* Inventory */
		NULL, /* Bank */
		NULL, /* Equip */
		NULL, /* Factors */
		NULL, /* Percent Factors */
		NULL, /* target character id (case usable (scroll) item) */
		NULL, /* Convert packet */
		NULL, /* restrict factor packet */
		NULL, /* Ego Item packet */
		NULL, /* Quest Item packet */
		NULL, /* skill template id */
		NULL, /* reuse time for reverse orb */
		NULL, /* status flag */
		NULL, /* option packet */
		NULL, /* skill plus packet */
		NULL, /* reuse time for transform */
		NULL, /* CashItem adding information */
		NULL); /* Extra information */
	ap_packet_set_length(mod->ap_packet, length);
}

void ap_item_make_disable_return_scroll_packet(
	struct ap_item_module * mod,
	uint32_t character_id)
{
	uint8_t type = AP_ITEM_PACKET_TYPE_DISABLE_RETURN_SCROLL;
	uint16_t length = 0;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	au_packet_make_packet(&mod->packet, 
		buffer, TRUE, &length,
		AP_ITEM_PACKET_TYPE, 
		&type,
		NULL, /* Status */
		NULL, /* Item ID */
		NULL, /* Item Template ID */
		&character_id, /* Item Owner CID */
		NULL, /* Item Count */
		NULL, /* Field */
		NULL, /* Inventory */
		NULL, /* Bank */
		NULL, /* Equip */
		NULL, /* Factors */
		NULL, /* Percent Factors */
		NULL, /* target character id (case usable (scroll) item) */
		NULL, /* Convert packet */
		NULL, /* restrict factor packet */
		NULL, /* Ego Item packet */
		NULL, /* Quest Item packet */
		NULL, /* skill template id */
		NULL, /* reuse time for reverse orb */
		NULL, /* status flag */
		NULL, /* option packet */
		NULL, /* skill plus packet */
		NULL, /* reuse time for transform */
		NULL, /* CashItem adding information */
		NULL); /* Extra information */
	ap_packet_set_length(mod->ap_packet, length);
}

void ap_item_make_pick_up_item_result_packet(
	struct ap_item_module * mod,
	const struct ap_item * item,
	enum ap_item_pick_up_result result)
{
	uint8_t type = AP_ITEM_PACKET_TYPE_PICKUP_ITEM_RESULT;
	uint16_t length = 0;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	au_packet_make_packet(&mod->packet, 
		buffer, TRUE, &length,
		AP_ITEM_PACKET_TYPE, 
		&type,
		&result, /* Status */
		&item->id, /* Item ID */
		&item->tid, /* Item Template ID */
		NULL, /* Item Owner CID */
		&item->stack_count, /* Item Count */
		NULL, /* Field */
		NULL, /* Inventory */
		NULL, /* Bank */
		NULL, /* Equip */
		NULL, /* Factors */
		NULL, /* Percent Factors */
		NULL, /* target character id (case usable (scroll) item) */
		NULL, /* Convert packet */
		NULL, /* restrict factor packet */
		NULL, /* Ego Item packet */
		NULL, /* Quest Item packet */
		NULL, /* skill template id */
		NULL, /* reuse time for reverse orb */
		NULL, /* status flag */
		NULL, /* option packet */
		NULL, /* skill plus packet */
		NULL, /* reuse time for transform */
		NULL, /* CashItem adding information */
		NULL); /* Extra information */
	ap_packet_set_length(mod->ap_packet, length);
}

void ap_item_make_custom_pick_up_item_result_packet(
	struct ap_item_module * mod,
	uint32_t item_tid,
	uint32_t stack_count,
	enum ap_item_pick_up_result result)
{
	uint8_t type = AP_ITEM_PACKET_TYPE_PICKUP_ITEM_RESULT;
	uint16_t length = 0;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	au_packet_make_packet(&mod->packet, 
		buffer, TRUE, &length,
		AP_ITEM_PACKET_TYPE, 
		&type,
		&result, /* Status */
		NULL, /* Item ID */
		&item_tid, /* Item Template ID */
		NULL, /* Item Owner CID */
		&stack_count, /* Item Count */
		NULL, /* Field */
		NULL, /* Inventory */
		NULL, /* Bank */
		NULL, /* Equip */
		NULL, /* Factors */
		NULL, /* Percent Factors */
		NULL, /* target character id (case usable (scroll) item) */
		NULL, /* Convert packet */
		NULL, /* restrict factor packet */
		NULL, /* Ego Item packet */
		NULL, /* Quest Item packet */
		NULL, /* skill template id */
		NULL, /* reuse time for reverse orb */
		NULL, /* status flag */
		NULL, /* option packet */
		NULL, /* skill plus packet */
		NULL, /* reuse time for transform */
		NULL, /* CashItem adding information */
		NULL); /* Extra information */
	ap_packet_set_length(mod->ap_packet, length);
}

void ap_item_make_update_reuse_time_for_reverse_orb_packet(
	struct ap_item_module * mod,
	uint32_t character_id,
	uint32_t reuse_time)
{
	uint8_t type = AP_ITEM_PACKET_TYPE_UPDATE_REUSE_TIME_FOR_REVERSE_ORB;
	uint16_t length = 0;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	au_packet_make_packet(&mod->packet, 
		buffer, TRUE, &length,
		AP_ITEM_PACKET_TYPE, 
		&type,
		NULL, /* Status */
		NULL, /* Item ID */
		NULL, /* Item Template ID */
		&character_id, /* Item Owner CID */
		NULL, /* Item Count */
		NULL, /* Field */
		NULL, /* Inventory */
		NULL, /* Bank */
		NULL, /* Equip */
		NULL, /* Factors */
		NULL, /* Percent Factors */
		NULL, /* target character id (case usable (scroll) item) */
		NULL, /* Convert packet */
		NULL, /* restrict factor packet */
		NULL, /* Ego Item packet */
		NULL, /* Quest Item packet */
		NULL, /* skill template id */
		&reuse_time, /* reuse time for reverse orb */
		NULL, /* status flag */
		NULL, /* option packet */
		NULL, /* skill plus packet */
		NULL, /* reuse time for transform */
		NULL, /* CashItem adding information */
		NULL); /* Extra information */
	ap_packet_set_length(mod->ap_packet, length);
}

void ap_item_make_update_use_time_packet(
	struct ap_item_module * mod,
	const struct ap_item * item)
{
	uint8_t type = AP_ITEM_PACKET_TYPE_UPDATE_ITEM_USE_TIME;
	uint16_t length = 0;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	void * cashpacket = ap_packet_get_temp_buffer(mod->ap_packet);
	au_packet_make_packet(
		&mod->packet_cash, 
		cashpacket, FALSE, NULL, 0,
		&item->in_use, /*	inuse */
		&item->remain_time, /*	RemainTime */
		&item->expire_time, /* ExpireTime */
		&item->cash_item_use_count, /*	EnableTrade */
		&item->stamina_remain_time); /*	StaminaRemainTime */
	au_packet_make_packet(&mod->packet, 
		buffer, TRUE, &length,
		AP_ITEM_PACKET_TYPE, 
		&type,
		NULL, /* Status */
		&item->id, /* Item ID */
		NULL, /* Item Template ID */
		&item->character_id, /* Item Owner CID */
		NULL, /* Item Count */
		NULL, /* Field */
		NULL, /* Inventory */
		NULL, /* Bank */
		NULL, /* Equip */
		NULL, /* Factors */
		NULL, /* Percent Factors */
		NULL, /* target character id (case usable (scroll) item) */
		NULL, /* Convert packet */
		NULL, /* restrict factor packet */
		NULL, /* Ego Item packet */
		NULL, /* Quest Item packet */
		NULL, /* skill template id */
		NULL, /* reuse time for reverse orb */
		NULL, /* status flag */
		NULL, /* option packet */
		NULL, /* skill plus packet */
		NULL, /* reuse time for transform */
		cashpacket, /* CashItem adding information */
		NULL); /* Extra information */
	ap_packet_set_length(mod->ap_packet, length);
	ap_packet_reset_temp_buffers(mod->ap_packet);
}
