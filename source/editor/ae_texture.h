#ifndef _AE_TEXTURE_H_
#define _AE_TEXTURE_H_

#include "core/macros.h"
#include "core/types.h"

#include "vendor/bgfx/c99/bgfx.h"

#define AE_TEXTURE_MODULE_NAME "AgemTexture"

BEGIN_DECLS

enum ac_dat_directory;

struct ae_texture_module * ae_texture_create_module();

/**
 * Loads textures that are pending in queue.
 * 
 * @param limit If not 0, limits the number of textures to be loaded.
 *              If 0, all textures are loaded and queue is emptied.
 */
void ae_texture_process_loading_queue(
	struct ae_texture_module * mod,
	uint32_t limit);

/*
 * Attempts to create a new browser for specified 
 * packed file directory.
 *
 * Returns UINT32_MAX if maximum number of concurrent 
 * browsers has been reached.
 */
uint32_t ae_texture_browse(
	struct ae_texture_module * mod,
	enum ac_dat_directory dir,
	const char * title);

/*
 * Renders browser and at the same time processes 
 * user input (ImGui).
 *
 * If a texture is selected, the browser will be 
 * destroyed and this function will return TRUE.
 *
 * If browser handle is invalid, function will return TRUE and 
 * `selected_tex` will be set to invalid handle value.
 */
boolean ae_texture_do_browser(
	struct ae_texture_module * mod,
	uint32_t handle,
	bgfx_texture_handle_t * selected_tex);

END_DECLS

#endif /* _AE_TEXTURE_H_ */