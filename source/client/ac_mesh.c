#include "client/ac_mesh.h"

#include "core/bin_stream.h"
#include "core/file_system.h"
#include "core/hash_map.h"
#include "core/log.h"
#include "core/os.h"
#include "core/malloc.h"
#include "core/string.h"
#include "core/vector.h"

#include "public/ap_config.h"

#include <client/ac_camera.h>
#include <client/ac_renderware.h>
#include <client/ac_texture.h>
#include <client/ac_camera.h>

//#include <al_runtime/ar_renderer.h>

#include <assert.h>
#include <time.h>

#define rwVENDORID_NHN (0xfffff0L)
#define rwID_MTEXTURE_NHN (0x01)
#define rwID_MTEXTURE \
	MAKECHUNKID(rwVENDORID_NHN, rwID_MTEXTURE_NHN)

/* Geometry format specifier (for RpGeometryCreate):-
 *
 *  0x00nn00gg
 *  nn = num tex coords
 *  gg = flags
 *
 * A gap is left for possible extension of the 8-bit flags 
 * field to 16-bit.
 * See also bageomet.h: rpGEOMETRYTEXCOORDSETS(_num)
 *
 * To determine number of texture coordinate sets, 
 * the 'num tex coords' field takes precedence, but if zero, 
 * we use the backward compatibility flags. */
#define GeometryFormatGetFlagsMacro(_fmt)\
	((_fmt) & rpGEOMETRYFLAGSMASK)

#define GeometryFormatGetNumTexCoordSetsMacro(_fmt)         \
    (((_fmt) & 0xff0000) ? (((_fmt) & 0xff0000)>>16) :      \
        (((_fmt) & rpGEOMETRYTEXTURED2) ? 2 :               \
            (((_fmt) & rpGEOMETRYTEXTURED) ? 1 : 0)))

#define rpGEOMETRYTEXCOORDSETS(_num) ((_num & 0xff) << 16)

struct rpGeometryChunkInfo {
	/* Compression flags and number of texture coord sets */
	RwUInt32 format;
	RwUInt32 numTriangles;
	RwUInt32 numVertices;
	RwUInt32 numMorphTargets;
};

struct rpTriangle {
	/* V0 index in top 16 bits, V1 index in bottom 16 bits */
	RwUInt32 vertex01;
	/* V2 index in top 16 bits, Material index in bottom 16 bit */
	RwUInt32 vertex2Mat;
};

struct rpMorphTarget {
	RwSphere boundingSphere;
	RwBool pointsPresent;
	RwBool normalsPresent;
};

struct binMeshHeader {
	RwUInt32 flags;
	RwUInt32 numMeshes;
	RwUInt32 totalIndicesInMesh;
};

struct binMesh {
	RwUInt32 numIndices;
	RwInt32 matIndex;
};

enum RpMTextureType {
	rpMTEXTURE_TYPE_NONE = 0,
	/* Put a texture without alpha on top.
	 *
	 * If there was rpMTEXTURE_TYPE_ALPHA before, 
	 * the corresponding Alpha will covert it. */
	rpMTEXTURE_TYPE_NORMAL,
	/* Put the texture with Alpha on top. 
	 *
	 * If there was rpMTEXTURE_TYPE_ALPHA just before, 
	 * it is ignored. */
	rpMTEXTURE_TYPE_NORMALALPHA,
	/* Apply Alpha. (Covers only rpMTEXTURE_TYPE_NORMAL.) */
	rpMTEXTURE_TYPE_ALPHA,
	/* Not implemented. */
	rpMTEXTURE_TYPE_BUMP,
};

struct rpAtomicBinary {
	RwInt32 frameIndex;
	RwInt32 geomIndex;
	RwInt32 flags;
	RwInt32 unused;
	RwInt16 renderType;
	RwInt16 blendMode;
	RwInt32 id;
};

struct mesh_ctx {
	void * ph;
};

struct ac_mesh_module {
	struct ap_module_instance instance;
	ap_module_t ac_dat;
	ap_module_t ac_texture;
};

static struct mesh_ctx * g_Ctx;

boolean arm_skip_extension(struct bin_stream * stream)
{
	RwChunkHeaderInfo c;
	if (!ac_renderware_find_chunk(stream, rwID_EXTENSION, &c)) {
		ERROR("Failed to find rwID_EXTENSION.");
		return FALSE;
	}
	while (c.length > 0) {
		RwChunkHeaderInfo tmp;
		if (!ac_renderware_read_chunk(stream, &tmp)) {
			ERROR("Failed to read chunk.");
			return FALSE;
		}
		if (!bstream_advance(stream, tmp.length)) {
			ERROR("Stream ended unexpectedly.");
			return FALSE;
		}
		c.length -= rwCHUNKHEADERSIZE + tmp.length;
	}
	return TRUE;
}

boolean arm_read_string(
	struct bin_stream * stream,
	char * dst,
	size_t maxcount)
{
	RwChunkHeaderInfo c;
	while (ac_renderware_read_chunk(stream, &c)) {
		if (c.type == rwID_STRING) {
			uint32_t n = (c.length >= maxcount) ? 
				(uint32_t)(maxcount - 1) : c.length;
			if (!bstream_read(stream, dst, n)) {
				ERROR("Stream ended unexpectedly.");
				return FALSE;
			}
			dst[maxcount - 1] = '\0';
			c.length -= n;
			if (c.length && !bstream_advance(stream, c.length)) {
				ERROR("Stream ended unexpectedly.");
				return FALSE;
			}
			return TRUE;
		}
		else if (!bstream_advance(stream, c.length)) {
			ERROR("Stream ended unexpectedly.");
			return FALSE;
		}
	}
	return FALSE;
}

boolean arm_write_string(
	struct bin_stream * stream,
	const char * str)
{
	size_t offset;
	size_t len = strlen(str) + 1;
	if (!ac_renderware_begin_chunk(stream, rwID_STRING, &offset)) {
		ERROR("Failed to create rwID_STRING chunk.");
		return FALSE;
	}
	len = ((len + 3) / 4) * 4;
	if (!bstream_write_str(stream, str, (uint32_t)len)) {
		ERROR("Failed to string.");
		return FALSE;
	}
	if (!ac_renderware_end_chunk(stream, offset)) {
		ERROR("Failed to complete rwID_STRING chunk.");
		return FALSE;
	}
	return TRUE;
}

boolean arm_read_texture(
	struct bin_stream * stream,
	char * dst,
	size_t maxcount)
{
	char tmp[64];
	uint32_t filter_addr;
	if (!ac_renderware_find_chunk(stream, rwID_STRUCT, NULL)) {
		ERROR("Failed to find rwID_STRUCT.");
		return FALSE;
	}
	if (!bstream_read_u32(stream, &filter_addr)) {
		ERROR("Stream ended unexpectedly.");
		return FALSE;
	}
	if (dst) {
		if (!arm_read_string(stream, dst, maxcount)) {
			ERROR("Failed to read diffuse texture name.");
			return FALSE;
		}
	}
	else {
		if (!arm_read_string(stream, tmp, sizeof(tmp))) {
			ERROR("Failed to read diffuse texture name.");
			return FALSE;
		}
	}
	if (!arm_read_string(stream, tmp, sizeof(tmp))) {
		ERROR("Failed to read texture mask name.");
		return FALSE;
	}
	assert(tmp[0] == '\0');
	if (!arm_skip_extension(stream)) {
		ERROR("Failed to skip texture extension.");
		return FALSE;
	}
	return TRUE;
}

