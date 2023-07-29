#include <assert.h>

#include "core/bin_stream.h"
#include "core/file_system.h"
#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"

#include "public/ap_octree.h"
#include "public/ap_sector.h"

static struct ap_octree_node * make_child(
	struct ap_octree_node * node,
	enum ap_octree_node_position pos)
{
	struct ap_octree_node * nw_node = alloc(sizeof(*nw_node));
	uint32_t nwlev = node->level + 1;
	uint32_t i;
	memset(nw_node, 0, sizeof(*nw_node));
	nw_node->parent = node;
	nw_node->hsize = node->hsize >> 1;
	nw_node->level = nwlev;
	for (i = 0; i < 8; i++)
		nw_node->child[i] = NULL;
	nw_node->has_child = FALSE;
	nw_node->id = node->id;
	nw_node->id |= ((pos & 0x7) << AP_OCTREE_INDEX_SHIFT[nwlev]);
	nw_node->id = ap_octree_set_depth(nw_node->id, nwlev);
	nw_node->id = ap_octree_set_is_leaf(nw_node->id, TRUE);
	nw_node->objectnum = 0;
	nw_node->bsphere.radius = 
		(float)nw_node->hsize * AP_OCTREE_SPHEREVAL;
	switch(pos) {
	case AP_OCTREE_TOP_LEFT_FRONT:
		nw_node->bsphere.center.x = node->bsphere.center.x - nw_node->hsize;
		nw_node->bsphere.center.y = node->bsphere.center.y + nw_node->hsize;
		nw_node->bsphere.center.z = node->bsphere.center.z + nw_node->hsize;
		break;

	case AP_OCTREE_TOP_LEFT_BACK:
		nw_node->bsphere.center.x = node->bsphere.center.x - nw_node->hsize;
		nw_node->bsphere.center.y = node->bsphere.center.y + nw_node->hsize;
		nw_node->bsphere.center.z = node->bsphere.center.z - nw_node->hsize;
		break;

	case AP_OCTREE_TOP_RIGHT_BACK:
		nw_node->bsphere.center.x = node->bsphere.center.x + nw_node->hsize;
		nw_node->bsphere.center.y = node->bsphere.center.y + nw_node->hsize;
		nw_node->bsphere.center.z = node->bsphere.center.z - nw_node->hsize;
		break;

	case AP_OCTREE_TOP_RIGHT_FRONT:
		nw_node->bsphere.center.x = node->bsphere.center.x + nw_node->hsize;
		nw_node->bsphere.center.y = node->bsphere.center.y + nw_node->hsize;
		nw_node->bsphere.center.z = node->bsphere.center.z + nw_node->hsize;
		break;

	case AP_OCTREE_BOTTOM_LEFT_FRONT:
		nw_node->bsphere.center.x = node->bsphere.center.x - nw_node->hsize;
		nw_node->bsphere.center.y = node->bsphere.center.y - nw_node->hsize;
		nw_node->bsphere.center.z = node->bsphere.center.z + nw_node->hsize;
		break;

	case AP_OCTREE_BOTTOM_LEFT_BACK:
		nw_node->bsphere.center.x = node->bsphere.center.x - nw_node->hsize;
		nw_node->bsphere.center.y = node->bsphere.center.y - nw_node->hsize;
		nw_node->bsphere.center.z = node->bsphere.center.z - nw_node->hsize;
		break;

	case AP_OCTREE_BOTTOM_RIGHT_BACK:
		nw_node->bsphere.center.x = node->bsphere.center.x + nw_node->hsize;
		nw_node->bsphere.center.y = node->bsphere.center.y - nw_node->hsize;
		nw_node->bsphere.center.z = node->bsphere.center.z - nw_node->hsize;
		break;

	case AP_OCTREE_BOTTOM_RIGHT_FRONT:
		nw_node->bsphere.center.x = node->bsphere.center.x + nw_node->hsize;
		nw_node->bsphere.center.y = node->bsphere.center.y - nw_node->hsize;
		nw_node->bsphere.center.z = node->bsphere.center.z + nw_node->hsize;
		break;
	}

	//m_csCSection.Unlock();

	return nw_node;
}

