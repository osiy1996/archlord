#include "public/ap_module_registry.h"

#include "core/malloc.h"
#include "core/string.h"
#include "core/vector.h"

struct ap_module_registry * ap_module_registry_new()
{
    struct ap_module_registry * reg = alloc(sizeof(*reg));
    memset(reg, 0, sizeof(*reg));
    reg->list = vec_new_reserved(sizeof(*reg->list), 32);
    return reg;
}

void ap_module_registry_destroy(struct ap_module_registry * registry)
{
    vec_free(registry->list);
    dealloc(registry);
}

boolean ap_module_registry_register(
    struct ap_module_registry * registry, 
    ap_module_t module_)
{
    uint32_t i;
    uint32_t count = vec_count(registry->list);
    struct ap_module * context = module_;
    for (i = 0; i < count; i++) {
        const struct ap_module * registered = registry->list[i];
        if (strcmp(registered->name, context->name) == 0)
            return FALSE;
    }
    vec_push_back((void **)&registry->list, (const void *)&module_);
    return TRUE;
}

ap_module_t ap_module_registry_get_module(
    struct ap_module_registry * registry, 
    const char * module_name)
{
    uint32_t i;
    uint32_t count = vec_count(registry->list);
    for (i = 0; i < count; i++) {
        const struct ap_module * context = registry->list[i];
        if (strcmp(context->name, module_name) == 0)
            return registry->list[i];
    }
    return NULL;
}
