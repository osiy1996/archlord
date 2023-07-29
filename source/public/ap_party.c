#include "public/ap_party.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"
#include "core/vector.h"

#include "public/ap_admin.h"
#include "public/ap_character.h"
#include "public/ap_packet.h"

#include "utility/au_packet.h"
#include "utility/au_table.h"

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>

struct ap_party_module {
	struct ap_module_instance instance;
	struct ap_character_module * ap_character;
	struct ap_packet_module * ap_packet;
	struct au_packet packet;
	struct au_packet packet_effect_area;
	struct au_packet packet_bonus_stats;
	struct au_packet packet_management;
	struct ap_party * freelist;
	size_t character_attachment_offset;
};

static boolean onregister(
	struct ap_party_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_character, AP_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_packet, AP_PACKET_MODULE_NAME);
	mod->character_attachment_offset = ap_character_attach_data(mod->ap_character,
		AP_CHARACTER_MDI_CHAR, sizeof(struct ap_party_character_attachment),
		mod, NULL, NULL);
	if (mod->character_attachment_offset == SIZE_MAX) {
		ERROR("Failed to attach character data.");
		return FALSE;
	}
	return TRUE;
}

static void onclose(struct ap_party_module * mod)
{
	struct ap_party * party = mod->freelist;
	while (party) {
		struct ap_party * next = party->next;
		ap_module_destroy_module_data(mod, AP_PARTY_MDI_PARTY, party);
		party = next;
	}
	mod->freelist = NULL;
}

static void onshutdown(struct ap_party_module * mod)
{
}

struct ap_party_module * ap_party_create_module()
{
	struct ap_party_module * mod = ap_module_instance_new(AP_PARTY_MODULE_NAME,
		sizeof(*mod), onregister, NULL, onclose, onshutdown);
	ap_module_set_module_data(mod, AP_PARTY_MDI_PARTY, sizeof(struct ap_party),
		NULL, NULL);
	au_packet_init(&mod->packet, sizeof(uint16_t),
		AU_PACKET_TYPE_INT8, 1, /* Operation */
		AU_PACKET_TYPE_INT32, 1, /* Party ID */
		AU_PACKET_TYPE_INT8, 1, /* # of MaxMember */
		AU_PACKET_TYPE_INT8, 1, /* # of current member */
		AU_PACKET_TYPE_INT32, 1, /* Add Remove member */
		AU_PACKET_TYPE_MEMORY_BLOCK, 1, /* party member list */
		AU_PACKET_TYPE_PACKET, 1, /* factor */
		AU_PACKET_TYPE_PACKET, 1, /* effect area */
		AU_PACKET_TYPE_INT8, 1, /* AgpmPartyCalcExpType */
		AU_PACKET_TYPE_PACKET, 1, /* Bonus Stats */
		AU_PACKET_TYPE_INT8, 1, /* ItemDivisionType */
		AU_PACKET_TYPE_END);
	au_packet_init(&mod->packet_effect_area, sizeof(uint8_t),
		AU_PACKET_TYPE_INT8, 1, /* Member 1 */
		AU_PACKET_TYPE_INT8, 1, /* Member 2 */
		AU_PACKET_TYPE_INT8, 1, /* Member 3 */
		AU_PACKET_TYPE_INT8, 1, /* Member 4 */
		AU_PACKET_TYPE_INT8, 1, /* Member 5 */
		AU_PACKET_TYPE_END);
	au_packet_init(&mod->packet_bonus_stats, sizeof(uint8_t),
		AU_PACKET_TYPE_INT8, 1, /* Damage */
		AU_PACKET_TYPE_INT8, 1, /* Defense */
		AU_PACKET_TYPE_INT16, 1, /* Max HP */
		AU_PACKET_TYPE_INT16, 1, /* Max MP */
		AU_PACKET_TYPE_END);
	au_packet_init(&mod->packet_management, sizeof(uint8_t),
		AU_PACKET_TYPE_INT8, 1, /* operation */
		AU_PACKET_TYPE_INT32, 1, /* operator id */
		AU_PACKET_TYPE_INT32, 1, /* target id */
		AU_PACKET_TYPE_CHAR, AP_CHARACTER_MAX_NAME_LENGTH + 1, /* target name */
		AU_PACKET_TYPE_END);
	return mod;
}