struct ap_octree_node * ap_octree_create_root(
	uint32_t sector_x,
	uint32_t sector_z,
	float y,
	struct ap_octree_root ** root)
{
	struct ap_octree_node * node;
	struct ap_octree_root_list * list;
	struct ap_octree_root * r = *root;
	float cy = y;
	float min_height = AP_SECTOR_MIN_HEIGHT;
	float min_height_offset = (AP_SECTOR_WIDTH / 2.0f) * 0.95f;
	if (sector_x >= AP_SECTOR_WORLD_INDEX_WIDTH || 
		sector_z >= AP_SECTOR_WORLD_INDEX_HEIGHT) {
		return NULL;
	}
	node = alloc(sizeof(*node));
	memset(node, 0, sizeof(*node));
	list = alloc(sizeof(*list));
	list->node = node;
	list->next = NULL;
	if (cy - min_height_offset < min_height) 
		cy = min_height + min_height_offset;
	if (!r) {
		list->root_index = 0;
		r = alloc(sizeof(*r));
		memset(r, 0, sizeof(*r));
		r->rootnum = 1;
		r->sector_x = sector_x;
		r->sector_z = sector_z;
		r->center_x = (ap_scr_get_start_x(sector_x) + 
				ap_scr_get_end_x(sector_x)) / 2.0f;
		r->roots = list;
	}
	else {
		boolean found = FALSE;
		struct ap_octree_root_list * cur = r->roots;
		while (cur) {
			struct au_pos pos = { r->center_x, cy, r->center_z };
			if (ap_octree_test_box_pos(&pos, 
					&cur->node->bsphere.center, 
					(float)cur->node->hsize)) {
				found = TRUE;
				break;
			}
			cur = cur->next;
		}
		if (!found) {
			if (r->rootnum < AP_OCTREE_MAX_ROOT_COUNT) {
				struct ap_octree_root_list * find_end = r->roots;
				list->root_index = r->rootnum;
				while (find_end && find_end->next) {
					find_end = find_end->next;
				}
				if (find_end)
					find_end->next = list;
				else
					r->roots = list;
				r->rootnum++;
				cur = r->roots;
				while (cur && cur->next) {
					if (cur->node->bsphere.center.y - 
							cur->node->hsize > cy + 
								cur->node->hsize &&
						cy > cur->node->bsphere.center.y - 
							cur->node->hsize * 2) {
						cy = cur->node->bsphere.center.y - 
							cur->node->hsize * 2;
					}
					else if (cur->node->bsphere.center.y + 
								cur->node->hsize > cy - 
									cur->node->hsize &&
							cy < cur->node->bsphere.center.y + 
								cur->node->hsize * 2) {
							cy = cur->node->bsphere.center.y + 
								cur->node->hsize * 2;
						}
					cur = cur->next;
				}
			}
			else {
				dealloc(list);
				dealloc(node);
				return	NULL;
			}
		}
	}
	node->has_child = FALSE;
	node->hsize = (uint32_t)(AP_SECTOR_WIDTH / 2.0f);
	node->bsphere.center.x = r->center_x;
	node->bsphere.center.y = cy;
	node->bsphere.center.z = r->center_z;
	node->bsphere.radius = node->hsize * AP_OCTREE_SPHEREVAL;
	node->level = 0;
	node->objectnum = 0;
	node->parent = NULL;
	node->id = ap_octree_set_root_index(0x00000000, r->rootnum - 1);
	node->id = ap_octree_set_depth(node->id, 0);
	node->id = ap_octree_set_is_leaf(node->id, TRUE);
	for (int i=0; i < 8; ++i)
		node->child[i] = NULL;
	*root = r;
	return node;
}

void ap_octree_divide_node(struct ap_octree_node * node)
{
	uint32_t i;
	if (node->has_child || node->level >= AP_OCTREE_MAXDEPTH)
		return;
	node->has_child = TRUE;
	node->id = ap_octree_set_is_leaf(node->id, FALSE);
	for (i = 0; i < 8; i++)
		node->child[i] = make_child(node, i);
}

void ap_octree_combine_node_children(struct ap_octree_node * node)
{
	uint32_t i;
	if (node->level >= AP_OCTREE_MAXDEPTH || !node->has_child )
		return;
	node->has_child = FALSE;
	node->id = ap_octree_set_is_leaf(node->id, TRUE);
	for (i = 0; i < 8; i++) {
		if (node->child[i]) {
			ap_octree_combine_node_children(node->child[i]);
			dealloc(node->child[i]);
			node->child[i] = NULL;
		}
	}
}

