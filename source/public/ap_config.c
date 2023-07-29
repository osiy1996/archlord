#include "public/ap_config.h"

#include "core/file_system.h"
#include "core/log.h"
#include "core/string.h"
#include "core/malloc.h"
#include "core/vector.h"

#include "utility/au_packet.h"

#include "public/ap_define.h"
#include "public/ap_module.h"
#include "public/ap_packet.h"

#define MAXSIZE 512

struct entry {
	char key[MAXSIZE];
	char value[MAXSIZE];
};

struct ap_config_module {
	struct ap_module_instance instance;
	struct ap_packet_module * ap_packet;
	struct au_packet packet;
	struct entry * entries;
};

static boolean onregister(
	struct ap_config_module * mod, 
	struct ap_module_registry * registry)
{
	file file;
	char line[1024];
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, 
		mod->ap_packet, AP_PACKET_MODULE_NAME);
	file = open_file("./config", FILE_ACCESS_READ);
	if (!file) {
		ERROR("Failed to load configuration file.");
		return FALSE;
	}
	while (read_line(file, line, sizeof(line))) {
		struct entry * e;
		const char * cursor;
		size_t count;
		if (!line[0])
			continue;
		cursor = strchr(line, '=');
		if (!cursor++)
			continue;
		count = (size_t)((uintptr_t)cursor - (uintptr_t)line);
		if (count > MAXSIZE)
			count = MAXSIZE;
		e = vec_add_empty(&mod->entries);
		strlcpy(e->key, line, count);
		strlcpy(e->value, cursor, sizeof(e->value));
	}
	close_file(file);
	return TRUE;
}

static void onshutdown(ap_module_t module_)
{
	struct ap_config_module * mod = module_;
	vec_free(mod->entries);
}

struct ap_config_module * ap_config_create_module()
{
	struct ap_config_module * mod = ap_module_instance_new("AgpmConfig", sizeof(*mod), 
		onregister, NULL, NULL, onshutdown);
	au_packet_init(&mod->packet, sizeof(uint32_t),
		AU_PACKET_TYPE_INT32, 1, /* bPCDropItemOnDeath */
		AU_PACKET_TYPE_INT32, 1, /* bExpPenaltyOnDeath */
		AU_PACKET_TYPE_INT8, 1, /* bIsTestServer */
		AU_PACKET_TYPE_END);
	mod->entries = vec_new_reserved(sizeof(*mod->entries), 128);
	return mod;
}

const char * ap_config_get(struct ap_config_module * mod, const char * key)
{
	uint32_t i;
	uint32_t count = vec_count(mod->entries);
	for (i = 0; i < count; i++) {
		if (strcmp(mod->entries[i].key, key) == 0)
			return mod->entries[i].value;
	}
	return NULL;
}

void ap_config_make_packet(
	struct ap_config_module * mod,
	boolean drop_item_on_death,
	boolean exp_penalty_on_death,
	boolean is_test_server)
{
	uint16_t length = 0;
	void * buffer = ap_packet_get_buffer(mod->ap_packet);
	au_packet_make_packet(&mod->packet, 
		buffer, TRUE, &length,
		AP_CONFIG_PACKET_TYPE, 
		&drop_item_on_death,
		&exp_penalty_on_death,
		&is_test_server);
	ap_packet_set_length(mod->ap_packet, length);
}
