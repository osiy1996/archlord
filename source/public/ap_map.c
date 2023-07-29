#include "public/ap_map.h"

#include "core/file_system.h"
#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"
#include "core/vector.h"

#include "public/ap_config.h"

#include "utility/au_md5.h"

#include <stdlib.h>

struct ap_map_module {
	struct ap_module_instance instance;
	struct ap_config_module * ap_config;
	struct ap_map_region_template region_templates[AP_MAP_MAX_REGION_ID + 1];
	struct ap_map_region_glossary * glossary;
};

static void match_glossary(struct ap_map_module * mod)
{
	uint32_t count = vec_count(mod->glossary);
	uint32_t i;
	for (i = 0; i <= COUNT_OF(mod->region_templates); i++) {
		struct ap_map_region_template * t = &mod->region_templates[i];
		uint32_t j;
		if (!t->in_use)
			continue;
		for (j = 0; j < count; j++) {
			if (strcmp(mod->glossary[j].name, t->name) == 0) {
				t->glossary = mod->glossary[j].label;
				break;
			}
		}
		if (!t->glossary) {
			t->glossary = "";
			WARN("Region name doesn't have a matching glossary (Id = %u).",
				t->id);
		}
	}
}

static boolean onregister(
	struct ap_map_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_config, 
		AP_CONFIG_MODULE_NAME);
	return TRUE;
}

static boolean oninitialize(struct ap_map_module * mod)
{
	char path[1024];
	const char * inidir = ap_config_get(mod->ap_config, "ServerIniDir");
	if (!inidir) {
		ERROR("Failed to retrieve ServerIniDir config.");
		return FALSE;
	}
	if (!make_path(path, sizeof(path), "%s/regiontemplate.ini", inidir)) {
		ERROR("Failed to create path (%s/regiontemplate.ini).",
			inidir);
		return FALSE;
	}
	if (!ap_map_read_region_tmp(mod, path, mod->region_templates, FALSE)) {
		ERROR("Failed to read region templates.");
		return FALSE;
	}
	if (!make_path(path, sizeof(path), "%s/regionglossary.txt", inidir)) {
		ERROR("Failed to create path (%s/regionglossary.txt).",
			inidir);
		return FALSE;
	}
	mod->glossary = ap_map_read_region_glossary(mod, path, FALSE);
	if (!mod->glossary) {
		ERROR("Failed to read region glossary.");
		return FALSE;
	}
	match_glossary(mod);
	return TRUE;
}

static void onshutdown(struct ap_map_module * mod)
{
	if (mod->glossary)
		vec_free(mod->glossary);
}

struct ap_map_module * ap_map_create_module()
{
	struct ap_map_module * mod = ap_module_instance_new(AP_MAP_MODULE_NAME,
		sizeof(*mod), onregister, oninitialize, NULL, onshutdown);
	return mod;
}

void ap_map_add_callback(
	struct ap_map_module * mod,
	enum ap_map_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback)
{
	ap_module_add_callback(mod, id, callback_module, callback);
}

static char * load_and_decrypt(
	const char * file_path,
	boolean decrypt)
{
	size_t size;
	char * buf;
	if (!get_file_size(file_path, &size)) {
		ERROR("Failed to retrieve file size: %s", file_path);
		return FALSE;
	}
	buf = alloc(size + 1);
	if (!load_file(file_path, buf, size)) {
		ERROR("Failed to load file: %s", file_path);
		dealloc(buf);
		return FALSE;
	}
	if (decrypt && !au_md5_crypt(buf, size, "1111", 4)) {
		ERROR("Failed to decrypt file: %s", file_path);
		dealloc(buf);
		return FALSE;
	}
	buf[size] = '\0';
	return buf;
}