/*

struct ap_octree_root * ap_octree_get_root(
	uint32_t sector_x,
	uint32_t sector_z)
{
	if (sector_x >= AP_SECTOR_WORLD_INDEX_WIDTH || 
		sector_z >= AP_SECTOR_WORLD_INDEX_HEIGHT) {
		return NULL;
	}
	return m_pOcTreeRoots[sector_x][sector_z];

}

struct ap_octree_root * ap_octree_get_root_by_world_pos(
	float world_x, 
	float world_z, 
	boolean load)
{
	uint32_t six;
	uint32_t siz;
	float pos[3] = { world_x, 0.0f, world_z };
	load = FALSE;
	if (!ap_scr_pos_to_index(pos, &six, &siz))
		return NULL;
	if (!m_pOcTreeRoots[six][siz] && load)
		m_pOcTreeRoots[six][siz] = ap_octree_load_from_file(six,siz);
	return m_pOcTreeRoots[six][siz];
}

boolean ap_octree_set_root(
	uint32_t sector_x, 
	uint32_t sector_z, 
	struct ap_octree_root * root)
{
	if (sector_x >= AP_SECTOR_WORLD_INDEX_WIDTH || 
		sector_z >= AP_SECTOR_WORLD_INDEX_HEIGHT) {
		return FALSE;
	}
	if (m_pOcTreeRoots[sector_x][sector_z] && 
		m_pOcTreeRoots[sector_x][sector_z] != root) {
		return FALSE;
	}
	m_pOcTreeRoots[sector_x][sector_z] = root;
	return TRUE;
}
*/

struct ap_octree_node * ap_octree_get_node(
	struct ap_octree_root * root,
	uint32_t id)
{
	uint32_t root_index = ap_octree_calc_root_index(id);
	struct ap_octree_root_list * cur = root->roots;
	while (cur) {
		if (cur->root_index == root_index)
			return ap_octree_find_node(cur->node, id);
		cur = cur->next;
	}
	return NULL;
}

struct ap_octree_node * ap_octree_find_node(
	struct ap_octree_node * head,
	uint32_t id)
{
	struct ap_octree_node * res_node = head;
	uint32_t depth = ap_octree_calc_depth(id);
	uint32_t i;
	for (i = 0; i < depth; ++i) {
		if(res_node) {
			uint32_t index = ap_octree_calc_index(id, i + 1);
			res_node = res_node->child[index];
		}
		else {
			return NULL;
		}
	}
	return res_node;
}

struct ap_octree_node * ap_octree_get_node_by_pos(
	struct ap_octree_root * root,
	struct au_pos * pos)
{
	if (root->rootnum == 1) {
		return ap_octree_find_node_by_pos(root->roots->node, pos);
	}
	else {
		struct ap_octree_root_list * cur = root->roots;
		uint32_t i;
		for (i = 0; root->rootnum; i++) {
			if (pos->y < cur->node->bsphere.center.y + 
					cur->node->hsize && 
				pos->y >= cur->node->bsphere.center.y - 
					cur->node->hsize) {
				return ap_octree_find_node_by_pos(cur->node, pos);
			}
			cur = cur->next;
		}
	}
	return NULL;
}

struct ap_octree_node * ap_octree_find_node_by_pos(
	struct ap_octree_node * head,
	struct au_pos * pos)
{
	struct au_pos center;
	float size;
	if (!head->has_child) {
		return head;
	}
	else {
		uint32_t i;
		for (i = 0; i < 8; i++) {
			center = head->child[i]->bsphere.center;
			size = (float)head->child[i]->hsize;
			if (pos->x < center.x + size && 
				pos->x >= center.x - size &&
				pos->y < center.y + size && 
				pos->y >= center.y - size &&
				pos->z < center.z + size && 
				pos->z >= center.z - size) {
				return ap_octree_find_node_by_pos(head->child[i], pos);
			}
		}
	}
	return NULL;
}

struct ap_octree_node * ap_octree_get_head_node(
	struct ap_octree_root * root , 
	uint32_t root_index)
{
	struct ap_octree_root_list * cur;
	uint32_t i;
	if (!root)
		return NULL;
	cur = root->roots;
	for (i = 0; i < root_index; i++) {
		if (cur)
			cur = cur->next;
		else
			return NULL;
	}
	if (cur)
		return cur->node;
	else
		return NULL;
}

