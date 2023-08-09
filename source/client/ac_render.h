#ifndef _AC_RENDER_H_
#define _AC_RENDER_H_

#include "core/macros.h"
#include "core/types.h"

#define HAVE_M_PI
#include "vendor/SDL2/SDL.h"
#include "vendor/bgfx/c99/bgfx.h"

#define AC_RENDER_MODULE_NAME "AgcmRender"

/* 'CRT' stands for ClumpRenderType. */
#define AC_RENDER_CRT_DATA_CUST_DATA_NONE -1
#define AC_RENDER_CRT_STREAM_NUM "CRT_CLUMP_RENDER_TYPE_NUM"
#define AC_RENDER_CRT_STREAM_RENDERTYPE "CRT_CLUMP_RENDER_TYPE"
#define AC_RENDER_CRT_STREAM_CUSTOM_DATA1 "CRT_CUSTOM_DATA1"
#define AC_RENDER_CRT_STREAM_CUSTOM_DATA2 "CRT_CUSTOM_DATA2"

BEGIN_DECLS

enum ac_render_button_bits {
	AC_RENDER_BUTTON_LEFT = 1u << 0,
	AC_RENDER_BUTTON_RIGHT = 1u << 1,
	AC_RENDER_BUTTON_MIDDLE = 1u << 2,
};
typedef uint32_t mouse_button;

struct ac_render_crt_data {
	int32_t * render_type;
	int32_t * cust_data;
};

struct ac_render_crt {
	uint32_t set_count;
	struct ac_render_crt_data render_type;
	uint32_t cb_count;
	int32_t * cust_data;
};

struct ac_render_module * ac_render_create_module();

SDL_Window * ac_render_get_window(struct ac_render_module * mod);

const boolean * ac_render_get_key_state(struct ac_render_module * mod);

boolean ac_render_button_down(struct ac_render_module * mod, mouse_button btn);

boolean ac_render_poll_window_event(struct ac_render_module * mod, SDL_Event * e);

boolean ac_render_process_window_event(struct ac_render_module * mod, SDL_Event * e);

/*
 * Clears back-buffer and prepares for rendering.
 *
 * All rendering functions must be called between 
 * this function and ``ac_render_end_frame``.
 *
 * Returns FALSE if window is minimized.
 */
boolean ac_render_begin_frame(
	struct ac_render_module * mod, 
	struct ac_camera * cam, 
	float dt);

/*
 * Call only if ``ac_render_begin_frame`` returns TRUE.
 */
void ac_render_end_frame(struct ac_render_module * mod);

uint32_t ac_render_get_current_frame(struct ac_render_module * mod);

/*
 * Attempts to load a shader.
 *
 * File path is constructed based on current render type.
 * ex (Vulkan): (rootdir)/content/shaders/spirv/(file_name).bin
 */
boolean ac_render_load_shader(
	const char * file_name,
	bgfx_shader_handle_t * shader);

void ac_render_reserve_crt_set(
	struct ac_render_crt * crt,
	uint32_t count);

boolean ac_render_stream_read_crt(
	struct ap_module_stream * stream,
	struct ac_render_crt * crt);

boolean ac_render_stream_write_clump_render_type(
	struct ap_module_stream * stream,
	struct ac_render_crt * crt);

int ac_render_allocate_view(struct ac_render_module * mod);

int ac_render_set_view(struct ac_render_module * mod, int view_id);

int ac_render_get_view(struct ac_render_module * mod);

END_DECLS

#endif /* _AC_RENDER_H_ */
