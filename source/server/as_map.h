#ifndef _AS_MAP_H_
#define _AS_MAP_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_define.h"
#include "public/ap_item.h"
#include "public/ap_map.h"
#include "public/ap_module.h"
#include "public/ap_object.h"
#include "public/ap_sector.h"

#define AS_MAP_MODULE_NAME "AgsmMap"

BEGIN_DECLS

struct ap_character;

enum as_map_callback_id {
	AS_MAP_CB_ENTER_SECTOR,
	AS_MAP_CB_LEAVE_SECTOR,
	/** \brief 
	 * Different from AS_MAP_CB_ENTER_SECTOR, this callback 
	 * is triggered when a sector becomes visible. */
	AS_MAP_CB_ENTER_SECTOR_VIEW,
	/** \brief 
	 * Different from AS_MAP_CB_LEAVE_SECTOR, this callback 
	 * is triggered when a sector is no longer visible. */
	AS_MAP_CB_LEAVE_SECTOR_VIEW,
};

enum as_map_tile_block {
	AS_MAP_TB_NONE = 0,
	AS_MAP_TB_GEOMETRY = 1u << 0,
	AS_MAP_TB_OBJECT = 1u << 1,
	AS_MAP_TB_SKY = 1u << 2,
};

enum as_map_geometry_block {
	AS_MAP_GB_NONE = 0,
	AS_MAP_GB_GROUND = 1u << 0,
	AS_MAP_GB_SKY = 1u << 1,
};

enum as_map_module_data_index {
	AS_MAP_MDI_SECTOR,
};

struct as_map_tile_info {
	uint8_t is_edge_turn : 1;
	uint8_t tile_type : 7;
	uint8_t geometry_block : 4;
	uint8_t has_no_layer : 1;
	uint8_t reserved : 3;
};

struct as_map_segment {
	struct as_map_tile_info tile;
	uint16_t region_id;
};

struct as_map_item_drop_link {
	struct as_map_item_drop * prev;
	struct as_map_item_drop * next;
};

struct as_map_item_drop {
	uint32_t item_id;
	struct ap_item * item;
	uint64_t expire_tick;
	uint64_t ownership_expire_tick;
	struct as_map_sector * bound_sector;
	struct as_map_item_drop_link in_sector;
	struct as_map_item_drop_link in_world;
};

struct as_map_sector {
	uint32_t index_x;
	uint32_t index_z;
	struct au_pos begin;
	struct au_pos end;
	struct as_map_segment segments[AP_SECTOR_DEFAULT_DEPTH][AP_SECTOR_DEFAULT_DEPTH];
	struct ap_character * characters;
	struct ap_object ** objects;
	struct as_map_item_drop * item_drops;
};

struct as_map_in_sector_character_link {
	struct ap_character * prev;
	struct ap_character * next;
};

/* 'struct ap_character' attachment. */
struct as_map_character {
	struct as_map_sector * sector;
	struct as_map_in_sector_character_link in_sector;
	uint32_t instance_id;
	uint32_t sync_instance_id;
	void * npc_view_packet;
	uint16_t npc_view_packet_len;
	void * npc_remove_packet;
	uint16_t npc_remove_packet_len;
	struct as_map_region * region;
};

struct as_map_region {
	boolean initialized;
	struct ap_map_region_template * temp;
	struct ap_character ** npcs;
};

/** \brief AS_MAP_CB_ENTER_SECTOR callback data. */
struct as_map_cb_enter_sector {
	struct ap_character * character;
	struct as_map_sector * sector;
};

/** \brief AS_MAP_CB_LEAVE_SECTOR callback data. */
struct as_map_cb_leave_sector {
	struct ap_character * character;
	struct as_map_sector * sector;
};

/** \brief AS_MAP_CB_ENTER_SECTOR_VIEW callback data. */
struct as_map_cb_enter_sector_view {
	struct ap_character * character;
	struct as_map_sector * sector;
};

/** \brief AS_MAP_CB_LEAVE_SECTOR_VIEW callback data. */
struct as_map_cb_leave_sector_view {
	struct ap_character * character;
	struct as_map_sector * sector;
};

struct as_map_module * as_map_create_module();

boolean as_map_read_segments(struct as_map_module * mod);

boolean as_map_load_objects(struct as_map_module * mod);

