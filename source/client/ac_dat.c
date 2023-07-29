#include "client/ac_dat.h"

#include "core/bin_stream.h"
#include "core/file_system.h"
#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"

#include "public/ap_config.h"

#include "utility/au_md5.h"

#include <assert.h>

struct dat_dictionary {
	char directory_name[128];
	char reference_path[512];
	char data_path[512];
	uint32_t resource_count;
	struct ac_dat_resource * resources;
	char encryption_key[32];
	boolean should_save;
	file data_file;
	uint32_t refcount;
};

struct ac_dat_module {
	struct ap_module_instance instance;
	struct ap_config_module * ap_config;
	struct dat_dictionary * dict[AC_DAT_DIR_COUNT];
};

static size_t calc_ref_size(struct dat_dictionary * d)
{
	size_t sz = 0;
	size_t name_len = strlen(d->directory_name) + 1;
	uint32_t i;
	/* Resource_count (4b) + Dir_name_len (4b) + Dir_name (Nb) */
	sz = (size_t)4 + 4 + name_len;
	for (i = 0; i < d->resource_count; i++) {
		struct ac_dat_resource * res = &d->resources[i];
		name_len = strlen(res->name) + 1;
		/* Res_name_length (4b) + Res_name (nb) + 
		* Res_offset (4b) + Res_size (4b) */
		sz += (size_t)4 + name_len + 4 + 4;
	}
	return sz;
}

static struct dat_dictionary * create_dictionary()
{
	struct dat_dictionary * d = (struct dat_dictionary *)alloc(sizeof(*d));
	memset(d, 0, sizeof(*d));
	return d;
}

static struct dat_dictionary * get_dictionary(
	struct ac_dat_module * mod,
	enum ac_dat_directory dir)
{
	if (dir < 0 || dir >= AC_DAT_DIR_COUNT)
		return NULL;
	return mod->dict[dir];
}

static boolean onregister(
	struct ac_dat_module * mod,
	struct ap_module_registry * registry)
{
	uint32_t i;
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ap_config, 
		AP_CONFIG_MODULE_NAME);
	for (i = 0; i < AC_DAT_DIR_COUNT; i++) {
		char path[512];
		make_path(path, sizeof(path), "%s/%s",
			ap_config_get(mod->ap_config, "ClientDir"), 
			ac_dat_get_dir_path(i));
		if (!ac_dat_load(mod, path, "1111", &mod->dict[i])) {
			ERROR("Failed to load packed directory (%s).",
				path);
			return FALSE;
		}
	}
	return TRUE;
}

static void onshutdown(struct ac_dat_module * mod)
{
	uint32_t i;
	for (i = 0; i < AC_DAT_DIR_COUNT; i++) {
		if (mod->dict[i]) {
			ac_dat_destroy(mod->dict[i]);
			mod->dict[i] = NULL;
		}
	}
}

struct ac_dat_module * ac_dat_create_module()
{
	struct ac_dat_module * mod = (struct ac_dat_module *)ap_module_instance_new(
		AC_DAT_MODULE_NAME, sizeof(*mod), onregister, NULL, NULL, onshutdown);
	return mod;
}

