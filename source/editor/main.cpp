#include <inttypes.h>
#include <stdio.h>
#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN
#define NOGDI
#include <Windows.h>
#endif
#undef near
#undef far

#include "core/core.h"
#include "core/file_system.h"
#include "core/log.h"
#include "core/os.h"

#include "task/task.h"

#include "public/ap_admin.h"
#include "public/ap_ai2.h"
#include "public/ap_auction.h"
#include "public/ap_bill_info.h"
#include "public/ap_character.h"
#include "public/ap_cash_mall.h"
#include "public/ap_chat.h"
#include "public/ap_config.h"
#include "public/ap_drop_item.h"
#include "public/ap_dungeon_wnd.h"
#include "public/ap_event_bank.h"
#include "public/ap_event_binding.h"
#include "public/ap_event_gacha.h"
#include "public/ap_event_guild.h"
#include "public/ap_event_item_convert.h"
#include "public/ap_event_manager.h"
#include "public/ap_event_nature.h"
#include "public/ap_event_npc_dialog.h"
#include "public/ap_event_npc_trade.h"
#include "public/ap_event_point_light.h"
#include "public/ap_event_product.h"
#include "public/ap_event_refinery.h"
#include "public/ap_event_skill_master.h"
#include "public/ap_event_quest.h"
#include "public/ap_event_teleport.h"
#include "public/ap_factors.h"
#include "public/ap_guild.h"
#include "public/ap_item.h"
#include "public/ap_item_convert.h"
#include "public/ap_login.h"
#include "public/ap_map.h"
#include "public/ap_object.h"
#include "public/ap_octree.h"
#include "public/ap_optimized_packet2.h"
#include "public/ap_packet.h"
#include "public/ap_party.h"
#include "public/ap_party_item.h"
#include "public/ap_plugin_boss_spawn.h"
#include "public/ap_private_trade.h"
#include "public/ap_pvp.h"
#include "public/ap_random.h"
#include "public/ap_refinery.h"
#include "public/ap_ride.h"
#include "public/ap_service_npc.h"
#include "public/ap_skill.h"
#include "public/ap_shrine.h"
#include "public/ap_spawn.h"
#include "public/ap_startup_encryption.h"
#include "public/ap_summons.h"
#include "public/ap_system_message.h"
#include "public/ap_tick.h"
#include "public/ap_ui_status.h"
#include "public/ap_world.h"

#include "client/ac_ambient_occlusion_map.h"
#include "client/ac_camera.h"
#include "client/ac_console.h"
#include "client/ac_dat.h"
#include "client/ac_event_point_light.h"
#include "client/ac_imgui.h"
#include "client/ac_mesh.h"
#include "client/ac_object.h"
#include "client/ac_render.h"
#include "client/ac_terrain.h"
#include "client/ac_texture.h"

#include "editor/ae_editor_action.h"
#include "editor/ae_event_binding.h"
#include "editor/ae_event_refinery.h"
#include "editor/ae_event_teleport.h"
#include "editor/ae_item.h"
#include "editor/ae_map.h"
#include "editor/ae_object.h"
#include "editor/ae_terrain.h"
#include "editor/ae_texture.h"

/* 20 FPS */
#define STEPTIME (1.0f / 50.0f)

struct camera_controls {
	boolean key_state[512];
};

typedef ap_module_t (*create_module_t)();

struct module_desc {
	const char * name;
	void * cb_create;
	ap_module_t module_;
	void * global_handle;
};

static struct ap_ai2_module * g_ApAi2;
static struct ap_auction_module * g_ApAuction;
static struct ap_base_module * g_ApBase;
static struct ap_bill_info_module * g_ApBillInfo;
static struct ap_cash_mall_module * g_ApCashMall;
static struct ap_character_module * g_ApCharacter;
static struct ap_chat_module * g_ApChat;
static struct ap_config_module * g_ApConfig;
static struct ap_drop_item_module * g_ApDropItem;
static struct ap_dungeon_wnd_module * g_ApDungeonWnd;
static struct ap_event_bank_module * g_ApEventBank;
static struct ap_event_binding_module * g_ApEventBinding;
static struct ap_event_gacha_module * g_ApEventGacha;
static struct ap_event_guild_module * g_ApEventGuild;
static struct ap_event_item_convert_module * g_ApEventItemConvert;
static struct ap_event_manager_module * g_ApEventManager;
static struct ap_event_nature_module * g_ApEventNature;
static struct ap_event_npc_dialog_module * g_ApEventNpcDialog;
static struct ap_event_npc_trade_module * g_ApEventNpcTrade;
static struct ap_event_point_light_module * g_ApEventPointLight;
static struct ap_event_product_module * g_ApEventProduct;
static struct ap_event_refinery_module * g_ApEventRefinery;
static struct ap_event_skill_master_module * g_ApEventSkillMaster;
static struct ap_event_quest_module * g_ApEventQuest;
static struct ap_event_teleport_module * g_ApEventTeleport;
static struct ap_factors_module * g_ApFactors;
static struct ap_guild_module * g_ApGuild;
static struct ap_item_module * g_ApItem;
static struct ap_item_convert_module * g_ApItemConvert;
static struct ap_login_module * g_ApLogin;
static struct ap_map_module * g_ApMap;
static struct ap_object_module * g_ApObject;
static struct ap_optimized_packet2_module * g_ApOptimizedPacket2;
static struct ap_packet_module * g_ApPacket;
static struct ap_party_module * g_ApParty;
static struct ap_party_item_module * g_ApPartyItem;
static struct ap_plugin_boss_spawn_module * g_ApPluginBossSpawn;
static struct ap_private_trade_module * g_ApPrivateTrade;
static struct ap_pvp_module * g_ApPvP;
static struct ap_random_module * g_ApRandom;
static struct ap_refinery_module * g_ApRefinery;
static struct ap_ride_module * g_ApRide;
static struct ap_service_npc_module * g_ApServiceNpc;
static struct ap_skill_module * g_ApSkill;
static struct ap_spawn_module * g_ApSpawn;
static struct ap_startup_encryption_module * g_ApStartupEncryption;
static struct ap_summons_module * g_ApSummons;
static struct ap_system_message_module * g_ApSystemMessage;
static struct ap_tick_module * g_ApTick;
static struct ap_ui_status_module * g_ApUiStatus;
static struct ap_world_module * g_ApWorld;

