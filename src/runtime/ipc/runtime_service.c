#include "avp/runtime/ipc/runtime_service.h"


enum {
    RUNTIME_SERVICE_CAPACITY = 8,
    RUNTIME_SERVICE_ARMED_TOKEN = 0xffff,
    RUNTIME_SERVICE_MESSAGE_WORDS = 4,
};

static RuntimeServiceOwner *service_owners[RUNTIME_SERVICE_CAPACITY];
static uint8_t service_owner_count;

static void runtime_service_reset_caller(RuntimeTask *caller) {
    if (caller == 0) {
        return;
    }
    *caller = (RuntimeTask){0};
}

static void runtime_service_copy(
    void *destination,
    const void *source,
    uint32_t size
) {
    uint8_t *output = destination;
    const uint8_t *input = source;
    for (uint32_t index = 0; index < size; ++index) {
        output[index] = input[index];
    }
}

static bool runtime_service_owner_is_registered(
    const RuntimeServiceOwner *owner
) {
    for (uint8_t index = 0; index < service_owner_count; ++index) {
        if (service_owners[index] == owner) {
            return true;
        }
    }
    return false;
}

static RuntimeServiceOwner *runtime_service_find(uint64_t service) {
    for (uint8_t index = 0; index < service_owner_count; ++index) {
        RuntimeServiceOwner *owner = service_owners[index];
        if (owner->domain->service_key == service) {
            return owner;
        }
    }
    return 0;
}

static RuntimeServiceOwner *runtime_service_owner_for_task(
    const RuntimeTask *task
) {
    for (uint8_t index = 0; index < service_owner_count; ++index) {
        if (&service_owners[index]->task == task) {
            return service_owners[index];
        }
    }
    return 0;
}

static RuntimeTask *runtime_service_waiter_head(
    const RuntimeServiceOwner *owner
) {
    return owner->endpoint.waiters.next;
}

static RuntimeTask *runtime_service_waiter_tail(
    const RuntimeServiceOwner *owner
) {
    return owner->endpoint.waiters.previous;
}

static void runtime_service_waiter_enqueue(
    RuntimeServiceOwner *owner,
    RuntimeTask *caller
) {
    if (caller == 0 || caller->ready_link.next != 0 ||
        caller->ready_link.previous != 0 ||
        caller == runtime_service_waiter_head(owner) ||
        caller == runtime_service_waiter_tail(owner)) {
        __builtin_trap();
    }
    RuntimeTask *tail = runtime_service_waiter_tail(owner);
    caller->ready_link.previous = tail;
    if (tail != 0) {
        tail->ready_link.next = caller;
    } else {
        owner->endpoint.waiters.next = caller;
    }
    owner->endpoint.waiters.previous = caller;
}

static RuntimeTask *runtime_service_waiter_dequeue(
    RuntimeServiceOwner *owner
) {
    RuntimeTask *caller = runtime_service_waiter_head(owner);
    if (caller == 0) {
        __builtin_trap();
    }
    owner->endpoint.waiters.next = caller->ready_link.next;
    RuntimeTask *head = runtime_service_waiter_head(owner);
    if (head != 0) {
        if (head->ready_link.previous != caller) {
            __builtin_trap();
        }
        head->ready_link.previous = 0;
    } else {
        if (runtime_service_waiter_tail(owner) != caller) {
            __builtin_trap();
        }
        owner->endpoint.waiters.previous = 0;
    }
    caller->ready_link.previous = 0;
    caller->ready_link.next = 0;
    return caller;
}

static uint64_t runtime_service_metadata(const RuntimeTask *caller) {
    return (caller->frame.x[2] & UINT64_C(0xffff)) |
        (uint64_t)caller->ipc_reply_token << 16 |
        (caller->frame.x[3] & UINT64_C(0xffff)) << 32 |
        (caller->frame.x[4] & UINT64_C(0x1f)) << 48 |
        (uint64_t)(uint8_t)caller->global_ordinal << 56;
}

