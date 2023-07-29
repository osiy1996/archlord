#include <stdio.h>
#include <string.h>

#include <core/core.h>
#include <core/log.h>
#include <core/os.h>
#include <core/file_system.h>

#include <task/task.h>

#include "public/ap_auction.h"
#include "public/ap_event_binding.h"
#include "public/ap_event_gacha.h"
#include "public/ap_event_manager.h"
#include "public/ap_event_nature.h"
#include "public/ap_event_point_light.h"
#include "public/ap_event_product.h"
#include "public/ap_event_quest.h"
#include "public/ap_event_teleport.h"
#include "public/ap_dungeon_wnd.h"
#include "public/ap_map.h"
#include "public/ap_object.h"
//#include "public/ap_plg_boss_spawn.h"
#include "public/ap_shrine.h"

#include "client/ac_amb_occl_map.h"
#include "client/ac_event_point_light.h"
#include "client/ac_object.h"

#include "runtime/ar_camera.h"
#include "runtime/ar_config.h"
#include "runtime/ar_console.h"
#include "runtime/ar_dat.h"
#include "runtime/ar_debug_draw.h"
#include "runtime/ar_effect.h"
#include "runtime/ar_renderer.h"
#include "runtime/ar_terrain.h"
#include "runtime/ar_texture.h"
#include "runtime/ar_imgui.h"
#include "runtime/ar_mesh.h"

#include "editor/ae_terrain.h"
#include "editor/ae_texture.h"
#include "editor/ae_object.h"

#include <vendor/SDL2/SDL_main.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <Windows.h>
#endif

#define CHECKSTARTUP(call)\
	if (!(call))\
		return FALSE;

struct camera_controls {
	boolean key_state[512];
};