boolean arm_write_texture(
	struct bin_stream * stream,
	const char * name,
	uint32_t filter_addr)
{
	size_t tex_offset;
	size_t struct_offset;
	size_t ext_offset;
	if (!ac_renderware_begin_chunk(stream, rwID_TEXTURE, &tex_offset)) {
		ERROR("Failed to create rwID_TEXTURE chunk.");
		return FALSE;
	}
	if (!ac_renderware_begin_chunk(stream, rwID_STRUCT, &struct_offset)) {
		ERROR("Failed to create rwID_STRUCT chunk.");
		return FALSE;
	}
	if (!bstream_write_u32(stream, filter_addr)) {
		ERROR("Failed to write texture filter address.");
		return FALSE;
	}
	if (!ac_renderware_end_chunk(stream, struct_offset)) {
		ERROR("Failed to complete rwID_STRUCT chunk.");
		return FALSE;
	}
	if (!arm_write_string(stream, name) ||
		!arm_write_string(stream, "")) {
		ERROR("Failed to write texture name.");
		return FALSE;
	}
	if (!ac_renderware_begin_chunk(stream, rwID_EXTENSION, &ext_offset)) {
		ERROR("Failed to create rwID_EXTENSION chunk.");
		return FALSE;
	}
	if (!ac_renderware_end_chunk(stream, ext_offset)) {
		ERROR("Failed to complete rwID_EXTENSION chunk.");
		return FALSE;
	}
	if (!ac_renderware_end_chunk(stream, tex_offset)) {
		ERROR("Failed to complete rwID_TEXTURE chunk.");
		return FALSE;
	}
	return TRUE;
}

static boolean write_mtexture(
	struct bin_stream * stream,
	const struct ac_mesh_material * m)
{
	size_t mtex_offset;
	uint16_t count = 0;
	uint16_t i;
	if (!ac_renderware_begin_chunk(stream, rwID_MTEXTURE, &mtex_offset)) {
		ERROR("Failed to create rwID_MTEXTURE chunk.");
		return FALSE;
	}
	for (i = 1; i < COUNT_OF(m->tex_name); i++) {
		if (!strisempty(m->tex_name[i]))
			count = i;
	}
	if (!bstream_write_u16(stream, count)) {
		ERROR("Failed to write texture count.");
		return FALSE;
	}
	for (i = 1; i < count + 1; i++) {
		if (!strisempty(m->tex_name[i])) {
			const enum RpMTextureType types[6] = {
				rpMTEXTURE_TYPE_NONE,
				rpMTEXTURE_TYPE_ALPHA,
				rpMTEXTURE_TYPE_NORMAL,
				rpMTEXTURE_TYPE_ALPHA,
				rpMTEXTURE_TYPE_NORMAL,
				rpMTEXTURE_TYPE_NORMALALPHA };
			const uint32_t filter_addr[6] = {
				0,
				0x11101,
				0x1106,
				0x11101,
				0x1106,
				0x1106 };
			if (!bstream_write_u32(stream, types[i]) ||
				!arm_write_texture(stream, m->tex_name[i],
					filter_addr[i])) {
				ERROR("Failed to write texture.");
				return FALSE;
			}
		}
		else {
			if (!bstream_write_u32(stream, rpMTEXTURE_TYPE_NONE)) {
				ERROR("Failed to write MTexture type.");
				return FALSE;
			}
		}
	}
	if (!ac_renderware_end_chunk(stream, mtex_offset)) {
		ERROR("Failed to complete rwID_MTEXTURE chunk.");
		return FALSE;
	}
	return TRUE;
}

boolean arm_read_material(
	struct ac_mesh_module * mod,
	struct bin_stream * stream,
	struct ac_mesh_material * m)
{
	RpMaterialChunkInfo ch;
	RwChunkHeaderInfo c;
	if (!ac_renderware_find_chunk(stream, rwID_STRUCT, NULL)) {
		ERROR("Failed to find rwID_STRUCT.");
		return FALSE;
	}
	if (!bstream_read(stream, &ch, sizeof(ch))) {
		ERROR("Stream ended unexpectedly.");
		return FALSE;
	}
	if (ch.textured) {
		if (!ac_renderware_find_chunk(stream, rwID_TEXTURE, NULL)) {
			ERROR("Failed to find rwID_TEXTURE.");
			return FALSE;
		}
		if (!arm_read_texture(stream, m->tex_name[0],
				sizeof(m->tex_name[0]))) {
			ERROR("Failed to read material texture.");
			return FALSE;
		}
		m->tex_handle[0] = ac_texture_load_packed(mod->ac_texture, 
			AC_DAT_DIR_COUNT, m->tex_name[0], TRUE, NULL);
		if (!BGFX_HANDLE_IS_VALID(m->tex_handle[0]))
			memset(m->tex_name[0], 0, sizeof(m->tex_handle[0]));
	}
	if (!ac_renderware_find_chunk(stream, rwID_EXTENSION, &c)) {
		ERROR("Failed to find rwID_EXTENSION.");
		return FALSE;
	}
	while (c.length > 0) {
		RwChunkHeaderInfo ec;
		if (!ac_renderware_read_chunk(stream, &ec)) {
			ERROR("Failed to read chunk.");
			return FALSE;
		}
		if (ec.type == rwID_MTEXTURE) {
			RwUInt16 count;
			RwUInt16 i;
			if (!bstream_read_u16(stream, &count)) {
				ERROR("Stream ended unexpectedly.");
				return FALSE;
			}
			if (count > 5) {
				ERROR("Invalid MTexture count.");
				return FALSE;
			}
			for (i = 0; i < count; i++) {
				uint32_t type = 0;
				if (!bstream_read_u32(stream, &type)) {
					ERROR("Stream ended unexpectedly.");
					return FALSE;
				}
				if (type != rpMTEXTURE_TYPE_NONE) {
					if (!ac_renderware_find_chunk(stream, rwID_TEXTURE, 
							NULL)) {
						ERROR("Failed to find rwID_TEXTURE.");
						return FALSE;
					}
					if (!arm_read_texture(stream, 
							m->tex_name[i + 1],
							sizeof(m->tex_name[i + 1]))) {
						ERROR("Failed to read texture stream.");
						return FALSE;
					}
					assert(m->tex_name[i + 1][0] != '\0');
					m->tex_handle[i + 1] = ac_texture_load_packed(mod->ac_texture,
						AC_DAT_DIR_COUNT, m->tex_name[i + 1],
						TRUE, NULL);
					if (!BGFX_HANDLE_IS_VALID(m->tex_handle[i + 1]))
						memset(m->tex_name[i + 1], 0, sizeof(m->tex_handle[i + 1]));
				}
			}
		}
		else if (!bstream_advance(stream, ec.length)) {
			ERROR("Stream ended unexpectedly.");
			return FALSE;
		}
		if (ec.type != rwID_MTEXTURE) {
			/*INFO("Mat. extension: %08X", ec.type);*/
		}
		c.length -= rwCHUNKHEADERSIZE + ec.length;
	}
	return TRUE;
}

