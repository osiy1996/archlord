#include "client/ac_effect.h"

#include "core/log.h"
#include "core/malloc.h"
#include "core/vector.h"

#include <assert.h>
#include <stdlib.h>

struct ac_effect_module {
	struct ap_module_instance instance;
};

static boolean onregister(
	struct ac_effect_module * mod,
	struct ap_module_registry * registry)
{
	return TRUE;
}

static void onshutdown(struct ac_effect_module * mod)
{
}

struct ac_effect_module * ac_effect_create_module()
{
	struct ac_effect_module * mod = ap_module_instance_new(AC_EFFECT_MODULE_NAME,
		sizeof(*mod), onregister, NULL, NULL, onshutdown);
	return mod;
}