static void logcb(
	enum LogLevel level,
	const char * file,
	uint32_t line,
	const char * message)
{
	const float * color;
	const char * label;
#ifdef _WIN32
	char str[1024];
	snprintf(str, sizeof(str), "%s:%u| %s\n", file, line,
		message);
	OutputDebugStringA(str);
#endif
	switch (level) {
	case LOG_LEVEL_TRACE: {
		static float c[4] = { 0.54f, 0.74f, 0.71f, 1.0f };
		color = c;
		label = "TRACE";
		break;
	}
	case LOG_LEVEL_INFO: {
		static float c[4] = { 0.80f, 0.80f, 0.80f, 1.0f };
		color = c;
		label = "INFO";
		break;
	}
	case LOG_LEVEL_WARN: {
		static float c[4] = { 0.94f, 0.77f, 0.45f, 1.0f };
		color = c;
		label = "WARNING";
		break;
	}
	case LOG_LEVEL_ERROR: {
		static float c[4] = { 0.80f, 0.40f, 0.40f, 1.0f };
		color = c;
		label = "ERROR";
		break;
	}
	default: {
		static float c[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		color = c;
		label = "UNKNOWN";
		break;
	}
	}
	ar_con_println_colored("%-10s %s", color, label, message);
}

static boolean startup()
{
	CHECKSTARTUP(core_startup());
	CHECKSTARTUP(task_startup());
	CHECKSTARTUP(ar_cfg_startup());
	CHECKSTARTUP(ar_dat_startup());
	CHECKSTARTUP(ar_rnd_startup());
	CHECKSTARTUP(ar_tex_startup());
	CHECKSTARTUP(ar_con_startup());
	CHECKSTARTUP(ar_dd_startup());
	CHECKSTARTUP(ar_mesh_startup());
	CHECKSTARTUP(ap_map_startup());
	CHECKSTARTUP(ap_obj_startup());
	CHECKSTARTUP(ac_obj_startup());
	CHECKSTARTUP(ar_eff_startup());
	CHECKSTARTUP(ar_trn_startup());
	CHECKSTARTUP(ae_tex_startup());
	CHECKSTARTUP(ae_trn_startup());
	CHECKSTARTUP(ae_obj_startup());
	return TRUE;
}

static void on_keydown_cam(
	struct camera_controls * ctrl,
	const boolean * key_state,
	const SDL_KeyboardEvent * e)
{
	if (e->repeat ||
		key_state[SDL_SCANCODE_LCTRL] ||
		key_state[SDL_SCANCODE_RCTRL])
		return;
	ctrl->key_state[e->keysym.scancode] = TRUE;
}

static void on_keyup_cam(
	struct camera_controls * ctrl,
	const SDL_KeyboardEvent * e)
{
	ctrl->key_state[e->keysym.scancode] = FALSE;
}

static void update_camera(
	struct ar_camera * cam,
	struct camera_controls * ctrl,
	float dt)
{
	const boolean * key_state = ctrl->key_state;
	float speed =
		key_state[SDL_SCANCODE_LSHIFT] ? 1000.0f : 10000.0f;
	uint32_t mb_state = SDL_GetMouseState(NULL, NULL);
	/*
	if (key_state[SDL_SCANCODE_W])
		ar_cam_translate(cam, dt * speed, 0.f, 0.f);
	else if (key_state[SDL_SCANCODE_S])
		ar_cam_translate(cam, -dt * speed, 0.f, 0.f);
	if (key_state[SDL_SCANCODE_D])
		ar_cam_translate(cam, 0.f, dt * speed, 0.f);
	else if (key_state[SDL_SCANCODE_A])
		ar_cam_translate(cam, 0.f, -dt * speed, 0.f);
	if (key_state[SDL_SCANCODE_UP])
		ar_cam_translate(cam, 0.f, 0.f, dt * speed);
	else if (key_state[SDL_SCANCODE_DOWN])
		ar_cam_translate(cam, 0.f, 0.f, -dt * speed);
	*/
	if ((mb_state & SDL_BUTTON(SDL_BUTTON_LEFT)) &&
		(mb_state & SDL_BUTTON(SDL_BUTTON_RIGHT))) {
		ar_cam_translate(cam, 
			speed * dt, 0.0f, 0.0f);
	}
	ar_cam_update(cam, dt);
}

static void render(struct ar_camera * cam, float dt)
{
	static bool b = true;
	if (!ar_rnd_begin_frame(cam, dt))
		return;
	ar_trn_render();
	ae_trn_render(cam);
	ac_obj_render();
	ae_obj_render_outline(cam);
	ar_imgui_new_frame();
	igBeginMainMenuBar();
	if (igBeginMenu("File", true)) {
		igEndMenu();
	}
	if (igBeginMenu("View", true)) {
		igEndMenu();
	}
	ar_con_render_icon();
	igEndMainMenuBar();
	ar_imgui_begin_toolbar();
	ae_trn_toolbar();
	ar_imgui_end_toolbar();
	ar_imgui_viewport();
	ar_con_render();
	ae_trn_imgui();
	ae_obj_imgui();
	ar_imgui_begin_toolbox();
	ae_trn_toolbox();
	ar_imgui_end_toolbox();
	ar_imgui_render();
	ar_dd_begin();
	ar_dd_end();
	ar_rnd_end_frame();
}

int main(int argc, char ** argv)
{
	timer_t timer;
	struct ar_camera cam;
	struct camera_controls cam_ctrl = { 0 };
	if (!log_init()) {
		fprintf(stderr, "log_init() failed.\n");
		return -1;
	}
	log_set_callback(logcb);
	if (!startup())
		return -1;
	if (!ar_rnd_init()) {
		ERROR("Failed to initialize renderer.");
		return -1;
	}
	if (!ar_con_init()) {
		ERROR("Failed to initialize ar_con module.");
		return -1;
	}
	if (!ar_dd_init()) {
		ERROR("Failed to initialize ar_dd module.");
		return -1;
	}
	if (!ar_mesh_init()) {
		ERROR("Failed to initialize ar_mesh module.");
		return -1;
	}
	if (!ap_map_init()) {
		ERROR("Failed to initialize ap_map module.");
		return -1;
	}
	if (!ap_obj_init()) {
		ERROR("Failed to initialize ap_obj module.");
		return -1;
	}
	if (!ap_plg_bsp_init()) {
		ERROR("Failed to initialize ap_plg_bsp module.");
		return -1;
	}
	if (!ac_obj_init()) {
		ERROR("Failed to initialize ac_obj module.");
		return -1;
	}
	if (!ac_aomap_init()) {
		ERROR("Failed to initialize ac_aomap module.");
		return -1;
	}
	if (!ap_em_init()) {
		ERROR("Failed to initialize ap_em module.");
		return -1;
	}
	if (!ap_eb_init()) {
		ERROR("Failed to initialize ap_eb module.");
		return -1;
	}
	if (!ap_egc_init()) {
		ERROR("Failed to initialize ap_egc module.");
		return -1;
	}
	if (!ap_ent_init()) {
		ERROR("Failed to initialize ap_ent module.");
		return -1;
	}
	if (!ap_epr_init()) {
		ERROR("Failed to initialize ap_epr module.");
		return -1;
	}
	if (!ap_eqs_init()) {
		ERROR("Failed to initialize ap_eqs module.");
		return -1;
	}
	if (!ap_etp_init()) {
		ERROR("Failed to initialize ap_etp module.");
		return -1;
	}
	if (!ap_auc_init()) {
		ERROR("Failed to initialize ap_auc module.");
		return -1;
	}
	if (!ap_shrine_init()) {
		ERROR("Failed to initialize ap_shrine module.");
		return -1;
	}
	if (!ap_dwnd_init()) {
		ERROR("Failed to initialize ap_dwnd module.");
		return -1;
	}
	if (!ap_epl_init()) {
		ERROR("Failed to initialize ap_epl module.");
		return -1;
	}
	if (!ac_epl_init()) {
		ERROR("Failed to initialize ac_epl module.");
		return -1;
	}
	if (!ar_trn_init()) {
		ERROR("Failed to initialize ar_trn module.");
		return -1;
	}
	if (!ar_eff_init()) {
		ERROR("Failed to initialize ar_eff module.");
		return -1;
	}
	if (!ar_imgui_init(255, ar_rnd_get_window())) {
		ERROR("Failed to initialize ar_imgui module.");
		return -1;
	}
	if (!ae_trn_init()) {
		ERROR("Failed to initialize ae_trn module.");
		return -1;
	}
	if (!ae_obj_init()) {
		ERROR("Failed to initialize ae_obj module.");
		return -1;
	}
	timer = create_timer();
	if (!timer) {
		ERROR("Failed to create timer.");
		return -1;
	}
	ar_cam_init(&cam, (vec3) { -437032.4f, 4785.9f, 161015.2f }, 
		5000.f, 45.f, 30.f,
		60.f, 200.f, 40000.f);
	ar_trn_sync(cam.center, TRUE);
	ac_obj_sync(cam.center, TRUE);
	while (!core_should_shutdown()) {
		uint64_t dt_nano = timer_delta(timer, TRUE);
		float dt = dt_nano / 1000000.f;
		SDL_Event e;
		while (ar_rnd_poll_window_event(&e)) {
			if (ar_imgui_process_event(&e))
				continue;
			ar_rnd_process_window_event(&e);
			switch (e.type) {
			case SDL_MOUSEMOTION:
				if (ar_rnd_button_down(AR_RND_BUTTON_RIGHT)) {
					ar_cam_rotate(&cam,
						e.motion.yrel * .3f,
						e.motion.xrel * .3f);
				}
				else if (ar_rnd_button_down(AR_RND_BUTTON_MIDDLE)) {
					ar_cam_slide(&cam,
						-(float)e.motion.xrel * 10.f,
						(float)e.motion.yrel * 10.f);
				}
				else {
					if (ae_trn_on_mmove(&cam, e.motion.x, 
							e.motion.y) ||
						ae_obj_on_mmove(&cam, 
							e.motion.x, e.motion.y, 
							e.motion.xrel, e.motion.yrel)) {
						break;
					}
				}
				break;
			case SDL_MOUSEWHEEL:
				if (!ae_trn_on_mwheel(e.wheel.preciseY))
					ar_cam_zoom(&cam, e.wheel.preciseY * 1000.f);
				break;
			case SDL_MOUSEBUTTONDOWN:
				if (e.button.button == SDL_BUTTON_LEFT) {
					uint32_t mb_state = 
						SDL_GetMouseState(NULL, NULL);
					if (mb_state & SDL_BUTTON(SDL_BUTTON_RIGHT))
						break;
					if (ae_obj_on_mdown(&cam, 
							e.button.x, e.button.y)) {
						break;
					}
					ae_trn_on_mdown(&cam, e.button.x, e.button.y);
				}
				break;
			case SDL_KEYDOWN:
				if (ae_trn_on_key_down(e.key.keysym.sym) ||
					ae_obj_on_key_down(&cam, e.key.keysym.sym)) {
					break;
				}
				on_keydown_cam(&cam_ctrl, ar_rnd_get_key_state(),
					&e.key);
				break;
			case SDL_KEYUP:
				on_keyup_cam(&cam_ctrl, &e.key);
				break;
			}
		}
		update_camera(&cam, &cam_ctrl, dt);
		ar_trn_sync(cam.center, FALSE);
		ar_trn_update(dt);
		ae_tex_update();
		ae_trn_update(dt);
		ac_obj_sync(cam.center, FALSE);
		ac_obj_update(dt);
		render(&cam, dt);
		task_do_post_cb();
		sleep(1);
	}
	ae_obj_shutdown();
	ae_trn_shutdown();
	ae_tex_shutdown();
	ar_trn_shutdown();
	ar_imgui_shutdown();
	ar_eff_shutdown();
	ac_epl_shutdown();
	ap_epl_shutdown();
	ac_obj_shutdown();
	ap_dwnd_shutdown();
	ap_obj_shutdown();
	ap_map_shutdown();
	ap_em_shutdown();
	ar_mesh_shutdown();
	ar_dd_shutdown();
	ar_con_shutdown();
	ar_tex_shutdown();
	ar_rnd_shutdown();
	task_shutdown();
	return 0;
}
