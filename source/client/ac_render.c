#include "client/ac_render.h"

#include "core/core.h"
#include "core/malloc.h"
#include "core/os.h"
#include "core/file_system.h"

#include "public/ap_module_instance.h"

#include "client/ac_camera.h"

#include "vendor/cglm/struct.h"
#include "vendor/SDL2/SDL_syswm.h"
#ifdef near
#undef near
#endif
#ifdef far
#undef far
#endif
#ifdef ERROR
#undef ERROR
#endif

#include "core/log.h"

#include <string.h>

struct vertex {
	float pos[3];
	float color[4];
};

struct ac_render_module {
	struct ap_module_instance instance;
	SDL_Window * wnd;
	uint32_t width;
	uint32_t height;
	boolean is_minimized;
	mouse_button is_mouse_btn_down;
	boolean key_state[SDL_NUM_SCANCODES];
	uint32_t reset_flags;
	int view_cursor;
	int view_id;
	uint32_t current_frame;
};

static boolean init_bgfx(struct ac_render_module * mod)
{
	bgfx_init_t init;
	SDL_SysWMinfo info;
	bgfx_init_ctor(&init);
	memset(&info, 0, sizeof(info));
	SDL_VERSION(&info.version);
	if (!SDL_GetWindowWMInfo(mod->wnd, &info))
		return FALSE;
#ifdef _WIN32
	init.platformData.nwh = info.info.win.window;
#endif
	init.resolution.width = mod->width;
	init.resolution.height = mod->height;
#ifdef DEBUGRENDER
	init.debug = true;
	init.profile = true;
#endif
	return bgfx_init(&init);
}

/*
static boolean create_demo(struct ac_render_module * rc)
{
	const struct vertex vertices[] = {
		{ {-1.0f,  1.0f,  1.0f }, { 1.f, 0.f, 0.f, 1.f } },
		{ { 1.0f,  1.0f,  1.0f }, { 1.f, 1.f, 0.f, 1.f } },
		{ {-1.0f, -1.0f,  1.0f }, { 1.f, 1.f, 1.f, 1.f } },
		{ { 1.0f, -1.0f,  1.0f }, { 0.f, 1.f, 1.f, 1.f } },
		{ {-1.0f,  1.0f, -1.0f }, { 0.f, 0.f, 1.f, 1.f } },
		{ { 1.0f,  1.0f, -1.0f }, { 0.f, 0.f, 0.f, 1.f } },
		{ {-1.0f, -1.0f, -1.0f }, { 0.f, 1.f, 0.f, 1.f } },
		{ { 1.0f, -1.0f, -1.0f }, { 0.f, 0.f, 1.f, 1.f } } };
	const uint16_t indices[] = {
		0, 1, 2, // 0
		1, 3, 2,
		4, 6, 5, // 2
		5, 6, 7,
		0, 2, 4, // 4
		4, 2, 6,
		1, 5, 3, // 6
		5, 7, 3,
		0, 4, 1, // 8
		4, 5, 1,
		2, 3, 6, // 10
		6, 3, 7, };
	bgfx_vertex_layout_t layout;
	const bgfx_memory_t * vbm;
	const bgfx_memory_t * ibm;
	bgfx_shader_handle_t vsh;
	bgfx_shader_handle_t fsh;
	bgfx_vertex_layout_begin(&layout,
		bgfx_get_render_type());
	bgfx_vertex_layout_add(&layout,
		BGFX_ATTRIB_POSITION, 3, BGFX_ATTRIB_TYPE_FLOAT,
		false, false);
	bgfx_vertex_layout_add(&layout,
		BGFX_ATTRIB_COLOR0, 4, BGFX_ATTRIB_TYPE_FLOAT,
		false, false);
	bgfx_vertex_layout_end(&layout);
	vbm = bgfx_copy(vertices, sizeof(vertices));
	rc->vbh = bgfx_create_vertex_buffer(vbm, &layout,
		BGFX_BUFFER_NONE);
	if (!BGFX_HANDLE_IS_VALID(rc->ibh)) {
		ERROR("Failed to create vertex buffer.");
		return FALSE;
	}
	ibm = bgfx_copy(indices, sizeof(indices));
	rc->ibh = bgfx_create_index_buffer(ibm, BGFX_BUFFER_NONE);
	if (!BGFX_HANDLE_IS_VALID(rc->ibh)) {
		ERROR("Failed to create index buffer.");
		return FALSE;
	}
	if (!load_shader("basic.vs", &vsh)) {
		ERROR("Failed to load vertex shader.");
		return FALSE;
	}
	if (!load_shader("basic.fs", &fsh)) {
		ERROR("Failed to load fragment shader.");
		return FALSE;
	}
	rc->program = bgfx_create_program(vsh, fsh, true);
	return TRUE;
}
*/

