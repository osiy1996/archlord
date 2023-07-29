#include "client/ac_imgui.h"

#include "core/log.h"
#include "core/malloc.h"

#include "public/ap_module_instance.h"

#include "client/ac_imgui_impl_sdl2.h"
#include "client/ac_imgui_impl_bgfx.h"

#include "vendor/imgui/imgui_internal.h"

struct ac_imgui_module {
	struct ap_module_instance instance;
	ImGuiID dock_id;
	float toolbox_height;
};

static void image_inner_border(
	bgfx_texture_handle_t tex,
	const ImVec2 & size,
	const ImVec2 & uv0,
	const ImVec2 & uv1,
	const ImVec4 & tint_col,
	const ImVec4 & border_col)
{
	using namespace ImGui;
	ImGuiWindow * window = GetCurrentWindow();
	ImRect bb;
	if (window->SkipItems)
		return;
	bb = ImRect(window->DC.CursorPos, window->DC.CursorPos + size);
	ItemSize(bb);
	if (!ItemAdd(bb, 0))
		return;
	if (border_col.w > 0.0f) {
		window->DrawList->AddRect(bb.Min, bb.Max, 
			GetColorU32(border_col), 0.0f);
		window->DrawList->AddImage((ImTextureID)tex.idx, 
			bb.Min, bb.Max,
			uv0, uv1, GetColorU32(tint_col));
	}
	else {
		window->DrawList->AddImage((ImTextureID)tex.idx,
			bb.Min, bb.Max, uv0, uv1, GetColorU32(tint_col));
	}
}

static void onshutdown(struct ac_imgui_module * mod)
{
	ImGui_ImplSDL2_Shutdown();
	ImGui_Implbgfx_Shutdown();
	ImGui::DestroyContext();
}

struct ac_imgui_module * ac_imgui_create_module()
{
	struct ac_imgui_module * mod = (struct ac_imgui_module *)ap_module_instance_new(
		AC_IMGUI_MODULE_NAME, sizeof(*mod), NULL, NULL, NULL, 
		(ap_module_instance_shutdown_t)onshutdown);
	return mod;
}

boolean ac_imgui_init(
	struct ac_imgui_module * mod,
	int bgfx_view, 
	struct SDL_Window * window)
{
	mod->toolbox_height = 220.0f;
	ImGui::CreateContext();
	ImGui::GetIO().ConfigFlags = ImGuiConfigFlags_DockingEnable;
	ImGui::GetIO().Fonts->AddFontDefault();
	ImGui_Implbgfx_Init(bgfx_view);
	switch (bgfx_get_renderer_type()) {
    case BGFX_RENDERER_TYPE_DIRECT3D9:
    case BGFX_RENDERER_TYPE_DIRECT3D11:
    case BGFX_RENDERER_TYPE_DIRECT3D12:
        return ImGui_ImplSDL2_InitForD3D(window);
    case BGFX_RENDERER_TYPE_METAL:
        return ImGui_ImplSDL2_InitForMetal(window);
    case BGFX_RENDERER_TYPE_VULKAN:
		return ImGui_ImplSDL2_InitForVulkan(window);
	default:
		return FALSE;
	}
}

void ac_imgui_new_frame(struct ac_imgui_module * mod)
{
	ImGui_Implbgfx_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();
}

boolean ac_imgui_process_event(
	struct ac_imgui_module * mod, 
	const SDL_Event * event)
{
	return ImGui_ImplSDL2_ProcessEvent(event);
}

void ac_imgui_render(struct ac_imgui_module * mod)
{
	ImGui::Render();
	ImGui_Implbgfx_RenderDrawLists(ImGui::GetDrawData());
}

void ac_imgui_begin_toolbar(struct ac_imgui_module * mod)
{
	ImGuiViewport * view = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(view->WorkPos, ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(view->WorkSize.x, 30.f),
		ImGuiCond_Always);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
		ImVec2(4.f, 4.f));
	ImGui::Begin("Toolbar", NULL,
		ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
	ImGui::PopStyleVar(1);
}

void ac_imgui_end_toolbar(struct ac_imgui_module * mod)
{
	ImGui::End();
}

void ac_imgui_begin_toolbox(struct ac_imgui_module * mod)
{
	ImGui::SetNextWindowPos(ImVec2(6, 60), ImGuiCond_Appearing);
	ImGui::SetNextWindowSize(ImVec2(50, mod->toolbox_height));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
		ImVec2(4.f, 10.f));
	ImGui::Begin("Toolbox", NULL, 
		ImGuiWindowFlags_NoMove | 
		ImGuiWindowFlags_NoDecoration | 
		ImGuiWindowFlags_NoResize);
}

void ac_imgui_end_toolbox(struct ac_imgui_module * mod)
{
	mod->toolbox_height = ImGui::GetCursorPos().y;
	ImGui::End();
	ImGui::PopStyleVar(1);
}

