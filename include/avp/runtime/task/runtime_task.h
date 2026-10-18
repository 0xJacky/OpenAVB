#pragma once

#include "avp/runtime/ipc/runtime_svc.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


typedef struct {
    uint64_t x[31];
    uint64_t signed_elr_el1;
    uint64_t sp_el0;
    uint64_t spsr_el1;
    uint64_t fpsr;
    uint64_t fpcr;
    uint64_t tpidr_el0;
    uint64_t tpidrro_el0;
    uint64_t ttbr1_el1;
    uint16_t full_vector_state;
    uint16_t address_space_selector;
    uint32_t reserved_13c;
    __uint128_t q[32];
    uint64_t stack_guard;
    uint8_t reserved_348[0x18];
    uint64_t ttbr0_el1;
    uint64_t reserved_368;
} __attribute__((aligned(16))) RuntimeTaskFrame;

_Static_assert(sizeof(RuntimeTaskFrame) == 0x370, "RuntimeTaskFrame size");
_Static_assert(
    offsetof(RuntimeTaskFrame, signed_elr_el1) == 0xf8,
    "signed ELR offset"
);
_Static_assert(offsetof(RuntimeTaskFrame, sp_el0) == 0x100, "SP_EL0 offset");
_Static_assert(offsetof(RuntimeTaskFrame, spsr_el1) == 0x108, "SPSR offset");
_Static_assert(offsetof(RuntimeTaskFrame, fpsr) == 0x110, "FPSR offset");
_Static_assert(offsetof(RuntimeTaskFrame, fpcr) == 0x118, "FPCR offset");
_Static_assert(offsetof(RuntimeTaskFrame, tpidr_el0) == 0x120, "TPIDR offset");
_Static_assert(offsetof(RuntimeTaskFrame, q[8]) == 0x1c0, "Q8 offset");
_Static_assert(offsetof(RuntimeTaskFrame, stack_guard) == 0x340, "guard offset");
_Static_assert(offsetof(RuntimeTaskFrame, ttbr0_el1) == 0x360, "TTBR0 offset");

typedef struct RuntimeTaskQueueLink {
    void *previous;
    uintptr_t previous_lower_bound;
    uintptr_t previous_upper_bound;
    uintptr_t previous_cookie;
    void *next;
    uintptr_t next_lower_bound;
    uintptr_t next_upper_bound;
    uintptr_t next_cookie;
} RuntimeTaskQueueLink;

_Static_assert(sizeof(RuntimeTaskQueueLink) == 0x40, "task link size");
_Static_assert(
    offsetof(RuntimeTaskQueueLink, next) == 0x20,
    "next capability"
);

struct RuntimeTimerEvent;
struct RuntimeEvent;
typedef void (*RuntimeTimerCallback)(
    struct RuntimeTimerEvent *event,
    void *context
);

typedef struct RuntimeTimerEvent {
    RuntimeTaskQueueLink queue_link;
    uint64_t deadline;
    uint64_t reserved_48;
    RuntimeTimerCallback callback;
    void *callback_metadata;
    struct RuntimeTask *owner;
    uintptr_t owner_lower_bound;
    uintptr_t owner_upper_bound;
    uintptr_t owner_cookie;
} RuntimeTimerEvent;

_Static_assert(sizeof(RuntimeTimerEvent) == 0x80, "timer event size");
_Static_assert(offsetof(RuntimeTimerEvent, deadline) == 0x40, "timer deadline");
_Static_assert(offsetof(RuntimeTimerEvent, callback) == 0x50, "timer callback");
_Static_assert(offsetof(RuntimeTimerEvent, owner) == 0x60, "timer owner");

typedef enum {
    RUNTIME_TASK_STATE_EMPTY = 0,
    RUNTIME_TASK_STATE_INITIALIZED = 1,
    RUNTIME_TASK_STATE_READY = 2,
    RUNTIME_TASK_STATE_RUNNING = 3,
    RUNTIME_TASK_STATE_PARKED = 4,
    RUNTIME_TASK_STATE_IPC_WAIT = 5,
    RUNTIME_TASK_STATE_TIMER_WAIT = 6,
    RUNTIME_TASK_STATE_EXITED = 7,
} RuntimeTaskState;

typedef struct {
    struct RuntimeEvent *event;
    uintptr_t event_lower_bound;
    uintptr_t event_upper_bound;
    uintptr_t event_cookie;
} RuntimeTaskWaitBinding;

_Static_assert(sizeof(RuntimeTaskWaitBinding) == 0x20, "wait binding size");

typedef void (*RuntimeTaskEntry)(void *context);

typedef struct {
    RuntimeTaskEntry entry;
    uintptr_t entry_lower_bound;
    uintptr_t entry_upper_bound;
    uintptr_t entry_cookie;
    void *entry_context;
    uintptr_t context_cookie;
} RuntimeTaskDescriptor;

_Static_assert(
    sizeof(RuntimeTaskDescriptor) == 0x30,
    "task descriptor size"
);

typedef struct RuntimeTask {
    RuntimeTaskFrame frame;
    RuntimeTaskQueueLink all_tasks_link;
    RuntimeTaskQueueLink ready_link;
    uint8_t reserved_3f0[0x10];
    uint32_t global_ordinal;
    uint32_t domain_ordinal;
    uint32_t state;
    uint32_t reserved_40c;
    RuntimeTimerEvent timer_event;
    RuntimeDomain *domain;
    uintptr_t domain_lower_bound;
    uintptr_t domain_upper_bound;
    uintptr_t domain_cookie;

    RuntimeTaskWaitBinding wait_binding;
    RuntimeTaskDescriptor descriptor;
    void *stack_mapping;
    void *stack_allocation;
    uintptr_t stack_base;
    uintptr_t stack_top;
    uintptr_t stack_guard_base;
    uintptr_t startup_sp;
    uintptr_t startup_context;
    RuntimeTaskQueueLink join_waiters;
    int32_t exit_result;
    uint32_t reserved_57c;
    void *lifecycle_aux;
    struct RuntimeTask *join_target;
    uintptr_t frame_context;
    void *ipc_packet_mapping;
    uintptr_t ipc_original_va;
    uint16_t reserved_5a8;
    uint16_t ipc_reply_token;
    uint16_t ipc_inflight_token;
    uint8_t projected_flag;
    uint8_t reserved_5af;
} __attribute__((aligned(16))) RuntimeTask;

