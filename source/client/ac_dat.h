#ifndef _AC_DAT_H_
#define _AC_DAT_H_

#include "public/ap_module_instance.h"

#define AC_DAT_MODULE_NAME "AgcmDat"
#define AC_DAT_MAX_RESOURCE_NAME_SIZE 128

BEGIN_DECLS

enum ac_dat_directory {
	AC_DAT_DIR_CHARACTER,
	AC_DAT_DIR_CHARACTER_ANIM,
	AC_DAT_DIR_CHARACTER_FACE,
	AC_DAT_DIR_CHARACTER_HAIR,
	AC_DAT_DIR_EFFECT_ANIM,
	AC_DAT_DIR_EFFECT_CLUMP,
	AC_DAT_DIR_INI_ITEMTEMPLATE,
	AC_DAT_DIR_OBJECT,
	AC_DAT_DIR_OBJECT_ANIM,
	AC_DAT_DIR_TEX_CHARACTER,
	AC_DAT_DIR_TEX_EFFECT,
	AC_DAT_DIR_TEX_EFFECT_LOW,
	AC_DAT_DIR_TEX_EFFECT_MED,
	AC_DAT_DIR_TEX_ETC,
	AC_DAT_DIR_TEX_ITEM,
	AC_DAT_DIR_TEX_MINIMAP,
	AC_DAT_DIR_TEX_OBJECT,
	AC_DAT_DIR_TEX_OBJECT_LOW,
	AC_DAT_DIR_TEX_OBJECT_MED,
	AC_DAT_DIR_TEX_SKILL,
	AC_DAT_DIR_TEX_UI,
	AC_DAT_DIR_TEX_UI_BASE,
	AC_DAT_DIR_TEX_WORLD,
	AC_DAT_DIR_TEX_WORLD_LOW,
	AC_DAT_DIR_TEX_WORLD_MED,
	AC_DAT_DIR_TEX_WORLD_MAP,
	AC_DAT_DIR_WORLD_OCTREE,
	AC_DAT_DIR_COUNT
};

typedef void * ac_dat_dictionary;

struct ac_dat_resource {
	char name[AC_DAT_MAX_RESOURCE_NAME_SIZE];
	uint32_t data_offset;
	uint32_t data_size;
	void * custom_data;
};

struct ac_dat_module * ac_dat_create_module();

boolean ac_dat_load(
	struct ac_dat_module * mod,
	const char * path_to_dir,
	const char * encryption_key,
	ac_dat_dictionary * dictionary);

void ac_dat_destroy(ac_dat_dictionary dictionary);

const char * ac_dat_get_dir_path(enum ac_dat_directory dir);

struct ac_dat_resource * ac_dat_get_resources(
	struct ac_dat_module * mod,
	enum ac_dat_directory dir,
	uint32_t * count);

struct ac_dat_resource * ac_dat_find(
	struct ac_dat_module * mod,
	ac_dat_dictionary dictionary,
	const char * resource_name);

boolean ac_dat_batch_begin(struct ac_dat_module * mod, enum ac_dat_directory dir);

void ac_dat_batch_end(struct ac_dat_module * mod, enum ac_dat_directory dir);

boolean ac_dat_load_resource(
	struct ac_dat_module * mod,
	enum ac_dat_directory dir,
	const char * resource_name,
	void ** data,
	size_t * size);

boolean ac_dat_load_resource_from_batch(
	struct ac_dat_module * mod,
	enum ac_dat_directory dir,
	const struct ac_dat_resource * resource,
	void ** data,
	size_t * size);

END_DECLS

#endif /* _AC_DAT_H_ */
