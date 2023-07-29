#include "client/ac_texture.h"

#include "core/bin_stream.h"
#include "core/file_system.h"
#include "core/hash_map.h"
#include "core/log.h"
#include "core/malloc.h"
#include "core/os.h"
#include "core/string.h"
#include "core/vector.h"

#include "public/ap_config.h"

#include "utility/au_md5.h"

#include "client/ac_dat.h"
#include "client/ac_renderware.h"

#include "vendor/bimg/decode.h"
#include "vendor/bx/allocator.h"

#include <time.h>

#define HAS_ALPHA           (1<<0)
#define IS_CUBE             (1<<1)
#define USE_AUTOMIPMAPGEN   (1<<2)
#define IS_COMPRESSED       (1<<3)

#define DDS_MAGIC        BX_MAKEFOURCC('D', 'D', 'S', ' ')
#define DDS_HEADER_SIZE  124

#define DDS_DXT1 BX_MAKEFOURCC('D', 'X', 'T', '1')
#define DDS_DXT2 BX_MAKEFOURCC('D', 'X', 'T', '2')
#define DDS_DXT3 BX_MAKEFOURCC('D', 'X', 'T', '3')
#define DDS_DXT4 BX_MAKEFOURCC('D', 'X', 'T', '4')
#define DDS_DXT5 BX_MAKEFOURCC('D', 'X', 'T', '5')
#define DDS_ATI1 BX_MAKEFOURCC('A', 'T', 'I', '1')
#define DDS_BC4U BX_MAKEFOURCC('B', 'C', '4', 'U')
#define DDS_ATI2 BX_MAKEFOURCC('A', 'T', 'I', '2')
#define DDS_BC5U BX_MAKEFOURCC('B', 'C', '5', 'U')
#define DDS_DX10 BX_MAKEFOURCC('D', 'X', '1', '0')

#define DDS_Ectx1     BX_MAKEFOURCC('E', 'T', 'C', '1')
#define DDS_Ectx2     BX_MAKEFOURCC('E', 'T', 'C', '2')
#define DDS_ET2A     BX_MAKEFOURCC('E', 'T', '2', 'A')
#define DDS_Pctx2     BX_MAKEFOURCC('P', 'T', 'C', '2')
#define DDS_Pctx4     BX_MAKEFOURCC('P', 'T', 'C', '4')
#define DDS_Actx      BX_MAKEFOURCC('A', 'T', 'C', ' ')
#define DDS_ActxE     BX_MAKEFOURCC('A', 'T', 'C', 'E')
#define DDS_ActxI     BX_MAKEFOURCC('A', 'T', 'C', 'I')
#define DDS_ASctx4x4  BX_MAKEFOURCC('A', 'S', '4', '4')
#define DDS_ASctx5x5  BX_MAKEFOURCC('A', 'S', '5', '5')
#define DDS_ASctx6x6  BX_MAKEFOURCC('A', 'S', '6', '6')
#define DDS_ASctx8x5  BX_MAKEFOURCC('A', 'S', '8', '5')
#define DDS_ASctx8x6  BX_MAKEFOURCC('A', 'S', '8', '6')
#define DDS_ASctx10x5 BX_MAKEFOURCC('A', 'S', ':', '5')

#define DDS_R8G8B8         20
#define DDS_A8R8G8B8       21
#define DDS_R5G6B5         23
#define DDS_A1R5G5B5       25
#define DDS_A4R4G4B4       26
#define DDS_A2B10G10R10    31
#define DDS_G16R16         34
#define DDS_A2R10G10B10    35
#define DDS_A16B16G16R16   36
#define DDS_A8L8           51
#define DDS_R16F           111
#define DDS_G16R16F        112
#define DDS_A16B16G16R16F  113
#define DDS_R32F           114
#define DDS_G32R32F        115
#define DDS_A32B32G32R32F  116

