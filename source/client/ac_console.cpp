#include "client/ac_console.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/os.h"
#include "core/string.h"
#include "core/vector.h"

#include "client/ac_texture.h"

#include "vendor/bgfx/c99/bgfx.h"
#define IMGUI_DEFINE_MATH_OPERATORS 
#include "vendor/imgui/imgui.h"
#include "vendor/imgui/imgui_internal.h"

#include <time.h>
#include <stdlib.h>

struct console_log {
	char * string;
	boolean has_color;
	float color[4];
};

struct ac_console_module {
	struct ap_module_instance instance;
	struct ac_texture_module * ac_texture;
	boolean is_displayed;
	boolean auto_scroll;
	boolean scroll_to_bottom;
	struct console_log * items;
	char input[256];
	mutex_t mutex;
	bgfx_texture_handle_t icon;
};

static void println_internal(
	struct ac_console_module * mod,
	const char * fmt,
	bool has_color,
	const float color[4],
	va_list ap)
{
	int size;
	struct tm tm;
	time_t t = time(NULL);
	char date[128];
	struct console_log log = { 0 };
	size_t len;
	memset(&tm, 0, sizeof(struct tm));
	localtime_s(&tm, &t);
	strftime(date, sizeof(date), "%a %b %d %T| ", &tm);
	size = vsnprintf(NULL, 0, fmt, ap);
	if (size <= 0) {
		va_end(ap);
		return;
	}
	len = strlen(date);
	log.string = (char *)alloc(size + len + 1);
	memcpy(log.string, date, len);
	vsnprintf(log.string + len, (size_t)size + 1, fmt, ap);
	log.has_color = has_color;
	memcpy(log.color, color, sizeof(log.color));
	lock_mutex(mod->mutex);
	vec_push_back((void **)&mod->items, &log);
	unlock_mutex(mod->mutex);
}

static void cleac_console(struct ac_console_module * mod)
{
	uint32_t i;
	lock_mutex(mod->mutex);
	for (i = 0; i < vec_count(mod->items); i++)
		dealloc(mod->items[i].string);
	vec_clear(mod->items);
	unlock_mutex(mod->mutex);
}

static int Stricmp(const char * s1, const char * s2)
{
	int d;
	while ((d = ImToUpper(*s2) - ImToUpper(*s1)) == 0 && *s1) {
		s1++;
		s2++;
	}
	return d;
}

static int strnicmp(const char * s1, const char * s2, int n)
{
	int d = 0;
	while (n > 0 && (d = ImToUpper(*s2) - ImToUpper(*s1)) == 0 && *s1) {
		s1++;
		s2++;
		n--;
	}
	return d;
}

static char * Strdup(const char * s)
{
	IM_ASSERT(s);
	size_t len = strlen(s) + 1;
	void * buf = alloc(len);
	IM_ASSERT(buf);
	return (char *)memcpy(buf, (const void*)s, len);
}

static void strtrim(char * s)
{
	char * str_end = s + strlen(s);
	while (str_end > s && str_end[-1] == ' ')
		str_end--;
	*str_end = 0;
}

static ImVec4 make_color(const float c[4])
{
	return ImVec4(c[0], c[1], c[2], c[3]);
}

static boolean onregister(
	struct ac_console_module * mod,
	struct ap_module_registry * registry)
{
	mod->ac_texture = (struct ac_texture_module *)ap_module_registry_get_module(
		registry, AC_TEXTURE_MODULE_NAME);
	if (!mod->ac_texture) {
		ERROR("Failed to retrieve module (%s).", AC_TEXTURE_MODULE_NAME);
		return FALSE;
	}
	return TRUE;
}

static boolean oninitialize(struct ac_console_module * mod)
{
	mod->icon = ac_texture_load(mod->ac_texture, "content/textures/command-line.png",
		NULL);
	if (!BGFX_HANDLE_IS_VALID(mod->icon)) {
		ERROR("Failed to load console icon.");
		return FALSE;
	}
	return TRUE;
}

static void onshutdown(struct ac_console_module * mod)
{
	cleac_console(mod);
	vec_free(mod->items);
	destroy_mutex(mod->mutex);
	ac_texture_release(mod->ac_texture, mod->icon);
}

struct ac_console_module * ac_console_create_module()
{
	struct ac_console_module * mod = 
		(struct ac_console_module *)ap_module_instance_new(AC_CONSOLE_MODULE_NAME,
			sizeof(*mod), (ap_module_instance_register_t)onregister, 
			(ap_module_instance_initialize_t)oninitialize, NULL, 
			(ap_module_instance_shutdown_t)onshutdown);
	mod->items = (struct console_log *)vec_new_reserved(sizeof(*mod->items), 128);
	mod->mutex = create_mutex();
	BGFX_INVALIDATE_HANDLE(mod->icon);
	mod->auto_scroll = TRUE;
	return mod;
}