struct ap_party * ap_party_new(struct ap_party_module * mod)
{
	struct ap_party * party = mod->freelist;
	if (party) {
		mod->freelist = party->next;
		ap_module_construct_module_data(mod, AP_PARTY_MDI_PARTY, party);
	}
	else {
		party = ap_module_create_module_data(mod, AP_PARTY_MDI_PARTY);
	}
	return party;
}

void ap_party_free(struct ap_party_module * mod, struct ap_party * party)
{
	ap_module_destruct_module_data(mod, AP_PARTY_MDI_PARTY, party);
	party->next = mod->freelist;
	mod->freelist = party;
}

void ap_party_add_callback(
	struct ap_party_module * mod,
	enum ap_party_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	ap_module_add_callback(mod, id, callback_module, callback);
}

size_t ap_party_attach_data(
	struct ap_party_module * mod,
	enum ap_party_module_data_index data_index,
	size_t data_size,
	ap_module_t callback_module,
	ap_module_default_t constructor,
	ap_module_default_t destructor)
{
	return ap_module_attach_data(mod, data_index, data_size, 
		callback_module, constructor, destructor);
}

struct ap_party_character_attachment * ap_party_get_character_attachment(
	struct ap_party_module * mod,
	struct ap_character * character)
{
	return ap_module_get_attached_data(character, mod->character_attachment_offset);
}

boolean ap_party_can_join(
	struct ap_party_module * mod, 
	struct ap_character * character)
{
	struct ap_party_cb_can_join cb = { 0 };
	cb.character = character;
	cb.attachment = ap_party_get_character_attachment(mod, character);
	return ap_module_enum_callback(mod, AP_PARTY_CB_RECEIVE, &cb);
}

struct ap_party * ap_party_form(
	struct ap_party_module * mod, 
	uint32_t member_count, ...)
{
	struct ap_party * party = ap_party_new(mod);
	uint32_t i;
	va_list ap;
	assert(member_count <= AP_PARTY_MAX_MEMBER_COUNT);
	assert(member_count >= 2);
	va_start(ap, member_count);
	for (i = 0; i < member_count; i++) {
		struct ap_character * c = va_arg(ap, struct ap_character *);
		party->members[i] = c;
		party->member_character_ids[i] = c->id;
		ap_party_get_character_attachment(mod, c)->party = party;
	}
	va_end(ap);
	party->member_count = member_count;
	party->exp_type = AP_PARTY_EXP_TYPE_BY_DAMAGE;
	party->item_division_type = AP_PARTY_ITEM_DIVISION_DAMAGE;
	for (i = 0; i < member_count; i++) {
		struct ap_character * c = party->members[i];
		struct ap_party_cb_join cb = { 0 };
		cb.party = party;
		cb.character = c;
		cb.is_party_newly_formed = TRUE;
		ap_module_enum_callback(mod, AP_PARTY_CB_JOIN, &cb);
	}
	return party;
}

void ap_party_add_member(
	struct ap_party_module * mod, 
	struct ap_party * party,
	struct ap_character * character)
{
	struct ap_party_cb_join cb = { 0 };
	assert(party->member_count < AP_PARTY_MAX_MEMBER_COUNT);
	ap_party_get_character_attachment(mod, character)->party = party;
	party->member_character_ids[party->member_count] = character->id;
	party->members[party->member_count++] = character;
	cb.party = party;
	cb.character = character;
	cb.is_party_newly_formed = FALSE;
	ap_module_enum_callback(mod, AP_PARTY_CB_JOIN, &cb);
}