static boolean onregister(
	struct ac_render_module * mod,
	struct ap_module_registry * registry)
{
	return TRUE;
}

static boolean oninitialize(struct ac_render_module * mod)
{
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		ERROR("Failed to initialize SDL.");
		return FALSE;
	}
	mod->wnd = SDL_CreateWindow("ArchLord Editor",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		mod->width, mod->height, SDL_WINDOW_RESIZABLE);
	if (!mod->wnd) {
		ERROR("Failed to create window.");
		return FALSE;
	}
	if (!init_bgfx(mod)) {
		ERROR("Failed to initialize bgfx.");
		return FALSE;
	}
	bgfx_set_view_clear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
		0x443355FF, 1.0f, 0);
	bgfx_set_view_rect(0, 0, 0, mod->width, mod->height);
	bgfx_reset(mod->width, mod->height, mod->reset_flags,
		BGFX_TEXTURE_FORMAT_COUNT);
	return TRUE;
}

static void onshutdown(struct ac_render_module * mod)
{
	bgfx_shutdown();
	SDL_DestroyWindow(mod->wnd);
}

struct ac_render_module * ac_render_create_module()
{
	struct ac_render_module * mod = ap_module_instance_new(AC_RENDER_MODULE_NAME,
		sizeof(*mod), onregister, oninitialize, NULL, onshutdown);
	mod->width = 1024;
	mod->height = 768;
	mod->reset_flags = BGFX_RESET_VSYNC;
	mod->view_cursor = 1;
	return mod;
}

SDL_Window * ac_render_get_window(struct ac_render_module * mod)
{
	return mod->wnd;
}

const boolean * ac_render_get_key_state(struct ac_render_module * mod)
{
	return mod->key_state;
}

boolean ac_render_button_down(struct ac_render_module * mod, mouse_button btn)
{
	return ((mod->is_mouse_btn_down & btn) != 0);
}

boolean ac_render_poll_window_event(struct ac_render_module * mod, SDL_Event * e)
{
	return SDL_PollEvent(e);
}

