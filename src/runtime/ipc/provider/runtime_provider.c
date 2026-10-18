#include "avp/runtime/ipc/runtime_provider.h"
#include "avp/runtime/support/recovered_symbol.h"


typedef struct {
    AVPRuntimeProvider *head;
    uintptr_t lower_bound;
    uintptr_t upper_bound;
    uintptr_t cookie;
} AVPRuntimeProviderHead;

#if defined(__ELF__)
#define RUNTIME_PROVIDER_HEAD_SECTION \
    __attribute__((section(".runtime_provider.head")))
#define RUNTIME_PROVIDER_STATE_SECTION \
    __attribute__((section(".runtime_task.state")))
#else
#define RUNTIME_PROVIDER_HEAD_SECTION
#define RUNTIME_PROVIDER_STATE_SECTION
#endif

static AVPRuntimeProviderHead provider_head RUNTIME_PROVIDER_HEAD_SECTION;
static size_t provider_registered_count RUNTIME_PROVIDER_STATE_SECTION;

static bool runtime_provider_string_equal(
    const char *left,
    const char *right
) {
    for (size_t index = 0; index < AVP_RUNTIME_PROVIDER_NAME_CAPACITY;
         ++index) {
        if (left[index] != right[index]) {
            return false;
        }
        if (left[index] == '\0') {
            return true;
        }
    }
    return true;
}

static void runtime_provider_copy_name(char *destination, const char *source) {
    if (source == 0 || source[0] == '\0') {
        __builtin_trap();
    }
    size_t index = 0;
    for (; index + 1 < AVP_RUNTIME_PROVIDER_NAME_CAPACITY; ++index) {
        destination[index] = source[index];
        if (source[index] == '\0') {
            break;
        }
    }
    if (index + 1 == AVP_RUNTIME_PROVIDER_NAME_CAPACITY) {
        if (source[index] != '\0') {
            __builtin_trap();
        }
        destination[index] = '\0';
    }
    for (++index; index < AVP_RUNTIME_PROVIDER_NAME_CAPACITY; ++index) {
        destination[index] = '\0';
    }
}

static bool runtime_provider_key_equal(
    const AVPRuntimeProvider *provider,
    const char *lookup_key
) {
    return runtime_provider_string_equal(provider->lookup_key, lookup_key);
}

static void runtime_provider_validate_link_target(
    const AVPRuntimeProvider *provider,
    uintptr_t lower_bound,
    uintptr_t upper_bound
) {
    uintptr_t address = (uintptr_t)provider;
    if (provider == 0 || address < lower_bound || upper_bound < address ||
        upper_bound - address < sizeof(*provider)) {
        __builtin_trap();
    }
}

static bool runtime_provider_is_linked(const AVPRuntimeProvider *provider) {
    const AVPRuntimeProvider *entry = provider_head.head;
    uintptr_t lower_bound = provider_head.lower_bound;
    uintptr_t upper_bound = provider_head.upper_bound;
    bool found = false;
    for (size_t index = 0; index < provider_registered_count;
         ++index) {
        runtime_provider_validate_link_target(
            entry,
            lower_bound,
            upper_bound
        );
        if (entry->state > AVP_RUNTIME_PROVIDER_STATE_RETIRING) {
            __builtin_trap();
        }
        if (entry == provider) {
            found = true;
        }
        lower_bound = entry->next_lower_bound;
        upper_bound = entry->next_upper_bound;
        entry = entry->next;
    }
    if (entry != 0) {
        __builtin_trap();
    }
    return found;
}

static void runtime_provider_validate_handle(
    AVPRuntimeProviderHandle handle
) {
    AVPRuntimeProvider *provider = handle.provider;
    if (provider == 0 || !runtime_provider_is_linked(provider) ||
        provider->generation != handle.generation ||
        (provider->state != AVP_RUNTIME_PROVIDER_STATE_PUBLISHED &&
            provider->state != AVP_RUNTIME_PROVIDER_STATE_RETIRING) ||
        provider->references == 0) {
        __builtin_trap();
    }
}