void ac_imgui_viewport(struct ac_imgui_module * mod)
{
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImVec2 pos = { viewport->WorkPos.x, viewport->WorkPos.y + 30.0f };
	ImVec2 size = { viewport->WorkSize.x, viewport->WorkSize.y - 30.0f };
    ImGuiWindowFlags host_window_flags = 0;
    char label[32];
    ImGuiID dockspace_id = ImGui::GetID(AC_IMGUI_MAIN_DOCK_ID);
    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(size);
    ImGui::SetNextWindowViewport(viewport->ID);
    host_window_flags =
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoNavFocus |
		ImGuiWindowFlags_NoBackground;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
		ImVec2(0.0f, 0.0f));
    ImFormatString(label, IM_ARRAYSIZE(label),
		"DockSpaceViewport_%08X", viewport->ID);
    ImGui::Begin(label, NULL, host_window_flags);
    ImGui::PopStyleVar(3);
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f),
		ImGuiDockNodeFlags_PassthruCentralNode, 0);
	static bool first_time = true;
	if (first_time)
	{
		first_time = false;

		/* TODO: Proper node setup framework. */
		ImGui::DockBuilderAddNode(dockspace_id, 
			ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_DockSpace);
		ImGui::DockBuilderSetNodeSize(dockspace_id, size);

		ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(
			dockspace_id, ImGuiDir_Right, 0.25f, 
			&dockspace_id, nullptr);
		ImGuiID dock_id_up = ImGui::DockBuilderSplitNode(
			dockspace_id, ImGuiDir_Up, 0.25f, 
			nullptr, &dockspace_id);
		ImGuiID dock_id_down = ImGui::DockBuilderSplitNode(
			dockspace_id, ImGuiDir_Down, 0.25f, 
			nullptr, &dockspace_id);
		ImGui::DockBuilderDockWindow("Objects", dock_id_up);
		ImGui::DockBuilderDockWindow("Properties", dock_id_down);
		ImGui::DockBuilderFinish(
			ImGui::GetID(AC_IMGUI_MAIN_DOCK_ID));
	}
	ImGui::End();
}

void ac_imgui_set_dockspace(struct ac_imgui_module * mod, uint32_t id)
{
	mod->dock_id = id;
}

uint32_t ac_imgui_get_dockspace(struct ac_imgui_module * mod)
{
	return mod->dock_id;
}

void ac_imgui_invalid_device_objects(struct ac_imgui_module * mod)
{
	ImGui_Implbgfx_InvalidateDeviceObjects();
}

boolean ac_imgui_create_device_objects(struct ac_imgui_module * mod)
{
	return ImGui_Implbgfx_CreateDeviceObjects();
}

boolean igSplitter(
	boolean split_vertically,
	float thickness,
	float padding,
	float * size1,
	float * size2,
	float min_size1,
	float min_size2,
	float splitter_long_axis_size)
{
	using namespace ImGui;
	ImGuiWindow * window = GImGui->CurrentWindow;
	ImGuiID id = window->GetID("##Splitter");
	ImRect bb;
	bb.Min = window->DC.CursorPos + 
		(split_vertically ? ImVec2(*size1 + padding, 0.0f) : 
			ImVec2(0.0f, *size1 + padding));
	bb.Max = bb.Min + CalcItemSize(split_vertically ? 
		ImVec2(thickness, splitter_long_axis_size) : 
		ImVec2(splitter_long_axis_size, thickness), 0.0f, 0.0f);
	return SplitterBehavior(bb, id, split_vertically ? 
		ImGuiAxis_X : ImGuiAxis_Y, size1, size2, 
		min_size1, min_size2, 0.0f);
}

boolean igIconButton(
	bgfx_texture_handle_t tex,
	const char * hint,
	boolean * btn_state,
	boolean is_disabled)
{
	float avail = ImGui::GetContentRegionAvail().y;
	boolean clicked = FALSE;
	ImVec4 tint = is_disabled ? ImVec4(.8f, .8f, .8f, .8f) : 
		ImVec4(1.f, 1.f, 1.f, 1.f);
	if (!BGFX_HANDLE_IS_VALID(tex))
		return FALSE;
	if (btn_state && *btn_state) {
		image_inner_border(tex, ImVec2(avail, avail), 
			ImVec2(0, 0), ImVec2(1, 1), tint,
			ImVec4(1, 1, 1, 1));
	}
	else {
		ImGui::Image((ImTextureID)tex.idx, 
			ImVec2(avail, avail), ImVec2(0, 0), ImVec2(1, 1),
			tint);
	}
	if (hint && ImGui::IsItemHovered()) {
		ImGui::BeginTooltip();
		ImGui::Text(hint);
		ImGui::EndTooltip();
	}
	clicked = !is_disabled && ImGui::IsItemClicked();
	if (clicked && btn_state)
		*btn_state ^= 1;
	return clicked;
}
