#ifndef _AS_SERVICE_NPC_H_
#define _AS_SERVICE_NPC_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_define.h"
#include "public/ap_service_npc.h"
#include "public/ap_module.h"

#define AS_SERVICE_NPC_MODULE_NAME "AgsmServiceNPC"

#define AS_SERVICE_NPC_MAX_LEVEL_UP_REWARD_MILESTONE_COUNT 32

BEGIN_DECLS

enum as_service_npc_character_database_attachment_id {
	AS_SERVICE_NPC_CHARACTER_DATABASE_ATTACHMENT_RECEIVED_LEVEL_UP_REWARD_MILESTONE,
};

enum as_service_npc_callback_id {
	AS_SERVICE_NPC_CB_,
};

/** \brief struct as_character_db attachment. */
struct as_service_npc_character_database_attachment {
	uint32_t received_level_up_reward_milestones[AS_SERVICE_NPC_MAX_LEVEL_UP_REWARD_MILESTONE_COUNT];
	uint32_t received_level_up_reward_milestone_count;
};

struct as_service_npc_module * as_service_npc_create_module();

void as_service_npc_add_callback(
	struct as_service_npc_module * mod,
	enum as_service_npc_callback_id id,
	ap_module_t callback_module,
	ap_module_default_t callback);

/**
 * Retrieve attached data.
 * \param[in] character Character database object pointer.
 * 
 * \return Character database object attachment.
 */
struct as_service_npc_character_database_attachment * as_service_npc_get_character_database_attachment(
	struct as_service_npc_module * mod,
	struct as_character_db * character);

boolean as_service_npc_add_received_level_up_reward_milestone(
	struct as_service_npc_module * mod,
	struct ap_character * character,
	uint32_t level);

void as_service_npc_reset_received_level_up_rewards(
	struct as_service_npc_module * mod,
	struct ap_character * character);

END_DECLS

#endif /* _AS_SERVICE_NPC_H_ */