#define DDS_FORMAT_R32G32B32A32_FLOAT  2
#define DDS_FORMAT_R32G32B32A32_UINT   3
#define DDS_FORMAT_R16G16B16A16_FLOAT  10
#define DDS_FORMAT_R16G16B16A16_UNORM  11
#define DDS_FORMAT_R16G16B16A16_UINT   12
#define DDS_FORMAT_R32G32_FLOAT        16
#define DDS_FORMAT_R32G32_UINT         17
#define DDS_FORMAT_R10G10B10A2_UNORM   24
#define DDS_FORMAT_R11G11B10_FLOAT     26
#define DDS_FORMAT_R8G8B8A8_UNORM      28
#define DDS_FORMAT_R8G8B8A8_UNORM_SRGB 29
#define DDS_FORMAT_R16G16_FLOAT        34
#define DDS_FORMAT_R16G16_UNORM        35
#define DDS_FORMAT_R32_FLOAT           41
#define DDS_FORMAT_R32_UINT            42
#define DDS_FORMAT_R8G8_UNORM          49
#define DDS_FORMAT_R16_FLOAT           54
#define DDS_FORMAT_R16_UNORM           56
#define DDS_FORMAT_R8_UNORM            61
#define DDS_FORMAT_R1_UNORM            66
#define DDS_FORMAT_BC1_UNORM           71
#define DDS_FORMAT_BC1_UNORM_SRGB      72
#define DDS_FORMAT_BC2_UNORM           74
#define DDS_FORMAT_BC2_UNORM_SRGB      75
#define DDS_FORMAT_BC3_UNORM           77
#define DDS_FORMAT_BC3_UNORM_SRGB      78
#define DDS_FORMAT_BC4_UNORM           80
#define DDS_FORMAT_BC5_UNORM           83
#define DDS_FORMAT_B5G6R5_UNORM        85
#define DDS_FORMAT_B5G5R5A1_UNORM      86
#define DDS_FORMAT_B8G8R8A8_UNORM      87
#define DDS_FORMAT_B8G8R8A8_UNORM_SRGB 91
#define DDS_FORMAT_BC6H_SF16           96
#define DDS_FORMAT_BC7_UNORM           98
#define DDS_FORMAT_BC7_UNORM_SRGB      99
#define DDS_FORMAT_B4G4R4A4_UNORM      115

#define DDS_DX10_DIMENSION_TEXTURE2D 3
#define DDS_DX10_DIMENSION_TEXTURE3D 4
#define DDS_DX10_MISC_TEXTURECUBE    4

#define DDSD_CAPS                  0x00000001
#define DDSD_HEIGHT                0x00000002
#define DDSD_WIDTH                 0x00000004
#define DDSD_PIctxH                 0x00000008
#define DDSD_PIXELFORMAT           0x00001000
#define DDSD_MIPMAPCOUNT           0x00020000
#define DDSD_LINEARSIZE            0x00080000
#define DDSD_DEPTH                 0x00800000

#define DDPF_ALPHAPIXELS           0x00000001
#define DDPF_ALPHA                 0x00000002
#define DDPF_FOURCC                0x00000004
#define DDPF_INDEXED               0x00000020
#define DDPF_RGB                   0x00000040
#define DDPF_YUV                   0x00000200
#define DDPF_LUMINANCE             0x00020000
#define DDPF_BUMPDUDV              0x00080000

#define DDSCAPS_COMPLEX            0x00000008
#define DDSCAPS_TEXTURE            0x00001000
#define DDSCAPS_MIPMAP             0x00400000

#define DDSCAPS2_VOLUME            0x00200000
#define DDSCAPS2_CUBEMAP           0x00000200
#define DDSCAPS2_CUBEMAP_POSITIVEX 0x00000400
#define DDSCAPS2_CUBEMAP_NEGATIVEX 0x00000800
#define DDSCAPS2_CUBEMAP_POSITIVEY 0x00001000
#define DDSCAPS2_CUBEMAP_NEGATIVEY 0x00002000
#define DDSCAPS2_CUBEMAP_POSITIVEZ 0x00004000
#define DDSCAPS2_CUBEMAP_NEGATIVEZ 0x00008000

