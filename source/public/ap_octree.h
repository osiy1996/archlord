#ifndef _AP_OCTREE_H_
#define _AP_OCTREE_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_define.h"
#include "public/ap_sector.h"

#define AP_OCTREE_SPHEREVAL 1.75f
#define AP_OCTREE_MAXDEPTH 2
#define AP_OCTREE_MAX_ROOT_COUNT 4
#define AP_OCTREE_ROOT_STATUS_EMPTY 0
#define AP_OCTREE_ROOT_STATUS_LOADING 1
#define AP_OCTREE_ROOT_STATUS_LOADED 2
#define AP_OCTREE_ROOT_STATUS_REMOVED 3

static uint32_t AP_OCTREE_INDEX_MASK[] = {
	0x00000000, 0x000001C0, 0x00000E00, 0x00007000,
	0x00038000, 0x001C0000, 0x00E00000, 0x07000000 };

static uint32_t AP_OCTREE_INDEX_SHIFT[] = {
	0, 6, 9, 12, 15, 18, 21, 24 };

BEGIN_DECLS

struct bin_stream;

enum ap_octree_data_index {
	AP_OCTREE_NODE_DATA,
	AP_OCTREE_ROOT_DATA
};

enum ap_octree_node_position {
	AP_OCTREE_TOP_LEFT_BACK,
	AP_OCTREE_TOP_LEFT_FRONT,
	AP_OCTREE_TOP_RIGHT_BACK,
	AP_OCTREE_TOP_RIGHT_FRONT,
	AP_OCTREE_BOTTOM_LEFT_BACK,
	AP_OCTREE_BOTTOM_LEFT_FRONT,
	AP_OCTREE_BOTTOM_RIGHT_BACK,
	AP_OCTREE_BOTTOM_RIGHT_FRONT
};

struct ap_octree_node {
	uint32_t id;
	uint16_t objectnum;
	boolean has_child;
	uint8_t level;
	uint32_t hsize;
	struct ap_octree_node * parent;
	struct ap_octree_node * child[8];
	struct au_sphere bsphere;
};

struct ap_octree_root_list {
	struct ap_octree_node * node;
	int32_t root_index;
	struct ap_octree_root_list * next;
};

struct ap_octree_root {
	uint8_t sector_x;
	uint8_t sector_z;
	uint8_t rootnum;
	uint8_t pad;
	float center_x;
	float center_z;
	struct ap_octree_root_list * roots;
};

struct ap_octree_id_list {
	int16_t six;
	int16_t siz;
	uint32_t id;
	struct ap_octree_node * added_node;
	void * added_clump_list_node;
	struct ap_octree_root * added_root;
	struct ap_octree_id_list * next;
};

struct ap_octree_custom_id {
	int16_t six;
	int16_t siz;
	uint32_t id;
	struct ap_octree_node * added_node;
	void * added_clump_list_node;
	struct ap_octree_root * added_root;
};

struct ap_octree_batch {
	struct bin_stream * stream;
	uint32_t offset[AP_SECTOR_DEFAULT_DEPTH][AP_SECTOR_DEFAULT_DEPTH];
	uint32_t size[AP_SECTOR_DEFAULT_DEPTH][AP_SECTOR_DEFAULT_DEPTH];
	void * buf;
	uint32_t buf_size;
};

struct ap_octree_node * ap_octree_create_root(
	uint32_t sector_x,
	uint32_t sector_z,
	float center_y,
	struct ap_octree_root ** root);

void ap_octree_divide_node(struct ap_octree_node * node);

void ap_octree_combine_node_children(struct ap_octree_node * node);

/*
struct ap_octree_root * ap_octree_get_root(
	uint32_t sector_x,
	uint32_t sector_z);

struct ap_octree_root * ap_octree_get_root_by_world_pos(
	float world_x, 
	float world_z, 
	boolean load);

boolean ap_octree_set_root(
	uint32_t sector_x, 
	uint32_t sector_z, 
	struct ap_octree_root * root);
*/

struct ap_octree_node * ap_octree_get_node(
	struct ap_octree_root * root,
	uint32_t id);