void as_map_add_callback(
	struct as_map_module * mod,
	enum as_map_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

size_t as_map_attach_data(
	struct as_map_module * mod,
	enum as_map_module_data_index data_index,
	size_t data_size,
	ap_module_t callback_module,
	ap_module_default_t constructor,
	ap_module_default_t destructor);

struct as_map_character * as_map_get_character_ad(
	struct as_map_module * mod,
	struct ap_character * character);

boolean as_map_add_character(
	struct as_map_module * mod, 
	struct ap_character * character);

struct ap_character * as_map_get_character(
	struct as_map_module * mod,
	uint32_t character_id);

boolean as_map_remove_character(
	struct as_map_module * mod,
	struct ap_character * character);

struct ap_character * as_map_iterate_characters(
	struct as_map_module * mod,
	size_t * index);

struct ap_character * as_map_get_npc(
	struct as_map_module * mod,
	uint32_t character_id);

void as_map_get_characters(
	struct as_map_module * mod,
	const struct au_pos * pos, 
	struct ap_character *** list);

void as_map_get_characters_in_instance(
	struct as_map_module * mod,
	const struct au_pos * pos, 
	uint32_t instance_id,
	struct ap_character *** list);

void as_map_get_characters_in_radius(
	struct as_map_module * mod,
	struct ap_character * character,
	const struct au_pos * pos, 
	float radius,
	struct ap_character *** list);

void as_map_get_characters_in_line(
	struct as_map_module * mod,
	struct ap_character * character,
	const struct au_pos * begin, 
	const struct au_pos * end, 
	float width,
	float length,
	struct ap_character *** list);

void as_map_broadcast(struct as_map_module * mod, struct ap_character * character);

void as_map_broadcast_with_exception(
	struct as_map_module * mod,
	struct ap_character * character,
	struct ap_character * exception);

void as_map_broadcast_around(
	struct as_map_module * mod, 
	const struct au_pos * position);

/**
 * Inform client about nearby entities such as 
 * NPCs, item drops, monsters and other players.
 *
 * This function only needs to be called when client 
 * is taking over a character that was already 
 * added to the world.
 *
 * At other times, such as when adding a new character 
 * or when a character is moving, various updates will 
 * be handled automatically.
 */
void as_map_inform_nearby(struct as_map_module * mod, struct ap_character * character);

struct as_map_tile_info as_map_get_tile(
	struct as_map_module * mod,
	const struct au_pos * pos);

/**
 * Get region at position.
 * \param[in] pos Target position.
 * 
 * \return Region pointer if successful. Otherwise NULL.
 */
struct as_map_region * as_map_get_region_at(
	struct as_map_module * mod,
	const struct au_pos * pos);

/**
 * Respawn character at target region.
 */
boolean as_map_respawn_at_region(
	struct as_map_module * mod,
	struct ap_character * character,
	uint32_t region_id);

struct as_map_sector * as_map_get_sector(
	struct as_map_module * mod,
	uint32_t x, 
	uint32_t z);

struct as_map_sector * as_map_get_sector_at(
	struct as_map_module * mod,
	const struct au_pos * pos);

boolean as_map_is_in_field_of_vision(
	struct as_map_module * mod,
	struct ap_character * character1,
	struct ap_character * character2);

void as_map_add_item_drop(
	struct as_map_module * mod, 
	struct ap_item * item,
	uint64_t expire_in_ms);

struct as_map_item_drop * as_map_find_item_drop(
	struct as_map_module * mod, 
	const struct au_pos * position,
	uint32_t item_id);

void as_map_remove_item_drop(
	struct as_map_module * mod, 
	struct as_map_item_drop * item_drop);

void as_map_clear_expired_item_drops(struct as_map_module * mod, uint64_t tick);

struct ap_object * as_map_find_object(
	struct as_map_module * mod, 
	uint32_t id,
	const struct au_pos * position);

static inline uint32_t as_map_get_character_instance_id(
	struct as_map_module * mod,
	struct ap_character * character)
{
	return as_map_get_character_ad(mod, character)->instance_id;
}

static inline boolean as_map_is_in_instance(
	struct as_map_module * mod,
	struct ap_character * character)
{
	return (as_map_get_character_ad(mod, character)->instance_id == 0);
}

static inline boolean as_map_is_in_same_instance(
	struct as_map_module * mod,
	struct ap_character * character1,
	struct ap_character * character2)
{
	return (as_map_get_character_ad(mod, character1)->instance_id ==
		as_map_get_character_ad(mod, character2)->instance_id);
}

END_DECLS

#endif /* _AS_MAP_H_ */