#define DSCAPS2_CUBEMAP_ALLSIDES (0      \
			| DDSCAPS2_CUBEMAP_POSITIVEX \
			| DDSCAPS2_CUBEMAP_NEGATIVEX \
			| DDSCAPS2_CUBEMAP_POSITIVEY \
			| DDSCAPS2_CUBEMAP_NEGATIVEY \
			| DDSCAPS2_CUBEMAP_POSITIVEZ \
			| DDSCAPS2_CUBEMAP_NEGATIVEZ \
			)

struct texture_entry {
	char * name;
	size_t len;
	bgfx_texture_handle_t handle;
	bgfx_texture_info_t info;
	uint32_t refcount;
};

struct handle_name_pair {
	bgfx_texture_handle_t handle;
	const char * name;
};

struct thread_dir {
	uint64_t thread_id;
	enum ac_dat_directory current_dir;
};

struct ac_texture_module {
	struct ap_module_instance instance;
	struct ap_config_module * ap_config;
	struct ac_dat_module * ac_dat;
	mutex_t mutex;
	hmap_t tex_map;
	handle_name_pair * handle_name_pairs;
	struct thread_dir * current_dir;
};

static bx::DefaultAllocator g_Allocator;

static enum ac_dat_directory get_current_dir(
	struct ac_texture_module * mod)
{
	uint32_t i;
	uint64_t id = get_current_thread_id();
	lock_mutex(mod->mutex);
	for (i = 0; i < vec_count(mod->current_dir); i++) {
		if (mod->current_dir[i].thread_id == id) {
			enum ac_dat_directory dir =
				mod->current_dir[i].current_dir;
			unlock_mutex(mod->mutex);
			return dir;
		}
	}
	unlock_mutex(mod->mutex);
	return AC_DAT_DIR_COUNT;
}

static uint64_t hash_entry(
	const void * item,
	uint64_t seed0,
	uint64_t seed1)
{
	const texture_entry * e = (const texture_entry *)item;
	return hmap_sip(e->name, e->len, seed0, seed1);
}

static int compare_entry(
	const void * a,
	const void * b,
	void * user_data)
{
	const texture_entry * e1 = (const texture_entry *)a;
	const texture_entry * e2 = (const texture_entry *)b;
	return strcasecmp(e1->name, e2->name);
}

static void img_release_cb(void * ptr, void * user_data)
{
	BX_UNUSED(ptr);
	bimg::ImageContainer * img =
		(bimg::ImageContainer *)user_data;
	bimg::imageFree(img);
}

static boolean get_extention(
	char * dst,
	size_t maxcount,
	const char * file_path)
{
	const char * cursor = strrchr(file_path, '.');
	size_t len;
	if (!cursor)
		return FALSE;
	len = strlen(++cursor);
	if (len + 1 > maxcount)
		return FALSE;
	memcpy(dst, cursor, len + 1);
	strlower(dst);
	return TRUE;
}

static void strip_extension(char * str, char * ext, size_t count)
{
	size_t len = strlen(str);
	char * s;
	char c;
	if (!len) {
		*ext = '\0';
		return;
	}
	s = str + (len - 1);
	c = *s--;
	while (c && c != '\\' && c != '/' && s + 1 >= str) {
		if (c == '.') {
			strlcpy(ext, s + 2, count);
			memset(s + 1, 0, strlen(s + 1));
			return;
		}
		c = *s--;
	}
}

static texture_entry * find_entry(
	struct ac_texture_module * mod,
	const char * file_path)
{
	texture_entry fe = { (char *)file_path, strlen(file_path) };
	texture_entry * e;
	lock_mutex(mod->mutex);
	e = (texture_entry *)hmap_get(mod->tex_map,
		(const void *)&fe);
	if (e)
		e->refcount++;
	unlock_mutex(mod->mutex);
	return e;
}