static struct ac_console_module * g_AcConsole;
static ap_module_t g_AcAmbientOcclusionMap;
static ap_module_t g_AcEventPointLight;
static ap_module_t g_AcDat;
static struct ac_render_module * g_AcRender;
static struct ac_imgui_module * g_AcImgui;
static ap_module_t g_AcTexture;
static ap_module_t g_AcMesh;
static struct ac_terrain_module * g_AcTerrain;
static struct ac_object_module * g_AcObject;

static struct ae_editor_action_module * g_AeEditorAction;
static struct ae_event_binding_module * g_AeEventBinding;
static struct ae_event_refinery_module * g_AeEventRefinery;
static struct ae_event_teleport_module * g_AeEventTeleport;
static struct ae_item_module * g_AeItem;
static struct ae_map_module * g_AeMap;
static struct ae_object_module * g_AeObject;
static struct ae_terrain_module * g_AeTerrain;
static struct ae_texture_module * g_AeTexture;

static struct module_desc g_Modules[] = {
	/* Public modules. */
	{ AP_PACKET_MODULE_NAME, ap_packet_create_module, NULL, &g_ApPacket },
	{ AP_TICK_MODULE_NAME, ap_tick_create_module, NULL, &g_ApTick },
	{ AP_RANDOM_MODULE_NAME, ap_random_create_module, NULL, &g_ApRandom },
	{ AP_CONFIG_MODULE_NAME, ap_config_create_module, NULL, &g_ApConfig },
	{ AP_BASE_MODULE_NAME, ap_base_create_module, NULL, &g_ApBase },
	{ AP_STARTUP_ENCRYPTION_MODULE_NAME, ap_startup_encryption_create_module, NULL, &g_ApStartupEncryption },
	{ AP_SYSTEM_MESSAGE_MODULE_NAME, ap_system_message_create_module, NULL, &g_ApSystemMessage },
	{ AP_FACTORS_MODULE_NAME, ap_factors_create_module, NULL, &g_ApFactors },
	{ AP_OBJECT_MODULE_NAME, ap_object_create_module, NULL, &g_ApObject },
	{ AP_DUNGEON_WND_MODULE_NAME, ap_dungeon_wnd_create_module, NULL, &g_ApDungeonWnd },
	{ AP_PLUGIN_BOSS_SPAWN_MODULE_NAME, ap_plugin_boss_spawn_create_module, NULL, &g_ApPluginBossSpawn },
	{ AP_CHARACTER_MODULE_NAME, ap_character_create_module, NULL, &g_ApCharacter },
	{ AP_SUMMONS_MODULE_NAME, ap_summons_create_module, NULL, &g_ApSummons },
	{ AP_PVP_MODULE_NAME, ap_pvp_create_module, NULL, &g_ApPvP },
	{ AP_PARTY_MODULE_NAME, ap_party_create_module, NULL, &g_ApParty },
	{ AP_SPAWN_MODULE_NAME, ap_spawn_create_module, NULL, &g_ApSpawn },
	{ AP_SKILL_MODULE_NAME, ap_skill_create_module, NULL, &g_ApSkill },
	{ AP_ITEM_MODULE_NAME, ap_item_create_module, NULL, &g_ApItem },
	{ AP_ITEM_CONVERT_MODULE_NAME, ap_item_convert_create_module, NULL, &g_ApItemConvert },
	{ AP_RIDE_MODULE_NAME, ap_ride_create_module, NULL, &g_ApRide },
	{ AP_REFINERY_MODULE_NAME, ap_refinery_create_module, NULL, &g_ApRefinery },
	{ AP_PRIVATE_TRADE_MODULE_NAME, ap_private_trade_create_module, NULL, &g_ApPrivateTrade },
	{ AP_AI2_MODULE_NAME, ap_ai2_create_module, NULL, &g_ApAi2 },
	{ AP_DROP_ITEM_MODULE_NAME, ap_drop_item_create_module, NULL, &g_ApDropItem },
	{ AP_BILL_INFO_MODULE_NAME, ap_bill_info_create_module, NULL, &g_ApBillInfo },
	{ AP_CASH_MALL_MODULE_NAME, ap_cash_mall_create_module, NULL, &g_ApCashMall },
	{ AP_PARTY_ITEM_MODULE_NAME, ap_party_item_create_module, NULL, &g_ApPartyItem },
	{ AP_UI_STATUS_MODULE_NAME, ap_ui_status_create_module, NULL, &g_ApUiStatus },
	{ AP_GUILD_MODULE_NAME, ap_guild_create_module, NULL, &g_ApGuild },
	{ AP_EVENT_MANAGER_MODULE_NAME, ap_event_manager_create_module, NULL, &g_ApEventManager },
	{ AP_EVENT_BANK_MODULE_NAME, ap_event_bank_create_module, NULL, &g_ApEventBank },
	{ AP_EVENT_BINDING_MODULE_NAME, ap_event_binding_create_module, NULL, &g_ApEventBinding },
	{ AP_EVENT_GACHA_MODULE_NAME, ap_event_gacha_create_module, NULL, &g_ApEventGacha },
	{ AP_EVENT_GUILD_MODULE_NAME, ap_event_guild_create_module, NULL, &g_ApEventGuild },
	{ AP_EVENT_ITEM_CONVERT_MODULE_NAME, ap_event_item_convert_create_module, NULL, &g_ApEventItemConvert },
	{ AP_EVENT_NATURE_MODULE_NAME, ap_event_nature_create_module, NULL, &g_ApEventNature },
	{ AP_EVENT_NPC_DIALOG_MODULE_NAME, ap_event_npc_dialog_create_module, NULL, &g_ApEventNpcDialog },
	{ AP_EVENT_NPC_TRADE_MODULE_NAME, ap_event_npc_trade_create_module, NULL, &g_ApEventNpcTrade },
	{ AP_EVENT_POINT_LIGHT_MODULE_NAME, ap_event_point_light_create_module, NULL, &g_ApEventPointLight },
	{ AP_EVENT_PRODUCT_MODULE_NAME, ap_event_product_create_module, NULL, &g_ApEventProduct },
	{ AP_EVENT_SKILL_MASTER_MODULE_NAME, ap_event_skill_master_create_module, NULL, &g_ApEventSkillMaster },
	{ AP_EVENT_QUEST_MODULE_NAME, ap_event_quest_create_module, NULL, &g_ApEventQuest },
	{ AP_EVENT_TELEPORT_MODULE_NAME, ap_event_teleport_create_module, NULL, &g_ApEventTeleport },
	{ AP_EVENT_REFINERY_MODULE_NAME, ap_event_refinery_create_module, NULL, &g_ApEventRefinery },
	{ AP_SERVICE_NPC_MODULE_NAME, ap_service_npc_create_module, NULL, &g_ApServiceNpc },
	{ AP_OPTIMIZED_PACKET2_MODULE_NAME, ap_optimized_packet2_create_module, NULL, &g_ApOptimizedPacket2 },
	{ AP_CHAT_MODULE_NAME, ap_chat_create_module, NULL, &g_ApChat },
	{ AP_MAP_MODULE_NAME, ap_map_create_module, NULL, &g_ApMap },
	{ AP_LOGIN_MODULE_NAME, ap_login_create_module, NULL, &g_ApLogin },
	{ AP_WORLD_MODULE_NAME, ap_world_create_module, NULL, &g_ApWorld },
	/* Client modules. */
	{ AC_AMBIENT_OCCLUSION_MAP_MODULE_NAME, ac_ambient_occlusion_map_create_module, NULL, &g_AcAmbientOcclusionMap },
	{ AC_EVENT_POINT_LIGHT_MODULE_NAME, ac_event_point_light_create_module, NULL, &g_AcEventPointLight },
	{ AC_DAT_MODULE_NAME, ac_dat_create_module, NULL, &g_AcDat },
	{ AC_RENDER_MODULE_NAME, ac_render_create_module, NULL, &g_AcRender },
	{ AC_TEXTURE_MODULE_NAME, ac_texture_create_module, NULL, &g_AcTexture },
	{ AC_IMGUI_MODULE_NAME, ac_imgui_create_module, NULL, &g_AcImgui },
	{ AC_CONSOLE_MODULE_NAME, ac_console_create_module, NULL, &g_AcConsole },
	{ AC_MESH_MODULE_NAME, ac_mesh_create_module, NULL, &g_AcMesh },
	{ AC_TERRAIN_MODULE_NAME, ac_terrain_create_module, NULL, &g_AcTerrain },
	{ AC_OBJECT_MODULE_NAME, ac_object_create_module, NULL, &g_AcObject },
	/* Editor modules. */
	{ AE_EDITOR_ACTION_MODULE_NAME, ae_editor_action_create_module, NULL, &g_AeEditorAction },
	{ AE_TEXTURE_MODULE_NAME, ae_texture_create_module, NULL, &g_AeTexture },
	{ AE_TERRAIN_MODULE_NAME, ae_terrain_create_module, NULL, &g_AeTerrain },
	{ AE_MAP_MODULE_NAME, ae_map_create_module, NULL, &g_AeMap },
	{ AE_EVENT_BINDING_MODULE_NAME, ae_event_binding_create_module, NULL, &g_AeEventBinding },
	{ AE_EVENT_REFINERY_MODULE_NAME, ae_event_refinery_create_module, NULL, &g_AeEventRefinery },
	{ AE_EVENT_TELEPORT_MODULE_NAME, ae_event_teleport_create_module, NULL, &g_AeEventTeleport },
	{ AE_OBJECT_MODULE_NAME, ae_object_create_module, NULL, &g_AeObject },
	{ AE_ITEM_MODULE_NAME, ae_item_create_module, NULL, &g_AeItem },
};