struct ap_octree_node * ap_octree_get_head_node_by_pos(
	struct ap_octree_root * root, 
	struct au_pos * pos)
{
	struct ap_octree_root_list * cur;
	struct au_pos center;
	float size;
	cur = root->roots;
	while (cur) {
		if (cur->node) {
			center = cur->node->bsphere.center;
			size = (float)cur->node->hsize;  
			if (pos->x < center.x + size && 
				pos->x >= center.x - size &&
				pos->y < center.y + size && 
				pos->y >= center.y - size &&
				pos->z < center.z + size && 
				pos->z >= center.z - size) {
				return cur->node;
			}
		}
		cur = cur->next;
	}
	return NULL;
}

struct ap_octree_node * ap_octree_find_head_node(
	struct ap_octree_node * node)
{
	struct ap_octree_node * cur = node;
	if (cur) {
		while (cur->parent)
			cur = cur->parent;
		return cur;
	}
	return 	NULL;
}

uint32_t ap_octree_get_node_id(struct ap_octree_node * node)
{
	return node->id;
}

struct ap_octree_node * ap_octree_get_node_for_insert(
	struct ap_octree_root * root, 
	uint32_t sector_x,
	uint32_t sector_z,
	uint32_t octree_id,
	boolean divide)
{
	uint32_t root_index;
	struct ap_octree_node * node;
	uint32_t depth;
	uint32_t i;
	if (!root) {
		//if(bLoad)
		//{
		//	root = LoadFromFiles(six,siz);
		//	if(!root)
		//		return NULL;
		//}
		//else
		//{
		//	return NULL;
		//}
		return NULL;
	}
	root_index = ap_octree_calc_root_index(octree_id);
	node = ap_octree_get_head_node(root, root_index);
	if (!node) 
		return NULL;
	depth = ap_octree_calc_depth(octree_id);
	for (i = 0; i < depth; i++) {
		uint32_t index = ap_octree_calc_index(octree_id, i + 1);
		if (!node) {
			return NULL;
		}
		else if (!node->child[index]) {
			if (divide) {
				ap_octree_divide_node(node);
				node = node->child[index];
			}
			else {
				return NULL;
			}
		}
		else {
			node = node->child[index];
		}
	}
	return node;
}

boolean ap_octree_test_box_pos(
	struct au_pos * world_pos,
	struct au_pos * box_center,
	float box_size)
{
	if (world_pos->x < box_center->x + box_size && 
		world_pos->x >= box_center->x - box_size &&
		world_pos->y < box_center->y + box_size && 
		world_pos->y >= box_center->y - box_size &&
		world_pos->z < box_center->z + box_size && 
		world_pos->z >= box_center->z - box_size) {
		return TRUE;
	}
	return FALSE;
}

void ap_octree_destroy_tree(struct ap_octree_root * root)
{
	struct ap_octree_root_list * cur;
	if (!root)
		return;
	cur = root->roots;
	while (cur) {
		struct ap_octree_root_list * next = cur->next;
		ap_octree_destroy_children(cur->node);
		dealloc(cur);
		cur = next;
	}
	root->rootnum = 0;
	root->roots = NULL;
	dealloc(root);
}

void ap_octree_destroy_children(struct ap_octree_node * node)
{
	uint32_t i;
	for (i = 0; i < 8; i++) {
		if (node->child[i])
			ap_octree_destroy_children(node->child[i]);
	}
	dealloc(node);
}

/*
void ap_octree_destroy_all()
{
	uint32_t x;
	for (x = 0; x < AP_SECTOR_WORLD_INDEX_WIDTH; x++) {
		uint32_t z;
		for (z = 0; z < AP_SECTOR_WORLD_INDEX_HEIGHT; z++) {
			if (m_pOcTreeRoots[x][z]) {
				ap_octree_destroy_tree(m_pOcTreeRoots[x][z]);
				m_pOcTreeRoots[x][z] = NULL;
			}
		}
	}
}
*/

void ap_octree_divide_all_trees(struct ap_octree_root * root)
{
	struct ap_octree_root_list * cur = root->roots;
	uint32_t i;
	for (i = 0; i < root->rootnum; i++) {
		ap_octree_divide_all_children(cur->node);
		cur = cur->next;
	}
}

void ap_octree_divide_all_children(struct ap_octree_node * node)
{
	uint32_t i;
	if (node->level >= AP_OCTREE_MAXDEPTH)
		return;
	node->has_child = TRUE;
	node->id = ap_octree_set_is_leaf(node->id, FALSE);
	for (i = 0; i < 8; i++) {
		if (!node->child[i])
			node->child[i] = make_child(node, i);
	}
	for (i = 0; i < 8; i++)
		ap_octree_divide_all_children(node->child[i]);
}

