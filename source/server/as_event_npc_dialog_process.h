#ifndef _AS_NPC_EVENT_DIALOG_PROCESS_H_
#define _AS_NPC_EVENT_DIALOG_PROCESS_H_

#include "core/macros.h"
#include "core/types.h"

#include "public/ap_character.h"
#include "public/ap_event_manager.h"
#include "public/ap_module.h"

#define AS_EVENT_NPC_DIALOG_PROCESS_MODULE_NAME "AgsmEventNPCDialogProcess"

BEGIN_DECLS

/**
 * \brief Character attachment.
 */
struct as_event_npc_dialog_process_character_attachment {
	/** \brief Target NPC to interact with. */
	struct ap_character * target_npc;
	/** \brief NPC dialog event. */
	const struct ap_event_manager_event * event;
};

struct as_event_npc_dialog_process_module * as_event_npc_dialog_process_create_module();

/**
 * \brief Get character attachment.
 * \param[in] character Character pointer.
 * 
 * \return Character attachment pointer.
 */
struct as_event_npc_dialog_process_character_attachment * as_event_npc_dialog_process_get_character_attachment(
	struct as_event_npc_dialog_process_module * mod,
	struct ap_character * character);

END_DECLS

#endif /* _AS_NPC_EVENT_DIALOG_PROCESS_H_ */
