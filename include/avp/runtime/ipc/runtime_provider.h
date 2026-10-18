#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


enum {
    AVP_RUNTIME_PROVIDER_NAME_CAPACITY = 16,
};

typedef enum {
    AVP_RUNTIME_PROVIDER_STATE_REGISTERED = 0,
    AVP_RUNTIME_PROVIDER_STATE_PUBLISHED = 1,
    AVP_RUNTIME_PROVIDER_STATE_RETIRING = 2,
} AVPRuntimeProviderState;

typedef struct AVPRuntimeProvider AVPRuntimeProvider;

typedef struct {
    AVPRuntimeProvider *provider;
    uint8_t generation;
} AVPRuntimeProviderHandle;

struct AVPRuntimeProvider {
    AVPRuntimeProvider *next;
    uintptr_t next_lower_bound;
    uintptr_t next_upper_bound;
    uintptr_t next_cookie;
    void *implementation;
    uintptr_t implementation_lower_bound;
    uintptr_t implementation_upper_bound;
    uintptr_t implementation_cookie;
    uint8_t reserved_040[0x20];
    uint64_t owner_domain;
    /* AVPBooter keeps these routing labels as opaque 16-byte values. */
    char metadata_a[AVP_RUNTIME_PROVIDER_NAME_CAPACITY];
    char metadata_b[AVP_RUNTIME_PROVIDER_NAME_CAPACITY];
    char lookup_key[AVP_RUNTIME_PROVIDER_NAME_CAPACITY];
    uint8_t state;
    uint8_t flags;
    uint8_t generation;
    uint8_t reserved_09b;
    uint32_t references;
};

_Static_assert(
    offsetof(AVPRuntimeProvider, implementation) == 0x20,
    "runtime provider implementation capability offset"
);
_Static_assert(
    offsetof(AVPRuntimeProvider, metadata_a) == 0x68,
    "runtime provider metadata-a offset"
);
_Static_assert(
    offsetof(AVPRuntimeProvider, metadata_b) == 0x78,
    "runtime provider metadata-b offset"
);
_Static_assert(
    offsetof(AVPRuntimeProvider, lookup_key) == 0x88,
    "runtime provider lookup-key offset"
);
_Static_assert(
    offsetof(AVPRuntimeProvider, state) == 0x98,
    "runtime provider published-state offset"
);
_Static_assert(
    offsetof(AVPRuntimeProvider, flags) == 0x99,
    "runtime provider original flag offset"
);
_Static_assert(sizeof(AVPRuntimeProvider) == 0xa0, "runtime provider size");

void runtime_provider_registry_initialize(void);
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
);
int runtime_provider_publish(void *implementation);
AVPRuntimeProviderHandle runtime_provider_lookup(
    const char *lookup_key
);
void *runtime_provider_resolve(AVPRuntimeProviderHandle handle);
int runtime_provider_retain_implementation(AVPRuntimeProviderHandle handle);
void runtime_provider_release(AVPRuntimeProviderHandle handle);
void runtime_provider_unpublish(AVPRuntimeProvider *provider);
void runtime_provider_unregister(AVPRuntimeProvider *provider);
size_t runtime_provider_registered_count(void);
size_t runtime_provider_published_count(void);
