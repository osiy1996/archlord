#ifndef _AC_TEXTURE_H_
#define _AC_TEXTURE_H_

#include "core/macros.h"
#include "core/types.h"

#include <client/ac_dat.h>

#include "vendor/bgfx/c99/bgfx.h"

#define AC_TEXTURE_MODULE_NAME "AgcmTexture"

BEGIN_DECLS

struct ac_texture_module * ac_texture_create_module();

bgfx_texture_handle_t ac_texture_load(
	struct ac_texture_module * mod,
	const char * file_path,
	bgfx_texture_info_t * info);

bgfx_texture_handle_t ac_texture_load_packed(
	struct ac_texture_module * mod,
	enum ac_dat_directory dir,
	const char * file_name,
	boolean try_other_extensions,
	bgfx_texture_info_t * info);

void ac_texture_set_dictionary(
	struct ac_texture_module * mod,
	enum ac_dat_directory dir);

/**
 * Increments texture handle reference.
 *
 * If texture handle is valid, returns the 
 * previous reference count.
 * Otherwise, returns UINT32_MAX.
 */
uint32_t ac_texture_copy(
	struct ac_texture_module * mod,
	bgfx_texture_handle_t tex);

void ac_texture_release(
	struct ac_texture_module * mod,
	bgfx_texture_handle_t tex);

void ac_texture_test(
	struct ac_texture_module * mod,
	bgfx_texture_handle_t tex);

void ac_texture_set_null(
	struct ac_texture_module * mod,
	uint32_t stage);

boolean ac_texture_get_name(
	struct ac_texture_module * mod,
	bgfx_texture_handle_t tex,
	boolean stripped,
	char * dst,
	size_t maxcount);

boolean ac_texture_add_to_default_dictionary_from_stream(
	struct ac_texture_module * mod,
	struct bin_stream * stream);

boolean ac_texture_replace_texture(
	struct ac_texture_module * mod,
	const char * name,
	bgfx_texture_handle_t tex,
	bgfx_texture_handle_t tex_to_replace);

boolean ac_texture_write_compressed_rws(
	struct bin_stream * stream,
	const char * name,
	uint16_t width,
	uint16_t height,
	const void * data);

END_DECLS

#endif /* _AC_TEXTURE_H_ */