void ap_octree_optimize_tree(
	struct ap_octree_root * root,
	uint32_t sector_x, 
	uint32_t sector_z)
{
	struct ap_octree_root_list * cur;
	uint32_t i;
	if (!root)
		return;
	cur = root->roots;
	for (i = 0; i < root->rootnum; i++) {
		ap_octree_optimize_child(cur->node);
		cur = cur->next;
	}
}

uint16_t ap_octree_optimize_child(struct ap_octree_node * node)
{
	if (node->has_child) {
		boolean unite = TRUE;
		uint32_t i;
		for (i = 0; i < 8; i++) {
			if (ap_octree_optimize_child(node->child[i]))
				unite = FALSE;
		}
		if (unite) {
			node->has_child = FALSE;
			for (i = 0; i < 8; i++) {
				/* Children of the child node should have been 
				 * recursively removed, make sure that that is 
				 * the case. */
				assert(node->child[i]->has_child == FALSE);
				dealloc(node->child[i]);
				node->child[i] = NULL;
			}
			node->id = ap_octree_set_is_leaf(node->id, TRUE);
		}
		return node->objectnum;	
	}
	else {
		return node->objectnum;
	}
}

uint32_t ap_octree_get_leaf_id(
	struct ap_octree_root * root,
	struct au_pos * pos)
{
	struct ap_octree_node * head_node = 
		ap_octree_get_head_node_by_pos(root, pos);
	uint32_t res_id;
	struct au_sphere bs;
	struct au_sphere calc_bs = { 0 };
	float dist;
	uint32_t i;
	if (!head_node)
		return 0xffffffff;
	res_id = head_node->id;
	res_id = ap_octree_set_depth(res_id, AP_OCTREE_MAXDEPTH);
	res_id = ap_octree_set_is_leaf(res_id, TRUE);
	bs = head_node->bsphere;
	dist = (float)(head_node->hsize >> 1);
	for (i = 0; i < AP_OCTREE_MAXDEPTH; i++) {
		uint32_t j;
		for (j = 0; j < 8; j++) {
			switch (j) {
			case AP_OCTREE_TOP_LEFT_FRONT:
				calc_bs.center.x = bs.center.x - dist;calc_bs.center.y = bs.center.y + dist;calc_bs.center.z = bs.center.z + dist;
				break;
			case AP_OCTREE_TOP_LEFT_BACK:
				calc_bs.center.x = bs.center.x - dist;calc_bs.center.y = bs.center.y + dist;calc_bs.center.z = bs.center.z - dist;
				break;
			case AP_OCTREE_TOP_RIGHT_BACK:
				calc_bs.center.x = bs.center.x + dist;calc_bs.center.y = bs.center.y + dist;calc_bs.center.z = bs.center.z - dist;
				break;
			case AP_OCTREE_TOP_RIGHT_FRONT:
				calc_bs.center.x = bs.center.x + dist;calc_bs.center.y = bs.center.y + dist;calc_bs.center.z = bs.center.z + dist;
				break;
			case AP_OCTREE_BOTTOM_LEFT_FRONT:
				calc_bs.center.x = bs.center.x - dist;calc_bs.center.y = bs.center.y - dist;calc_bs.center.z = bs.center.z + dist;
				break;
			case AP_OCTREE_BOTTOM_LEFT_BACK:
				calc_bs.center.x = bs.center.x - dist;calc_bs.center.y = bs.center.y - dist;calc_bs.center.z = bs.center.z - dist;
				break;
			case AP_OCTREE_BOTTOM_RIGHT_BACK:
				calc_bs.center.x = bs.center.x + dist;calc_bs.center.y = bs.center.y - dist;calc_bs.center.z = bs.center.z - dist;
				break;
			case AP_OCTREE_BOTTOM_RIGHT_FRONT:
				calc_bs.center.x = bs.center.x + dist;calc_bs.center.y = bs.center.y - dist;calc_bs.center.z = bs.center.z + dist;
				break;
			}
			if (pos->x >= calc_bs.center.x - dist &&
				pos->x < calc_bs.center.x + dist &&
				pos->y >= calc_bs.center.y - dist &&
				pos->y < calc_bs.center.y + dist &&
				pos->z >= calc_bs.center.z - dist &&
				pos->z < calc_bs.center.z + dist ) {
				bs = calc_bs;
				res_id |= ((j & 0x7) << AP_OCTREE_INDEX_SHIFT[i + 1]);
				break;
			}
		}
		if (j == 8)
			return 0xffffffff;
		dist *= 0.5f;
	}
	return res_id;
}

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
	uint32_t sector_z)
{
	uint32_t six = sector_x / AP_SECTOR_DEFAULT_DEPTH * 
		AP_SECTOR_DEFAULT_DEPTH;
	uint32_t siz = sector_z / AP_SECTOR_DEFAULT_DEPTH * 
		AP_SECTOR_DEFAULT_DEPTH;
	uint32_t offx = sector_x - six;
	uint32_t offz = sector_z - siz;
	uint32_t division_index = (siz / 16) + 100 * (six / 16);
	void * data;
	size_t size;
	struct bin_stream * stream;
	uint32_t foffset = 0;
	uint32_t load_size = 0;
	uint32_t version = 0;
	uint32_t * load_buf;
	uint32_t load_index;
	uint32_t root_num;
	float cy;
	struct ap_octree_node * start_node;
	struct ap_octree_root * root;
	if (!get_file_size(file_path, &size) || !size)
		return NULL;
	data = alloc(size);
	if (!load_file(file_path, data, size)) {
		dealloc(data);
		return NULL;
	}
	bstream_from_buffer(data, size, TRUE, &stream);
	foffset = 0;
	load_size = 0;
	version = 0;
	if (!bstream_read_u32(stream, &version)) {
		ERROR("Failed to read file version.");
		bstream_destroy(stream);
		return NULL;
	}
	if (version != 1) {
		ERROR("Invalid file version.");
		bstream_destroy(stream);
		return NULL;
	}
	foffset = offz * (AP_SECTOR_DEFAULT_DEPTH << 3) + 
		(offx << 3) + 4;
	if (!bstream_seek(stream, foffset) ||
		!bstream_read_u32(stream, &foffset) ||
		!bstream_read_u32(stream, &load_size)) {
		ERROR("Stream ended unexpectedly.");
		bstream_destroy(stream);
		return NULL;
	}
	if (!load_size) {
		bstream_destroy(stream);
		return NULL;
	}
	load_buf = alloc(load_size);
	load_index = 0;
	root_num = 0;
	if (!bstream_seek(stream, foffset) ||
		!bstream_read(stream, load_buf, load_size)) {
		ERROR("Stream ended unexpectedly.");
		dealloc(load_buf);
		bstream_destroy(stream);
		return NULL;
	}
	root_num = load_buf[load_index++];
	root = NULL;
	for (uint32_t i = 0; i < root_num; i++) {
		// center y load
		//ReadFile(fd,&cy ,sizeof(float),&FP,NULL);
		cy = ((float *)load_buf)[load_index++];
		start_node =
			ap_octree_create_root(sector_x, sector_z, cy, &root);
		assert(i < AP_OCTREE_MAX_ROOT_COUNT);
		if (start_node) {
			ap_octree_load_node(start_node, load_buf, &load_index,
				(uint32_t *)((uintptr_t)load_buf + load_size));
		}
		else {
			assert(0);
			ERROR("Failed to create start_node.");
			break;
		}
	}
	dealloc(load_buf);
	bstream_destroy(stream);
	return root;
}

