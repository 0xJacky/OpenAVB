#include "avp/runtime/ipc/runtime_capability.h"
#include "avp/runtime/task/runtime_event.h"


enum {
    RUNTIME_EVENT_CAPACITY = 16,
};

#if defined(__ELF__)
#define RUNTIME_EVENT_SECTION __attribute__((section(".runtime_task.state")))
#else
#define RUNTIME_EVENT_SECTION
#endif

static RuntimeEvent event_pool[RUNTIME_EVENT_CAPACITY] RUNTIME_EVENT_SECTION;
static uint8_t event_count RUNTIME_EVENT_SECTION;

static const RuntimeFirebloomType runtime_event_type = {
    .identity = UINT64_C(0x6576656e74),
    .element_size = sizeof(RuntimeEvent),
    .bytewise_copyable = false,
};

static void runtime_event_clear_object(RuntimeEvent *event) {
    uint64_t *words = (uint64_t *)event;
    for (uint32_t index = 0;
         index < sizeof(*event) / sizeof(uint64_t);
         ++index) {
        words[index] = 0;
    }
}

void runtime_event_runtime_initialize(void) {
    for (uint8_t index = 0; index < RUNTIME_EVENT_CAPACITY; ++index) {
        runtime_event_clear_object(&event_pool[index]);
    }
    event_count = 0;
}

static uint16_t runtime_event_initialize_with_rights(
    RuntimeEvent *event,
    bool auto_reset,
    bool initially_signaled,
    uint8_t access_options,
    uint8_t domain
) {
    const uint8_t rights = access_options & UINT8_C(7);
    const bool shared_audience = (access_options & UINT8_C(8)) != 0;
    if (event == 0 || rights == 0 ||
        (access_options & ~UINT8_C(0xf)) != 0 || domain == 0 || domain > 15 ||
        (shared_audience && (rights & UINT8_C(4)) != 0)) {
        return 0;
    }
    if (event->waiters.previous != 0 || event->waiters.next != 0 ||
        event->signaled != 0 || event->auto_reset != 0 ||
        event->owner_ordinal != 0) {
        __builtin_trap();
    }
    runtime_event_clear_object(event);
    event->auto_reset = auto_reset;
    event->signaled = initially_signaled;
    return runtime_capability_publish(
        event,
        &runtime_event_type,
        RUNTIME_CAPABILITY_TYPE_EVENT,
        rights,
        shared_audience
            ? RUNTIME_CAPABILITY_AUDIENCE_ALL
            : runtime_capability_audience_for_domain(domain)
    );
}

uint16_t runtime_event_initialize(
    RuntimeEvent *event,
    bool auto_reset,
    bool initially_signaled
) {
    return runtime_event_initialize_with_rights(
        event,
        auto_reset,
        initially_signaled,
        UINT8_C(0xb),
        runtime_current_domain()
    );
}

RuntimeTask *runtime_event_create_from_exception(RuntimeTask *trapped_task) {
    if (trapped_task == 0 || trapped_task != runtime_task_current() ||
        trapped_task->state != RUNTIME_TASK_STATE_RUNNING ||
        event_count == RUNTIME_EVENT_CAPACITY) {
        __builtin_trap();
    }
    RuntimeEvent *event = &event_pool[event_count++];
    const uint8_t domain = runtime_current_domain();
    if (domain == 0 || domain > 15) {
        __builtin_trap();
    }
    const uint16_t handle = runtime_event_initialize_with_rights(
        event,
        trapped_task->frame.x[0] != 0,
        trapped_task->frame.x[1] != 0,
        (uint8_t)trapped_task->frame.x[2],
        domain
    );
    if (handle == 0) {
        __builtin_trap();
    }
    trapped_task->frame.x[0] = handle;
    return trapped_task;
}