boolean ac_render_process_window_event(struct ac_render_module * mod, SDL_Event * e)
{
	switch (e->type) {
	case SDL_WINDOWEVENT:
		if (e->window.event == SDL_WINDOWEVENT_RESIZED ||
			e->window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
			mod->width = (uint32_t)e->window.data1;
			mod->height = (uint32_t)e->window.data2;
			bgfx_reset(mod->width, mod->height, mod->reset_flags,
				BGFX_TEXTURE_FORMAT_COUNT);
		}
		if (e->window.event == SDL_WINDOWEVENT_MINIMIZED) {
			bgfx_reset(0, 0, mod->reset_flags,
				BGFX_TEXTURE_FORMAT_COUNT);
			mod->is_minimized = TRUE;
		}
		if (e->window.event == SDL_WINDOWEVENT_RESTORED) {
			bgfx_reset(mod->width, mod->height, mod->reset_flags,
				BGFX_TEXTURE_FORMAT_COUNT);
			mod->is_minimized = FALSE;
		}
		return TRUE;
	case SDL_MOUSEBUTTONDOWN: {
		mouse_button mb;
		if (e->button.button == SDL_BUTTON_LEFT)
			mb = AC_RENDER_BUTTON_LEFT;
		else if (e->button.button == SDL_BUTTON_RIGHT)
			mb = AC_RENDER_BUTTON_RIGHT;
		else if (e->button.button == SDL_BUTTON_MIDDLE)
			mb = AC_RENDER_BUTTON_MIDDLE;
		else
			break;
		mod->is_mouse_btn_down |= mb;
		break;
	}
	case SDL_MOUSEBUTTONUP: {
		mouse_button mb;
		if (e->button.button == SDL_BUTTON_LEFT)
			mb = AC_RENDER_BUTTON_LEFT;
		else if (e->button.button == SDL_BUTTON_RIGHT)
			mb = AC_RENDER_BUTTON_RIGHT;
		else if (e->button.button == SDL_BUTTON_MIDDLE)
			mb = AC_RENDER_BUTTON_MIDDLE;
		else
			break;
		mod->is_mouse_btn_down &= ~mb;
		break;
	}
	case SDL_KEYDOWN:
		mod->key_state[e->key.keysym.scancode] = TRUE;
		break;
	case SDL_KEYUP:
		mod->key_state[e->key.keysym.scancode] = FALSE;
		break;
	case SDL_QUIT:
		core_signal_shutdown();
		return TRUE;
	}
	return FALSE;
}

boolean ac_render_begin_frame(
	struct ac_render_module * mod, 
	struct ac_camera * cam, 
	float dt)
{
	if (mod->is_minimized)
		return FALSE;
	bgfx_set_view_rect(0, 0, 0, mod->width, mod->height);
	if (cam) {
		vec3 up = {0.0f, 1.0f, 0.0f};
		uint64_t state = BGFX_STATE_WRITE_RGB |
			BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z |
			BGFX_STATE_DEPTH_TEST_LESS |
			BGFX_STATE_CULL_CCW | BGFX_STATE_MSAA;
		glm_lookat(cam->eye, cam->center, up, cam->view);
		glm_perspective(glm_rad(cam->fov),
			mod->width / (float)mod->height, 
			cam->near, cam->far, cam->proj);
		bgfx_set_view_transform(0, cam->view, cam->proj);
		bgfx_touch(0);
		//bgfx_set_state(state, 0xffffffff);
		//glm_mat4_identity(mtx);
		//glm_translate(mtx, (vec3){ 0.f, -2.f, 0.f });
		//glm_rotate(mtx, -glm_rad(mod->demo_counter * .1f), axis);
		//bgfx_set_transform(&mtx, 1);
		//bgfx_set_vertex_buffer(0, mod->vbh, 0, 8);
		//bgfx_set_index_buffer(mod->ibh, 0, 12 * 3);
		//bgfx_submit(0, mod->program, 0, BGFX_DISCARD_ALL);
	}
	return TRUE;
}

void ac_render_end_frame(struct ac_render_module * mod)
{
	mod->current_frame = bgfx_frame(false);
}

uint32_t ac_render_get_current_frame(struct ac_render_module * mod)
{
	return mod->current_frame;
}

boolean ac_render_load_shader(
	const char * file_name,
	bgfx_shader_handle_t * shader)
{
	const char * sub_path;
	char path[1024];
	size_t file_size;
	const bgfx_memory_t * mem;
	bgfx_shader_handle_t sh;
	switch (bgfx_get_renderer_type()) {
	case BGFX_RENDERER_TYPE_VULKAN:
		sub_path = "content/shaders/spirv";
		break;
	case BGFX_RENDERER_TYPE_DIRECT3D9:
		sub_path = "content/shaders/dx9";
		break;
	case BGFX_RENDERER_TYPE_DIRECT3D11:
	case BGFX_RENDERER_TYPE_DIRECT3D12:
		sub_path = "content/shaders/dx11";
		break;
	default:
		return FALSE;
	}
	make_path(path, sizeof(path), "%s/%s.bin",
		sub_path, file_name);
	if (!get_file_size(path, &file_size))
		return FALSE;
	mem = bgfx_alloc((uint32_t)file_size);
	if (!load_file(path, mem->data, file_size))
		return FALSE;
	sh = bgfx_create_shader(mem);
	if (!BGFX_HANDLE_IS_VALID(sh))
		return FALSE;
	*shader = sh;
	return TRUE;
}