struct ap_octree_node * ap_octree_find_node(
	struct ap_octree_node * head,
	uint32_t id);

struct ap_octree_node * ap_octree_get_node_by_pos(
	struct ap_octree_root * root,
	struct au_pos * pos);

struct ap_octree_node * ap_octree_find_node_by_pos(
	struct ap_octree_node * head,
	struct au_pos * pos);

struct ap_octree_node * ap_octree_get_head_node(
	struct ap_octree_root * root, 
	uint32_t root_index);

struct ap_octree_node * ap_octree_get_head_node_by_pos(
	struct ap_octree_root * root, 
	struct au_pos * pos);

struct ap_octree_node * ap_octree_find_head_node(
	struct ap_octree_node * node);

uint32_t ap_octree_get_node_id(struct ap_octree_node * node);

struct ap_octree_node * ap_octree_get_node_for_insert(
	struct ap_octree_root * root, 
	uint32_t sector_x,
	uint32_t sector_z,
	uint32_t octree_id,
	boolean divide);

boolean ap_octree_test_box_pos(
	struct au_pos * world_pos,
	struct au_pos * box_center,
	float box_size);

void ap_octree_destroy_tree(struct ap_octree_root * root);

void ap_octree_destroy_children(struct ap_octree_node * node);

void ap_octree_destroy_all();

void ap_octree_divide_all_trees(struct ap_octree_root * root);

void ap_octree_divide_all_children(struct ap_octree_node * node);

void ap_octree_optimize_tree(
	struct ap_octree_root * root,
	uint32_t sector_x, 
	uint32_t sector_z);

uint16_t ap_octree_optimize_child(struct ap_octree_node * node);

uint32_t ap_octree_get_leaf_id(
	struct ap_octree_root * root,
	struct au_pos * pos);

boolean ap_octree_save_to_files(
	const char * dir,
	uint32_t load_x1,
	uint32_t load_x2,
	uint32_t load_z1,
	uint32_t load_z2);

//void SaveNode(struct ap_octree_node * node,int32_t* Foffset,HANDLE fd,uint32_t* FP);

struct ap_octree_root * ap_octree_load_from_file(
	const char * file_path,
	uint32_t sector_x,
	uint32_t sector_z);

struct ap_octree_root * ap_octree_load_from_batch(
	struct ap_octree_batch * batch,
	uint32_t sector_x,
	uint32_t sector_z);

void ap_octree_load_node(
	struct ap_octree_node * node,
	uint32_t * load_buffer,
	uint32_t * load_index,
	uint32_t * load_index_end);

struct ap_octree_batch * ap_octree_create_read_batch(
	void * data,
	size_t size,
	uint32_t map_x, 
	uint32_t map_z);

void ap_octree_destroy_batch(struct ap_octree_batch * batch);

//boolean SetCallbackInitRoot ( ApModuleDefaultCallBack pfCallback, void * pClass );
//static boolean CB_LoadSector ( void * pData, void * pClass, void * pCustData );
//static boolean CB_ClearSector ( void * pData, void * pClass, void * pCustData );

inline uint32_t ap_octree_calc_root_index(uint32_t id)
{
	return (id & 0x3);
}

inline uint32_t ap_octree_set_root_index(
	uint32_t id,
	uint32_t root_index)
{
	return ((id & 0xfffffffc) | (root_index & 0x3));
}

inline uint32_t ap_octree_calc_depth(uint32_t id)
{
	return ((id & 0x38) >> 3);
}

inline uint32_t ap_octree_set_depth(uint32_t id,uint32_t depth)
{
	return ((id & 0xffffffc7) | ((depth & 0x7) << 3));
}

inline uint32_t ap_octree_set_is_leaf(uint32_t id, boolean is_leaf)
{
	return (id | ((is_leaf & 0x1) << 2));
}

// 1~7
inline uint32_t ap_octree_calc_index(
	uint32_t id,
	int32_t lev)
{
	return 
		((id & AP_OCTREE_INDEX_MASK[lev]) >> AP_OCTREE_INDEX_SHIFT[lev]);
}

END_DECLS

#endif /* _AP_OCTREE_H_ */