boolean ac_dat_load(
	struct ac_dat_module * mod,
	const char * path_to_dir,
	const char * encryption_key,
	ac_dat_dictionary * dictionary)
{
	struct dat_dictionary * d;
	void * data;
	size_t size;
	char reference_path[512];
	char data_path[512];
	uint32_t name_len = 0;
	uint32_t count = 0;
	struct bin_stream * stream;
	make_path(reference_path, COUNT_OF(reference_path),
		"%s/Reference.Dat", path_to_dir);
	make_path(data_path, COUNT_OF(data_path),
		"%s/Data.Dat", path_to_dir);
	if (!get_file_size(reference_path, &size))
		return FALSE;
	data = alloc(size);
	if (!load_file(reference_path, data, size)) {
		dealloc(data);
		return FALSE;
	}
	/* Reference.Dat file is encrypted so first we want to 
	 * decrypt using a common key.
	 *
	 * In global client versions, it's '1111'.
	 *
	 * In chinese client versions, it might be 
	 * 'HeavensHell' or something else. */
	if (!au_md5_crypt(data, (uint32_t)size, (const uint8_t *)"1111", 4)) {
		dealloc(data);
		return FALSE;
	}
	bstream_from_buffer(data, size, TRUE, &stream);
	if (!bstream_read_u32(stream, &count)) {
		bstream_destroy(stream);
		return FALSE;
	}
	d = create_dictionary();
	strlcpy(d->reference_path, reference_path, 
		COUNT_OF(d->reference_path));
	strlcpy(d->data_path, data_path, 
		COUNT_OF(d->data_path));
	strlcat(d->encryption_key, encryption_key, 
		COUNT_OF(d->encryption_key));
	if (!bstream_read_u32(stream, &name_len) ||
		!bstream_read(stream, d->directory_name, name_len)) {
		ac_dat_destroy(d);
		bstream_destroy(stream);
		return FALSE;
	}
	strlower(d->directory_name);
	d->resource_count = count;
	d->resources = alloc(count * sizeof(*d->resources));
	memset(d->resources, 0, count * sizeof(*d->resources));
	for (uint32_t i = 0; i < count; i++) {
		struct ac_dat_resource * res = &d->resources[i];
		if (!bstream_read_u32(stream, &name_len) ||
			!bstream_read(stream, res->name, name_len) ||
			!bstream_read_u32(stream, &res->data_offset) ||
			!bstream_read_u32(stream, &res->data_size)) {
			ac_dat_destroy(d);
			bstream_destroy(stream);
			return FALSE;
		}
		strupper(res->name);
	}
	bstream_destroy(stream);
	*dictionary = d;
	return TRUE;
}

void ac_dat_destroy(ac_dat_dictionary dictionary)
{
	struct dat_dictionary * d = dictionary;
	uint32_t i;
	assert(d->refcount == 0 && d->data_file == NULL);
	for (i = 0; i < d->resource_count; i++) {
		if (d->resources[i].custom_data)
			dealloc(d->resources[i].custom_data);
	}
	if (d->resources)
		dealloc(d->resources);
	dealloc(d);
}

const char * ac_dat_get_dir_path(enum ac_dat_directory dir)
{
	switch (dir) {
	case AC_DAT_DIR_CHARACTER:
		return "character";
	case AC_DAT_DIR_CHARACTER_ANIM:
		return "character/animation";
	case AC_DAT_DIR_CHARACTER_FACE:
		return "character/defaulthead/face";
	case AC_DAT_DIR_CHARACTER_HAIR:
		return "character/defaulthead/hair";
	case AC_DAT_DIR_EFFECT_ANIM:
		return "effect/animation";
	case AC_DAT_DIR_EFFECT_CLUMP:
		return "effect/clump";
	case AC_DAT_DIR_INI_ITEMTEMPLATE:
		return "ini/itemtemplate";
	case AC_DAT_DIR_OBJECT:
		return "object";
	case AC_DAT_DIR_OBJECT_ANIM:
		return "object/animation";
	case AC_DAT_DIR_TEX_CHARACTER:
		return "texture/character";
	case AC_DAT_DIR_TEX_EFFECT:
		return "texture/effect";
	case AC_DAT_DIR_TEX_EFFECT_LOW:
		return "texture/effect/low";
	case AC_DAT_DIR_TEX_EFFECT_MED:
		return "texture/effect/medium";
	case AC_DAT_DIR_TEX_ETC:
		return "texture/etc";
	case AC_DAT_DIR_TEX_ITEM:
		return "texture/item";
	case AC_DAT_DIR_TEX_MINIMAP:
		return "texture/minimap";
	case AC_DAT_DIR_TEX_OBJECT:
		return "texture/object";
	case AC_DAT_DIR_TEX_OBJECT_LOW:
		return "texture/object/low";
	case AC_DAT_DIR_TEX_OBJECT_MED:
		return "texture/object/medium";
	case AC_DAT_DIR_TEX_SKILL:
		return "texture/skill";
	case AC_DAT_DIR_TEX_UI:
		return "texture/ui";
	case AC_DAT_DIR_TEX_UI_BASE:
		return "texture/ui/base";
	case AC_DAT_DIR_TEX_WORLD:
		return "texture/world";
	case AC_DAT_DIR_TEX_WORLD_LOW:
		return "texture/world/low";
	case AC_DAT_DIR_TEX_WORLD_MED:
		return "texture/world/medium";
	case AC_DAT_DIR_TEX_WORLD_MAP:
		return "texture/worldmap";
	case AC_DAT_DIR_WORLD_OCTREE:
		return "world/octree";
	default:
		return "INVALID";
	}
}

