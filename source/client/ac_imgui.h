#ifndef _AC_IMGUI_H_
#define _AC_IMGUI_H_

#include "core/macros.h"
#include "core/types.h"

#include "vendor/bgfx/c99/bgfx.h"
#ifdef __cplusplus
#define IMGUI_DEFINE_MATH_OPERATORS
#include "vendor/imgui/imgui.h"
#endif

#define AC_IMGUI_MODULE_NAME "AgcmImgui"
#define AC_IMGUI_MAIN_DOCK_ID "MainDockSpace"

BEGIN_DECLS

struct SDL_Window;
union SDL_Event;

struct ac_imgui_module * ac_imgui_create_module();

boolean ac_imgui_init(
	struct ac_imgui_module * mod, 
	int bgfx_view, 
	struct SDL_Window * window);

void ac_imgui_new_frame(struct ac_imgui_module * mod);

boolean ac_imgui_process_event(
	struct ac_imgui_module * mod, 
	const SDL_Event * event);

void ac_imgui_render(struct ac_imgui_module * mod);

void ac_imgui_begin_toolbar(struct ac_imgui_module * mod);

void ac_imgui_end_toolbar(struct ac_imgui_module * mod);

void ac_imgui_begin_toolbox(struct ac_imgui_module * mod);

void ac_imgui_end_toolbox(struct ac_imgui_module * mod);

void ac_imgui_viewport(struct ac_imgui_module * mod);

void ac_imgui_invalid_device_objects(struct ac_imgui_module * mod);

boolean ac_imgui_create_device_objects(struct ac_imgui_module * mod);

boolean igSplitter(
	boolean split_vertically,
	float thickness,
	float padding,
	float * size1,
	float * size2,
	float min_size1,
	float min_size2,
	float splitter_long_axis_size);

boolean igIconButton(
	bgfx_texture_handle_t tex,
	const char * hint,
	boolean * btn_state,
	boolean is_disabled);

END_DECLS

#endif /* _AC_IMGUI_H_ */