boolean arm_write_material(
	struct bin_stream * stream,
	const struct ac_mesh_material * m)
{
	size_t mat_offset;
	size_t struct_offset;
	size_t ext_offset;
	RpMaterialChunkInfo ci = { 0 };
	RwBool textured = (m->tex_name[0][0] != '\0');
	if (!ac_renderware_begin_chunk(stream, rwID_MATERIAL, &mat_offset)) {
		ERROR("Failed to create rwID_MATERIAL chunk.");
		return FALSE;
	}
	if (!ac_renderware_begin_chunk(stream, rwID_STRUCT, &struct_offset)) {
		ERROR("Failed to create rwID_STRUCT chunk.");
		return FALSE;
	}
	ci.flags = 0;
	ci.color.red = 255;
	ci.color.green = 255;
	ci.color.blue = 255;
	ci.color.alpha = 255;
	ci.textured = textured;
	ci.surfaceProps.ambient = 0.3f;
	ci.surfaceProps.specular = 1.0f;
	ci.surfaceProps.diffuse = 0.7f;
	if (!bstream_write(stream, &ci, sizeof(ci))) {
		ERROR("Failed to write material chunk info.");
		return FALSE;
	}
	if (!ac_renderware_end_chunk(stream, struct_offset)) {
		ERROR("Failed to complete rwID_STRUCT chunk.");
		return FALSE;
	}
	if (textured && !arm_write_texture(stream, m->tex_name[0], 0x1106)) {
		ERROR("Failed to write material texture.");
		return FALSE;
	}
	if (!ac_renderware_begin_chunk(stream, rwID_EXTENSION, &ext_offset)) {
		ERROR("Failed to create rwID_EXTENSION chunk.");
		return FALSE;
	}
	if (!write_mtexture(stream, m)) {
		ERROR("Failed to write extension (rwID_MTEXTURE).");
		return FALSE;
	}
	if (!ac_renderware_end_chunk(stream, ext_offset)) {
		ERROR("Failed to complete rwID_EXTENSION chunk.");
		return FALSE;
	}
	if (!ac_renderware_end_chunk(stream, mat_offset)) {
		ERROR("Failed to complete rwID_MATERIAL chunk.");
		return FALSE;
	}
	return TRUE;
}

boolean arm_read_matlist(
	struct ac_mesh_module * mod,
	struct bin_stream * stream,
	struct ac_mesh_geometry * geo)
{
	RwInt32 count;
	RwInt32 * indices;
	RwInt32 i;
	if (!ac_renderware_find_chunk(stream, rwID_STRUCT, NULL)) {
		ERROR("Failed to find rwID_STRUCT.");
		return FALSE;
	}
	if (!bstream_read_i32(stream, &count)) {
		ERROR("Stream ended unexpectedly.");
		return FALSE;
	}
	if (count < (RwInt32)geo->material_count) {
		ERROR("Stream ended unexpectedly.");
		return FALSE;
	}
	indices = alloc(count * sizeof(*indices));
	if (!bstream_read(stream, indices, count * sizeof(*indices))) {
		ERROR("Stream ended unexpectedly.");
		return FALSE;
	}
	for (i = 0; i < count; i++) {
		struct ac_mesh_material tmp = { 0 };
		struct ac_mesh_material * m;
		if (i < (RwInt32)geo->material_count)
			m = &geo->materials[i];
		else
			m = &tmp;
		if (indices[i] < 0) {
			uint32_t j;
			for (j = 0; j < COUNT_OF(m->tex_handle); j++) {
				m->tex_handle[j] =
					(bgfx_texture_handle_t)BGFX_INVALID_HANDLE;
			}
			if (!ac_renderware_find_chunk(stream, rwID_MATERIAL, NULL)) {
				ERROR("Failed to find rwID_MATERIAL.");
				return FALSE;
			}
			if (!arm_read_material(mod, stream, m)) {
				ERROR("Failed to read material stream.");
				return FALSE;
			}
		}
		else if (i >= (RwInt32)geo->material_count) {
			continue;
		}
		else if (indices[i] >= i) {
			ERROR("Invalid material index.");
			return FALSE;
		}
		else {
			memcpy(m, &geo->materials[indices[i]], sizeof(*m));
		}
	}
	dealloc(indices);
	return TRUE;
}

boolean arm_write_matlist(
	struct bin_stream * stream,
	const struct ac_mesh_geometry * g)
{
	size_t matlist_offset;
	size_t struct_offset;
	uint32_t i;
	if (!ac_renderware_begin_chunk(stream, rwID_MATLIST, &matlist_offset)) {
		ERROR("Failed to create rwID_MATLIST chunk.");
		return FALSE;
	}
	if (!ac_renderware_begin_chunk(stream, rwID_STRUCT, &struct_offset)) {
		ERROR("Failed to create rwID_STRUCT chunk.");
		return FALSE;
	}
	if (!bstream_write_u32(stream, g->material_count)) {
		ERROR("Failed to write material count.");
		return FALSE;
	}
	for (i = 0; i < g->material_count; i++) {
		if (!bstream_write_i32(stream, -1)) {
			ERROR("Failed to write material index.");
			return FALSE;
		}
	}
	if (!ac_renderware_end_chunk(stream, struct_offset)) {
		ERROR("Failed to complete rwID_STRUCT chunk.");
		return FALSE;
	}
	for (i = 0; i < g->material_count; i++) {
		const struct ac_mesh_material * m = &g->materials[i];
		if (!arm_write_material(stream, m)) {
			ERROR("Failed to write material.");
			return FALSE;
		}
	}
	if (!ac_renderware_end_chunk(stream, matlist_offset)) {
		ERROR("Failed to complete rwID_MATLIST chunk.");
		return FALSE;
	}
	return TRUE;
}

static void set_and_advance(void ** dst, void ** mem, size_t size)
{
	*dst = *mem;
	*mem = (void *)((uintptr_t)*mem + size);
}

static boolean write_binmesh_plg(
	struct bin_stream * stream,
	const struct ac_mesh_geometry * g)
{
	size_t bm_offset;
	struct binMeshHeader h = { 0 };
	uint32_t i;
	if (!ac_renderware_begin_chunk(stream, rwID_BINMESHPLUGIN, &bm_offset)) {
		ERROR("Failed to create rwID_BINMESHPLUGIN chunk.");
		return FALSE;
	}
	h.flags = 0;
	h.numMeshes = g->split_count;
	for (i = 0; i < g->split_count; i++)
		h.totalIndicesInMesh += g->splits[i].index_count;
	if (!bstream_write(stream, &h, sizeof(h))) {
		ERROR("Failed to write binmesh header.");
		return FALSE;
	}
	for (i = 0; i < g->split_count; i++)  {
		const struct ac_mesh_split * s = &g->splits[i];
		struct binMesh bm = { 0 };
		uint32_t j;
		bm.numIndices = s->index_count;
		bm.matIndex = (RwInt32)s->material_index;
		if (!bstream_write(stream, &bm, sizeof(bm))) {
			ERROR("Failed to write binmesh info.");
			return FALSE;
		}
		for (j = 0; j < s->index_count; j++) {
			uint32_t index = g->indices[s->index_offset + j];
			if (!bstream_write_u32(stream, index)) {
				ERROR("Failed to write binmesh indices.");
				return FALSE;
			}
		}
	}
	if (!ac_renderware_end_chunk(stream, bm_offset)) {
		ERROR("Failed to complete rwID_BINMESHPLUGIN chunk.");
		return FALSE;
	}
	return TRUE;
}

static size_t arm_calc_geo_size(
	uint32_t vertex_count,
	uint32_t triangle_count,
	uint32_t split_count,
	uint32_t material_count)
{
	return (sizeof(struct ac_mesh_geometry)
		+ vertex_count * sizeof(struct ac_mesh_vertex)
		+ vertex_count * sizeof(RwRGBA)
		+ triangle_count * sizeof(struct ac_mesh_triangle)
		+ (size_t)triangle_count * 3 * sizeof(uint16_t)
		+ material_count * sizeof(struct ac_mesh_split));
}