static bool runtime_service_arguments_valid(
    const RuntimeServiceOwner *owner,
    const RuntimeTask *caller
) {
    const uint64_t packet = caller->frame.x[1];
    const uint64_t packet_size = caller->frame.x[2];
    const uint16_t method = (uint16_t)caller->frame.x[3];
    const uint64_t encoding_depth = caller->frame.x[4];
    if (packet == 0 || packet_size >= AVP_RUNTIME_SERVICE_PACKET_LIMIT ||
        (packet_size & 7) != 0 || method == 0 || encoding_depth >= 0x11 ||
        packet + packet_size < packet ||
        method >= owner->domain->service_method_count) {
        return false;
    }
    const uint16_t *acl = owner->domain->service_method_acl;
    return acl == 0 || acl[method] == 0 ||
        (acl[method] & (uint16_t)(1u << caller->domain->domain_id)) != 0;
}

static bool runtime_service_ring_has_space(
    const RuntimeServiceEndpoint *endpoint
) {
    return endpoint->count_words + RUNTIME_SERVICE_MESSAGE_WORDS <=
        endpoint->capacity_words;
}

static void runtime_service_ring_write(
    RuntimeServiceOwner *owner,
    RuntimeTask *caller
) {
    RuntimeServiceEndpoint *endpoint = &owner->endpoint;
    if (!runtime_service_ring_has_space(endpoint) ||
        !runtime_service_arguments_valid(owner, caller)) {
        __builtin_trap();
    }
    const uint64_t message[RUNTIME_SERVICE_MESSAGE_WORDS] = {
        caller->frame.x[0],
        caller->frame.x[1],
        runtime_service_metadata(caller),
        0,
    };
    for (uint16_t index = 0; index < RUNTIME_SERVICE_MESSAGE_WORDS; ++index) {
        endpoint->words[endpoint->write_index] = message[index];
        endpoint->write_index =
            (uint16_t)((endpoint->write_index + 1) % endpoint->capacity_words);
    }
    endpoint->count_words += RUNTIME_SERVICE_MESSAGE_WORDS;
}

static void runtime_service_promote_waiter(RuntimeServiceOwner *owner) {
    if (runtime_service_waiter_head(owner) == 0) {
        return;
    }
    RuntimeTask *caller = runtime_service_waiter_dequeue(owner);
    if (caller->state != RUNTIME_TASK_STATE_IPC_WAIT ||
        caller->ipc_inflight_token != caller->ipc_reply_token) {
        __builtin_trap();
    }
    runtime_service_ring_write(owner, caller);
}

static void runtime_service_ring_read(
    RuntimeServiceOwner *owner,
    uint64_t message[RUNTIME_SERVICE_MESSAGE_WORDS]
) {
    RuntimeServiceEndpoint *endpoint = &owner->endpoint;
    if (endpoint->count_words < RUNTIME_SERVICE_MESSAGE_WORDS) {
        __builtin_trap();
    }
    for (uint16_t index = 0; index < RUNTIME_SERVICE_MESSAGE_WORDS; ++index) {
        message[index] = endpoint->words[endpoint->read_index];
        endpoint->words[endpoint->read_index] = 0;
        endpoint->read_index =
            (uint16_t)((endpoint->read_index + 1) % endpoint->capacity_words);
    }
    endpoint->count_words -= RUNTIME_SERVICE_MESSAGE_WORDS;
    runtime_service_promote_waiter(owner);
}

static RuntimeTask *runtime_service_active_caller(
    const RuntimeServiceOwner *owner
) {
    const RuntimeDomain *domain = owner->domain;
    if (domain->service_active_token == 0 ||
        domain->service_active_token == RUNTIME_SERVICE_ARMED_TOKEN) {
        return 0;
    }
    return runtime_task_lookup_ordinal(
        domain->service_active_caller_ordinal
    );
}