static void insert_entry(
	struct ac_texture_module * mod,
	const char * file_path,
	bgfx_texture_handle_t handle,
	bgfx_texture_info_t * info)
{
	texture_entry e;
	handle_name_pair p = { 0 };
	memset(&e, 0, sizeof(e));
	e.len = strlen(file_path);
	e.name = (char *)alloc(e.len + 1);
	memcpy(e.name, file_path, e.len + 1);
	e.handle = handle;
	e.refcount = 1;
	memcpy(&e.info, info, sizeof(*info));
	p.handle = handle;
	p.name = e.name;
	lock_mutex(mod->mutex);
	hmap_set(mod->tex_map, &e);
	vec_push_back((void **)&mod->handle_name_pairs, &p);
	unlock_mutex(mod->mutex);
}

static boolean entry_report(const void * item, void * user_data)
{
	texture_entry * e = (texture_entry *)item;
	WARN("Texture was not released at shutdown (%04u:%08u:%s).",
		e->handle.idx, e->refcount, e->name);
	return true;
}

static struct bin_stream * dds_from_rws(
	const void * data,
	size_t size)
{
	struct bin_stream * rws;
	struct bin_stream * dds;
	RwChunkHeaderInfo c;
	bstream_from_buffer(data, size, FALSE, &rws);
	if (!ac_renderware_read_chunk(rws, &c)) {
		ERROR("Failed to read chunk.");
		return NULL;
	}
	if (c.type == rwID_TEXDICTIONARY) {
		RwUInt16 count;
		RwUInt32 filter_addr;
		RwUInt32 fmt;
		RwUInt32 d3d_fmt;
		RwUInt16 width;
		RwUInt16 height;
		RwUInt8 depth;
		RwUInt8 mip_levels;
		RwUInt8 type;
		RwUInt8 flags;
		uint32_t tmp;
		if (!ac_renderware_find_chunk(rws, rwID_STRUCT, NULL)) {
			ERROR("Failed to find rwID_STRUCT.");
			return NULL;
		}
		if (!bstream_read_u16(rws, &count)) {
			ERROR("Stream ended unexpectedly.");
			return NULL;
		}
		if (count != 1) {
			ERROR("Multiple textures in same stream is not supported.");
			return NULL;
		}
		/* Skip device id. */
		if (!bstream_advance(rws, 2)) {
			ERROR("Stream ended unexpectedly.");
			return NULL;
		}
		if (!ac_renderware_find_chunk(rws, rwID_TEXTURENATIVE, NULL)) {
			ERROR("Failed to find rwID_TEXTURENATIVE.");
			return NULL;
		}
		if (!ac_renderware_find_chunk(rws, rwID_STRUCT, &c)) {
			ERROR("Failed to find rwID_STRUCT.");
			return NULL;
		}
		if (!bstream_advance(rws, 4) || /* rwID_PCD3D9. */
			!bstream_read_u32(rws, &filter_addr) ||
			!bstream_advance(rws, 64) || /* Name/Mask */
			!bstream_read_u32(rws, &fmt) ||
			!bstream_read_u32(rws, &d3d_fmt) ||
			!bstream_read_u16(rws, &width) ||
			!bstream_read_u16(rws, &height) ||
			!bstream_read_u8(rws, &depth) ||
			!bstream_read_u8(rws, &mip_levels) ||
			!bstream_read_u8(rws, &type) ||
			!bstream_read_u8(rws, &flags)) {
			ERROR("Stream ended unexpectedly.");
			return NULL;
		}
		if (!mip_levels) {
			ERROR("At least one mip level is required.");
			return NULL;
		}
		if (flags & IS_CUBE) {
			ERROR("Cube textures are not supported.");
			return NULL;
		}
		if (!(flags & IS_COMPRESSED)) {
			ERROR("Uncompressed textures are not supported.");
			return NULL;
		}
		bstream_for_write(NULL, rws->size * 2, &dds);
		bstream_write_u32(dds, DDS_MAGIC);
		bstream_write_u32(dds, DDS_HEADER_SIZE);
		tmp = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH |
			DDSD_PIXELFORMAT;
		if (mip_levels > 1)
			tmp |= DDSD_MIPMAPCOUNT;
		bstream_write_u32(dds, tmp);
		bstream_write_u32(dds, height);
		bstream_write_u32(dds, width);
		bstream_write_u32(dds, 0); /* Pictxh */
		bstream_write_u32(dds, 0); /* Depth */
		bstream_write_u32(dds, mip_levels);
		bstream_fill(dds, 0, 44); /* Reserved */
		bstream_write_u32(dds, 4); /* Pixel format size */
		tmp = DDPF_FOURCC;
		if (flags & HAS_ALPHA)
			tmp |= DDPF_ALPHAPIXELS;
		bstream_write_u32(dds, tmp); /* Pixel flags */
		bstream_write_u32(dds, d3d_fmt); /* FourCC */
		bstream_write_u32(dds, 0); /* Bit count */
		bstream_fill(dds, 0, 16); /* Bit mask */
		tmp = DDSCAPS_COMPLEX | DDSCAPS_TEXTURE;
		if (mip_levels > 1)
			tmp |= DDSCAPS_MIPMAP;
		bstream_write_u32(dds, tmp); /* Caps[0] */
		bstream_write_u32(dds, 0); /* Caps[1] */
		bstream_write_u32(dds, 0); /* Caps[2] */
		bstream_write_u32(dds, 0); /* Caps[3] */
		bstream_write_u32(dds, 0); /* Reserved */
		for (tmp = 0; tmp < mip_levels; tmp++) {
			uint32_t sz;
			if (!bstream_read_u32(rws, &sz) ||
				!bstream_write(dds, rws->cursor, sz) ||
				!bstream_advance(rws, sz)) {
				ERROR("Stream ended unexpectedly.");
				return NULL;
			}
		}
		bstream_destroy(rws);
		return dds;
	}
	else {
		ERROR("Unsupported chunk type (%08X).", c.type);
		return NULL;
	}
}