static struct ac_mesh_geometry * arm_alloc_geo(
	uint32_t vertex_count,
	uint32_t triangle_count,
	uint32_t split_count,
	uint32_t material_count)
{
	size_t sz = arm_calc_geo_size( vertex_count, triangle_count,
		split_count, material_count);
	void * mem = alloc(sz);
	struct ac_mesh_geometry * g = NULL;
	uint32_t i;
	memset(mem, 0, sz);
	set_and_advance(&g, &mem, sizeof(*g));
	BGFX_INVALIDATE_HANDLE(g->vertex_buffer);
	BGFX_INVALIDATE_HANDLE(g->index_buffer);
	g->vertex_count = vertex_count;
	set_and_advance(&g->vertices, &mem,
		g->vertex_count * sizeof(*g->vertices));
	set_and_advance(&g->vertex_colors, &mem,
		g->vertex_count * sizeof(*g->vertex_colors));
	g->triangle_count = triangle_count;
	set_and_advance(&g->triangles, &mem,
		g->triangle_count * sizeof(*g->triangles));
	g->index_count = triangle_count * 3;
	set_and_advance(&g->indices, &mem,
		g->index_count * sizeof(*g->indices));
	g->split_count = split_count;
	set_and_advance(&g->splits, &mem,
		g->split_count * sizeof(*g->splits));
	g->material_count = material_count;
	g->materials = alloc(material_count * sizeof(*g->materials));
	memset(g->materials, 0, g->material_count * sizeof(*g->materials));
	for (i = 0; i < g->material_count; i++) {
		struct ac_mesh_material * material = &g->materials[i];
		uint32_t j;
		for (j = 0; j < COUNT_OF(material->tex_handle); j++)
			BGFX_INVALIDATE_HANDLE(material->tex_handle[j]);
	}
	return g;
}

struct ac_mesh_geometry * arm_read_geometry(
	struct ac_mesh_module * mod,
	struct bin_stream * stream)
{
	struct rpGeometryChunkInfo gc;
	uint32_t flags;
	uint32_t tc_count;
	struct ac_mesh_geometry * g = NULL;
	size_t offset;
	uint32_t i;
	uint32_t split_index_count[256];
	RwUInt16 unique_mat[256] = { 0 };
	uint32_t split_count = 0;
	uint32_t max_mat_index = UINT32_MAX;
	struct rpMorphTarget morph;
	if (!ac_renderware_find_chunk(stream, rwID_STRUCT, NULL)) {
		ERROR("Failed to find rwID_STRUCT.");
		return NULL;
	}
	if (!bstream_read(stream, &gc, sizeof(gc))) {
		ERROR("Stream ended unexpectedly.");
		return NULL;
	}
	if (!gc.numMorphTargets) {
		ERROR("Geometry doesn't have any morph targets.");
		return NULL;
	}
	if (gc.format & rpGEOMETRYNATIVE) {
		ERROR("Native geometry is not supported.");
		return NULL;
	}
	tc_count = GeometryFormatGetNumTexCoordSetsMacro(gc.format);
	assert(tc_count <= rwMAXTEXTURECOORDS);
	offset = bstream_offset(stream);
	if ((gc.format & rpGEOMETRYPRELIT) &&
		!bstream_advance(stream,
			gc.numVertices * sizeof(RwRGBA))) {
		ERROR("Stream ended unexpectedly.");
		return NULL;
	}
	if (!bstream_advance(stream, (size_t)tc_count * 
			gc.numVertices * sizeof(RwTexCoords))) {
		ERROR("Stream ended unexpectedly.");
		return NULL;
	}
	memset(split_index_count, 0, sizeof(split_index_count));
	for (i = 0; i < gc.numTriangles; i++) {
		struct rpTriangle tri;
		RwUInt16 mat;
		uint32_t j;
		uint32_t split_idx = UINT32_MAX;
		if (!bstream_read(stream, &tri, sizeof(tri))) {
			ERROR("Stream ended unexpectedly.");
			return NULL;
		}
		mat = tri.vertex2Mat & 0xFFFF;
		if (max_mat_index == UINT32_MAX || mat > max_mat_index)
			max_mat_index = mat;
		for (j = 0; j < split_count; j++) {
			if (unique_mat[j] == mat) {
				split_idx = j;
				break;
			}
		}
		if (split_idx == UINT32_MAX) {
			if (split_count == COUNT_OF(unique_mat)) {
				ERROR("Geometry has too many materials.");
				return NULL;
			}
			split_index_count[split_count] = 3;
			unique_mat[split_count++] = mat;
		}
		else {
			split_index_count[split_idx] += 3;
		}
	}
	g = arm_alloc_geo(gc.numVertices, gc.numTriangles,
		split_count, max_mat_index + 1);
	flags = GeometryFormatGetFlagsMacro(gc.format);
	flags = (flags & ~(rpGEOMETRYTEXTURED | rpGEOMETRYTEXTURED2))
		| ((tc_count == 1) ? rpGEOMETRYTEXTURED :
			((tc_count > 1) ? rpGEOMETRYTEXTURED2 : 0));
	bstream_seek(stream, offset);
	if ((gc.format & rpGEOMETRYPRELIT) &&
		!bstream_read(stream, g->vertex_colors,
			gc.numVertices * sizeof(RwRGBA))) {
		ERROR("Stream ended unexpectedly.");
		return NULL;
	}
	for (i = 0; i < tc_count; i++) {
		uint32_t j;
		for (j = 0; j < g->vertex_count; j++) {
			if (!bstream_read(stream,
				g->vertices[j].texcoord[i], 8)) {
				ERROR("Stream ended unexpectedly.");
				return NULL;
			}
		}
	}
	g->index_count = 0;
	for (i = 0; i < g->split_count; i++) {
		g->splits[i].index_offset = g->index_count;
		g->index_count += split_index_count[i];
	}
	for (i = 0; i < gc.numTriangles; i++) {
		struct rpTriangle tri;
		uint32_t j;
		struct ac_mesh_split * split = NULL;
		struct ac_mesh_triangle * triangle = &g->triangles[i];
		if (!bstream_read(stream, &tri, sizeof(tri))) {
			ERROR("Stream ended unexpectedly.");
			return NULL;
		}
		for (j = 0; j < split_count; j++) {
			if (unique_mat[j] == (tri.vertex2Mat & 0xFFFF)) {
				split = &g->splits[j];
				split->material_index = unique_mat[j];
				break;
			}
		}
		if (!split) {
			assert(split);
			ERROR("Invalid triangle material index.");
			return NULL;
		}
		g->indices[split->index_offset + split->index_count++] =
			(tri.vertex01 >> 16) & 0xFFFF;
		g->indices[split->index_offset + split->index_count++] =
			tri.vertex01 & 0xFFFF;
		g->indices[split->index_offset + split->index_count++] =
			(tri.vertex2Mat >> 16) & 0xFFFF;
		triangle->indices[0] = (tri.vertex01 >> 16) & 0xFFFF;
		triangle->indices[1] = tri.vertex01 & 0xFFFF;
		triangle->indices[2] = (tri.vertex2Mat >> 16) & 0xFFFF;
		triangle->material_index = tri.vertex2Mat & 0xFFFF;
	}
	if (!bstream_read(stream, &morph, sizeof(morph))) {
		ERROR("Stream ended unexpectedly.");
		return NULL;
	}
	if (morph.pointsPresent) {
		for (i = 0; i < g->vertex_count; i++) {
			if (!bstream_read(stream, 
					g->vertices[i].position, 12)) {
				ERROR("Stream ended unexpectedly.");
				return NULL;
			}
		}
	}
	if (morph.normalsPresent) {
		for (i = 0; i < g->vertex_count; i++) {
			if (!bstream_read(stream, 
					g->vertices[i].normal, 12)) {
				ERROR("Stream ended unexpectedly.");
				return NULL;
			}
		}
	}
	/* Skip remaining morph targets. */
	for (i = 1; i < gc.numMorphTargets; i++) {
		if (!bstream_read(stream, &morph, sizeof(morph))) {
			ERROR("Stream ended unexpectedly.");
			return NULL;
		}
		if (morph.pointsPresent &&
			!bstream_advance(stream, gc.numVertices * 12)) {
			ERROR("Stream ended unexpectedly.");
			return NULL;
		}
		if (morph.normalsPresent &&
			!bstream_advance(stream, gc.numVertices * 12)) {
			ERROR("Stream ended unexpectedly.");
			return NULL;
		}
	}
	if (!ac_renderware_find_chunk(stream, rwID_MATLIST, NULL)) {
		ERROR("Failed to find rwID_MATLIST.");
		return NULL;
	}
	if (!arm_read_matlist(mod, stream, g)) {
		ERROR("Failed to read material list.");
		return NULL;
	}
	if (!arm_skip_extension(stream)) {
		ERROR("Failed to skip geometry extensions.");
		return NULL;
	}
	g->bsphere = morph.boundingSphere;
	return g;
}

