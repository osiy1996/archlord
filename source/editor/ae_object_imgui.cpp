#include <assert.h>
#include <string.h>

#include "core/core.h"
#include "core/malloc.h"
#include "core/log.h"
#include "core/string.h"
#include "core/vector.h"

#include "public/ap_sector.h"

#include "client/ac_object.h"

#include "runtime/ar_terrain.h"
#include "runtime/ar_texture.h"
#include "runtime/ar_renderer.h"
#include "runtime/ar_imgui.h"
#include "editor/ae_object.h"
#include "editor/ae_texture.h"

#include "ae_object_internal.h"

#include "vendor/imgui/imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "vendor/imgui/imgui_internal.h"

static void render_outliner(struct object_ctx * ctx)
{
	ImGuiListClipper clipper;
	if (!ImGui::Begin("Objects", (bool *)&ctx->display_outliner)) {
		ImGui::End();
		return;
	}
	vec_clear(ctx->objects);
	ac_obj_query_visible_objects(&ctx->objects);
	clipper.Begin((int)vec_count(ctx->objects));
	while (clipper.Step()) {
		for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
			struct ap_object * obj = ctx->objects[i];
			struct ac_object * objc = ac_obj_get_object(obj);
			char label[128];
			snprintf(label, sizeof(label), "%u (tid = %u)##%p", 
				obj->object_id, obj->tid, obj);
			if (ImGui::Selectable(label, 
					obj == ctx->active_object)) {
				ctx->active_object = obj;
			}
		}
	}
	clipper.End();
	ImGui::End();
}

static void obj_type_prop(
	uint32_t * object_type, 
	uint32_t bit,
	const char * label)
{
	bool b = (*object_type & bit) != 0;
	if (ImGui::Checkbox(label, &b)) {
		if (b) {
			*object_type |= bit;
		}
		else {
			*object_type &= ~bit;
		}
	}
}

static void obj_transform(struct ap_object * obj)
{
	struct au_pos pos = obj->position;
	bool move = false;
	move |= ImGui::DragFloat("Position X", &pos.x);
	move |= ImGui::DragFloat("Position Y", &pos.y);
	move |= ImGui::DragFloat("Position Z", &pos.z);
	ImGui::DragFloat("Scale X", &obj->scale.x, 0.001f);
	ImGui::DragFloat("Scale Y", &obj->scale.y, 0.001f);
	ImGui::DragFloat("Scale Z", &obj->scale.z, 0.001f);
	ImGui::DragFloat("Rotation X", &obj->rotation_x);
	ImGui::DragFloat("Rotation Y", &obj->rotation_y);
	if (move)
		ap_obj_move_object(obj, &pos);
}

static void render_properties(struct object_ctx * ctx)
{
	struct ap_object * obj = ctx->active_object;
	struct ac_object * objc;
	if (!ImGui::Begin("Properties", 
			(bool *)&ctx->display_properties) ||
		!obj) {
		ImGui::End();
		return;
	}
	objc = ac_obj_get_object(obj);
	ImGui::InputScalar("Object ID", ImGuiDataType_U32, 
		&obj->object_id, NULL, NULL, NULL, 
		ImGuiInputTextFlags_ReadOnly);
	ImGui::InputScalar("Object TID", ImGuiDataType_U32, 
		&obj->tid, NULL, NULL, NULL, 
		ImGuiInputTextFlags_ReadOnly);
	obj_transform(obj);
	if (ImGui::TreeNodeEx("Public Properties", 
			ImGuiTreeNodeFlags_Framed)) {
		obj_type_prop(&obj->object_type, AC_OBJ_TYPE_BLOCKING, 
			"BLOCKING");
		obj_type_prop(&obj->object_type, AC_OBJ_TYPE_RIDABLE, 
			"RIDABLE");
		ImGui::TreePop();
	}
	if (ImGui::TreeNodeEx("Client Properties", 
			ImGuiTreeNodeFlags_Framed)) {
		obj_type_prop(&objc->object_type, AC_OBJ_TYPE_BLOCKING, 
			"BLOCKING");
		obj_type_prop(&objc->object_type, AC_OBJ_TYPE_RIDABLE, 
			"RIDABLE");
		obj_type_prop(&objc->object_type, AC_OBJ_TYPE_NO_CAMERA_ALPHA, 
			"NO_CAMERA_ALPHA");
		obj_type_prop(&objc->object_type, AC_OBJ_TYPE_USE_ALPHA, 
			"USE_ALPHA");
		obj_type_prop(&objc->object_type, AC_OBJ_TYPE_USE_AMBIENT, 
			"USE_AMBIENT");
		obj_type_prop(&objc->object_type, AC_OBJ_TYPE_USE_LIGHT, 
			"USE_LIGHT");
		obj_type_prop(&objc->object_type, AC_OBJ_TYPE_USE_PRE_LIGHT, 
			"USE_PRE_LIGHT");
		obj_type_prop(&objc->object_type, AC_OBJ_TYPE_USE_FADE_IN_OUT, 
			"USE_FADE_IN_OUT");
		obj_type_prop(&objc->object_type, AC_OBJ_TYPE_IS_SYSTEM_OBJECT, 
			"IS_SYSTEM_OBJECT");
		obj_type_prop(&objc->object_type, AC_OBJ_TYPE_MOVE_INSECTOR, 
			"MOVE_INSECTOR");
		obj_type_prop(&objc->object_type, AC_OBJ_TYPE_NO_INTERSECTION, 
			"NO_INTERSECTION");
		obj_type_prop(&objc->object_type, AC_OBJ_TYPE_WORLDADD, 
			"WORLDADD");
		obj_type_prop(&objc->object_type, AC_OBJ_TYPE_OBJECTSHADOW, 
			"OBJECTSHADOW");
		obj_type_prop(&objc->object_type, AC_OBJ_TYPE_RENDER_UDA, 
			"RENDER_UDA");
		obj_type_prop(&objc->object_type, AC_OBJ_TYPE_USE_ALPHAFUNC, 
			"USE_ALPHAFUNC");
		obj_type_prop(&objc->object_type, AC_OBJ_TYPE_OCCLUDER, 
			"OCCLUDER");
		obj_type_prop(&objc->object_type, AC_OBJ_TYPE_DUNGEON_STRUCTURE, 
			"DUNGEON_STRUCTURE");
		obj_type_prop(&objc->object_type, AC_OBJ_TYPE_DUNGEON_DOME, 
			"DUNGEON_DOME");
		obj_type_prop(&objc->object_type, AC_OBJ_TYPE_CAM_ZOOM, 
			"CAM_ZOOM");
		obj_type_prop(&objc->object_type, AC_OBJ_TYPE_CAM_ALPHA, 
			"CAM_ALPHA");
		obj_type_prop(&objc->object_type, AC_OBJ_TYPE_INVISIBLE, 
			"INVISIBLE");
		obj_type_prop(&objc->object_type, AC_OBJ_TYPE_FORCED_RENDER_EFFECT, 
			"FORCED_RENDER_EFFECT");
		obj_type_prop(&objc->object_type, AC_OBJ_TYPE_DONOT_CULL, 
			"DONOT_CULL");
		ImGui::TreePop();
	}
	ImGui::End();
}

void ae_obj_imgui()
{
	struct object_ctx * ctx = ae_obj_get_ctx();
	render_outliner(ctx);
	render_properties(ctx);
}