boolean ap_map_read_region_tmp(
	struct ap_map_module * mod,
	const char * file_path,
	struct ap_map_region_template * region_tmp,
	boolean decrypt)
{
	char * buf;
	void * head;
	char line[1024];
	uint32_t line_number = 0;
	struct ap_map_region_template * tmp = NULL;
	uint32_t count = 0;
	uint32_t i;
	buf = load_and_decrypt(file_path, decrypt);
	if (!buf)
		return FALSE;
	head = buf;
	while (TRUE) {
		const char * cursor;
		char key[32];
		size_t maxcount;
		buf = (char *)read_line_buffer(buf, line, sizeof(line));
		if (!buf)
			break;
		line_number++;
		if (!line[0]) {
			/* Empty line. */
			continue;
		}
		if (line[0] == '[') {
			uint32_t id = strtoul(line + 1, NULL, 10);
			if (id > AP_MAP_MAX_REGION_ID) {
				ERROR("Invalid region id (%u) in file (%s), line %u.",
					id, file_path, line_number);
				dealloc(head);
				return FALSE;
			}
			tmp = &region_tmp[id];
			if (tmp->in_use) {
				ERROR("More than one region template with same id (%u) in file (%s), line %u.",
					id, file_path, line_number);
				dealloc(head);
				return FALSE;
			}
			memset(tmp, 0, sizeof(*tmp));
			tmp->in_use = TRUE;
			tmp->id = id;
			count++;
			continue;
		}
		if (!tmp) {
			ERROR("Value outside of a template in file (%s), line %u.",
				file_path, line_number);
			dealloc(head);
			return FALSE;
		}
		cursor = strchr(line, '=');
		if (!cursor++) {
			ERROR("Invalid format in file (%s), line %u.",
				file_path, line_number);
			dealloc(head);
			return FALSE;
		}
		maxcount = MIN((size_t)(cursor - line),
			sizeof(key));
		strlcpy(key, line, maxcount);
		if (strcmp(key, "Name") == 0) {
			strlcpy(tmp->name, cursor, sizeof(tmp->name));
		}
		else if (strcmp(key, "ParentIndex") == 0) {
			tmp->parent_id = strtol(cursor, NULL, 10);
		}
		else if (strcmp(key, "ItemSection") == 0) {
			tmp->disabled_item_section = strtoul(cursor, NULL, 10);
		}
		else if (strcmp(key, "Type") == 0) {
			tmp->type.value = strtol(cursor, NULL, 10);
		}
		else if (strcmp(key, "Comment") == 0) {
			strlcpy(tmp->comment, cursor, sizeof(tmp->comment));
		}
		else if (strcmp(key, "WorldMap") == 0) {
			tmp->world_map_index = strtoul(cursor, NULL, 10);
		}
		else if (strcmp(key, "SkySet") == 0) {
			tmp->sky_index = strtoul(cursor, NULL, 10);
		}
		else if (strcmp(key, "VisibleDistance") == 0) {
			tmp->visible_distance = (float)strtol(cursor, NULL, 10);
		}
		else if (strcmp(key, "TopViewHeight") == 0) {
			tmp->max_camera_height = (float)strtol(cursor, NULL, 10);
		}
		else if (strcmp(key, "LevelLimit") == 0) {
			tmp->level_limit = strtoul(cursor, NULL, 10);
		}
		else if (strcmp(key, "LevelMin") == 0) {
			tmp->level_min = strtoul(cursor, NULL, 10);
		}
		else if (strcmp(key, "ResurrectionX") == 0) {
			tmp->resurrection_pos[0] = strtof(cursor, NULL);
		}
		else if (strcmp(key, "ResurrectionZ") == 0) {
			tmp->resurrection_pos[1] = strtof(cursor, NULL);
		}
		else if (strcmp(key, "ZoneSrcX") == 0) {
			tmp->zone_src[0] = strtof(cursor, NULL);
		}
		else if (strcmp(key, "ZoneSrcZ") == 0) {
			tmp->zone_src[1] = strtof(cursor, NULL);
		}
		else if (strcmp(key, "ZoneDstX") == 0) {
			tmp->zone_dst[0] = strtof(cursor, NULL);
		}
		else if (strcmp(key, "ZoneDstZ") == 0) {
			tmp->zone_dst[1] = strtof(cursor, NULL);
		}
		else if (strcmp(key, "ZoneHeight") == 0) {
			tmp->zone_height = strtof(cursor, NULL);
		}
		else if (strcmp(key, "ZoneRadius") == 0) {
			tmp->zone_radius = strtof(cursor, NULL);
		}
		else if (strcmp(key, "DisableSummon") == 0) {
			if (strtol(cursor, NULL, 10))
				tmp->peculiarity |= AP_MAP_RP_DISABLE_SUMMON;
		}
		else if (strcmp(key, "DisableGo") == 0) {
			if (strtol(cursor, NULL, 10))
				tmp->peculiarity |= AP_MAP_RP_DISABLE_GO;
		}
		else if (strcmp(key, "DisableParty") == 0) {
			if (strtol(cursor, NULL, 10))
				tmp->peculiarity |= AP_MAP_RP_DISABLE_PARTY;
		}
		else if (strcmp(key, "DisableLogin") == 0) {
			if (strtol(cursor, NULL, 10))
				tmp->peculiarity |= AP_MAP_RP_DISABLE_LOGIN;
		}
		else if (strcmp(key, "EnumEnd") == 0) {
			tmp = NULL;
		}
		else {
			WARN("Unrecognized key (%s) in file (%s), line %u.",
				key, file_path, line_number);
		}
	}
	dealloc(head);
	for (i = 0; i < count; i++) {
		struct ap_map_region_template * r = &region_tmp[i];
		if (!r->in_use)
			continue;
		if (!ap_module_enum_callback(mod, AP_MAP_CB_INIT_REGION, r)) {
			ERROR("Failed to initialize region (%u:%s).", 
				r->id, r->glossary);
			return FALSE;
		}
	}
	INFO("Loaded %u region templates.", count);
	return TRUE;
}