boolean arm_write_geometry(
	struct bin_stream * stream,
	const struct ac_mesh_geometry * g)
{
	size_t geometry_offset;
	size_t struct_offset;
	size_t ext_offset;
	struct rpGeometryChunkInfo gc = { 0 };
	uint32_t i;
	struct rpMorphTarget morph = { 0 };
	if (!ac_renderware_begin_chunk(stream, rwID_GEOMETRY, &geometry_offset)) {
		ERROR("Failed to create rwID_GEOMETRY chunk.");
		return FALSE;
	}
	if (!ac_renderware_begin_chunk(stream, rwID_STRUCT, &struct_offset)) {
		ERROR("Failed to create rwID_STRUCT chunk.");
		return FALSE;
	}
	gc.format = rpGEOMETRYPRELIT | rpGEOMETRYNORMALS | 
		rpGEOMETRYLIGHT | rpGEOMETRYTEXTURED2;
	gc.format |= rpGEOMETRYTEXCOORDSETS(6);
	gc.numTriangles = g->triangle_count;
	gc.numVertices = g->vertex_count;
	gc.numMorphTargets = 1;
	if (!bstream_write(stream, &gc, sizeof(gc))) {
		ERROR("Failed to write geometry chunk info.");
		return FALSE;
	}
	if (!bstream_write(stream, g->vertex_colors,
			g->vertex_count * sizeof(*g->vertex_colors))) {
		ERROR("Failed to write vertex colors.");
		return FALSE;
	}
	for (i = 0; i < 6; i++) {
		uint32_t j;
		for (j = 0; j < g->vertex_count; j++) {
			if (!bstream_write(stream, g->vertices[j].texcoord[i],
					8)) {
				ERROR("Failed to write texcoords.");
				return FALSE;
			}
		}
	}
	for (i = 0; i < g->triangle_count; i++) {
		const struct ac_mesh_triangle * t = &g->triangles[i];
		struct rpTriangle tri = { 0 };
		tri.vertex01 = ((RwUInt32)t->indices[0] << 16) |
			t->indices[1];
		tri.vertex2Mat = ((RwUInt32)t->indices[2] << 16) |
			t->material_index;
		if (!bstream_write(stream, &tri, sizeof(tri))) {
			ERROR("Failed to write triangles.");
			return FALSE;
		}
	}
	morph.boundingSphere = g->bsphere;
	morph.pointsPresent = TRUE;
	morph.normalsPresent = TRUE;
	if (!bstream_write(stream, &morph, sizeof(morph))) {
		ERROR("Failed to write morph info.");
		return FALSE;
	}
	for (i = 0; i < g->vertex_count; i++) {
		if (!bstream_write(stream, g->vertices[i].position,
				12)) {
			ERROR("Failed to write vertex positions.");
			return FALSE;
		}
	}
	for (i = 0; i < g->vertex_count; i++) {
		if (!bstream_write(stream, g->vertices[i].normal,
				12)) {
			ERROR("Failed to write vertex normals.");
			return FALSE;
		}
	}
	if (!ac_renderware_end_chunk(stream, struct_offset)) {
		ERROR("Failed to complete rwID_STRUCT chunk.");
		return FALSE;
	}
	if (!arm_write_matlist(stream, g)) {
		ERROR("Failed to write material list.");
		return FALSE;
	}
	if (!ac_renderware_begin_chunk(stream, rwID_EXTENSION, &ext_offset)) {
		ERROR("Failed to create rwID_EXTENSION chunk.");
		return FALSE;
	}
	if (!write_binmesh_plg(stream, g)) {
		ERROR("Failed to write binary mesh plugin extension.");
		return FALSE;
	}
	if (!ac_renderware_end_chunk(stream, ext_offset)) {
		ERROR("Failed to complete rwID_EXTENSION chunk.");
		return FALSE;
	}
	if (!ac_renderware_end_chunk(stream, geometry_offset)) {
		ERROR("Failed to complete rwID_GEOMETRY chunk.");
		return FALSE;
	}
	return TRUE;
}

void arm_destroy_geometry_list(
	struct ac_mesh_module * mod,
	struct ac_mesh_geometry ** list,
	boolean destroy_geometry)
{
	if (destroy_geometry) {
		uint32_t i;
		uint32_t count = vec_count(list);
		for (i = 0; i < count; i++)
			ac_mesh_destroy_geometry(mod, list[i]);
	}
	vec_free(list);
}

struct ac_mesh_geometry ** arm_read_geometry_list(
	struct ac_mesh_module * mod,
	struct bin_stream * stream)
{
	struct ac_mesh_geometry ** gl;
	int32_t i;
	int32_t count;
	if (!ac_renderware_find_chunk(stream, rwID_STRUCT, NULL)) {
		ERROR("Failed to find rwID_STRUCT.");
		return NULL;
	}
	if (!bstream_read_i32(stream, &count)) {
		ERROR("Failed to read geometry count.");
		return NULL;
	}
	if (count > 0)
		gl = vec_new_reserved(sizeof(*gl), (uint32_t)count);
	else
		gl = vec_new(sizeof(*gl));
	for (i = 0; i < count; i++) {
		struct ac_mesh_geometry * g;
		if (!ac_renderware_find_chunk(stream, rwID_GEOMETRY, NULL)) {
			ERROR("Failed to find rwID_GEOMETRY.");
			arm_destroy_geometry_list(mod, gl, TRUE);
			return NULL;
		}
		g = arm_read_geometry(mod, stream);
		if (!g) {
			ERROR("Failed to read geometry stream.");
			arm_destroy_geometry_list(mod, gl, TRUE);
			return NULL;
		}
		vec_push_back((void **)&gl, &g);
	}
	return gl;
}

/*
 * Creates a new geometry object and copies parts of data 
 * from source geometry.
 *
 * Split and material data is not copied and is expected 
 * to be filled in later.
 */
static struct ac_mesh_geometry * duplicate_geo(
	struct ac_mesh_geometry * src,
	uint32_t split_count,
	uint32_t material_count)
{
	struct ac_mesh_geometry * g = arm_alloc_geo(src->vertex_count,
		src->triangle_count, split_count, material_count);
	memcpy(g->vertices, src->vertices,
		src->vertex_count * sizeof(*src->vertices));
	memcpy(g->vertex_colors, src->vertex_colors,
		src->vertex_count * sizeof(*src->vertex_colors));
	memcpy(g->triangles, src->triangles,
		src->triangle_count * sizeof(*src->triangles));
	memcpy(g->indices, src->indices,
		src->index_count * sizeof(*src->indices));
	memcpy(&g->bsphere, &src->bsphere, sizeof(src->bsphere));
	return g;
}