static RuntimeTask *runtime_service_deliver_head(
    RuntimeServiceOwner *owner
) {
    RuntimeDomain *domain = owner->domain;
    if (runtime_service_active_caller(owner) != 0 ||
        (domain->service_active_token != 0 &&
            domain->service_active_token != RUNTIME_SERVICE_ARMED_TOKEN) ||
        owner->endpoint.count_words < RUNTIME_SERVICE_MESSAGE_WORDS) {
        __builtin_trap();
    }
    uint64_t message[RUNTIME_SERVICE_MESSAGE_WORDS];
    runtime_service_ring_read(owner, message);
    const uint64_t metadata = message[2];
    const uint8_t caller_ordinal = (uint8_t)(metadata >> 56);
    RuntimeTask *caller = runtime_task_lookup_ordinal(caller_ordinal);
    if (caller == 0 || message[0] != domain->service_key ||
        message[1] != (uintptr_t)caller->ipc_packet_mapping ||
        (uint16_t)metadata != caller->frame.x[2] ||
        (uint16_t)(metadata >> 16) != caller->ipc_reply_token ||
        (uint16_t)(metadata >> 32) != (uint16_t)caller->frame.x[3] ||
        ((metadata >> 48) & 0x1f) != caller->frame.x[4] ||
        caller->ipc_inflight_token != caller->ipc_reply_token) {
        __builtin_trap();
    }
    runtime_service_copy(
        domain->service_packet,
        caller->ipc_packet_mapping,
        (uint32_t)caller->frame.x[2]
    );
    domain->service_active_token = caller->ipc_reply_token;
    domain->service_active_caller_ordinal = caller_ordinal;
    owner->task.frame.x[3] = message[0];
    owner->task.frame.x[4] = (uintptr_t)domain->service_packet;
    owner->task.frame.x[5] = metadata;
    owner->task.frame.x[6] = 0;
    return caller;
}

__attribute__((noreturn))
static void runtime_service_owner_entry(void *context) {
    RuntimeServiceOwner *owner = context;
    for (;;) {
        runtime_service_receive();
        RuntimeTask *caller = runtime_service_active_caller(owner);
        if (caller == 0 || runtime_task_current() != &owner->task ||
            owner->handler == 0) {
            __builtin_trap();
        }
        owner->handler(
            owner->handler_context,
            (uint16_t)caller->frame.x[3],
            (uint16_t)caller->frame.x[4],
            owner->domain->service_packet,
            (uint32_t)caller->frame.x[2]
        );
        runtime_service_complete();
    }
}

static void runtime_service_registry_reset(void) {
    for (uint8_t index = 0; index < service_owner_count; ++index) {
        RuntimeServiceOwner *owner = service_owners[index];
        RuntimeDomain *domain = owner->domain;
        runtime_service_reset_caller(runtime_service_active_caller(owner));
        for (uint16_t offset = 0;
             offset < owner->endpoint.count_words;
             offset += RUNTIME_SERVICE_MESSAGE_WORDS) {
            const uint16_t metadata_index = (uint16_t)(
                (owner->endpoint.read_index + offset + 2) %
                    owner->endpoint.capacity_words
            );
            runtime_service_reset_caller(runtime_task_lookup_ordinal(
                (uint8_t)(owner->endpoint.words[metadata_index] >> 56)
            ));
        }
        RuntimeTask *waiter = runtime_service_waiter_head(owner);
        while (waiter != 0) {
            RuntimeTask *next = waiter->ready_link.next;
            runtime_service_reset_caller(waiter);
            waiter = next;
        }
        domain->service_method_acl = 0;
        domain->service_method_acl_lower_bound = 0;
        domain->service_method_acl_upper_bound = 0;
        domain->service_method_acl_cookie = 0;
        domain->service_method_count = 0;
        domain->service_packet = 0;
        domain->service_receive_endpoint_handle = 0;
        domain->service_active_token = 0;
        domain->service_send_endpoint_handle = 0;
        domain->service_active_caller_ordinal = 0;
        domain->service_key = 0;
        *owner = (RuntimeServiceOwner){0};
    }
    for (uint32_t index = 0; index < RUNTIME_SERVICE_CAPACITY; ++index) {
        service_owners[index] = 0;
    }
    service_owner_count = 0;
}

void runtime_service_runtime_initialize(void) {
    runtime_service_registry_reset();
    runtime_scheduler_initialize();
}

