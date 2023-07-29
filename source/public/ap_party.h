#ifndef _AP_PARTY_H_
#define _AP_PARTY_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_character.h"
#include "public/ap_define.h"
#include "public/ap_module_instance.h"

#include <assert.h>

#define AP_PARTY_MODULE_NAME "AgpmParty"

#define AP_PARTY_MAX_MEMBER_COUNT 5
#define AP_PARTY_MAX_NAME_SIZE 40
#define AP_PARTY_MAX_EFFECT 10
#define AP_PARTY_MAX_INVITATION_COUNT 16
#define AP_PARTY_LEADER_INDEX 0

BEGIN_DECLS

enum ap_party_calc_exp_type {
	AP_PARTY_EXP_TYPE_BY_DAMAGE = 0,
	AP_PARTY_EXP_TYPE_BY_LEVEL,
};

enum ap_party_item_division_type {
	AP_PARTY_ITEM_DIVISION_SEQUENCE = 0,
	AP_PARTY_ITEM_DIVISION_FREE,
	AP_PARTY_ITEM_DIVISION_DAMAGE,
} ;

enum ap_party_packet_type {
	AP_PARTY_PACKET_ADD = 0,
	AP_PARTY_PACKET_REMOVE,
	AP_PARTY_PACKET_UPDATE,
	AP_PARTY_PACKET_ADD_MEMBER,
	AP_PARTY_PACKET_REMOVE_MEMBER,
	AP_PARTY_PACKET_UPDATE_EXP_TYPE,
	AP_PARTY_PACKET_UPDATE_ITEM_DIVISION,
};

enum ap_party_management_packet_type {
	AP_PARTY_MANAGEMENT_PACKET_INVITE = 0,
	AP_PARTY_MANAGEMENT_PACKET_REJECT,
	AP_PARTY_MANAGEMENT_PACKET_INVITE_ACCEPT,
	AP_PARTY_MANAGEMENT_PACKET_LEAVE,
	AP_PARTY_MANAGEMENT_PACKET_BANISH,
	AP_PARTY_MANAGEMENT_PACKET_INVITE_FAILED_ALREADY_OTHER_PARTY_MEMBER,
	AP_PARTY_MANAGEMENT_PACKET_INVITE_FAILED_ALREADY_PARTY_MEMBER,
	AP_PARTY_MANAGEMENT_PACKET_INVITE_FAILED_PARTY_MEMBER_IS_FULL,
	AP_PARTY_MANAGEMENT_PACKET_INVITE_FAILED_NOT_LEADER,
	AP_PARTY_MANAGEMENT_PACKET_FAILED,
	AP_PARTY_MANAGEMENT_PACKET_INVITE_FAILED_NO_LOGIN_MEMBER,
	AP_PARTY_MANAGEMENT_PACKET_DELEGATION_LEADER,
	AP_PARTY_MANAGEMENT_PACKET_DELEGATION_LEADER_FAILED,
	AP_PARTY_MANAGEMENT_PACKET_INVITE_FAILED_LEVEL_DIFF,
	AP_PARTY_MANAGEMENT_PACKET_INVITE_FAILED_REFUSE,
	AP_PARTY_MANAGEMENT_PACKET_REQUEST_RECALL,
	AP_PARTY_MANAGEMENT_PACKET_ACCEPT_RECALL,
	AP_PARTY_MANAGEMENT_PACKET_REJECT_RECALL,
	AP_PARTY_MANAGEMENT_PACKET_INVITE_FAILED_MURDERER_OPERATOR,
	AP_PARTY_MANAGEMENT_PACKET_INVITE_FAILED_MURDERER_TARGET,
	AP_PARTY_MANAGEMENT_PACKET_LEAVE_BY_MURDERER,
};

enum ap_party_callback_id {
	AP_PARTY_CB_RECEIVE,
	AP_PARTY_CB_RECEIVE_MANAGEMENT,
	/** Triggered to determine if the character can join a party. */
	AP_PARTY_CB_CAN_JOIN,
	/** Triggered when a character joins a party. */
	AP_PARTY_CB_JOIN,
	/** Triggered to determine if the character can leave the party. */
	AP_PARTY_CB_CAN_LEAVE,
	/** Triggered when a character leaves a party. */
	AP_PARTY_CB_LEAVE,
};

enum ap_party_mode_data_index {
	AP_PARTY_MDI_PARTY = 0,
};

struct ap_party {
	uint32_t id;
	uint32_t member_character_ids[AP_PARTY_MAX_MEMBER_COUNT];
	struct ap_character * members[AP_PARTY_MAX_MEMBER_COUNT];
	uint32_t member_count;
	enum ap_party_calc_exp_type exp_type;
	enum ap_party_item_division_type item_division_type;
	struct ap_party * next;
};