void ac_console_println(struct ac_console_module * mod, const char * fmt, ...)
{
	float c[4] = { 0 };
	va_list ap;
	va_start(ap, fmt);
	println_internal(mod, fmt, false, c, ap);
	va_end(ap);
}

void ac_console_vprintln(
	struct ac_console_module * mod, 
	const char * fmt, 
	va_list ap)
{
	float c[4] = { 0 };
	println_internal(mod, fmt, false, c, ap);
}

void ac_console_println_colored(
	struct ac_console_module * mod, 
	const char * fmt, 
	const float color[4], ...)
{
	va_list ap;
	va_start(ap, color);
	println_internal(mod, fmt, true, color, ap);
	va_end(ap);
}

void ac_console_render(struct ac_console_module * mod)
{
	ImGuiListClipper clipper;
	if (!mod->is_displayed)
		return;
	ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Console", (bool *)&mod->is_displayed)) {
		ImGui::End();
		return;
	}
	//static float t = 0.0f; if (ImGui::GetTime() - t > 0.02f) { t = ImGui::GetTime(); AddLog("Spam %f", t); }
	// Reserve enough left-over height for 1 separator + 1 input text
	const float footer_height_to_reserve =
		ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
	ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve),
		false, ImGuiWindowFlags_HorizontalScrollbar);
	if (ImGui::BeginPopupContextWindow()) {
		if (ImGui::Selectable("Clear"))
			cleac_console(mod);
		ImGui::EndPopup();
	}
	// Display every line as a separate entry so we can change their color or add custom widgets.
	// If you only want raw text you can use ImGui::TextUnformatted(log.begin(), log.end());
	// NB- if you have thousands of entries this approach may be too inefficient and may require user-side clipping
	// to only process visible items. The clipper will automatically measure the height of your first item and then
	// "seek" to display only items in the visible area.
	// To use the clipper we can replace your standard loop:
	//      for (int i = 0; i < Items.Size; i++)
	//   With:
	//      ImGuiListClipper clipper;
	//      clipper.Begin(Items.Size);
	//      while (clipper.Step())
	//         for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
	// - That your items are evenly spaced (same height)
	// - That you have cheap random access to your elements (you can access them given their index,
	//   without processing all the ones before)
	// You cannot this code as-is if a filter is active because it breaks the 'cheap random-access' property.
	// We would need random-access on the post-filtered list.
	// A typical application wanting coarse clipping and filtering may want to pre-compute an array of indices
	// or offsets of items that passed the filtering test, recomputing this array when user changes the filter,
	// and appending newly elements as they are inserted. This is left as a task to the user until we can manage
	// to improve this example code!
	// If your items are of variable height:
	// - Split them into same height items would be simpler and facilitate random-seeking into your list.
	// - Consider using manual call to IsRectVisible() and skipping extraneous decoration from your items.
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing
	lock_mutex(mod->mutex);
	clipper.Begin((int)vec_count(mod->items));
	while (clipper.Step()) {
		for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
			const struct console_log & log = mod->items[i];
			// Normally you would store more information in your item than just a string.
			// (e.g. make Items[] an array of structure, store color/type etc.)
			ImVec4 color;
			if (log.has_color) {
				ImGui::PushStyleColor(ImGuiCol_Text,
					make_color(log.color));
			}
			ImGui::TextUnformatted(log.string);
			if (log.has_color)
				ImGui::PopStyleColor();
		}
	}
	clipper.End();
	unlock_mutex(mod->mutex);
	if (mod->scroll_to_bottom ||
		(mod->auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())) {
		ImGui::SetScrollHereY(1.0f);
	}
	mod->scroll_to_bottom = false;
	ImGui::PopStyleVar();
	ImGui::EndChild();
	ImGui::Separator();
	// Command-line
	bool reclaim_focus = false;
	ImGuiInputTextFlags input_text_flags =
		ImGuiInputTextFlags_EnterReturnsTrue;
	if (ImGui::InputText("Input", mod->input, 
		IM_ARRAYSIZE(mod->input), input_text_flags)) {
		char * s = mod->input;
		strtrim(s);
		//if (s[0])
		//	ExecCommand(s);
		strcpy(s, "");
		reclaim_focus = true;
	}
	// Auto-focus on window apparition
	ImGui::SetItemDefaultFocus();
	if (reclaim_focus)
		ImGui::SetKeyboardFocusHere(-1); // Auto focus previous widget
	ImGui::End();
}

void ac_console_render_icon(struct ac_console_module * mod)
{
	ImVec2 cur;
	cur = ImGui::GetCursorPos();
	ImGui::SetCursorPos(ImVec2(ImGui::GetIO().DisplaySize.x - 18,
		cur.y + ((ImGui::GetWindowHeight() - 16) / 2.0f)));
	ImGui::Image((ImTextureID)mod->icon.idx, ImVec2(16, 16));
	if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
		mod->is_displayed ^= TRUE;
	ImGui::SetCursorPos(cur);
}