void ap_party_remove_member(
	struct ap_party_module * mod,
	struct ap_party * party,
	struct ap_character * character)
{
	struct ap_party_cb_leave cb = { 0 };
	uint32_t i;
	uint32_t index = UINT32_MAX;
	struct ap_party_character_attachment * attachment;
	for (i = 0; i < party->member_count; i++) {
		if (party->members[i] == character) {
			index = i;
			break;
		}
	}
	assert(index != UINT32_MAX);
	if (index == UINT32_MAX)
		return;
	memmove(&party->member_character_ids[index], 
		&party->member_character_ids[index + 1],
		sizeof(party->member_character_ids[0]) * (party->member_count - index - 1));
	memmove(&party->members[index], &party->members[index + 1],
		sizeof(party->members[0]) * (party->member_count - index - 1));
	party->member_count--;
	attachment = ap_party_get_character_attachment(mod, character);
	attachment->party = NULL;
	memset(attachment->invited_character_id, 0, 
		sizeof(attachment->invited_character_id));
	attachment->invited_count = 0;
	cb.party = party;
	cb.character = character;
	cb.was_party_leader = (index == AP_PARTY_LEADER_INDEX);
	ap_module_enum_callback(mod, AP_PARTY_CB_LEAVE, &cb);
}

boolean ap_party_can_leave(
	struct ap_party_module * mod, 
	struct ap_character * character)
{
	struct ap_party_cb_can_leave cb = { 0 };
	cb.character = character;
	return ap_module_enum_callback(mod, AP_PARTY_CB_CAN_LEAVE, &cb);
}

boolean ap_party_on_receive(
	struct ap_party_module * mod,
	const void * data,
	uint16_t length,
	void * user_data)
{
	struct ap_party_cb_receive cb = { 0 };
	if (!au_packet_get_field(&mod->packet, TRUE, data, length,
			&cb.type, /* Packet Type */ 
			NULL, /* Party ID */
			NULL, /* # of MaxMember */
			NULL, /* # of current member */
			NULL, /* Add Remove member */
			NULL, NULL, /* party member list */
			NULL, /* factor */
			NULL, /* effect area */
			&cb.exp_type, /* AgpmPartyCalcExpType */
			NULL, /* Bonus Stats */
			&cb.item_division_type)) { /* ItemDivisionType */
		return FALSE;
	}
	cb.user_data = user_data;
	return ap_module_enum_callback(mod, AP_PARTY_CB_RECEIVE, &cb);
}

boolean ap_party_on_receive_management(
	struct ap_party_module * mod,
	const void * data,
	uint16_t length,
	void * user_data)
{
	struct ap_party_cb_receive_management cb = { 0 };
	const char * targetname = NULL;
	if (!au_packet_get_field(&mod->packet_management, TRUE, data, length,
			&cb.type, /* operation */ 
			&cb.character_id, /* operator id */
			&cb.target_id, /* target id */
			&targetname)) { /* target name */
		return FALSE;
	}
	AU_PACKET_GET_STRING(cb.target_name, targetname);
	cb.user_data = user_data;
	return ap_module_enum_callback(mod, AP_PARTY_CB_RECEIVE_MANAGEMENT, &cb);
}

void ap_party_make_add_packet(
	struct ap_party_module * mod, 
	struct ap_party * party)
{
	uint8_t type = AP_PARTY_PACKET_ADD;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	uint16_t length = 0;
	uint32_t maxcount = AP_PARTY_MAX_MEMBER_COUNT;
	uint16_t membersize = (uint16_t)(sizeof(uint32_t) * party->member_count);
	au_packet_make_packet(&mod->packet, buffer, TRUE, &length,
		AP_PARTY_PACKET_TYPE,
		&type, /* Packet Type */ 
		&party->id, /* Party ID */
		&maxcount, /* # of MaxMember */
		&party->member_count, /* # of current member */
		NULL, /* Add Remove member */
		party->member_character_ids, &membersize, /* party member list */
		NULL, /* factor */
		NULL, /* effect area */
		&party->exp_type, /* AgpmPartyCalcExpType */
		NULL, /* Bonus Stats */
		&party->item_division_type); /* ItemDivisionType */
	ap_packet_set_length(mod->ap_packet, length);
}