static boolean onregister(
	struct ac_mesh_module * mod,
	struct ap_module_registry * registry)
{
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ac_dat, AC_DAT_MODULE_NAME);
	AP_MODULE_INSTANCE_FIND_IN_REGISTRY(registry, mod->ac_texture, AC_TEXTURE_MODULE_NAME);
	return TRUE;
}

struct ac_mesh_module * ac_mesh_create_module()
{
	struct ac_mesh_module * mod = ap_module_instance_new(AC_MESH_MODULE_NAME,
		sizeof(*mod), onregister, NULL, NULL, NULL);
	return mod;
}

struct ac_mesh_geometry * ac_mesh_read_rp_atomic(
	struct ac_mesh_module * mod,
	struct bin_stream * stream,
	struct ac_mesh_geometry ** geo,
	uint32_t geo_count)
{
	struct rpAtomicBinary a;
	struct ac_mesh_geometry * g;
	RwChunkHeaderInfo ch;
	if (!ac_renderware_find_chunk(stream, rwID_STRUCT, NULL)) {
		ERROR("Failed to find rwID_STRUCT.");
		return NULL;
	}
	if (!bstream_read(stream, &a, sizeof(a))) {
		ERROR("Stream ended unexpectedly.");
		return NULL;
	}
	assert(a.flags == 5);
	assert(a.id == -1);
	if (geo_count) {
		if (a.geomIndex < 0 ||
			(RwUInt32)a.geomIndex >= geo_count) {
			ERROR("Invalid geometry index.");
			return NULL;
		}
		g = geo[a.geomIndex];
	}
	else {
		if (!ac_renderware_find_chunk(stream, rwID_GEOMETRY, NULL)) {
			ERROR("Failed to find rwID_GEOMETRY.");
			return NULL;
		}
		g = arm_read_geometry(mod, stream);
		if (!g) {
			ERROR("Failed to read geometry stream.");
			return NULL;
		}
	}
	if (!ac_renderware_find_chunk(stream, rwID_EXTENSION, &ch)) {
		ERROR("Failed to find rwID_EXTENSION.");
		return NULL;
	}
	while (ch.length > 0) {
		RwChunkHeaderInfo tmp;
		if (!ac_renderware_read_chunk(stream, &tmp)) {
			ERROR("Failed to read chunk.");
			return NULL;
		}
		if (!bstream_advance(stream, tmp.length)) {
			ERROR("Stream ended unexpectedly.");
			return NULL;
		}
		ch.length -= rwCHUNKHEADERSIZE + tmp.length;
	}
	/*
	{
		struct bin_stream * s;
		static mutex_t lock;
		static int done = ;
		if (!lock) {
			lock = create_mutex();
		}
		lock_mutex(lock);
		if (!done) {
			done = 1;
			bstream_for_write(NULL, 1ull << 20, &s);
			ac_mesh_write_rp_atomic(s, g);
			INFO("Stream length: %llu", bstream_offset(s));
			make_file("geo_original",
				(void *)((uintptr_t)stream->head + 4),
				stream->size - 4);
			make_file("geo_created", s->head, bstream_offset(s));
		}
		unlock_mutex(lock);
	}
	*/
	return g;
}

boolean ac_mesh_write_rp_atomic(
	struct bin_stream * stream,
	struct ac_mesh_geometry * g)
{
	size_t atomic_offset;
	size_t struct_offset;
	size_t ext_offset;
	size_t mtex_offset;
	struct rpAtomicBinary a = { 0 };
	if (!ac_renderware_begin_chunk(stream, rwID_ATOMIC, &atomic_offset)) {
		ERROR("Failed to create rwID_ATOMIC chunk.");
		return FALSE;
	}
	if (!ac_renderware_begin_chunk(stream, rwID_STRUCT, &struct_offset)) {
		ERROR("Failed to create rwID_STRUCT chunk.");
		return FALSE;
	}
	a.frameIndex = 0;
	a.geomIndex = 4;
	a.flags = rpATOMICCOLLISIONTEST | rpATOMICRENDER;
	a.renderType = 1;
	a.blendMode = 0;
	a.id = -1;
	if (!bstream_write(stream, &a, sizeof(a))) {
		ERROR("Failed to write geometry header.");
		return FALSE;
	}
	if (!ac_renderware_end_chunk(stream, struct_offset)) {
		ERROR("Failed to complete rwID_STRUCT chunk.");
		return FALSE;
	}
	if (!arm_write_geometry(stream, g)) {
		ERROR("Failed to write geometry.");
		return FALSE;
	}
	if (!ac_renderware_begin_chunk(stream, rwID_EXTENSION, &ext_offset)) {
		ERROR("Failed to create rwID_EXTENSION chunk.");
		return FALSE;
	}
	if (!ac_renderware_begin_chunk(stream, rwID_MTEXTURE, &mtex_offset)) {
		ERROR("Failed to create rwID_MTEXTURE chunk.");
		return FALSE;
	}
	if (!bstream_write_u32(stream, 1)) {
		ERROR("Failed to write rwID_MTEXTURE extension data.");
		return FALSE;
	}
	if (!ac_renderware_end_chunk(stream, mtex_offset)) {
		ERROR("Failed to complete rwID_MTEXTURE chunk.");
		return FALSE;
	}
	if (!ac_renderware_end_chunk(stream, ext_offset)) {
		ERROR("Failed to complete rwID_EXTENSION chunk.");
		return FALSE;
	}
	if (!ac_renderware_end_chunk(stream, atomic_offset)) {
		ERROR("Failed to complete rwID_ATOMIC chunk.");
		return FALSE;
	}
	return TRUE;
}

struct ac_mesh_clump * ac_mesh_read_rp_clump(
	struct ac_mesh_module * mod,
	struct bin_stream * stream)
{
	struct ac_mesh_clump * c;
	struct RpClumpChunkInfo cci;
	RwChunkHeaderInfo ch;
	RwInt32 i;
	struct ac_mesh_geometry ** gl;
	struct ac_mesh_geometry * links[128];
	uint32_t linkcount = 0;
	struct ac_mesh_geometry * link;
	if (!ac_renderware_find_chunk(stream, rwID_STRUCT, NULL)) {
		ERROR("Failed to find rwID_STRUCT.");
		return NULL;
	}
	if (!bstream_read(stream, &cci, sizeof(cci))) {
		ERROR("Failed to read clump chunk info.");
		return NULL;
	}
	if (!ac_renderware_find_chunk(stream, rwID_FRAMELIST, &ch)) {
		ERROR("Failed to find rwID_FRAMELIST.");
		return NULL;
	}
	if (!bstream_advance(stream, ch.length)) {
		ERROR("Stream ended unexpectedly.");
		return NULL;
	}
	if (!ac_renderware_find_chunk(stream, rwID_GEOMETRYLIST, NULL)) {
		ERROR("Failed to find rwID_GEOMETRYLIST.");
		return NULL;
	}
	gl = arm_read_geometry_list(mod, stream);
	if (!gl) {
		ERROR("Failed to read geometry list.");
		return NULL;
	}
	c = alloc(sizeof(*c));
	memset(c, 0, sizeof(*c));
	for (i = 0; i < cci.numAtomics; i++) {
		struct ac_mesh_geometry * g;
		if (!ac_renderware_find_chunk(stream, rwID_ATOMIC, NULL)) {
			ERROR("Failed to find rwID_ATOMIC.");
			arm_destroy_geometry_list(mod, gl, TRUE);
			dealloc(c);
			return NULL;
		}
		g = ac_mesh_read_rp_atomic(mod, stream, gl, vec_count(gl));
		if (!g) {
			ERROR("Failed to read RpAtomic.");
			arm_destroy_geometry_list(mod, gl, TRUE);
			dealloc(c);
			return NULL;
		}
		g->next = c->glist;
		c->glist = g;
	}
	arm_destroy_geometry_list(mod, gl, FALSE);
	/* Fix recursive list. */
	link = c->glist;
	while (link) {
		uint32_t j;
		if (linkcount >= COUNT_OF(links)) {
			WARN("Too many geometries in clump.");
			break;
		}
		links[linkcount++] = link;
		if (!link->next) {
			break;
		}
		for (j = 0; j < linkcount; j++) {
			if (link->next == links[j]) {
				link->next = NULL;
				TRACE("Cleared recursive clump.");
				break;
			}
		}
		link = link->next;
	}
	return c;
}