struct ap_party_character_attachment {
	struct ap_party * party;
	uint32_t invited_character_id[AP_PARTY_MAX_INVITATION_COUNT];
	uint32_t invited_count;
};

struct ap_party_cb_receive {
	enum ap_party_packet_type type;
	enum ap_party_calc_exp_type exp_type;
	enum ap_party_item_division_type item_division_type;
	void * user_data;
};

struct ap_party_cb_receive_management {
	enum ap_party_management_packet_type type;
	uint32_t character_id;
	uint32_t target_id;
	char target_name[AP_CHARACTER_MAX_NAME_LENGTH + 1];
	void * user_data;
};

struct ap_party_cb_can_join {
	struct ap_character * character;
	struct ap_party_character_attachment * attachment;
};

struct ap_party_cb_join {
	struct ap_party * party;
	struct ap_character * character;
	boolean is_party_newly_formed;
};

struct ap_party_cb_can_leave {
	struct ap_character * character;
};

struct ap_party_cb_leave {
	struct ap_party * party;
	struct ap_character * character;
	boolean was_party_leader;
};

struct ap_party_module * ap_party_create_module();

struct ap_party * ap_party_new(struct ap_party_module * mod);

void ap_party_free(struct ap_party_module * mod, struct ap_party * party);

boolean ap_party_read_data(
	struct ap_party_module * mod, 
	const char * file_path, 
	boolean decrypt);

boolean ap_party_read_instances(
	struct ap_party_module * mod,
	const char * file_path, 
	boolean decrypt);

void ap_party_add_callback(
	struct ap_party_module * mod,
	enum ap_party_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

size_t ap_party_attach_data(
	struct ap_party_module * mod,
	enum ap_party_module_data_index data_index,
	size_t data_size,
	ap_module_t callback_module,
	ap_module_default_t constructor,
	ap_module_default_t destructor);

struct ap_party_character_attachment * ap_party_get_character_attachment(
	struct ap_party_module * mod,
	struct ap_character * character);

boolean ap_party_can_join(
	struct ap_party_module * mod, 
	struct ap_character * character);

struct ap_party * ap_party_form(
	struct ap_party_module * mod, 
	uint32_t member_count, ...);

void ap_party_add_member(
	struct ap_party_module * mod, 
	struct ap_party * party,
	struct ap_character * character);

void ap_party_remove_member(
	struct ap_party_module * mod,
	struct ap_party * party,
	struct ap_character * character);

boolean ap_party_can_leave(
	struct ap_party_module * mod, 
	struct ap_character * character);

boolean ap_party_on_receive(
	struct ap_party_module * mod,
	const void * data,
	uint16_t length,
	void * user_data);

boolean ap_party_on_receive_management(
	struct ap_party_module * mod,
	const void * data,
	uint16_t length,
	void * user_data);

void ap_party_make_add_packet(
	struct ap_party_module * mod, 
	struct ap_party * party);

void ap_party_make_remove_packet(
	struct ap_party_module * mod, 
	struct ap_party * party);

void ap_party_make_member_packet(
	struct ap_party_module * mod, 
	enum ap_party_packet_type type,
	struct ap_party * party,
	uint32_t character_id);

void ap_party_make_update_exp_packet(
	struct ap_party_module * mod,
	struct ap_party * party);

void ap_party_make_update_item_packet(
	struct ap_party_module * mod,
	struct ap_party * party);

void ap_party_make_management_packet(
	struct ap_party_module * mod,
	enum ap_party_management_packet_type type,
	uint32_t * character_id,
	uint32_t * target_id,
	const char * target_name);

static inline boolean ap_party_is_full(struct ap_party * party)
{
	return (party->member_count >= AP_PARTY_MAX_MEMBER_COUNT);
}

static inline boolean ap_party_add_invitation(
	struct ap_party_character_attachment * attachment,
	uint32_t character_id)
{
	if (attachment->invited_count >= AP_PARTY_MAX_INVITATION_COUNT)
		return FALSE;
	attachment->invited_character_id[attachment->invited_count++] = character_id;
	return TRUE;
}

static inline boolean ap_party_consume_invitation(
	struct ap_party_character_attachment * attachment,
	uint32_t character_id)
{
	uint32_t i;
	for (i = 0; i < attachment->invited_count; i++) {
		if (attachment->invited_character_id[i] == character_id) {
			attachment->invited_character_id[i] = 
				attachment->invited_character_id[--attachment->invited_count];
			return TRUE;
		}
	}
	return FALSE;
}

static inline struct ap_party * ap_party_get_character_party(
	struct ap_party_module * mod,
	struct ap_character * character)
{
	return ap_party_get_character_attachment(mod, character)->party;
}

END_DECLS

#endif /* _AP_PARTY_H_ */