struct ap_octree_root * ap_octree_load_from_batch(
	struct ap_octree_batch * batch,
	uint32_t sector_x,
	uint32_t sector_z)
{
	struct bin_stream * stream = batch->stream;
	uint32_t x = sector_x - (sector_x / 
		AP_SECTOR_DEFAULT_DEPTH * AP_SECTOR_DEFAULT_DEPTH);
	uint32_t z = sector_z - (sector_z / 
		AP_SECTOR_DEFAULT_DEPTH * AP_SECTOR_DEFAULT_DEPTH);
	uint32_t load_index;
	uint32_t * load_buf;
	uint32_t root_num;
	struct ap_octree_node * start_node;
	struct ap_octree_root * root;
	float cy;
	assert(x < AP_SECTOR_DEFAULT_DEPTH);
	assert(z < AP_SECTOR_DEFAULT_DEPTH);
	if (!batch->size[x][z])
		return NULL;
	if (batch->size[x][z] > batch->buf_size) {
		batch->buf = reallocate(batch->buf, batch->size[x][z]);
		batch->buf_size = batch->size[x][z];
	}
	load_index = 0;
	root_num = 0;
	if (!bstream_seek(stream, batch->offset[x][z]) ||
		!bstream_read(stream, batch->buf, batch->size[x][z])) {
		ERROR("Stream ended unexpectedly.");
		return NULL;
	}
	load_buf = batch->buf;
	root_num = load_buf[load_index++];
	root = NULL;
	for (uint32_t i = 0; i < root_num; i++) {
		cy = ((float *)load_buf)[load_index++];
		start_node =
			ap_octree_create_root(sector_x, sector_z, cy, &root);
		assert(i < AP_OCTREE_MAX_ROOT_COUNT);
		if (start_node) {
			ap_octree_load_node(start_node, load_buf, &load_index,
				(uint32_t *)((uintptr_t)load_buf + 
					batch->size[x][z]));
		}
		else {
			assert(0);
			ERROR("Failed to create start_node.");
			break;
		}
	}
	return root;
}