/* With this definition added, any module context 
 * without 'static' storage modifier 
 * will raise a linker error.  */
void * g_Ctx = NULL;

static boolean create_modules()
{
	uint32_t i;
	INFO("Creating modules..");
	for (i = 0; i < COUNT_OF(g_Modules); i++) {
		struct module_desc * m = &g_Modules[i];
		m->module_ = ((ap_module_t *(*)())m->cb_create)();
		if (!m->module_) {
			ERROR("Failed to create module (%s).", m->name);
			return FALSE;
		}
		if (m->global_handle)
			*(ap_module_t *)m->global_handle = m->module_;
	}
	INFO("Completed module creation.");
	return TRUE;
}

static boolean register_modules(struct ap_module_registry * registry)
{
	uint32_t i;
	INFO("Registering modules..");
	for (i = 0; i < COUNT_OF(g_Modules); i++) {
		struct module_desc * m = &g_Modules[i];
		struct ap_module_instance * instance = (struct ap_module_instance *)m->module_;
		if (!ap_module_registry_register(registry, m->module_)) {
			ERROR("Module registration failed (%s).", m->name);
			return FALSE;
		}
		if (instance->cb_register && !instance->cb_register(instance, registry)) {
			ERROR("Module registration callback failed (%s).", m->name);
			return FALSE;
		}
	}
	INFO("Completed module registration.");
	return TRUE;
}

