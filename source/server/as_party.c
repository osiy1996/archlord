#include "server/as_party.h"

#include "core/log.h"
#include "core/malloc.h"

#include "public/ap_admin.h"

#include "server/as_player.h"

#include <assert.h>

#define ID_OFFSET 100

struct as_party_module {
	struct ap_module_instance instance;
	struct ap_party_module * ap_party;
	struct as_player_module * as_player;
	size_t party_offset;
	uint32_t id_counter;
};

static boolean partyctor(
	struct as_party_module * mod,
	struct ap_party * party)
{
	uint32_t id = ++mod->id_counter;
	if (!id) {
		mod->id_counter = ID_OFFSET;
		id = ++mod->id_counter;
	}
	party->id = id;
	return TRUE;
}

static boolean onregister(
	struct as_party_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_party, AP_PARTY_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->as_player, AS_PLAYER_MODULE_NAME);
	mod->party_offset = ap_party_attach_data(mod->ap_party, AP_PARTY_MDI_PARTY,
		sizeof(struct as_party_attachment), mod, partyctor, NULL);
	if (mod->party_offset == SIZE_MAX) {
		ERROR("Failed to attach party data.");
		return FALSE;
	}
	return TRUE;
}

struct as_party_module * as_party_create_module()
{
	struct as_party_module * mod = ap_module_instance_new(AS_PARTY_MODULE_NAME,
		sizeof(*mod), onregister, NULL, NULL, NULL);
	mod->id_counter = ID_OFFSET;
	return mod;
}

void as_party_add_callback(
	struct as_party_module * mod,
	enum as_party_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	ap_module_add_callback(mod, id, callback_module, callback);
}

struct as_party_attachment * as_party_get_attachment(
	struct as_party_module * mod,
	struct ap_party * party)
{
	return ap_module_get_attached_data(party, mod->party_offset);
}

void as_party_send_packet(struct as_party_module * mod, struct ap_party * party)
{
	uint32_t i;
	for (i = 0; i < party->member_count; i++)
		as_player_send_packet(mod->as_player, party->members[i]);
}

void as_party_send_packet_except(
	struct as_party_module * mod, 
	struct ap_party * party,
	struct ap_character * character)
{
	uint32_t i;
	for (i = 0; i < party->member_count; i++) {
		if (party->members[i] != character)
			as_player_send_packet(mod->as_player, party->members[i]);
	}
}
