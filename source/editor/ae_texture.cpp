#include "editor/ae_texture.h"

#include "core/bin_stream.h"
#include "core/core.h"
#include "core/log.h"
#include "core/malloc.h"
#include "core/os.h"
#include "core/string.h"
#include "core/vector.h"

#include "public/ap_config.h"

#include "client/ac_camera.h"
#include "client/ac_texture.h"
#include "client/ac_render.h"
#include "client/ac_imgui.h"

#include "vendor/cglm/cglm.h"
#include "vendor/imgui/imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "vendor/imgui/imgui_internal.h"
#include <assert.h>
#include <string.h>

#define MAX_BROWSER_COUNT 8

#define INVALIDATE_HANDLE(tex) \
	(tex).idx = UINT16_MAX;

struct browser_tex {
	char name[128];
	bgfx_texture_info_t info;
	bgfx_texture_handle_t handle;
	boolean pending_load;
};

struct browser_data {
	enum ac_dat_directory dir;
	uint32_t tex_count;
	struct browser_tex * textures;
	uint32_t refcount;
};

struct browser_instance {
	boolean in_use;
	boolean is_open;
	char title[128];
	struct browser_data * data;
};

struct ae_texture_module {
	struct ap_module_instance instance;
	struct ac_dat_module * ac_dat;
	struct ac_texture_module * ac_texture;
	boolean terminate_task;
	struct browser_data * browser_data_[AC_DAT_DIR_COUNT];
	struct browser_instance browser_instance[MAX_BROWSER_COUNT];
};

static void deref_browser_data(
	struct ae_texture_module * mod,
	struct browser_data * browser);

static struct browser_data * load_directory(
	struct ae_texture_module * mod,
	enum ac_dat_directory dir)
{
	uint32_t count;
	struct ac_dat_resource * r;
	uint32_t i;
	struct browser_data * browser = mod->browser_data_[dir];
	if (browser) {
		browser->refcount++;
		return browser;
	}
	r = ac_dat_get_resources(mod->ac_dat, dir, &count);
	if (!r || !count)
		return NULL;
	browser = (struct browser_data *)alloc(sizeof(*browser));
	memset(browser, 0, sizeof(*browser));
	browser->dir = dir;
	browser->refcount = 1;
	mod->browser_data_[dir] = browser;
	browser->tex_count = count;
	browser->textures = (struct browser_tex *)alloc(count * sizeof(*browser->textures));
	memset(browser->textures, 0,
		count * sizeof(*browser->textures));
	for (i = 0; i < count; i++) {
		struct browser_tex * tex = &browser->textures[i];
		/* Set browser texture data. */
		strlcpy(tex->name, r[i].name, sizeof(tex->name));
		INVALIDATE_HANDLE(tex->handle);
		tex->pending_load = TRUE;
	}
	return browser;
}

static void deref_browser_data(
	struct ae_texture_module * mod,
	struct browser_data * browser)
{
	uint32_t i;
	assert(browser->refcount != 0);
	if (--browser->refcount)
		return;
	for (i = 0; i < browser->tex_count; i++) {
		const struct browser_tex * t = &browser->textures[i];
		if (BGFX_HANDLE_IS_VALID(t->handle))
			ac_texture_release(mod->ac_texture, t->handle);
	}
	browser->tex_count = 0;
	dealloc(browser->textures);
	browser->textures = NULL;
	mod->browser_data_[browser->dir] = NULL;
	dealloc(browser);
}

static void close_browser(
	struct ae_texture_module * mod,
	struct browser_instance * instance)
{
	deref_browser_data(mod, instance->data);
	instance->in_use = FALSE;
}

static boolean onregister(
	struct ae_texture_module * mod,
	struct ap_module_registry * registry)
{
	mod->ac_dat = (struct ac_dat_module *)ap_module_registry_get_module(
		registry, AC_DAT_MODULE_NAME);
	if (!mod->ac_dat) {
		ERROR("Failed to retrieve module (%s).", AC_DAT_MODULE_NAME);
		return FALSE;
	}
	mod->ac_texture = (struct ac_texture_module *)ap_module_registry_get_module(
		registry, AC_TEXTURE_MODULE_NAME);
	if (!mod->ac_texture) {
		ERROR("Failed to retrieve module (%s).", AC_TEXTURE_MODULE_NAME);
		return FALSE;
	}
	return TRUE;
}