_Static_assert(sizeof(RuntimeTask) == 0x5b0, "RuntimeTask size");
_Static_assert(offsetof(RuntimeTask, all_tasks_link) == 0x370, "all-task link");
_Static_assert(offsetof(RuntimeTask, ready_link) == 0x3b0, "ready link");
_Static_assert(offsetof(RuntimeTask, global_ordinal) == 0x400, "global ordinal");
_Static_assert(offsetof(RuntimeTask, state) == 0x408, "task state");
_Static_assert(offsetof(RuntimeTask, timer_event) == 0x410, "timer event");
_Static_assert(offsetof(RuntimeTask, domain) == 0x490, "domain record");
_Static_assert(offsetof(RuntimeTask, wait_binding) == 0x4b0, "wait binding");
_Static_assert(offsetof(RuntimeTask, descriptor) == 0x4d0, "task descriptor");
_Static_assert(offsetof(RuntimeTask, stack_mapping) == 0x500, "stack mapping");
_Static_assert(
    offsetof(RuntimeTask, stack_allocation) == 0x508,
    "stack allocation"
);
_Static_assert(offsetof(RuntimeTask, stack_base) == 0x510, "stack base");
_Static_assert(offsetof(RuntimeTask, stack_top) == 0x518, "stack top");
_Static_assert(
    offsetof(RuntimeTask, join_waiters) == 0x538,
    "join waiter queue"
);
_Static_assert(offsetof(RuntimeTask, exit_result) == 0x578, "exit result");
_Static_assert(offsetof(RuntimeTask, ipc_packet_mapping) == 0x598, "IPC mapping");
_Static_assert(offsetof(RuntimeTask, ipc_original_va) == 0x5a0, "IPC original VA");
_Static_assert(offsetof(RuntimeTask, ipc_reply_token) == 0x5aa, "IPC reply token");
_Static_assert(offsetof(RuntimeTask, ipc_inflight_token) == 0x5ac, "IPC inflight token");
_Static_assert(offsetof(RuntimeTask, projected_flag) == 0x5ae, "projection flag");

void runtime_scheduler_initialize(void);
bool runtime_task_register(
    RuntimeTask *task,
    RuntimeDomain *domain,
    uintptr_t stack_top,
    RuntimeTaskEntry entry,
    void *entry_context
);
bool runtime_task_register_parked(
    RuntimeTask *task,
    RuntimeDomain *domain,
    uintptr_t stack_top,
    RuntimeTaskEntry entry,
    void *entry_context
);
bool runtime_task_activate(RuntimeTask *task);
uint16_t runtime_task_capability_handle(const RuntimeTask *task);
RuntimeTask *runtime_task_activate_from_exception(
    RuntimeTask *trapped_task,
    uint16_t capability_handle,
    uint64_t stack_guard_seed
);
RuntimeTask *runtime_task_create_from_exception(RuntimeTask *trapped_task);
RuntimeTask *runtime_task_exit_from_exception(
    RuntimeTask *trapped_task,
    int32_t result
);
RuntimeTask *runtime_task_join_from_exception(
    RuntimeTask *trapped_task,
    uint16_t capability_handle
);
RuntimeTask *runtime_task_wait_until_from_exception(
    RuntimeTask *trapped_task,
    uint64_t deadline
);
RuntimeTask *runtime_task_idle_pump_from_exception(RuntimeTask *trapped_task);
RuntimeTask *runtime_task_timer_fiq_from_exception(RuntimeTask *trapped_task);
RuntimeTask *runtime_task_schedule_from_exception(RuntimeTask *trapped_task);
RuntimeTask *runtime_task_wait_and_wake_from_exception(
    RuntimeTask *trapped_task,
    RuntimeTask *wake_task
);
RuntimeTask *runtime_task_wait_from_exception(RuntimeTask *trapped_task);
RuntimeTask *runtime_task_wake_from_exception(
    RuntimeTask *trapped_task,
    RuntimeTask *wake_task
);
RuntimeTask *runtime_task_lookup_ordinal(uint8_t ordinal);
RuntimeTask *runtime_task_require_exception_context(void);
const RuntimeTask *runtime_task_current(void);
const RuntimeTask *runtime_task_idle(void);
uint32_t runtime_task_ready_count(void);
uint32_t runtime_task_switch_count(void);
void runtime_task_wait_until(uint64_t deadline);
uint64_t runtime_idle_pump(void);
void runtime_task_yield(void);
uint64_t runtime_task_mask_interrupts(void);
uint64_t runtime_task_unmask_interrupts(void);
void runtime_task_activate_capability(
    uint16_t capability_handle,
    uint64_t stack_guard_seed
);
uint16_t runtime_task_create(
    uint16_t domain_handle,
    uint8_t rights,
    const RuntimeTaskDescriptor *descriptor,
    uintptr_t descriptor_lower_bound,
    uintptr_t descriptor_upper_bound,
    bool projected
);
__attribute__((noreturn)) void runtime_task_exit(int32_t result);
int32_t runtime_task_join(uint16_t capability_handle);