void runtime_provider_registry_initialize(void) {
    AVPRuntimeProvider *entry = provider_head.head;
    uintptr_t lower_bound = provider_head.lower_bound;
    uintptr_t upper_bound = provider_head.upper_bound;
    for (size_t index = 0; index < provider_registered_count;
         ++index) {
        runtime_provider_validate_link_target(
            entry,
            lower_bound,
            upper_bound
        );
        AVPRuntimeProvider *next = entry->next;
        lower_bound = entry->next_lower_bound;
        upper_bound = entry->next_upper_bound;
        if (entry->references != 0) {
            __builtin_trap();
        }
        uint8_t generation = entry->generation;
        *entry = (AVPRuntimeProvider){.generation = generation};
        entry = next;
    }
    if (entry != 0) {
        __builtin_trap();
    }
    provider_head = (AVPRuntimeProviderHead){0};
    provider_registered_count = 0;
}

void runtime_provider_register(
    AVPRuntimeProvider *provider,
    uint64_t owner_domain,
    const char *lookup_key,
    const char *metadata_a,
    const char *metadata_b,
    void *implementation,
    uintptr_t implementation_lower_bound,
    uintptr_t implementation_upper_bound,
    uintptr_t implementation_cookie
) {
    if (provider == 0 || runtime_provider_is_linked(provider) ||
        owner_domain == 0 ||
        implementation == 0 ||
        (uintptr_t)implementation < implementation_lower_bound ||
        (uintptr_t)implementation >= implementation_upper_bound) {
        __builtin_trap();
    }
    for (AVPRuntimeProvider *entry = provider_head.head;
         entry != 0; entry = entry->next) {
        if (runtime_provider_key_equal(
                entry,
                lookup_key
            )) {
            __builtin_trap();
        }
    }
    uint8_t generation = (uint8_t)(provider->generation + 1);
    if (generation == 0) {
        generation = 1;
    }
    *provider = (AVPRuntimeProvider){
        .next = provider_head.head,
        .next_lower_bound = provider_head.lower_bound,
        .next_upper_bound = provider_head.upper_bound,
        .next_cookie = provider_head.cookie,
        .implementation = implementation,
        .implementation_lower_bound = implementation_lower_bound,
        .implementation_upper_bound = implementation_upper_bound,
        .implementation_cookie = implementation_cookie,
        .owner_domain = owner_domain,
        .state = AVP_RUNTIME_PROVIDER_STATE_REGISTERED,
        .generation = generation,
    };
    runtime_provider_copy_name(
        provider->lookup_key,
        lookup_key
    );
    runtime_provider_copy_name(provider->metadata_a, metadata_a);
    runtime_provider_copy_name(provider->metadata_b, metadata_b);
    provider_head.head = provider;
    provider_head.lower_bound = (uintptr_t)provider;
    provider_head.upper_bound = (uintptr_t)provider + sizeof(*provider);
    provider_head.cookie = 0;
    ++provider_registered_count;
}

int runtime_provider_publish(void *implementation) {
    AVPRuntimeProvider *provider = provider_head.head;
    uintptr_t lower_bound = provider_head.lower_bound;
    uintptr_t upper_bound = provider_head.upper_bound;
    for (size_t index = 0; index < provider_registered_count;
         ++index) {
        runtime_provider_validate_link_target(
            provider,
            lower_bound,
            upper_bound
        );
        if (provider->implementation == implementation) {
            provider->state = AVP_RUNTIME_PROVIDER_STATE_PUBLISHED;
            return 0;
        }
        lower_bound = provider->next_lower_bound;
        upper_bound = provider->next_upper_bound;
        provider = provider->next;
    }
    if (provider != 0) {
        __builtin_trap();
    }
    return -1;
}

AVP_RECOVERED_ENTRY AVPRuntimeProviderHandle runtime_provider_lookup(
    const char *lookup_key
) {
    if (lookup_key == 0) {
        __builtin_trap();
    }
    AVPRuntimeProvider *provider = provider_head.head;
    uintptr_t lower_bound = provider_head.lower_bound;
    uintptr_t upper_bound = provider_head.upper_bound;
    for (size_t index = 0; index < provider_registered_count;
         ++index) {
        runtime_provider_validate_link_target(
            provider,
            lower_bound,
            upper_bound
        );
        if (provider->state == AVP_RUNTIME_PROVIDER_STATE_PUBLISHED &&
            runtime_provider_key_equal(
                provider,
                lookup_key
            )) {
            if (provider->references == UINT32_MAX) {
                __builtin_trap();
            }
            ++provider->references;
            return (AVPRuntimeProviderHandle){
                .provider = provider,
                .generation = provider->generation,
            };
        }
        lower_bound = provider->next_lower_bound;
        upper_bound = provider->next_upper_bound;
        provider = provider->next;
    }
    if (provider != 0) {
        __builtin_trap();
    }
    return (AVPRuntimeProviderHandle){0};
}