void ap_party_make_remove_packet(
	struct ap_party_module * mod, 
	struct ap_party * party)
{
	uint8_t type = AP_PARTY_PACKET_REMOVE;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	uint16_t length = 0;
	au_packet_make_packet(&mod->packet, buffer, TRUE, &length,
		AP_PARTY_PACKET_TYPE,
		&type, /* Packet Type */ 
		&party->id, /* Party ID */
		NULL, /* # of MaxMember */
		NULL, /* # of current member */
		NULL, /* Add Remove member */
		NULL, /* party member list */
		NULL, /* factor */
		NULL, /* effect area */
		NULL, /* AgpmPartyCalcExpType */
		NULL, /* Bonus Stats */
		NULL); /* ItemDivisionType */
	ap_packet_set_length(mod->ap_packet, length);
}

void ap_party_make_member_packet(
	struct ap_party_module * mod, 
	enum ap_party_packet_type type,
	struct ap_party * party,
	uint32_t character_id)
{
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	uint16_t length = 0;
	au_packet_make_packet(&mod->packet, buffer, TRUE, &length,
		AP_PARTY_PACKET_TYPE,
		&type, /* Packet Type */ 
		&party->id, /* Party ID */
		NULL, /* # of MaxMember */
		NULL, /* # of current member */
		&character_id, /* Add Remove member */
		NULL, /* party member list */
		NULL, /* factor */
		NULL, /* effect area */
		NULL, /* AgpmPartyCalcExpType */
		NULL, /* Bonus Stats */
		NULL); /* ItemDivisionType */
	ap_packet_set_length(mod->ap_packet, length);
}

void ap_party_make_update_exp_packet(
	struct ap_party_module * mod,
	struct ap_party * party)
{
	uint8_t type = AP_PARTY_PACKET_UPDATE_EXP_TYPE;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	uint16_t length = 0;
	au_packet_make_packet(&mod->packet, buffer, TRUE, &length,
		AP_PARTY_PACKET_TYPE,
		&type, /* Packet Type */ 
		&party->id, /* Party ID */
		NULL, /* # of MaxMember */
		NULL, /* # of current member */
		&party->member_character_ids[AP_PARTY_LEADER_INDEX], /* Add Remove member */
		NULL, /* party member list */
		NULL, /* factor */
		NULL, /* effect area */
		&party->exp_type, /* AgpmPartyCalcExpType */
		NULL, /* Bonus Stats */
		NULL); /* ItemDivisionType */
	ap_packet_set_length(mod->ap_packet, length);
}

void ap_party_make_update_item_packet(
	struct ap_party_module * mod,
	struct ap_party * party)
{
	uint8_t type = AP_PARTY_PACKET_UPDATE_ITEM_DIVISION;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	uint16_t length = 0;
	au_packet_make_packet(&mod->packet, buffer, TRUE, &length,
		AP_PARTY_PACKET_TYPE,
		&type, /* Packet Type */ 
		&party->id, /* Party ID */
		NULL, /* # of MaxMember */
		NULL, /* # of current member */
		&party->member_character_ids[AP_PARTY_LEADER_INDEX], /* Add Remove member */
		NULL, /* party member list */
		NULL, /* factor */
		NULL, /* effect area */
		NULL, /* AgpmPartyCalcExpType */
		NULL, /* Bonus Stats */
		&party->item_division_type); /* ItemDivisionType */
	ap_packet_set_length(mod->ap_packet, length);
}

void ap_party_make_management_packet(
	struct ap_party_module * mod,
	enum ap_party_management_packet_type type,
	uint32_t * character_id,
	uint32_t * target_id,
	const char * target_name)
{
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	uint16_t length = 0;
	au_packet_make_packet(&mod->packet_management, buffer, TRUE, &length,
		AP_PARTY_MANAGEMENT_PACKET_TYPE,
		&type,
		character_id,
		target_id,
		target_name);
	ap_packet_set_length(mod->ap_packet, length);
}