struct ac_dat_resource * ac_dat_get_resources(
	struct ac_dat_module * mod,
	enum ac_dat_directory dir,
	uint32_t * count)
{
	struct dat_dictionary * d = get_dictionary(mod, dir);
	if (!d) {
		*count = 0;
		return NULL;
	}
	*count = d->resource_count;
	return d->resources;
}

struct ac_dat_resource * ac_dat_find(
	struct ac_dat_module * mod,
	ac_dat_dictionary dictionary,
	const char * resource_name)
{
	struct dat_dictionary * d = dictionary;
	uint32_t i;
	for (i = 0; i < d->resource_count; i++) {
		if (strcasecmp(d->resources[i].name, resource_name) == 0)
			return &d->resources[i];
	}
	return NULL;
}

boolean ac_dat_batch_begin(struct ac_dat_module * mod, enum ac_dat_directory dir)
{
	struct dat_dictionary * d = get_dictionary(mod, dir);
	if (!d)
		return FALSE;
	if (d->data_file) {
		d->refcount++;
		return TRUE;
	}
	d->data_file = open_file(d->data_path, FILE_ACCESS_READ);
	if (!d->data_file)
		return FALSE;
	d->refcount = 1;
	return TRUE;
}

void ac_dat_batch_end(struct ac_dat_module * mod, enum ac_dat_directory dir)
{
	struct dat_dictionary * d = get_dictionary(mod, dir);
	if (!d)
		return;
	assert(d->refcount > 0);
	if (!d->refcount || !--d->refcount) {
		close_file(d->data_file);
		d->data_file = NULL;
	}
}

boolean ac_dat_load_resource(
	struct ac_dat_module * mod,
	enum ac_dat_directory dir,
	const char * resource_name,
	void ** data,
	size_t * size)
{
	struct dat_dictionary * d = get_dictionary(mod, dir);
	struct ac_dat_resource * r;
	file f;
	void * buf;
	if (!d)
		return FALSE;
	r = ac_dat_find(mod, d, resource_name);
	if (!r)
		return FALSE;
	f = open_file(d->data_path, FILE_ACCESS_READ);
	if (!f)
		return FALSE;
	if (!seek_file(f, r->data_offset, TRUE)) {
		close_file(f);
		return FALSE;
	}
	buf = alloc(r->data_size);
	if (!read_file(f, buf, r->data_size)) {
		dealloc(buf);
		close_file(f);
		return FALSE;
	}
	close_file(f);
	*data = buf;
	*size = r->data_size;
	return TRUE;
}

boolean ac_dat_load_resource_from_batch(
	struct ac_dat_module * mod,
	enum ac_dat_directory dir,
	const struct ac_dat_resource * resource,
	void ** data,
	size_t * size)
{
	struct dat_dictionary * d = get_dictionary(mod, dir);
	void * buf;
	if (!d)
		return FALSE;
	assert(d->data_file != NULL);
	if (!seek_file(d->data_file, resource->data_offset, TRUE))
		return FALSE;
	buf = alloc(resource->data_size);
	if (!read_file(d->data_file, buf, resource->data_size)) {
		dealloc(buf);
		return FALSE;
	}
	*data = buf;
	*size = resource->data_size;
	return TRUE;
}