static bgfx_texture_handle_t load_internal(
	struct ac_texture_module * mod,
	const char * file_path,
	void * data,
	size_t size,
	bgfx_texture_info_t * info)
{
	bimg::ImageContainer * img;
	const bgfx_memory_t * mem;
	bgfx_texture_handle_t handle = BGFX_INVALID_HANDLE;
	char ext[8];
	bgfx_texture_info_t ninfo;
	struct bin_stream * dds = NULL;
	if (!get_extention(ext, sizeof(ext), file_path))
		return BGFX_INVALID_HANDLE;
	if (strcmp(ext, "tx1") == 0) {
		if (!au_md5_crypt(data, size,
				(const uint8_t *)"asdfqwer", 8)) {
			return BGFX_INVALID_HANDLE;
		}
		if (size <= 0x1C) {
			return BGFX_INVALID_HANDLE;
		}
		dds = dds_from_rws(data, size);
		if (!dds) {
			ERROR("Failed to create DDS data from RenderWare stream.");
			return BGFX_INVALID_HANDLE;
		}
		data = dds->head;
		size = dds->size;
	}
	img = bimg::imageParse(&g_Allocator, data,
		(uint32_t)size);
	if (dds)
		bstream_destroy(dds);
	if (!img)
		return BGFX_INVALID_HANDLE;
	mem = bgfx_make_ref_release(img->m_data, img->m_size,
		img_release_cb, img);
	if (img->m_cubeMap) {
		handle = bgfx_create_texture_cube((uint16_t)img->m_width,
			img->m_numMips > 1, img->m_numLayers,
			(bgfx_texture_format)img->m_format, 0, mem);
	}
	else if (img->m_depth > 1) {
		handle = bgfx_create_texture_3d(
			(uint16_t)img->m_width, (uint16_t)img->m_height,
			(uint16_t)img->m_depth, img->m_numMips > 1,
			(bgfx_texture_format)img->m_format, 0, mem);
	}
	else if (bgfx_is_texture_valid(0, false, img->m_numLayers,
		(bgfx_texture_format)img->m_format, 0)) {
		handle = bgfx_create_texture_2d(
			(uint16_t)img->m_width, (uint16_t)img->m_height,
			img->m_numMips > 1, img->m_numLayers,
			(bgfx_texture_format)img->m_format, 0, mem);
	}
	if (!BGFX_HANDLE_IS_VALID(handle))
		return BGFX_INVALID_HANDLE;
	bgfx_set_texture_name(handle, file_path, INT32_MAX);
	bgfx_calc_texture_size(&ninfo, (uint16_t)img->m_width,
		(uint16_t)img->m_height, (uint16_t)img->m_depth,
		img->m_cubeMap, img->m_numMips > 1, img->m_numLayers,
		(bgfx_texture_format)img->m_format);
	insert_entry(mod, file_path, handle, &ninfo);
	if (info)
		memcpy(info, &ninfo, sizeof(ninfo));
	return handle;
}