AVP_RECOVERED_ENTRY void *runtime_provider_resolve(
    AVPRuntimeProviderHandle handle
) {
    runtime_provider_validate_handle(handle);
    AVPRuntimeProvider *provider = handle.provider;
    if ((uintptr_t)provider->implementation <
            provider->implementation_lower_bound ||
        (uintptr_t)provider->implementation >=
            provider->implementation_upper_bound) {
        __builtin_trap();
    }
    return provider->implementation;
}

AVP_RECOVERED_ENTRY int runtime_provider_retain_implementation(
    AVPRuntimeProviderHandle handle
) {
    runtime_provider_validate_handle(handle);
    AVPRuntimeProvider *provider = handle.provider;
    if (provider->references == UINT32_MAX) {
        __builtin_trap();
    }
    ++provider->references;
    return 0;
}

AVP_RECOVERED_ENTRY void runtime_provider_release(
    AVPRuntimeProviderHandle handle
) {
    runtime_provider_validate_handle(handle);
    AVPRuntimeProvider *provider = handle.provider;
    --provider->references;
    if (provider->state == AVP_RUNTIME_PROVIDER_STATE_RETIRING &&
        provider->references == 0) {
        provider->state = AVP_RUNTIME_PROVIDER_STATE_REGISTERED;
    }
}

void runtime_provider_unpublish(AVPRuntimeProvider *provider) {
    if (provider == 0 || !runtime_provider_is_linked(provider) ||
        provider->state != AVP_RUNTIME_PROVIDER_STATE_PUBLISHED) {
        __builtin_trap();
    }
    /* Existing handles remain valid while new lookup visibility closes. */
    provider->state = provider->references == 0 ?
        AVP_RUNTIME_PROVIDER_STATE_REGISTERED :
        AVP_RUNTIME_PROVIDER_STATE_RETIRING;
}

void runtime_provider_unregister(AVPRuntimeProvider *provider) {
    if (provider == 0 || !runtime_provider_is_linked(provider) ||
        provider->state !=
            AVP_RUNTIME_PROVIDER_STATE_REGISTERED ||
        provider->references != 0) {
        __builtin_trap();
    }
    AVPRuntimeProvider *previous = 0;
    AVPRuntimeProvider *entry = provider_head.head;
    while (entry != provider) {
        previous = entry;
        entry = entry->next;
    }
    if (previous == 0) {
        provider_head.head = provider->next;
        provider_head.lower_bound = provider->next_lower_bound;
        provider_head.upper_bound = provider->next_upper_bound;
        provider_head.cookie = provider->next_cookie;
    } else {
        previous->next = provider->next;
        previous->next_lower_bound = provider->next_lower_bound;
        previous->next_upper_bound = provider->next_upper_bound;
        previous->next_cookie = provider->next_cookie;
    }
    --provider_registered_count;
    uint8_t generation = provider->generation;
    *provider = (AVPRuntimeProvider){.generation = generation};
}

size_t runtime_provider_registered_count(void) {
    return provider_registered_count;
}

size_t runtime_provider_published_count(void) {
    size_t count = 0;
    const AVPRuntimeProvider *provider = provider_head.head;
    uintptr_t lower_bound = provider_head.lower_bound;
    uintptr_t upper_bound = provider_head.upper_bound;
    for (size_t index = 0; index < provider_registered_count;
         ++index) {
        runtime_provider_validate_link_target(
            provider,
            lower_bound,
            upper_bound
        );
        if (provider->state == AVP_RUNTIME_PROVIDER_STATE_PUBLISHED) {
            ++count;
        }
        lower_bound = provider->next_lower_bound;
        upper_bound = provider->next_upper_bound;
        provider = provider->next;
    }
    if (provider != 0) {
        __builtin_trap();
    }
    return count;
}