/*
 * Here we need to free resources that 
 * depend on other modules still being partly functional.
 * 
 * An example is destroying module data.
 * Because module data can have attachments, and modules 
 * that have added these attachments are shutdown earlier 
 * than the base module, we can only destroy 
 * module data BEFORE shutting down modules.
 *
 * Another example is the server module.
 * Closing a server will trigger disconnect callbacks, 
 * and these callbacks will require higher level modules 
 * to be partly functional in order to free various data 
 * such as accounts, characters, etc.
 */
static void close()
{
	int32_t i;
	INFO("Closing modules..");
	/* Close modules in reverse. */
	for (i = COUNT_OF(g_Modules) - 1; i >=0; i--) {
		const struct module_desc * m = &g_Modules[i];
		const struct ap_module_instance * instance = (struct ap_module_instance *)m->module_;
		if (instance->cb_close)
			instance->cb_close(m->module_);
	}
	INFO("Closed all modules.");
}

static void shutdown()
{
	int32_t i;
	INFO("Shutting down modules..");
	/* Shutdown modules in reverse. */
	for (i = COUNT_OF(g_Modules) - 1; i >=0; i--) {
		struct module_desc * m = &g_Modules[i];
		struct ap_module_instance * instance = (struct ap_module_instance *)m->module_;
		if (instance->cb_shutdown)
			instance->cb_shutdown(m->module_);
		ap_module_instance_destroy(m->module_);
		m->module_ = NULL;
		*(ap_module_t *)m->global_handle = NULL;
	}
	INFO("All modules are shutdown.");
}

