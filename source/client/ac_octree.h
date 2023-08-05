#ifndef _AC_OCTREE_H_
#define _AC_OCTREE_H_

#include "public/ap_module_instance.h"

#include "vendor/RenderWare/rwcore.h"

#define AC_OCTREE_MODULE_NAME "AgcmOctree"

BEGIN_DECLS

struct ac_octree_render_data {
	RwV3d top_verts_max[4];
	int8_t move;
	int8_t is_occluder;
	int8_t occluder_box_count;
	int8_t pad;
	RwV3d * top_verts;
};

END_DECLS

#endif /* _AC_OCTREE_H_ */