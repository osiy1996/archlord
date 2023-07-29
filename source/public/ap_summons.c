#include "public/ap_summons.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"
#include "core/vector.h"

#include "public/ap_guild.h"
#include "public/ap_packet.h"
#include "public/ap_tick.h"

#include "utility/au_packet.h"

#include <assert.h>

struct ap_summons_module {
	struct ap_module_instance instance;
	struct ap_character_module * ap_character;
	struct ap_packet_module * ap_packet;
	struct ap_tick_module * ap_tick;
	size_t character_offset;
	struct au_packet packet;
};

static boolean cbcharctor(
	struct ap_summons_module * mod,
	struct ap_character * character)
{
	struct ap_summons_character_attachment * attachment =
		ap_summons_get_character_attachment(mod, character);
	return TRUE;
}

static boolean cbchardtor(
	struct ap_summons_module * mod,
	struct ap_character * character)
{
	struct ap_summons_character_attachment * attachment =
		ap_summons_get_character_attachment(mod, character);
	return TRUE;
}

static boolean onregister(
	struct ap_summons_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_character, AP_CHARACTER_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_packet, AP_PACKET_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_tick, AP_TICK_MODULE_NAME);
	mod->character_offset = ap_character_attach_data(mod->ap_character,
		AP_CHARACTER_MDI_CHAR, sizeof(struct ap_summons_character_attachment),
		mod, cbcharctor, cbchardtor);
	return TRUE;
}

static void onclose(struct ap_summons_module * mod)
{
}

static void onshutdown(struct ap_summons_module * mod)
{
}

struct ap_summons_module * ap_summons_create_module()
{
	struct ap_summons_module * mod = ap_module_instance_new(AP_SUMMONS_MODULE_NAME,
		sizeof(*mod), onregister, NULL, onclose, onshutdown);
	au_packet_init(&mod->packet, sizeof(uint16_t),
		AU_PACKET_TYPE_END);
	return mod;
}

void ap_summons_add_callback(
	struct ap_summons_module * mod,
	enum ap_summons_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	ap_module_add_callback(mod, id, callback_module, callback);
}

size_t ap_summons_attach_data(
	struct ap_summons_module * mod,
	enum ap_summons_module_data_index data_index,
	size_t data_size,
	ap_module_t callback_module,
	ap_module_default_t constructor,
	ap_module_default_t destructor)
{
	return ap_module_attach_data(mod, data_index, data_size, 
		callback_module, constructor, destructor);
}

struct ap_summons_character_attachment * ap_summons_get_character_attachment(
	struct ap_summons_module * mod,
	struct ap_character * character)
{
	return ap_module_get_attached_data(character, mod->character_offset);
}