static boolean initialize()
{
	char path[1024];
	uint32_t i;
	const char * inidir = NULL;
	INFO("Initializing..");
	for (i = 0; i < COUNT_OF(g_Modules); i++) {
		const struct module_desc * m = &g_Modules[i];
		const struct ap_module_instance * instance = (struct ap_module_instance *)m->module_;
		if (instance->cb_initialize && !instance->cb_initialize(m->module_)) {
			ERROR("Module initialization failed (%s).", m->name);
			return FALSE;
		}
	}
	inidir = ap_config_get(g_ApConfig, "ServerIniDir");
	if (!inidir) {
		ERROR("Failed to retrieve ServerIniDir config.");
		return FALSE;
	}
	if (!make_path(path, sizeof(path), "%s/chartype.ini", inidir)) {
		ERROR("Failed to create path (chartype.ini).");
		return FALSE;
	}
	if (!ap_factors_read_char_type(g_ApFactors, path, FALSE)) {
		ERROR("Failed to load character type names.");
		return FALSE;
	}
	if (!make_path(path, sizeof(path), "%s/objecttemplate.ini", inidir)) {
		ERROR("Failed to create path (objecttemplate.ini).");
		return FALSE;
	}
	if (!ap_object_load_templates(g_ApObject, path, FALSE)) {
		ERROR("Failed to load object templates.");
		return FALSE;
	}
	if (!make_path(path, sizeof(path), 
			"%s/charactertemplatepublic.ini", inidir)) {
		ERROR("Failed to create path (charactertemplatepublic.ini).");
		return FALSE;
	}
	if (!ap_character_read_templates(g_ApCharacter, path, FALSE)) {
		ERROR("Failed to read character templates (%s).", path);
		return FALSE;
	}
	if (!make_path(path, sizeof(path), "%s/characterdatatable.txt", inidir)) {
		ERROR("Failed to create path (characterdatatable.txt).");
		return FALSE;
	}
	if (!ap_character_read_import_data(g_ApCharacter, path, FALSE)) {
		ERROR("Failed to read character import data (%s).", path);
		return FALSE;
	}
	if (!make_path(path, sizeof(path), "%s/growupfactor.ini", inidir)) {
		ERROR("Failed to create path (growupfactor.ini).");
		return FALSE;
	}
	if (!ap_character_read_grow_up_table(g_ApCharacter, path, FALSE)) {
		ERROR("Failed to read character grow up factor (%s).", path);
		return FALSE;
	}
	if (!make_path(path, sizeof(path), "%s/levelupexp.ini", inidir)) {
		ERROR("Failed to create path (levelupexp.ini).");
		return FALSE;
	}
	if (!ap_character_read_level_up_exp(g_ApCharacter, path, FALSE)) {
		ERROR("Failed to read character level up exp (%s).", path);
		return FALSE;
	}
	if (!make_path(path, sizeof(path), "%s/skilltemplate.ini", inidir)) {
		ERROR("Failed to create path (skilltemplate.ini).");
		return FALSE;
	}
	if (!ap_skill_read_templates(g_ApSkill, path, FALSE)) {
		ERROR("Failed to read skill templates (%s).", path);
		return FALSE;
	}
	if (!make_path(path, sizeof(path), "%s/skillspec.txt", inidir)) {
		ERROR("Failed to create path (skillspec.txt).");
		return FALSE;
	}
	if (!ap_skill_read_spec(g_ApSkill, path, FALSE)) {
		ERROR("Failed to read skill specialization (%s).", path);
		return FALSE;
	}
	if (!make_path(path, sizeof(path), "%s/skillconst.txt", inidir)) {
		ERROR("Failed to create path (skillconst.txt).");
		return FALSE;
	}
	if (!ap_skill_read_const(g_ApSkill, path, FALSE)) {
		ERROR("Failed to read skill const (%s).", path);
		return FALSE;
	}
	if (!make_path(path, sizeof(path), "%s/skillconst2.txt", inidir)) {
		ERROR("Failed to create path (skillconst2.txt).");
		return FALSE;
	}
	if (!ap_skill_read_const2(g_ApSkill, path, FALSE)) {
		ERROR("Failed to read skill const2 (%s).", path);
		return FALSE;
	}
	if (!make_path(path, sizeof(path), "%s/itemoptiontable.txt", inidir)) {
		ERROR("Failed to create path (itemoptiontable.txt).");
		return FALSE;
	}
	if (!ap_item_read_option_data(g_ApItem, path, FALSE)) {
		ERROR("Failed to read item option data (%s).", path);
		return FALSE;
	}
	if (!make_path(path, sizeof(path), "%s/itemdatatable.txt", inidir)) {
		ERROR("Failed to create path (itemdatatable.txt).");
		return FALSE;
	}
	if (!ap_item_read_import_data(g_ApItem, path, FALSE)) {
		ERROR("Failed to read item import data (%s).", path);
		return FALSE;
	}
	if (!make_path(path, sizeof(path), "%s/itemlotterybox.txt", inidir)) {
		ERROR("Failed to create path (%s/itemlotterybox.txt).", inidir);
		return FALSE;
	}
	if (!ap_item_read_lottery_box(g_ApItem, path, FALSE)) {
		ERROR("Failed to read item lottery box (%s).", path);
		return FALSE;
	}
	if (!make_path(path, sizeof(path), "%s/avatarset.ini", inidir)) {
		ERROR("Failed to create path (%s/avatarset.ini).", inidir);
		return FALSE;
	}
	if (!ap_item_read_avatar_set(g_ApItem, path, FALSE)) {
		ERROR("Failed to read item avatar sets (%s).", path);
		return FALSE;
	}
	if (!make_path(path, sizeof(path), "%s/itemconverttable.txt", inidir)) {
		ERROR("Failed to create path (%s/itemconverttable.txt).", inidir);
		return FALSE;
	}
	if (!ap_item_convert_read_convert_table(g_ApItemConvert, path)) {
		ERROR("Failed to read item convert table (%s).", path);
		return FALSE;
	}
	if (!make_path(path, sizeof(path), "%s/itemruneattributetable.txt", inidir)) {
		ERROR("Failed to create path (%s/itemruneattributetable.txt).", inidir);
		return FALSE;
	}
	if (!ap_item_convert_read_rune_attribute_table(g_ApItemConvert, path, FALSE)) {
		ERROR("Failed to read item rune attribute table (%s).", path);
		return FALSE;
	}
	if (!make_path(path, sizeof(path), "%s/refineryrecipetable.txt", inidir)) {
		ERROR("Failed to create path (%s/refineryrecipetable.txt).", inidir);
		return FALSE;
	}
	if (!ap_refinery_read_recipe_table(g_ApRefinery, path, FALSE)) {
		ERROR("Failed to read refinery recipe table (%s).", path);
		return FALSE;
	}
	if (!make_path(path, sizeof(path), "%s/aidatatable.txt", inidir)) {
		ERROR("Failed to create path (%s/aidatatable.txt).", inidir);
		return FALSE;
	}
	if (!ap_ai2_read_data_table(g_ApAi2, path, FALSE)) {
		ERROR("Failed to read ai data table (%s).", path);
		return FALSE;
	}
	if (!make_path(path, sizeof(path), "%s/optionnumdroprate.txt", inidir)) {
		ERROR("Failed to create path (%s/optionnumdroprate.txt).", inidir);
		return FALSE;
	}
	if (!ap_drop_item_read_option_num_drop_rate(g_ApDropItem, path, FALSE)) {
		ERROR("Failed to read option number drop rates (%s).", path);
		return FALSE;
	}
	if (!make_path(path, sizeof(path), "%s/socketnumdroprate.txt", inidir)) {
		ERROR("Failed to create path (%s/socketnumdroprate.txt).", inidir);
		return FALSE;
	}
	if (!ap_drop_item_read_socket_num_drop_rate(g_ApDropItem, path, FALSE)) {
		ERROR("Failed to read socket number drop rates (%s).", path);
		return FALSE;
	}
	if (!make_path(path, sizeof(path), "%s/droprankrate.txt", inidir)) {
		ERROR("Failed to create path (%s/droprankrate.txt).", inidir);
		return FALSE;
	}
	if (!ap_drop_item_read_drop_rank_rate(g_ApDropItem, path, FALSE)) {
		ERROR("Failed to read item drop rank rates (%s).", path);
		return FALSE;
	}
	if (!make_path(path, sizeof(path), "%s/groupdroprate.txt", inidir)) {
		ERROR("Failed to create path (%s/groupdroprate.txt).", inidir);
		return FALSE;
	}
	if (!ap_drop_item_read_drop_group_rate(g_ApDropItem, path, FALSE)) {
		ERROR("Failed to read item drop group rates (%s).", path);
		return FALSE;
	}
	if (!make_path(path, sizeof(path), "%s/itemdroptable.txt", inidir)) {
		ERROR("Failed to create path (%s/itemdroptable.txt).", inidir);
		return FALSE;
	}
	if (!ap_drop_item_read_drop_table(g_ApDropItem, path, FALSE)) {
		ERROR("Failed to read item drop table (%s).", path);
		return FALSE;
	}
	if (!make_path(path, sizeof(path), "%s/leveluprewardtable.txt", inidir)) {
		ERROR("Failed to create path (%s/leveluprewardtable.txt).", inidir);
		return FALSE;
	}
	if (!ap_service_npc_read_level_up_reward_table(g_ApServiceNpc, path, FALSE)) {
		ERROR("Failed to read level up reward table (%s).", path);
		return FALSE;
	}
	if (!make_path(path, sizeof(path), "%s/cashmall.txt", inidir)) {
		ERROR("Failed to create path (%s/cashmall.txt).", inidir);
		return FALSE;
	}
	if (!ap_cash_mall_read_import_data(g_ApCashMall, path, FALSE)) {
		ERROR("Failed to read cash mall data (%s).", path);
		return FALSE;
	}
	if (!make_path(path, sizeof(path), "%s/teleportpoint.ini", inidir)) {
		ERROR("Failed to create path (%s/teleportpoint.ini).", inidir);
		return FALSE;
	}
	if (!ap_event_teleport_read_teleport_points(g_ApEventTeleport, path, FALSE)) {
		ERROR("Failed to read teleport points (%s).", path);
		return FALSE;
	}
	if (!make_path(path, sizeof(path), "%s/npctradelist.txt", inidir)) {
		ERROR("Failed to create path (%s/npctradelist.txt).", inidir);
		return FALSE;
	}
	if (!ap_event_npc_trade_read_trade_lists(g_ApEventNpcTrade, path)) {
		ERROR("Failed to read npc trade list templates.");
		return FALSE;
	}
	if (!make_path(path, sizeof(path), "%s/npc.ini", inidir)) {
		ERROR("Failed to create path (npc.ini).");
		return FALSE;
	}
	if (!ap_character_read_static(g_ApCharacter, path, FALSE)) {
		ERROR("Failed to read static characters (%s).", path);
		return FALSE;
	}
	if (!make_path(path, sizeof(path), "%s/spawndatatable.txt", inidir)) {
		ERROR("Failed to create path (spawndatatable.txt).");
		return FALSE;
	}
	if (!ap_spawn_read_data(g_ApSpawn, path, FALSE)) {
		ERROR("Failed to read spawn data (%s).", path);
		return FALSE;
	}
	if (!make_path(path, sizeof(path), "%s/spawninstancetable.txt", inidir)) {
		ERROR("Failed to create path (spawninstancetable.txt).");
		return FALSE;
	}
	if (!ap_spawn_read_instances(g_ApSpawn, path, FALSE)) {
		ERROR("Failed to read spawn instances (%s).", path);
		return FALSE;
	}
	if (!ac_imgui_init(g_AcImgui, 255, ac_render_get_window(g_AcRender))) {
		ERROR("Failed to initialize ImGui.");
		return FALSE;
	}
	INFO("Completed initialization.");
	return TRUE;
}

