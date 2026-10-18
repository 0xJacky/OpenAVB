#pragma once

#include "avp/runtime/task/runtime_task.h"

#include <stdbool.h>
#include <stdint.h>


#define AVP_RUNTIME_SERVICE_STORAGE UINT64_C(0x656761726f7473)

enum {
    AVP_RUNTIME_SERVICE_PACKET_LIMIT = 0x4000,
    AVP_RUNTIME_SERVICE_ENDPOINT_WORDS = 8,
};

typedef struct {
    RuntimeTaskQueueLink secondary;
    RuntimeTaskQueueLink waiters;
    uint16_t capacity_words;
    uint16_t count_words;
    uint16_t read_index;
    uint16_t write_index;
    uint8_t flags;
    uint8_t reserved_089[7];
    uint64_t words[AVP_RUNTIME_SERVICE_ENDPOINT_WORDS];
} RuntimeServiceEndpoint;

_Static_assert(
    offsetof(RuntimeServiceEndpoint, waiters) == 0x40,
    "service waiter anchor"
);
_Static_assert(
    offsetof(RuntimeServiceEndpoint, capacity_words) == 0x80,
    "service capacity"
);
_Static_assert(
    offsetof(RuntimeServiceEndpoint, count_words) == 0x82,
    "service occupancy"
);
_Static_assert(
    offsetof(RuntimeServiceEndpoint, read_index) == 0x84,
    "service read cursor"
);
_Static_assert(
    offsetof(RuntimeServiceEndpoint, write_index) == 0x86,
    "service write cursor"
);
_Static_assert(
    offsetof(RuntimeServiceEndpoint, flags) == 0x88,
    "service endpoint flags"
);
_Static_assert(
    offsetof(RuntimeServiceEndpoint, words) == 0x90,
    "service ring"
);
_Static_assert(sizeof(RuntimeServiceEndpoint) == 0xd0, "service endpoint size");

typedef void (*RuntimeServiceMethodHandler)(
    void *handler_context,
    uint16_t method,
    uint16_t encoding_depth,
    void *packet,
    uint32_t packet_size
);

typedef struct {
    RuntimeTask task;
    RuntimeServiceEndpoint endpoint;
    RuntimeDomain *domain;
    RuntimeServiceMethodHandler handler;
    void *handler_context;
    bool initialized;
    bool started;
} RuntimeServiceOwner;

void runtime_service_runtime_initialize(void);
bool runtime_service_owner_initialize(
    RuntimeServiceOwner *owner,
    RuntimeDomain *domain,
    uintptr_t stack_top,
    void *provider_packet
);
bool runtime_service_register(
    RuntimeServiceOwner *owner,
    uint64_t service,
    RuntimeServiceMethodHandler handler,
    void *handler_context
);
bool runtime_service_owner_start(RuntimeServiceOwner *owner);
uint8_t runtime_service_pending_count(const RuntimeServiceOwner *owner);
uint8_t runtime_service_blocked_count(const RuntimeServiceOwner *owner);
bool runtime_service_is_armed(const RuntimeServiceOwner *owner);
bool runtime_service_has_active_call(const RuntimeServiceOwner *owner);
RuntimeTask *runtime_service_pending_head(const RuntimeServiceOwner *owner);
RuntimeTask *runtime_service_blocked_head(const RuntimeServiceOwner *owner);
uint64_t runtime_service_call(
    uint64_t service,
    void *packet,
    uint64_t packet_size,
    uint64_t method,
    uint64_t encoding_depth
);
void runtime_service_receive(void);
void runtime_service_complete(void);
RuntimeTask *runtime_service_call_from_exception(RuntimeTask *caller);
RuntimeTask *runtime_service_receive_from_exception(RuntimeTask *owner_task);
RuntimeTask *runtime_service_complete_from_exception(RuntimeTask *owner_task);