bool runtime_service_owner_initialize(
    RuntimeServiceOwner *owner,
    RuntimeDomain *domain,
    uintptr_t stack_top,
    void *provider_packet
) {
    if (owner == 0 || domain == 0 || provider_packet == 0 ||
        service_owner_count == RUNTIME_SERVICE_CAPACITY ||
        runtime_service_owner_is_registered(owner) ||
        domain->service_packet != 0 || domain->service_key != 0) {
        return false;
    }
    for (uint8_t index = 0; index < service_owner_count; ++index) {
        if (service_owners[index]->domain == domain) {
            return false;
        }
    }
    owner->endpoint = (RuntimeServiceEndpoint){
        .capacity_words = AVP_RUNTIME_SERVICE_ENDPOINT_WORDS,
        .flags = 1,
    };
    owner->domain = domain;
    owner->handler = 0;
    owner->handler_context = 0;
    owner->initialized = false;
    owner->started = false;
    domain->service_packet = provider_packet;
    domain->service_receive_endpoint_handle = service_owner_count + 1;
    domain->service_active_token = 0;
    domain->service_send_endpoint_handle = service_owner_count + 1;
    domain->service_active_caller_ordinal = 0;
    if (!runtime_task_register(
            &owner->task,
            domain,
            stack_top,
            runtime_service_owner_entry,
            owner
        )) {
        domain->service_packet = 0;
        domain->service_receive_endpoint_handle = 0;
        domain->service_send_endpoint_handle = 0;
        return false;
    }
    service_owners[service_owner_count++] = owner;
    owner->initialized = true;
    return true;
}

bool runtime_service_register(
    RuntimeServiceOwner *owner,
    uint64_t service,
    RuntimeServiceMethodHandler handler,
    void *handler_context
) {
    if (owner == 0 || !owner->initialized ||
        !runtime_service_owner_is_registered(owner) || owner->handler != 0 ||
        service == 0 || handler == 0 || runtime_service_find(service) != 0) {
        return false;
    }
    owner->domain->service_method_acl = 0;
    owner->domain->service_method_count = UINT16_MAX;
    owner->domain->service_key = service;
    owner->handler = handler;
    owner->handler_context = handler_context;
    return true;
}

bool runtime_service_owner_start(RuntimeServiceOwner *owner) {
    if (owner == 0 || !owner->initialized || owner->started ||
        owner->handler == 0 || !runtime_task_activate(&owner->task)) {
        return false;
    }
    owner->started = true;
    return true;
}

uint8_t runtime_service_pending_count(const RuntimeServiceOwner *owner) {
    return (uint8_t)(owner->endpoint.count_words /
        RUNTIME_SERVICE_MESSAGE_WORDS);
}

uint8_t runtime_service_blocked_count(const RuntimeServiceOwner *owner) {
    uint8_t count = 0;
    for (RuntimeTask *task = runtime_service_waiter_head(owner);
         task != 0;
         task = task->ready_link.next) {
        ++count;
    }
    return count;
}

bool runtime_service_is_armed(const RuntimeServiceOwner *owner) {
    return owner->domain->service_active_token == RUNTIME_SERVICE_ARMED_TOKEN;
}

bool runtime_service_has_active_call(const RuntimeServiceOwner *owner) {
    return runtime_service_active_caller(owner) != 0;
}

RuntimeTask *runtime_service_pending_head(const RuntimeServiceOwner *owner) {
    if (owner->endpoint.count_words < RUNTIME_SERVICE_MESSAGE_WORDS) {
        return 0;
    }
    const uint16_t metadata_index = (uint16_t)(
        (owner->endpoint.read_index + 2) % owner->endpoint.capacity_words
    );
    return runtime_task_lookup_ordinal(
        (uint8_t)(owner->endpoint.words[metadata_index] >> 56)
    );
}

RuntimeTask *runtime_service_blocked_head(const RuntimeServiceOwner *owner) {
    return runtime_service_waiter_head(owner);
}

