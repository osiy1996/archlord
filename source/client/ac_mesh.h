#ifndef _AC_MESH_H_
#define _AC_MESH_H_

#include "core/macros.h"
#include "core/types.h"

#include <utility/au_math.h>

#include "vendor/bgfx/c99/bgfx.h"
#include "vendor/RenderWare/rwcore.h"

#define AC_MESH_MODULE_NAME "AgcmMesh"
#define AC_MESH_INVALID_HANDLE UINT16_MAX
#define AC_MESH_HANDLE_IS_VALID(h) ((h) != AC_MESH_INVALID_HANDLE)
#define AC_MESH_MAX_TEXCOORD_COUNT 8

BEGIN_DECLS

struct ar_camera;
struct bin_stream;

struct ac_mesh_frame {
	RwMatrix model;
	RwMatrix ltm;
	struct ac_mesh_frame * next;
	struct ac_mesh_frame * parent;
	struct ac_mesh_frame * child;
	struct ac_mesh_frame * root;
};

struct ac_mesh_vertex {
	float position[3];
	float normal[3];
	float texcoord[AC_MESH_MAX_TEXCOORD_COUNT][2];
};

struct ac_mesh_material {
	char tex_name[6][64];
	bgfx_texture_handle_t tex_handle[6];
};

struct ac_mesh_triangle {
	uint16_t indices[3];
	uint32_t material_index;
};

struct ac_mesh_split {
	uint32_t index_count;
	uint32_t index_offset;
	uint32_t material_index;
};

struct ac_mesh_geometry {
	uint32_t vertex_count;
	struct ac_mesh_vertex * vertices;
	RwRGBA * vertex_colors;
	uint32_t triangle_count;
	struct ac_mesh_triangle * triangles;
	uint32_t index_count;
	uint16_t * indices;
	uint32_t split_count;
	struct ac_mesh_split * splits;
	uint32_t material_count;
	struct ac_mesh_material * materials;
	RwSphere bsphere;
	bgfx_vertex_buffer_handle_t vertex_buffer;
	bgfx_index_buffer_handle_t index_buffer;
	struct ac_mesh_geometry * prev;
	struct ac_mesh_geometry * next;
};

struct ac_mesh_clump {
	/* Geometry linked-list */
	struct ac_mesh_geometry * glist;
};

struct ac_mesh_module * ac_mesh_create_module();

struct ac_mesh_geometry * ac_mesh_read_rp_atomic(
	struct ac_mesh_module * mod,
	struct bin_stream * stream,
	struct ac_mesh_geometry ** geo,
	uint32_t geo_count);

boolean ac_mesh_write_rp_atomic(
	struct bin_stream * stream,
	struct ac_mesh_geometry * g);

struct ac_mesh_clump * ac_mesh_read_rp_clump(
	struct ac_mesh_module * mod,
	struct bin_stream * stream);

/*
 * Initialize material with unset handles.
 */
void ac_mesh_init_material(struct ac_mesh_material * m);

/*
 * Compares two materials.
 * Returns 0 if they are identical.
 */
int ac_mesh_cmp_material(
	const struct ac_mesh_material * m1,
	const struct ac_mesh_material * m2);

/*
 * Copies material.
 *
 * This function will increase texture 
 * handle reference count.
 */
void ac_mesh_copy_material(
	struct ac_mesh_module * mod,
	struct ac_mesh_material * dst,
	const struct ac_mesh_material * src);

/*
 * Releases internal handles (texture,etc.).
 */
void ac_mesh_release_material(
	struct ac_mesh_module * mod,
	struct ac_mesh_material * m);

void ac_mesh_destroy_geometry(
	struct ac_mesh_module * mod,
	struct ac_mesh_geometry * g);

bgfx_vertex_layout_t ac_mesh_vertex_layout();

struct ac_mesh_geometry * ac_mesh_rebuild_splits(
	struct ac_mesh_module * mod,
	struct ac_mesh_geometry * g);

/*
 * Sets triangle material index.
 *
 * Returns TRUE if material index is changed, FALSE if 
 * previous material and `m` are identical.
 *
 * If a matching material is not found, 
 * a new material is added to geometry.
 *
 * If geometry is being rendered through splits, 
 * it is necessary to call `ac_mesh_rebuild_splits` 
 * in order to reflect changes.
 */
boolean ac_mesh_set_material(
	struct ac_mesh_module * mod,
	struct ac_mesh_geometry * g,
	struct ac_mesh_triangle * tri,
	const struct ac_mesh_material * m);

void ac_mesh_calculate_normals(struct ac_mesh_geometry * g);

void ac_mesh_calculate_normals_ex(
	struct ac_mesh_vertex * vertices,
	uint32_t vertex_count,
	uint32_t * indices,
	uint32_t index_count);

void ac_mesh_calculate_surface_normal(
	const struct ac_mesh_geometry * g,
	const struct ac_mesh_triangle * t,
	vec3 n);

void ac_mesh_query_triangles(
	struct ac_mesh_geometry * g,
	struct ac_mesh_triangle ** triangles,
	const vec3 pos);

boolean ac_mesh_eq_pos(
	const struct ac_mesh_vertex * v1,
	const struct ac_mesh_vertex * v2);

void ac_mesh_destroy_clump(struct ac_mesh_module * mod, struct ac_mesh_clump * c);

END_DECLS

#endif /* _AC_MESH_H_ */