boolean ap_map_write_region_tmp(
	struct ap_map_module * mod,
	const char * file_path,
	const struct ap_map_region_template * region_tmp,
	boolean encrypt)
{
	size_t maxcount = (size_t)1u << 20;
	char * buf = alloc(maxcount);
	void * head = buf;
	uint32_t i;
	for (i = 0; i <= AP_MAP_MAX_REGION_ID; i++) {
		const struct ap_map_region_template * tmp = &region_tmp[i];
		if (!tmp->in_use)
			continue;
		buf = write_line_bufferv(buf, maxcount, "[%u]", tmp->id);
		buf = write_line_bufferv(buf, maxcount,
			"Name=%s", tmp->name);
		buf = write_line_bufferv(buf, maxcount,
			"ParentIndex=%d", tmp->parent_id);
		buf = write_line_bufferv(buf, maxcount,
			"ItemSection=%u", tmp->disabled_item_section);
		buf = write_line_bufferv(buf, maxcount,
			"Type=%d", tmp->type.value);
		buf = write_line_bufferv(buf, maxcount,
			"Comment=%s", tmp->comment);
		buf = write_line_bufferv(buf, maxcount,
			"ResurrectionX=%d", (int)tmp->resurrection_pos[0]);
		buf = write_line_bufferv(buf, maxcount,
			"ResurrectionZ=%d", (int)tmp->resurrection_pos[1]);
		buf = write_line_bufferv(buf, maxcount,
			"WorldMap=%u", tmp->world_map_index);
		buf = write_line_bufferv(buf, maxcount,
			"SkySet=%u", tmp->sky_index);
		buf = write_line_bufferv(buf, maxcount,
			"VisibleDistance=%d", (int)tmp->visible_distance);
		buf = write_line_bufferv(buf, maxcount,
			"TopViewHeight=%d", (int)tmp->max_camera_height);
		if (tmp->level_limit) {
			buf = write_line_bufferv(buf, maxcount,
				"LevelLimit=%u", tmp->level_limit);
		}
		if (tmp->level_min) {
			buf = write_line_bufferv(buf, maxcount,
				"LevelMin=%u", tmp->level_min);
		}
		if (tmp->zone_src[0] != 0.0f) {
			buf = write_line_bufferv(buf, maxcount,
				"ZoneSrcX=%d", (int)tmp->zone_src[0]);
		}
		if (tmp->zone_src[1] != 0.0f) {
			buf = write_line_bufferv(buf, maxcount,
				"ZoneSrcZ=%d", (int)tmp->zone_src[1]);
		}
		if (tmp->zone_height != 0.0f) {
			buf = write_line_bufferv(buf, maxcount,
				"ZoneHeight=%d", (int)tmp->zone_height);
		}
		if (tmp->zone_dst[0] != 0.0f) {
			buf = write_line_bufferv(buf, maxcount,
				"ZoneDstX=%d", (int)tmp->zone_dst[0]);
		}
		if (tmp->zone_dst[1] != 0.0f) {
			buf = write_line_bufferv(buf, maxcount,
				"ZoneDstZ=%d", (int)tmp->zone_dst[1]);
		}
		if (tmp->zone_radius != 0.0f) {
			buf = write_line_bufferv(buf, maxcount,
				"ZoneRadius=%d", (int)tmp->zone_radius);
		}
	}
	if (encrypt && !au_md5_crypt(head, (size_t)(buf - (char *)head),
			"1111", 4)) {
		ERROR("Failed to encrypt file (%s).", file_path);
		dealloc(head);
		return FALSE;
	}
	if (!make_file(file_path, head, (size_t)(buf - (char *)head))) {
		ERROR("Failed to create file (%s).", file_path);
		dealloc(head);
		return FALSE;
	}
	dealloc(head);
	return TRUE;
}