void ac_mesh_init_material(struct ac_mesh_material * m)
{
	uint32_t i;
	memset(m, 0, sizeof(*m));
	for (i = 0; i < COUNT_OF(m->tex_handle); i++) {
		m->tex_handle[i] =
			(bgfx_texture_handle_t)BGFX_INVALID_HANDLE;
	}
}

int ac_mesh_cmp_material(
	const struct ac_mesh_material * m1,
	const struct ac_mesh_material * m2)
{
	uint32_t i;
	for (i = 0; i < COUNT_OF(m1->tex_name); i++) {
		int n = strcasecmp(m1->tex_name[i], m2->tex_name[i]);
		assert(m1->tex_name[i][0] != -51);
		assert(m2->tex_name[i][0] != -51);
		if (n != 0)
			return (n < 0) ? -1 : 1;
	}
	return 0;
}

void ac_mesh_copy_material(
	struct ac_mesh_module * mod,
	struct ac_mesh_material * dst,
	const struct ac_mesh_material * src)
{
	uint32_t i;
	memcpy(dst, src, sizeof(*src));
	for (i = 0; i < COUNT_OF(src->tex_handle); i++) {
		if (BGFX_HANDLE_IS_VALID(src->tex_handle[i]))
			ac_texture_copy(mod->ac_texture, src->tex_handle[i]);
	}
}

void ac_mesh_release_material(
	struct ac_mesh_module * mod,
	struct ac_mesh_material * m)
{
	uint32_t i;
	for (i = 0; i < COUNT_OF(m->tex_handle); i++) {
		if (BGFX_HANDLE_IS_VALID(m->tex_handle[i]))
			ac_texture_release(mod->ac_texture, m->tex_handle[i]);
		m->tex_handle[i] = (bgfx_texture_handle_t)BGFX_INVALID_HANDLE;
		memset(m->tex_name[i], 0, sizeof(m->tex_name[i]));
	}
}

void ac_mesh_destroy_geometry(
	struct ac_mesh_module * mod,
	struct ac_mesh_geometry * g)
{
	uint32_t i;
	for (i = 0; i < g->material_count; i++) {
		struct ac_mesh_material * m = &g->materials[i];
		uint32_t j;
		for (j = 0; j < COUNT_OF(m->tex_handle); j++) {
			if (BGFX_HANDLE_IS_VALID(m->tex_handle[j])) {
				ac_texture_release(mod->ac_texture, m->tex_handle[j]);
			}
		}
	}
	if (BGFX_HANDLE_IS_VALID(g->vertex_buffer)) {
		bgfx_destroy_vertex_buffer(g->vertex_buffer);
	}
	if (BGFX_HANDLE_IS_VALID(g->index_buffer)) {
		bgfx_destroy_index_buffer(g->index_buffer);
	}
	dealloc(g->materials);
	dealloc(g);
}

bgfx_vertex_layout_t ac_mesh_vertex_layout()
{
	bgfx_vertex_layout_t layout;
	bgfx_vertex_layout_begin(&layout,
		bgfx_get_renderer_type());
	bgfx_vertex_layout_add(&layout,
		BGFX_ATTRIB_POSITION, 3, BGFX_ATTRIB_TYPE_FLOAT,
		false, false);
	bgfx_vertex_layout_add(&layout,
		BGFX_ATTRIB_NORMAL, 3, BGFX_ATTRIB_TYPE_FLOAT,
		false, false);
	bgfx_vertex_layout_add(&layout,
		BGFX_ATTRIB_TEXCOORD0, 2, BGFX_ATTRIB_TYPE_FLOAT,
		false, false);
	bgfx_vertex_layout_add(&layout,
		BGFX_ATTRIB_TEXCOORD1, 2, BGFX_ATTRIB_TYPE_FLOAT,
		false, false);
	bgfx_vertex_layout_add(&layout,
		BGFX_ATTRIB_TEXCOORD2, 2, BGFX_ATTRIB_TYPE_FLOAT,
		false, false);
	bgfx_vertex_layout_add(&layout,
		BGFX_ATTRIB_TEXCOORD3, 2, BGFX_ATTRIB_TYPE_FLOAT,
		false, false);
	bgfx_vertex_layout_add(&layout,
		BGFX_ATTRIB_TEXCOORD4, 2, BGFX_ATTRIB_TYPE_FLOAT,
		false, false);
	bgfx_vertex_layout_add(&layout,
		BGFX_ATTRIB_TEXCOORD5, 2, BGFX_ATTRIB_TYPE_FLOAT,
		false, false);
	bgfx_vertex_layout_add(&layout,
		BGFX_ATTRIB_TEXCOORD6, 2, BGFX_ATTRIB_TYPE_FLOAT,
		false, false);
	bgfx_vertex_layout_add(&layout,
		BGFX_ATTRIB_TEXCOORD7, 2, BGFX_ATTRIB_TYPE_FLOAT,
		false, false);
	bgfx_vertex_layout_end(&layout);
	return layout;
}