static boolean onregister(
	struct ac_texture_module * mod,
	struct ap_module_registry * registry)
{
	mod->ap_config = (struct ap_config_module *)ap_module_registry_get_module(registry, AP_CONFIG_MODULE_NAME);
	if (!mod->ap_config) {
		ERROR("Failed to retrieve module (%s).", AP_CONFIG_MODULE_NAME);
		return FALSE;
	}
	mod->ac_dat = (struct ac_dat_module *)ap_module_registry_get_module(registry, AC_DAT_MODULE_NAME);
	if (!mod->ac_dat) {
		ERROR("Failed to retrieve module (%s).", AC_DAT_MODULE_NAME);
		return FALSE;
	}
	return TRUE;
}

static void onshutdown(struct ac_texture_module * mod)
{
	hmap_scan(mod->tex_map, entry_report, NULL);
	hmap_clear(mod->tex_map, FALSE);
	hmap_free(mod->tex_map);
	vec_free(mod->handle_name_pairs);
	destroy_mutex(mod->mutex);
}

struct ac_texture_module * ac_texture_create_module()
{
	struct ac_texture_module * mod = (ac_texture_module *)ap_module_instance_new(
		AC_TEXTURE_MODULE_NAME, sizeof(*mod), 
		(ap_module_instance_register_t)onregister, NULL, NULL, 
		(ap_module_instance_shutdown_t)onshutdown);
	mod->mutex = create_mutex();
	mod->tex_map = hmap_new(sizeof(texture_entry), 64,
		(uint64_t)time(NULL), (uint64_t)time,
		hash_entry, compare_entry, NULL, NULL);
	mod->handle_name_pairs = (handle_name_pair *)vec_new_reserved(
		sizeof(*mod->handle_name_pairs), 128);
	mod->current_dir = (thread_dir *)vec_new_reserved(
		sizeof(*mod->current_dir), 128);
	return mod;
}

bgfx_texture_handle_t ac_texture_load(
	struct ac_texture_module * mod,
	const char * file_path,
	bgfx_texture_info_t * info)
{
	size_t size;
	void * data;
	texture_entry * e = find_entry(mod, file_path);
	bgfx_texture_handle_t handle;
	if (e) {
		if (info)
			memcpy(info, &e->info, sizeof(*info));
		return e->handle;
	}
	if (!get_file_size(file_path, &size) ||
		!size ||
		size > UINT32_MAX)
		return BGFX_INVALID_HANDLE;
	data = alloc(size);
	if (!load_file(file_path, data, size)) {
		dealloc(data);
		return BGFX_INVALID_HANDLE;
	}
	handle = load_internal(mod, file_path, data, size, info);
	dealloc(data);
	return handle;
}