void ap_octree_load_node(
	struct ap_octree_node * node,
	uint32_t * load_buffer,
	uint32_t * load_index,
	uint32_t * load_index_end)
{
	assert(NULL != node);
	assert(NULL != load_buffer);
	assert(NULL != load_index);
	if (load_index_end < load_index + 9) {
		assert(0);
		return;
	}
	node->id = ((uint32_t *)load_buffer)[(*load_index)++];
	node->objectnum = ((uint32_t *)load_buffer)[(*load_index)++];
	node->has_child = (boolean)((uint32_t *)load_buffer)[(*load_index)++];
	node->bsphere.radius = ((float *)load_buffer)[(*load_index)++];
	node->bsphere.center.x = ((float *)load_buffer)[(*load_index)++];
	node->bsphere.center.y = ((float *)load_buffer)[(*load_index)++];
	node->bsphere.center.z = ((float *)load_buffer)[(*load_index)++];
	node->hsize = ((uint32_t *)load_buffer)[(*load_index)++];
	node->level = ((uint32_t *)load_buffer)[(*load_index)++];
	if (node->has_child) {
		uint32_t i;
		for (i = 0; i < 8; i++) {
			if (!node->child[i]) {
				uint32_t j;
				node->child[i] = alloc(sizeof(*node));
				memset(node->child[i], 0, sizeof(*node));
				node->child[i]->parent = node;
				for (j = 0; j < 8; j++)
					node->child[i]->child[j] = NULL;
			}
			if (node != node->child[i]) {
				ap_octree_load_node(node->child[i], load_buffer, 
					load_index, load_index_end);
			}
		}
	}
}

struct ap_octree_batch * ap_octree_create_read_batch(
	void * data,
	size_t size,
	uint32_t map_x, 
	uint32_t map_z)
{
	uint32_t division_index = map_z + 100 * map_x;
	struct bin_stream * stream;
	uint32_t foffset = 0;
	uint32_t load_size = 0;
	uint32_t version = 0;
	uint32_t z;
	struct ap_octree_batch * batch;
	bstream_from_buffer(data, size, FALSE, &stream);
	foffset = 0;
	load_size = 0;
	version = 0;
	if (!bstream_read_u32(stream, &version)) {
		ERROR("Failed to read file version.");
		bstream_destroy(stream);
		return NULL;
	}
	if (version != 1) {
		ERROR("Invalid file version.");
		bstream_destroy(stream);
		return NULL;
	}
	batch = alloc(sizeof(*batch));
	memset(batch, 0, sizeof(*batch));
	batch->stream = stream;
	for (z = 0; z < AP_SECTOR_DEFAULT_DEPTH; z++) {
		uint32_t x;
		for (x = 0; x < AP_SECTOR_DEFAULT_DEPTH; x++) {
			if (!bstream_read_u32(stream, &batch->offset[x][z]) ||
				!bstream_read_u32(stream, &batch->size[x][z])) {
				ERROR("Failed to read tree index.");
				dealloc(batch);
				bstream_destroy(stream);
				return NULL;
			}
		}
	}
	return batch;
}

void ap_octree_destroy_batch(struct ap_octree_batch * batch)
{
	if (batch->stream)
		bstream_destroy(batch->stream);
	dealloc(batch->buf);
	dealloc(batch);
}