struct ac_mesh_geometry * ac_mesh_rebuild_splits(
	struct ac_mesh_module * mod,
	struct ac_mesh_geometry * g)
{
	struct ac_mesh_geometry * ng = NULL;
	uint32_t i;
	uint32_t * indices = alloc(g->material_count * sizeof(*indices));
	uint32_t * split_index_count = alloc(
		g->material_count * sizeof(*split_index_count));
	uint32_t * mat_table = alloc(g->material_count * sizeof(*mat_table));
	uint32_t count = 0;
	memset(split_index_count, 0,
		g->material_count * sizeof(*split_index_count));
	memset(mat_table, 0xFF,
		g->material_count * sizeof(*mat_table));
	for (i = 0; i < g->triangle_count; i++) {
		uint32_t j;
		uint32_t split_index = UINT32_MAX;
		for (j = 0; j < count; j++) {
			if (indices[j] == g->triangles[i].material_index) {
				split_index = j;
				break;
			}
		}
		if (split_index == UINT32_MAX) {
			assert(count < g->material_count);
			mat_table[g->triangles[i].material_index] = count;
			indices[count] = g->triangles[i].material_index;
			split_index_count[count++] = 3;
		}
		else {
			split_index_count[split_index] += 3;
		}
	}
	ng = duplicate_geo(g, count, count);
	ng->material_count = 0;
	ng->index_count = 0;
	for (i = 0; i < ng->split_count; i++) {
		ng->splits[i].index_offset = ng->index_count;
		ng->splits[i].material_index = i;
		ng->index_count += split_index_count[i];
		memcpy(&ng->materials[ng->material_count++],
			&g->materials[indices[i]], sizeof(*g->materials));
	}
	for (i = 0; i < ng->triangle_count; i++) {
		struct ac_mesh_triangle * tri = &ng->triangles[i];
		uint32_t mat_index = mat_table[tri->material_index];
		struct ac_mesh_split * s = &ng->splits[mat_index];
		uint32_t j;
		tri->material_index = mat_index;
		for (j = 0; j < 3; j++) {
			ng->indices[s->index_offset + s->index_count++] =
				tri->indices[j];
		}
	}
	for (i = 0; i < g->material_count; i++) {
		uint32_t j;
		boolean in_use = FALSE;
		for (j = 0; j < count; j++) {
			if (indices[j] == i) {
				in_use = TRUE;
				break;
			}
		}
		if (!in_use)
			ac_mesh_release_material(mod, &g->materials[i]);
	}
	dealloc(indices);
	dealloc(split_index_count);
	dealloc(mat_table);
	if (BGFX_HANDLE_IS_VALID(g->vertex_buffer)) {
		bgfx_vertex_layout_t layout = ac_mesh_vertex_layout();
		const bgfx_memory_t * mem;
		bgfx_destroy_vertex_buffer(g->vertex_buffer);
		mem = bgfx_copy(ng->vertices, ng->vertex_count * sizeof(*ng->vertices));
		ng->vertex_buffer = bgfx_create_vertex_buffer(mem, &layout,
			BGFX_BUFFER_NONE);
		if (!BGFX_HANDLE_IS_VALID(ng->vertex_buffer)) {
			ERROR("Failed to rough geometry create vertex buffer.");
		}
	}
	if (BGFX_HANDLE_IS_VALID(g->index_buffer)) {
		const bgfx_memory_t * mem;
		bgfx_destroy_index_buffer(g->index_buffer);
		mem = bgfx_copy(ng->indices, ng->index_count * sizeof(*ng->indices));
		ng->index_buffer = bgfx_create_index_buffer(mem, 0);
		if (!BGFX_HANDLE_IS_VALID(ng->index_buffer)) {
			ERROR("Failed to rough geometry create index buffer.");
			bgfx_destroy_vertex_buffer(ng->vertex_buffer);
			BGFX_INVALIDATE_HANDLE(ng->vertex_buffer);
		}
	}
	dealloc(g->materials);
	dealloc(g);
	return ng;
}

boolean ac_mesh_set_material(
	struct ac_mesh_module * mod,
	struct ac_mesh_geometry * g,
	struct ac_mesh_triangle * tri,
	const struct ac_mesh_material * m)
{
	struct ac_mesh_material * nm;
	uint32_t i;
	if (ac_mesh_cmp_material(&g->materials[tri->material_index], m) == 0)
		return FALSE;
	for (i = 0; i < g->material_count; i++) {
		if (ac_mesh_cmp_material(&g->materials[i], m) == 0) {
			tri->material_index = i;
			return TRUE;
		}
	}
	tri->material_index = g->material_count;
	nm = alloc((size_t)(g->material_count + 1) * sizeof(*nm));
	memcpy(nm, g->materials, g->material_count * sizeof(*nm));
	ac_mesh_copy_material(mod, &nm[g->material_count++], m);
	dealloc(g->materials);
	g->materials = nm;
	return TRUE;
}

void ac_mesh_calculate_normals(struct ac_mesh_geometry * g)
{
	uint32_t i;
	for (i = 0; i < g->vertex_count; i++) {
		struct ac_mesh_vertex * v = &g->vertices[i];
		v->normal[0] = 0.0f;
		v->normal[1] = 0.0f;
		v->normal[2] = 0.0f;
	}
	for (i = 0; i < g->triangle_count; i++) {
		const struct ac_mesh_triangle * t = &g->triangles[i];
		struct ac_mesh_vertex * v[3] = {
			&g->vertices[t->indices[0]],
			&g->vertices[t->indices[1]],
			&g->vertices[t->indices[2]] };
		vec3 edges[2] = {
			{
				v[1]->position[0] - v[0]->position[0],
				v[1]->position[1] - v[0]->position[1],
				v[1]->position[2] - v[0]->position[2] },
			{
				v[2]->position[0] - v[0]->position[0],
				v[2]->position[1] - v[0]->position[1],
				v[2]->position[2] - v[0]->position[2] } };
		vec3 n;
		uint32_t j;
		glm_vec3_cross(edges[0], edges[1], n);
		/* Add normal as a weight to all vertices 
		 * sharing the same position. */
		for (j = 0; j < 3; j++) {
			uint32_t k;
			for (k = 0; k < g->vertex_count; k++) {
				struct ac_mesh_vertex * vtx = &g->vertices[k];
				if (vtx->position[0] == v[j]->position[0] &&
					vtx->position[1] == v[j]->position[1] &&
					vtx->position[2] == v[j]->position[2]) {
					vtx->normal[0] += n[0];
					vtx->normal[1] += n[1];
					vtx->normal[2] += n[2];
				}
			}
		}
	}
	for (i = 0; i < g->vertex_count; i++) {
		struct ac_mesh_vertex * v = &g->vertices[i];
		vec3 n = { v->normal[0], v->normal[1], v->normal[2] };
		glm_vec3_normalize(n);
		v->normal[0] = n[0];
		v->normal[1] = n[1];
		v->normal[2] = n[2];
	}
}

void ac_mesh_calculate_normals_ex(
	struct ac_mesh_vertex * vertices,
	uint32_t vertex_count,
	uint32_t * indices,
	uint32_t index_count)
{
}

void ac_mesh_calculate_surface_normal(
	const struct ac_mesh_geometry * g,
	const struct ac_mesh_triangle * t,
	vec3 n)
{
	struct ac_mesh_vertex * v[3] = {
		&g->vertices[t->indices[0]],
		&g->vertices[t->indices[1]],
		&g->vertices[t->indices[2]] };
	vec3 edges[2] = {
		{	v[1]->position[0] - v[0]->position[0],
			v[1]->position[1] - v[0]->position[1],
			v[1]->position[2] - v[0]->position[2] },
		{	v[2]->position[0] - v[0]->position[0],
			v[2]->position[1] - v[0]->position[1],
			v[2]->position[2] - v[0]->position[2] } };
	glm_vec3_cross(edges[0], edges[1], n);
}

void ac_mesh_query_triangles(
	struct ac_mesh_geometry * g,
	struct ac_mesh_triangle ** triangles,
	const vec3 pos)
{
	uint32_t i;
	for (i = 0; i < g->triangle_count; i++) {
		const struct ac_mesh_triangle * t = &g->triangles[i];
		struct ac_mesh_vertex * v[3] = {
			&g->vertices[t->indices[0]],
			&g->vertices[t->indices[1]],
			&g->vertices[t->indices[2]] };
		uint32_t j;
		for (j = 0; j < 3; j++) {
			if (v[j]->position[0] == pos[0] &&
				v[j]->position[1] == pos[1] &&
				v[j]->position[2] == pos[2]) {
				vec_push_back(triangles, &t);
				break;
			}
		}
	}
}

boolean ac_mesh_eq_pos(
	const struct ac_mesh_vertex * v1,
	const struct ac_mesh_vertex * v2)
{
	return (
		v1->position[0] == v2->position[0] &&
		v1->position[1] == v2->position[1] &&
		v1->position[2] == v2->position[2]);
}

void ac_mesh_destroy_clump(struct ac_mesh_module * mod, struct ac_mesh_clump * c)
{
	struct ac_mesh_geometry * g = c->glist;
	while (g) {
		struct ac_mesh_geometry * next = g->next;
		ac_mesh_destroy_geometry(mod, g);
		g = next;
	}
	dealloc(c);
}

struct ac_mesh_geometry * ac_mesh_alloc_geometry(
	uint32_t vertex_count,
	uint32_t triangle_count,
	uint32_t material_count)
{
	return arm_alloc_geo(vertex_count, triangle_count, material_count, material_count);
}