bgfx_texture_handle_t ac_texture_load_packed(
	struct ac_texture_module * mod,
	enum ac_dat_directory dir,
	const char * file_name,
	boolean try_other_extensions,
	bgfx_texture_info_t * info)
{
	void * data;
	size_t size;
	char name[128];
	char file_path[1024];
	texture_entry * e;
	bgfx_texture_handle_t handle = BGFX_INVALID_HANDLE;
	const char * ext[] = { "tx1", "dds", "png" };
	const char * ext_options[3] = { NULL };
	uint32_t ext_count = 0;
	char fext[32] = "";
	uint32_t i;
	uint32_t ext_index = UINT32_MAX;
	if (dir < 0 || dir >= AC_DAT_DIR_COUNT) {
		dir = get_current_dir(mod);
		if (dir == AC_DAT_DIR_COUNT) {
			ERROR("Invalid directory.");
			return BGFX_INVALID_HANDLE;
		}
	}
	strlcpy(name, file_name, sizeof(name));
	strip_extension(name, fext, sizeof(fext));
	for (i = 0; i < COUNT_OF(ext); i++) {
		if (strcasecmp(fext, ext[i]) == 0) {
			ext_index = i;
			break;
		}
	}
	if (ext_index != UINT32_MAX)
		ext_options[ext_count++] = fext;
	if (try_other_extensions) {
		for (i = 0; i < COUNT_OF(ext); i++) {
			if (i != ext_index)
				ext_options[ext_count++] = ext[i];
		}
	}
	for (i = 0; i < ext_count; i++) {
		char name_with_ext[128];
		snprintf(name_with_ext, sizeof(name_with_ext), "%s.%s",
			name, ext_options[i]);
		make_path(file_path, sizeof(file_path), "%s/%s/%s",
			ap_config_get((struct ap_config_module *)mod->ap_config, "ClientDir"), 
			ac_dat_get_dir_path(dir), name_with_ext);
		lock_mutex(mod->mutex);
		e = find_entry(mod, file_path);
		if (e) {
			if (info)
				memcpy(info, &e->info, sizeof(*info));
			unlock_mutex(mod->mutex);
			return e->handle;
		}
		if (!ac_dat_load_resource((struct ac_dat_module *)mod->ac_dat, dir, 
				name_with_ext, &data, &size)) {
			unlock_mutex(mod->mutex);
			continue;
		}
		handle = load_internal(mod, file_path, data, size, info);
		unlock_mutex(mod->mutex);
		dealloc(data);
		if (BGFX_HANDLE_IS_VALID(handle))
			break;
	}
	return handle;
}

void ac_texture_set_dictionary(
	struct ac_texture_module * mod,
	enum ac_dat_directory dir)
{
	uint64_t id = get_current_thread_id();
	uint32_t i;
	struct thread_dir nd = { 0 };
	lock_mutex(mod->mutex);
	for (i = 0; i < vec_count(mod->current_dir); i++) {
		struct thread_dir * d = &mod->current_dir[i];
		if (d->thread_id == id) {
			d->current_dir = dir;
			unlock_mutex(mod->mutex);
			return;
		}
	}
	nd.thread_id = id;
	nd.current_dir = dir;
	vec_push_back((void **)&mod->current_dir, &nd);
	unlock_mutex(mod->mutex);
}

uint32_t ac_texture_copy(
	struct ac_texture_module * mod,
	bgfx_texture_handle_t tex)
{
	uint32_t i;
	texture_entry * e;
	texture_entry fe = { 0 };
	handle_name_pair * pair_ref = NULL;
	uint32_t c;
	if (!BGFX_HANDLE_IS_VALID(tex))
		return UINT32_MAX;
	lock_mutex(mod->mutex);
	for (i = 0; i < vec_count(mod->handle_name_pairs); i++) {
		if (mod->handle_name_pairs[i].handle.idx == tex.idx) {
			pair_ref = &mod->handle_name_pairs[i];
			break;
		}
	}
	if (!pair_ref) {
		unlock_mutex(mod->mutex);
		ERROR("Invalid handle value!");
		return UINT32_MAX;
	}
	fe.len = strlen(pair_ref->name);
	fe.name = (char *)pair_ref->name;
	fe.handle = tex;
	e = (texture_entry *)hmap_get(mod->tex_map,
		(const void *)&fe);
	if (!e) {
		ERROR("Texture entry was removed, but handle still exists (%s).",
			fe.name);
		unlock_mutex(mod->mutex);
		return UINT32_MAX;
	}
	c = e->refcount++;
	unlock_mutex(mod->mutex);
	return c;
}

