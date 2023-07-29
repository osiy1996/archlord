#ifndef _AE_OBJECT_INTERNAL_H_
#define _AE_OBJECT_INTERNAL_H_

#include "core/macros.h"
#include "core/types.h"

#include "vendor/cglm/cglm.h"
#include "vendor/bgfx/c99/bgfx.h"

#include "public/ap_map.h"

#include "client/ac_object.h"

BEGIN_DECLS

enum tool_type {
	TOOL_TYPE_NONE,
	TOOL_TYPE_MOVE,
	TOOL_TYPE_COUNT
};

enum move_axis {
	MOVE_AXIS_NONE,
	MOVE_AXIS_X,
	MOVE_AXIS_Y,
	MOVE_AXIS_Z,
	MOVE_AXIS_COUNT
};

struct move_tool {
	enum move_axis axis;
	struct au_pos prev_pos;
	float dx;
	float dy;
};

struct object_ctx {
	boolean display_outliner;
	boolean display_properties;
	struct ap_object ** objects;
	struct ap_object * active_object;
	bgfx_program_handle_t program;
	/* Active tool type. */
	enum tool_type tool_type;
	/* Object move tool context. */
	struct move_tool move_tool;
};

struct object_ctx * ae_obj_get_ctx();

inline void set_move_tool_axis(
	struct move_tool * t, 
	enum move_axis axis,
	struct ap_object * obj)
{
	if (obj)
		obj->position = t->prev_pos;
	t->axis = axis;
}

END_DECLS

#endif /* _AE_OBJECT_INTERNAL_H_ */