struct ap_map_region_glossary * ap_map_read_region_glossary(
	struct ap_map_module * mod,
	const char * file_path,
	boolean decrypt)
{
	char * buf;
	void * head;
	char line[1024];
	uint32_t line_number = 0;
	uint32_t count;
	uint32_t c = 0;
	struct ap_map_region_glossary * g;
	buf = load_and_decrypt(file_path, decrypt);
	if (!buf)
		return FALSE;
	head = buf;
	while (TRUE) {
		buf = (char *)read_line_buffer(buf, line, sizeof(line));
		if (!buf)
			break;
		line_number++;
		if (!line[0]) {
			/* Empty line. */
			continue;
		}
		c++;
	}
	if (!c) {
		dealloc(head);
		return NULL;
	}
	buf = head;
	g = vec_new_reserved(sizeof(*g), c);
	count = c;
	c = 0;
	while (TRUE) {
		const char * cursor;
		size_t maxcount;
		struct ap_map_region_glossary * glossary;
		buf = (char *)read_line_buffer(buf, line, sizeof(line));
		if (!buf)
			break;
		line_number++;
		if (!line[0]) {
			/* Empty line. */
			continue;
		}
		cursor = strchr(line, '\t');
		if (!cursor++) {
			ERROR("Invalid format in file (%s), line %u.",
				file_path, line_number);
			vec_free(g);
			dealloc(head);
			return FALSE;
		}
		glossary = vec_add_empty(&g);
		maxcount = MIN((size_t)(cursor - line),
			sizeof(glossary->name));
		strlcpy(glossary->name, line, maxcount);
		strlcpy(glossary->label, cursor, sizeof(glossary->label));
		c++;
	}
	if (c != count) {
		ERROR("Failed to parse file (%s).", file_path);
		vec_free(g);
		dealloc(head);
		return FALSE;
	}
	dealloc(head);
	return g;
}
struct ap_map_region_template * ap_map_get_region_template(
	struct ap_map_module * mod,
	uint32_t region_id)
{
	if (region_id > AP_MAP_MAX_REGION_ID ||
		!mod->region_templates[region_id].in_use) {
		return NULL;
	}
	return &mod->region_templates[region_id];
}

struct ap_map_region_template * ap_map_get_region_template_by_name(
	struct ap_map_module * mod,
	const char * region_name)
{
	uint32_t i;
	for (i = 0; i < COUNT_OF(mod->region_templates); i++) {
		struct ap_map_region_template * r = 
			&mod->region_templates[i];
		if (r->in_use && strcmp(r->name, region_name) == 0)
			return r;
	}
	return NULL;
}