static void logcb(
	enum LogLevel level,
	const char * file,
	uint32_t line,
	const char * message)
{
	const float * color;
	const char * label;
#ifdef _WIN32
	char str[1024];
	snprintf(str, sizeof(str), "%s:%u| %s\n", file, line,
		message);
	OutputDebugStringA(str);
#endif
	if (!g_AcConsole)
		return;
	switch (level) {
	case LOG_LEVEL_TRACE: {
		static float c[4] = { 0.54f, 0.74f, 0.71f, 1.0f };
		color = c;
		label = "TRACE";
		break;
	}
	case LOG_LEVEL_INFO: {
		static float c[4] = { 0.80f, 0.80f, 0.80f, 1.0f };
		color = c;
		label = "INFO";
		break;
	}
	case LOG_LEVEL_WARN: {
		static float c[4] = { 0.94f, 0.77f, 0.45f, 1.0f };
		color = c;
		label = "WARNING";
		break;
	}
	case LOG_LEVEL_ERROR: {
		static float c[4] = { 0.80f, 0.40f, 0.40f, 1.0f };
		color = c;
		label = "ERROR";
		break;
	}
	default: {
		static float c[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		color = c;
		label = "UNKNOWN";
		break;
	}
	}
	ac_console_println_colored(g_AcConsole, "%-10s %s", color, label, message);
}

static void updatetick(uint64_t * last, float * dt)
{
	uint64_t t = ap_tick_get(g_ApTick);
	uint64_t l = *last;
	if (t < l) {
		*dt = 0.0f;
		return;
	}
	*dt = (t - l) / 1000.0f;
	*last = t;
}

static void render(struct ac_camera * cam, float dt)
{
	static bool b = true;
	if (!ac_render_begin_frame(g_AcRender, cam, dt))
		return;
	ac_terrain_render(g_AcTerrain);
	ae_terrain_render(g_AeTerrain, cam);
	ac_object_render(g_AcObject);
	ae_object_render_outline(g_AeObject, cam);
	ac_imgui_new_frame(g_AcImgui);
	ImGui::BeginMainMenuBar();
	if (ImGui::BeginMenu("File", true)) {
		if (ImGui::Selectable("Save"))
			ae_editor_action_commit_changes(g_AeEditorAction);
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("View", true)) {
		ae_editor_action_render_view_menu(g_AeEditorAction);
		ImGui::EndMenu();
	}
	ac_console_render_icon(g_AcConsole);
	ImGui::EndMainMenuBar();
	ac_imgui_begin_toolbar(g_AcImgui);
	ae_terrain_toolbar(g_AeTerrain);
	ac_imgui_end_toolbar(g_AcImgui);
	ac_imgui_viewport(g_AcImgui);
	ac_console_render(g_AcConsole);
	ae_terrain_imgui(g_AeTerrain);
	ae_object_imgui(g_AeObject);
	ae_editor_action_render_editors(g_AeEditorAction);
	ac_imgui_begin_toolbox(g_AcImgui);
	ae_terrain_toolbox(g_AeTerrain);
	ac_imgui_end_toolbox(g_AcImgui);
	ac_imgui_render(g_AcImgui);
	//ar_dd_begin();
	//ar_dd_end();
	ac_render_end_frame(g_AcRender);
}

static void on_keydown_cam(
	struct camera_controls * ctrl,
	const boolean * key_state,
	const SDL_KeyboardEvent * e)
{
	if (e->repeat ||
		key_state[SDL_SCANCODE_LCTRL] ||
		key_state[SDL_SCANCODE_RCTRL])
		return;
	ctrl->key_state[e->keysym.scancode] = TRUE;
}

static void on_keyup_cam(
	struct camera_controls * ctrl,
	const SDL_KeyboardEvent * e)
{
	ctrl->key_state[e->keysym.scancode] = FALSE;
}

static void update_camera(
	struct ac_camera * cam,
	struct camera_controls * ctrl,
	float dt)
{
	const boolean * key_state = ctrl->key_state;
	float speed =
		key_state[SDL_SCANCODE_LSHIFT] ? 1000.0f : 10000.0f;
	uint32_t mb_state = SDL_GetMouseState(NULL, NULL);
	/*
	if (key_state[SDL_SCANCODE_W])
		ar_cam_translate(cam, dt * speed, 0.f, 0.f);
	else if (key_state[SDL_SCANCODE_S])
		ar_cam_translate(cam, -dt * speed, 0.f, 0.f);
	if (key_state[SDL_SCANCODE_D])
		ar_cam_translate(cam, 0.f, dt * speed, 0.f);
	else if (key_state[SDL_SCANCODE_A])
		ar_cam_translate(cam, 0.f, -dt * speed, 0.f);
	if (key_state[SDL_SCANCODE_UP])
		ar_cam_translate(cam, 0.f, 0.f, dt * speed);
	else if (key_state[SDL_SCANCODE_DOWN])
		ar_cam_translate(cam, 0.f, 0.f, -dt * speed);
	*/
	if ((mb_state & SDL_BUTTON(SDL_BUTTON_LEFT)) &&
		(mb_state & SDL_BUTTON(SDL_BUTTON_RIGHT))) {
		ac_camera_translate(cam, 
			speed * dt, 0.0f, 0.0f);
	}
	ac_camera_update(cam, dt);
}

static void setspawnpos(struct ac_camera * cam)
{
	vec3 center = { -462922.5f,3217.9f,-44894.8f };
	const char * config = ap_config_get(g_ApConfig, "EditorStartPosition");
	if (config) {
		vec3 c = { 0 };
		if (sscanf(config, "%f,%f,%f", &c[0], &c[1], &c[2]))
			glm_vec3_copy(c, center);
	}
	ac_camera_init(cam, center, 5000.0f, 45.0f, 30.0f, 60.0f, 200.0f, 40000.0f);
	ac_terrain_sync(g_AcTerrain, cam->center, TRUE);
	ac_object_sync(g_AcObject, cam->center, TRUE);
}

int main(int argc, char * argv[])
{
	uint64_t last = 0;
	float dt = 0.0f;
	float accum = 0.0f;
	struct ap_module_registry * registry = NULL;
	struct ac_camera cam = { 0 };
	struct camera_controls cam_ctrl = { 0 };
	if (!log_init()) {
		fprintf(stderr, "log_init() failed.\n");
		return -1;
	}
	log_set_callback(logcb);
	if (!core_startup()) {
		ERROR("Failed to startup core module.");
		return -1;
	}
	if (!task_startup()) {
		ERROR("Failed to startup task module.");
		return -1;
	}
	if (!create_modules()) {
		ERROR("Module creation failed.");
		return -1;
	}
	registry = ap_module_registry_new();
	if (!register_modules(registry)) {
		ERROR("Failed to register modules.");
		return -1;
	}
	if (!initialize()) {
		ERROR("Failed to initialize.");
		return -1;
	}
	setspawnpos(&cam);
	last = ap_tick_get(g_ApTick);
	INFO("Entering main loop..");
	while (!core_should_shutdown()) {
		uint64_t tick = ap_tick_get(g_ApTick);
		SDL_Event e;
		updatetick(&last, &dt);
		while (ac_render_poll_window_event(g_AcRender, &e)) {
			if (ac_imgui_process_event(g_AcImgui, &e))
				continue;
			ac_render_process_window_event(g_AcRender, &e);
			switch (e.type) {
			case SDL_MOUSEMOTION:
				if (ac_render_button_down(g_AcRender, AC_RENDER_BUTTON_RIGHT)) {
					ac_camera_rotate(&cam,
						e.motion.yrel * .3f,
						e.motion.xrel * .3f);
				}
				else if (ac_render_button_down(g_AcRender, AC_RENDER_BUTTON_MIDDLE)) {
					ac_camera_slide(&cam,
						-(float)e.motion.xrel * 10.f,
						(float)e.motion.yrel * 10.f);
				}
				else {
					if (ae_terrain_on_mmove(g_AeTerrain, &cam, e.motion.x, 
							e.motion.y) ||
						ae_object_on_mmove(g_AeObject, &cam, 
							e.motion.x, e.motion.y, 
							e.motion.xrel, e.motion.yrel)) {
						break;
					}
				}
				break;
			case SDL_MOUSEWHEEL:
				if (!ae_terrain_on_mwheel(g_AeTerrain, e.wheel.preciseY))
					ac_camera_zoom(&cam, e.wheel.preciseY * 1000.f);
				break;
			case SDL_MOUSEBUTTONDOWN:
				if (e.button.button == SDL_BUTTON_LEFT) {
					uint32_t mb_state = 
						SDL_GetMouseState(NULL, NULL);
					if (mb_state & SDL_BUTTON(SDL_BUTTON_RIGHT))
						break;
					if (ae_object_on_mdown(g_AeObject, &cam, e.button.x, e.button.y))
						break;
					ae_terrain_on_mdown(g_AeTerrain, &cam, e.button.x, e.button.y);
				}
				break;
			case SDL_KEYDOWN:
				if (ae_terrain_on_key_down(g_AeTerrain, e.key.keysym.sym) ||
					ae_object_on_key_down(g_AeObject, &cam, e.key.keysym.sym)) {
					break;
				}
				on_keydown_cam(&cam_ctrl, ac_render_get_key_state(g_AcRender), &e.key);
				break;
			case SDL_KEYUP:
				on_keyup_cam(&cam_ctrl, &e.key);
				break;
			}
		}
		accum += dt;
		while (accum >= STEPTIME) {
			accum -= STEPTIME;
		}
		update_camera(&cam, &cam_ctrl, dt);
		ac_terrain_sync(g_AcTerrain, cam.center, FALSE);
		ac_terrain_update(g_AcTerrain, dt);
		ac_object_sync(g_AcObject, cam.center, FALSE);
		ac_object_update(g_AcObject, dt);
		ae_terrain_update(g_AeTerrain, dt);
		ae_texture_process_loading_queue(g_AeTexture, 5);
		ae_object_update(g_AeObject, dt);
		render(&cam, dt);
		task_do_post_cb();
		sleep(1);
	}
	INFO("Exited main loop.");
	close();
	shutdown();
	return 0;
}