static void onshutdown(struct ae_texture_module * mod)
{
	uint32_t i;
	mod->terminate_task = TRUE;
	for (i = 0; i < COUNT_OF(mod->browser_data_); i++) {
		while (mod->browser_data_[i])
			deref_browser_data(mod, mod->browser_data_[i]);
	}
}

struct ae_texture_module * ae_texture_create_module()
{
	struct ae_texture_module * mod = (struct ae_texture_module *)ap_module_instance_new(
		AE_TEXTURE_MODULE_NAME, sizeof(*mod), 
		(ap_module_instance_register_t)onregister, NULL, NULL, 
		(ap_module_instance_shutdown_t)onshutdown);
	return mod;
}

void ae_texture_process_loading_queue(
	struct ae_texture_module * mod,
	uint32_t limit)
{
	uint32_t i;
	for (i = 0; i < MAX_BROWSER_COUNT; i++) {
		struct browser_instance * browser = &mod->browser_instance[i];
		uint32_t j;
		if (!browser->in_use)
			continue;
		for (j = 0; j < browser->data->tex_count; j++) {
			struct browser_tex * tex = &browser->data->textures[j];
			if (!tex->pending_load)
				continue;
			tex->pending_load = FALSE;
			tex->handle = ac_texture_load_packed(mod->ac_texture, 
				browser->data->dir, tex->name, FALSE, &tex->info);
		}
	}
}

uint32_t ae_texture_browse(
	struct ae_texture_module * mod,
	enum ac_dat_directory dir,
	const char * title)
{
	uint32_t i;
	struct browser_instance * browser = NULL;
	for (i = 0; i < MAX_BROWSER_COUNT; i++) {
		if (!mod->browser_instance[i].in_use) {
			browser = &mod->browser_instance[i];
			break;
		}
	}
	if (!browser)
		return UINT32_MAX;
	browser->data = load_directory(mod, dir);
	if (!browser->data)
		return UINT32_MAX;
	browser->in_use = TRUE;
	browser->is_open = TRUE;
	strlcpy(browser->title, title, sizeof(browser->title));
	return i;
}

boolean ae_texture_do_browser(
	struct ae_texture_module * mod,
	uint32_t handle,
	bgfx_texture_handle_t * selected_tex)
{
	struct browser_instance * instance;
	struct browser_data * data;
	char label[256];
	uint32_t i;
	if (handle >= MAX_BROWSER_COUNT) {
		if (selected_tex)
			INVALIDATE_HANDLE(*selected_tex);
		return TRUE;
	}
	instance = &mod->browser_instance[handle];
	if (!instance->in_use) {
		if (selected_tex)
			INVALIDATE_HANDLE(*selected_tex);
		return TRUE;
	}
	data = instance->data;
	snprintf(label, sizeof(label), "%s###%p",
		instance->title, instance);
	if (!instance->is_open) {
		close_browser(mod, instance);
		if (selected_tex)
			INVALIDATE_HANDLE(*selected_tex);
		return TRUE;
	}
	ImGui::SetNextWindowSize(ImVec2(580.f, 580.f),
		ImGuiCond_Appearing);
	if (!ImGui::Begin(label, (bool *)&instance->is_open, 0)) {
		ImGui::End();
		return FALSE;
	}
	for (i = 0; i < data->tex_count; i++) {
		ImVec2 avail;
		const struct browser_tex * t = &data->textures[i];
		if (i)
			ImGui::SameLine(0.0f, -1.0f);
		avail = ImGui::GetContentRegionAvail();
		if (avail.x < 128.f)
			ImGui::NewLine();
		if (BGFX_HANDLE_IS_VALID(t->handle)) {
			ImGui::Image((ImTextureID)t->handle.idx,
				ImVec2(128.f, 128.f),
				ImVec2(0.f, 0.f), ImVec2(1.f, 1.f),
				ImVec4(1.f, 1.f, 1.f, 1.f),
				ImVec4(.2f, 0.2f, 0.2f, 0.2f));
		}
		else {
			ImGui::Dummy(ImVec2(128.f, 128.f));
		}
		if (ImGui::IsItemHovered() && ImGui::BeginTooltip()) {
			ImGui::Text("Name: %s", t->name);
			ImGui::Text("Size: %ux%u", t->info.width, t->info.height);
			ImGui::EndTooltip();
		}
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
			ImGui::End();
			if (selected_tex) {
				*selected_tex = t->handle;
				ac_texture_copy(mod->ac_texture, t->handle);
			}
			close_browser(mod, instance);
			return TRUE;
		}
	}
	ImGui::End();
	return FALSE;
}