void ac_render_reserve_crt_set(
	struct ac_render_crt * crt,
	uint32_t count)
{
	struct ac_render_crt_data d = { 0 };
	uint32_t i;
	if (!(count > crt->set_count))
		return;
	d.render_type = alloc(count * sizeof(*d.render_type));
	d.cust_data = alloc(count * sizeof(*d.cust_data));
	memcpy(d.render_type, crt->render_type.render_type,
		crt->set_count * sizeof(*d.render_type));
	memcpy(d.cust_data, crt->render_type.cust_data,
		crt->set_count * sizeof(*d.cust_data));
	for (i = crt->set_count; i < count; i++) {
		d.render_type[i] = 0;
		d.cust_data[i] = AC_RENDER_CRT_DATA_CUST_DATA_NONE;
	}
	dealloc(crt->render_type.render_type);
	dealloc(crt->render_type.cust_data);
	crt->render_type = d;
	crt->set_count = count;
}

enum ac_render_stream_read_result ac_render_stream_read_crt(
	struct ap_module_stream * stream,
	struct ac_render_crt * crt)
{
	const char * value_name = ap_module_stream_get_value_name(stream);
	if (strncmp(value_name, AC_RENDER_CRT_STREAM_NUM,
			strlen(AC_RENDER_CRT_STREAM_NUM)) == 0) {
		uint32_t count;
		ap_module_stream_get_u32(stream, &count);
		ac_render_reserve_crt_set(crt, count);
	}
	else if (strncmp(value_name, AC_RENDER_CRT_STREAM_RENDERTYPE,
			strlen(AC_RENDER_CRT_STREAM_RENDERTYPE)) == 0) {
		const char * value = ap_module_stream_get_value(stream);
		uint32_t index;
		int32_t render_type;
		int32_t cust_data;
		if (sscanf(value, "%u:%d:%d", &index, &render_type, 
				&cust_data) != 3) {
			ERROR("sscanf() failed.");
			return AC_RENDER_STREAM_READ_RESULT_ERROR;
		}
		if (index >= crt->set_count) {
			ERROR("Invalid index value.");
			return AC_RENDER_STREAM_READ_RESULT_ERROR;
		}
		crt->render_type.render_type[index] = render_type;
		crt->render_type.cust_data[index] = cust_data;
	}
	else {
		return AC_RENDER_STREAM_READ_RESULT_PASS;
	}
	return AC_RENDER_STREAM_READ_RESULT_READ;
}

boolean ac_render_stream_write_clump_render_type(
	struct ap_module_stream * stream,
	struct ac_render_crt * crt)
{
	boolean result = TRUE;
	char valuename[256];
	char value[256];
	if (crt->set_count > 0) {
		uint32_t i;
		result &= ap_module_stream_write_i32(stream, AC_RENDER_CRT_STREAM_NUM, crt->set_count);
		for (i = 0; i < crt->set_count; i++) {
			snprintf(valuename, sizeof(valuename), "%s%d", AC_RENDER_CRT_STREAM_RENDERTYPE, i);
			snprintf(value, sizeof(value), "%d:%d:%d", i, 
				crt->render_type.render_type[i], crt->render_type.cust_data[i]);
			result &= ap_module_stream_write_value(stream, valuename, value);
		}
	}
	return result;
}

int ac_render_allocate_view(struct ac_render_module * mod)
{
	return mod->view_cursor++;
}

int ac_render_set_view(struct ac_render_module * mod, int view_id)
{
	int view = mod->view_id;
	mod->view_id = view_id;
	return view;
}

int ac_render_get_view(struct ac_render_module * mod)
{
	return mod->view_id;
}