RuntimeTask *runtime_service_call_from_exception(RuntimeTask *caller) {
    if (caller == 0 || caller != runtime_task_current() ||
        caller->state != RUNTIME_TASK_STATE_RUNNING ||
        caller->ipc_packet_mapping != 0 || caller->ipc_original_va != 0 ||
        caller->ipc_reply_token == 0 || caller->ipc_inflight_token != 0) {
        __builtin_trap();
    }
    RuntimeServiceOwner *owner = runtime_service_find(caller->frame.x[0]);
    if (owner == 0 ||
        !runtime_service_arguments_valid(owner, caller)) {
        __builtin_trap();
    }
    caller->ipc_packet_mapping = (void *)(uintptr_t)caller->frame.x[1];
    caller->ipc_original_va = caller->frame.x[1];
    caller->ipc_inflight_token = caller->ipc_reply_token;
    if (runtime_service_ring_has_space(&owner->endpoint)) {
        runtime_service_ring_write(owner, caller);
    } else {
        runtime_service_waiter_enqueue(owner, caller);
    }

    if (owner->task.state == RUNTIME_TASK_STATE_IPC_WAIT) {
        if (!runtime_service_is_armed(owner) ||
            runtime_service_has_active_call(owner) ||
            owner->endpoint.count_words < RUNTIME_SERVICE_MESSAGE_WORDS) {
            __builtin_trap();
        }
        RuntimeTask *delivered = runtime_service_deliver_head(owner);
        if (delivered != caller && delivered->state != RUNTIME_TASK_STATE_IPC_WAIT) {
            __builtin_trap();
        }
        return runtime_task_wait_and_wake_from_exception(caller, &owner->task);
    }
    return runtime_task_wait_from_exception(caller);
}

RuntimeTask *runtime_service_receive_from_exception(RuntimeTask *owner_task) {
    RuntimeServiceOwner *owner = runtime_service_owner_for_task(owner_task);
    if (owner == 0 || owner_task != runtime_task_current() ||
        owner_task->state != RUNTIME_TASK_STATE_RUNNING ||
        runtime_service_has_active_call(owner) ||
        owner->domain->service_active_token == RUNTIME_SERVICE_ARMED_TOKEN) {
        __builtin_trap();
    }
    owner->domain->service_active_token = RUNTIME_SERVICE_ARMED_TOKEN;
    owner->domain->service_active_caller_ordinal = 0;
    if (owner->endpoint.count_words >= RUNTIME_SERVICE_MESSAGE_WORDS) {
        (void)runtime_service_deliver_head(owner);
        return owner_task;
    }
    return runtime_task_wait_from_exception(owner_task);
}

RuntimeTask *runtime_service_complete_from_exception(RuntimeTask *owner_task) {
    RuntimeServiceOwner *owner = runtime_service_owner_for_task(owner_task);
    RuntimeTask *caller = owner == 0 ? 0 : runtime_service_active_caller(owner);
    if (owner == 0 || owner_task != runtime_task_current() ||
        owner_task->state != RUNTIME_TASK_STATE_RUNNING || caller == 0 ||
        caller->state != RUNTIME_TASK_STATE_IPC_WAIT ||
        caller->ipc_packet_mapping == 0 ||
        caller->ipc_packet_mapping != (void *)(uintptr_t)caller->frame.x[1] ||
        caller->ipc_original_va != caller->frame.x[1] ||
        caller->ipc_reply_token == 0 ||
        caller->ipc_inflight_token != caller->ipc_reply_token ||
        owner->domain->service_packet == 0 ||
        owner->domain->service_active_token != caller->ipc_reply_token ||
        owner->domain->service_active_caller_ordinal != caller->global_ordinal ||
        caller->ipc_original_va == 0) {
        __builtin_trap();
    }
    RuntimeTask *current = runtime_task_wake_from_exception(owner_task, caller);
    runtime_service_copy(
        (void *)caller->ipc_original_va,
        owner->domain->service_packet,
        (uint32_t)caller->frame.x[2]
    );
    caller->ipc_packet_mapping = 0;
    caller->ipc_original_va = 0;
    caller->ipc_inflight_token = 0;
    owner->domain->service_active_token = 0;
    owner->domain->service_active_caller_ordinal = 0;
    return current;
}