void ac_texture_release(
	struct ac_texture_module * mod,
	bgfx_texture_handle_t tex)
{
	uint32_t i;
	texture_entry * e;
	texture_entry fe = { 0 };
	handle_name_pair * pair_ref = NULL;
	if (!BGFX_HANDLE_IS_VALID(tex))
		return;
	lock_mutex(mod->mutex);
	for (i = 0; i < vec_count(mod->handle_name_pairs); i++) {
		if (mod->handle_name_pairs[i].handle.idx == tex.idx) {
			pair_ref = &mod->handle_name_pairs[i];
			break;
		}
	}
	if (!pair_ref) {
		unlock_mutex(mod->mutex);
		ERROR("Invalid handle value!");
		return;
	}
	fe.len = strlen(pair_ref->name);
	fe.name = (char *)pair_ref->name;
	fe.handle = tex;
	e = (texture_entry *)hmap_get(mod->tex_map,
		(const void *)&fe);
	if (!e) {
		ERROR("Texture entry was removed, but handle still exists (%s).",
			fe.name);
		unlock_mutex(mod->mutex);
		return;
	}
	if (!--e->refcount) {
		e = (texture_entry *)hmap_delete(mod->tex_map, &fe);
		dealloc(e->name);
		bgfx_destroy_texture(tex);
		vec_erase(mod->handle_name_pairs, pair_ref);
	}
	unlock_mutex(mod->mutex);
}

void ac_texture_test(
	struct ac_texture_module * mod,
	bgfx_texture_handle_t tex)
{
	uint32_t i;
	texture_entry * e;
	texture_entry fe = { 0 };
	handle_name_pair * pair_ref = NULL;
	lock_mutex(mod->mutex);
	for (i = 0; i < vec_count(mod->handle_name_pairs); i++) {
		if (mod->handle_name_pairs[i].handle.idx == tex.idx) {
			pair_ref = &mod->handle_name_pairs[i];
			break;
		}
	}
	if (!pair_ref) {
		unlock_mutex(mod->mutex);
		ERROR("Invalid handle value!");
		return;
	}
	fe.len = strlen(pair_ref->name);
	fe.name = (char *)pair_ref->name;
	fe.handle = tex;
	e = (texture_entry *)hmap_get(mod->tex_map,
		(const void *)&fe);
	if (!e) {
		ERROR("Texture entry was removed, but handle still exists (%s).",
			fe.name);
		unlock_mutex(mod->mutex);
		return;
	}
	unlock_mutex(mod->mutex);
}

boolean ac_texture_get_name(
	struct ac_texture_module * mod,
	bgfx_texture_handle_t tex,
	boolean stripped,
	char * dst,
	size_t maxcount)
{
	uint32_t i;
	texture_entry * e;
	texture_entry fe = { 0 };
	handle_name_pair * pair_ref = NULL;
	boolean r;
	lock_mutex(mod->mutex);
	for (i = 0; i < vec_count(mod->handle_name_pairs); i++) {
		if (mod->handle_name_pairs[i].handle.idx == tex.idx) {
			pair_ref = &mod->handle_name_pairs[i];
			break;
		}
	}
	if (!pair_ref) {
		unlock_mutex(mod->mutex);
		ERROR("Invalid handle value!");
		return FALSE;
	}
	fe.len = strlen(pair_ref->name);
	fe.name = (char *)pair_ref->name;
	fe.handle = tex;
	e = (texture_entry *)hmap_get(mod->tex_map,
		(const void *)&fe);
	if (!e) {
		ERROR("Texture entry was removed, but handle still exists (%s).",
			fe.name);
		unlock_mutex(mod->mutex);
		return FALSE;
	}
	if (stripped)
		r = (strip_file_name(e->name, dst, maxcount) < maxcount);
	else
		r = (strlcpy(e->name, dst, maxcount) < maxcount);
	unlock_mutex(mod->mutex);
	return r;
}
