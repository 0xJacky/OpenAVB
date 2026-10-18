#include "avp/runtime/ipc/runtime_capability.h"
#include "avp/runtime/task/runtime_event.h"
#include "avp/runtime/support/runtime_state.h"
#include "avp/runtime/ipc/runtime_svc.h"
#include "avp/runtime/task/runtime_task.h"
#include "avp/runtime/support/recovered_symbol.h"
#include "avp/runtime/ipc/runtime_rpc_codec_recovered.h"
#include "avp/runtime/task/runtime_timer.h"


enum {
    RUNTIME_TASK_CAPACITY = 20,
    RUNTIME_DYNAMIC_TASK_CAPACITY = RUNTIME_TASK_CAPACITY - 1,
    RUNTIME_DYNAMIC_STACK_SIZE = 0x4000,
    RUNTIME_IDLE_STACK_TOP = 0x701c0000,
};

#if defined(__ELF__)
#define RUNTIME_SECTION(name) __attribute__((section(name)))
#else
#define RUNTIME_SECTION(name)
#endif

extern void runtime_task_arch_initialize_frame(RuntimeTaskFrame *frame);
extern void runtime_task_arch_set_current(RuntimeTask *task);
extern uint64_t runtime_task_arch_generate_guard_seed(void);
#if defined(__ELF__)
extern uint64_t runtime_task_arch_sign_exception_return(uint64_t address);
#else
static uint64_t runtime_task_arch_sign_exception_return(uint64_t address) {
    return address;
}
#endif

static RuntimeTask boot_task
    RUNTIME_SECTION(".runtime_task.boot");
static RuntimeTask idle_task
    RUNTIME_SECTION(".runtime_task.idle");
static RuntimeTask *current_task
    RUNTIME_SECTION(".runtime_task.state");
static RuntimeTask *registered_tasks[RUNTIME_TASK_CAPACITY]
    RUNTIME_SECTION(".runtime_task.state");
static uint16_t task_capability_handles[RUNTIME_TASK_CAPACITY]
    RUNTIME_SECTION(".runtime_task.state");
static RuntimeTaskQueueLink ready_queue_root
    RUNTIME_SECTION(".runtime_ready.queue");
static uint8_t registered_task_count
    RUNTIME_SECTION(".runtime_task.state");
static uint32_t ready_count
    RUNTIME_SECTION(".runtime_ready.count");
static uint32_t switch_count
    RUNTIME_SECTION(".runtime_task.state");
static RuntimeTask dynamic_tasks[RUNTIME_DYNAMIC_TASK_CAPACITY]
    RUNTIME_SECTION(".runtime_task.state");
static uint8_t dynamic_stacks[
    RUNTIME_DYNAMIC_TASK_CAPACITY
][RUNTIME_DYNAMIC_STACK_SIZE]
    __attribute__((aligned(16))) RUNTIME_SECTION(".runtime_task.state");
static bool dynamic_slots[RUNTIME_DYNAMIC_TASK_CAPACITY]
    RUNTIME_SECTION(".runtime_task.state");
typedef struct {
    RuntimeTask *task;
    uintptr_t lower_bound;
    uintptr_t upper_bound;
    uintptr_t cookie;
} RuntimeParkedSlot;
static RuntimeParkedSlot parked_slot
    RUNTIME_SECTION(".runtime_task.parked");

static const RuntimeFirebloomType runtime_task_type = {
    .identity = UINT64_C(0x7461736b),
    .element_size = sizeof(RuntimeTask),
    .bytewise_copyable = false,
};

__attribute__((noreturn))
static void runtime_idle_entry(void *context) {
    (void)context;
    for (;;) {
        (void)runtime_idle_pump();
    }
}

static void runtime_task_clear(RuntimeTask *task) {
    uint64_t *words = (uint64_t *)task;
    for (uint32_t index = 0; index < sizeof(*task) / sizeof(uint64_t); ++index) {
        words[index] = 0;
    }
}

static bool runtime_task_is_registered(const RuntimeTask *task) {
    for (uint8_t index = 0; index < registered_task_count; ++index) {
        if (registered_tasks[index] == task) {
            return true;
        }
    }
    return false;
}

static RuntimeTask *runtime_ready_head(void) {
    return ready_queue_root.next;
}

static RuntimeTask *runtime_ready_tail(void) {
    return ready_queue_root.previous;
}

static bool runtime_task_register_common(
    RuntimeTask *task,
    RuntimeDomain *domain,
    uintptr_t stack_top,
    uint8_t capability_rights
) {
    if (task == 0 || domain == 0 || (stack_top & 0xf) != 0 ||
        current_task == 0 || registered_task_count == RUNTIME_TASK_CAPACITY ||
        runtime_domain_lookup(domain->domain_id) != domain ||
        runtime_task_is_registered(task) || capability_rights == 0 ||
        (capability_rights & ~UINT8_C(7)) != 0) {
        return false;
    }

    runtime_task_clear(task);
    task->domain = domain;
    task->domain_lower_bound = (uintptr_t)domain;
    task->domain_upper_bound = (uintptr_t)(domain + 1);
    task->global_ordinal = registered_task_count;
    task->domain_ordinal = domain->task_ordinal_count++;
    task->stack_top = stack_top;
    task->state = RUNTIME_TASK_STATE_INITIALIZED;
    task->ipc_reply_token = (uint16_t)(task->global_ordinal + 1);
    registered_tasks[registered_task_count++] = task;
    runtime_state_publish_task_count(registered_task_count);
    task_capability_handles[task->global_ordinal] =
        runtime_capability_publish(
            task,
            &runtime_task_type,
            RUNTIME_CAPABILITY_TYPE_TASK,
            capability_rights,
            RUNTIME_CAPABILITY_AUDIENCE_ALL
        );
    if (task_capability_handles[task->global_ordinal] == 0) {
        __builtin_trap();
    }
    return true;
}

static int32_t runtime_dynamic_task_index(const RuntimeTask *task) {
    for (uint32_t index = 0; index < RUNTIME_DYNAMIC_TASK_CAPACITY; ++index) {
        if (&dynamic_tasks[index] == task && dynamic_slots[index]) {
            return (int32_t)index;
        }
    }
    return -1;
}

static RuntimeTask *runtime_dynamic_task_allocate(void) {
    for (uint32_t index = 0; index < RUNTIME_DYNAMIC_TASK_CAPACITY; ++index) {
        if (!dynamic_slots[index]) {
            dynamic_slots[index] = true;
            return &dynamic_tasks[index];
        }
    }
    __builtin_trap();
}

static RuntimeDomain *runtime_task_resolve_domain_handle(uint16_t handle) {
    for (uint8_t domain_id = 1; domain_id <= 15; ++domain_id) {
        RuntimeDomain *domain = runtime_domain_lookup(domain_id);
        if (domain != 0 && domain->runtime_handle == handle) {
            return domain;
        }
    }
    return 0;
}

static void runtime_task_validate_descriptor(
    const RuntimeTaskDescriptor *descriptor,
    uintptr_t lower_bound,
    uintptr_t upper_bound
) {
    const uintptr_t address = (uintptr_t)descriptor;
    if (descriptor == 0 || lower_bound > address || address >= upper_bound ||
        upper_bound - address < sizeof(*descriptor) ||
        descriptor->entry == 0 ||
        descriptor->entry_lower_bound > (uintptr_t)descriptor->entry ||
        (uintptr_t)descriptor->entry >= descriptor->entry_upper_bound) {
        __builtin_trap();
    }
}

static void runtime_task_acquire_stack(RuntimeTask *task) {
    const int32_t index = runtime_dynamic_task_index(task);
    if (index < 0 || task->stack_mapping != 0 ||
        task->stack_allocation != 0 || task->stack_base != 0 ||
        task->stack_top != 0) {
        __builtin_trap();
    }
    uint8_t *stack = dynamic_stacks[index];
    for (uint32_t offset = 0; offset < RUNTIME_DYNAMIC_STACK_SIZE; ++offset) {
        stack[offset] = 0;
    }
    task->stack_mapping = stack;
    task->stack_allocation = stack;
    task->stack_base = (uintptr_t)stack;
    task->stack_top = (uintptr_t)(stack + RUNTIME_DYNAMIC_STACK_SIZE);
    task->stack_guard_base = task->stack_base;
    task->startup_sp = task->stack_top;
}

static void runtime_task_release_stack(RuntimeTask *task) {
    if (runtime_dynamic_task_index(task) < 0 || task->stack_mapping == 0 ||
        task->stack_allocation == 0 || task->stack_base == 0 ||
        task->stack_top <= task->stack_base ||
        task->stack_top - task->stack_base != RUNTIME_DYNAMIC_STACK_SIZE ||
        task->stack_mapping != task->stack_allocation ||
        (uintptr_t)task->stack_allocation != task->stack_base) {
        __builtin_trap();
    }
    uint8_t *stack = task->stack_allocation;
    for (uint32_t index = 0; index < RUNTIME_DYNAMIC_STACK_SIZE; ++index) {
        stack[index] = 0;
    }
    task->stack_mapping = 0;
    task->stack_allocation = 0;
    task->stack_base = 0;
    task->stack_top = 0;
    task->stack_guard_base = 0;
    task->startup_sp = 0;
    task->startup_context = 0;
}

static void runtime_ready_enqueue(RuntimeTask *task) {
    if (task == 0 || task->state != RUNTIME_TASK_STATE_READY ||
        task->ready_link.next != 0 || task->ready_link.previous != 0 ||
        task == runtime_ready_head() || task == runtime_ready_tail()) {
        __builtin_trap();
    }
    RuntimeTask *tail = runtime_ready_tail();
    task->ready_link.previous = tail;
    if (tail != 0) {
        tail->ready_link.next = task;
    } else {
        ready_queue_root.next = task;
    }
    ready_queue_root.previous = task;
    ++ready_count;
}

static RuntimeTask *runtime_ready_dequeue(void) {
    RuntimeTask *task = runtime_ready_head();
    if (task == 0 || ready_count == 0) {
        __builtin_trap();
    }
    ready_queue_root.next = task->ready_link.next;
    RuntimeTask *head = runtime_ready_head();
    if (head != 0) {
        head->ready_link.previous = 0;
    } else {
        ready_queue_root.previous = 0;
    }
    task->ready_link.next = 0;
    task->ready_link.previous = 0;
    --ready_count;
    return task;
}

static RuntimeTask *runtime_join_waiter_head(const RuntimeTask *target) {
    return target->join_waiters.next;
}

static RuntimeTask *runtime_join_waiter_tail(const RuntimeTask *target) {
    return target->join_waiters.previous;
}

static void runtime_join_waiter_enqueue(
    RuntimeTask *target,
    RuntimeTask *waiter
) {
    if (target == 0 || waiter == 0 || waiter->join_target != target ||
        waiter->state != RUNTIME_TASK_STATE_IPC_WAIT ||
        waiter->ready_link.next != 0 || waiter->ready_link.previous != 0 ||
        waiter == runtime_join_waiter_head(target) ||
        waiter == runtime_join_waiter_tail(target)) {
        __builtin_trap();
    }
    RuntimeTask *tail = runtime_join_waiter_tail(target);
    waiter->ready_link.previous = tail;
    if (tail != 0) {
        tail->ready_link.next = waiter;
    } else {
        target->join_waiters.next = waiter;
    }
    target->join_waiters.previous = waiter;
}

static RuntimeTask *runtime_join_waiter_dequeue(RuntimeTask *target) {
    RuntimeTask *waiter = runtime_join_waiter_head(target);
    if (waiter == 0 || waiter->join_target != target ||
        waiter->state != RUNTIME_TASK_STATE_IPC_WAIT) {
        __builtin_trap();
    }
    target->join_waiters.next = waiter->ready_link.next;
    RuntimeTask *head = runtime_join_waiter_head(target);
    if (head != 0) {
        if (head->ready_link.previous != waiter) {
            __builtin_trap();
        }
        head->ready_link.previous = 0;
    } else {
        if (runtime_join_waiter_tail(target) != waiter) {
            __builtin_trap();
        }
        target->join_waiters.previous = 0;
    }
    waiter->ready_link.next = 0;
    waiter->ready_link.previous = 0;
    waiter->join_target = 0;
    return waiter;
}

static void runtime_task_wake_joiners(RuntimeTask *target) {
    while (runtime_join_waiter_head(target) != 0) {
        RuntimeTask *waiter = runtime_join_waiter_dequeue(target);
        waiter->frame.x[0] = (uint64_t)(int64_t)target->exit_result;
        waiter->state = RUNTIME_TASK_STATE_READY;
        runtime_ready_enqueue(waiter);
    }
}

static RuntimeTask *runtime_event_waiter_head(const RuntimeEvent *event) {
    return event->waiters.next;
}

static RuntimeTask *runtime_event_waiter_tail(const RuntimeEvent *event) {
    return event->waiters.previous;
}

static void runtime_event_waiter_enqueue(
    RuntimeEvent *event,
    RuntimeTask *task
) {
    if (event == 0 || task == 0 ||
        task->state != RUNTIME_TASK_STATE_IPC_WAIT ||
        task->ready_link.next != 0 || task->ready_link.previous != 0 ||
        task == runtime_event_waiter_head(event) ||
        task == runtime_event_waiter_tail(event)) {
        __builtin_trap();
    }
    RuntimeTask *tail = runtime_event_waiter_tail(event);
    task->ready_link.previous = tail;
    if (tail != 0) {
        tail->ready_link.next = task;
    } else {
        event->waiters.next = task;
    }
    event->waiters.previous = task;
}

static void runtime_event_waiter_remove(
    RuntimeEvent *event,
    RuntimeTask *task
) {
    if (event == 0 || task == 0 ||
        task->wait_binding.event != event) {
        __builtin_trap();
    }
    RuntimeTask *previous = task->ready_link.previous;
    RuntimeTask *next = task->ready_link.next;
    if (previous != 0) {
        if (previous->ready_link.next != task) {
            __builtin_trap();
        }
        previous->ready_link.next = next;
    } else {
        if (runtime_event_waiter_head(event) != task) {
            __builtin_trap();
        }
        event->waiters.next = next;
    }
    if (next != 0) {
        if (next->ready_link.previous != task) {
            __builtin_trap();
        }
        next->ready_link.previous = previous;
    } else {
        if (runtime_event_waiter_tail(event) != task) {
            __builtin_trap();
        }
        event->waiters.previous = previous;
    }
    task->ready_link.previous = 0;
    task->ready_link.next = 0;
}

static void runtime_task_publish(RuntimeTask *task) {
    if (task == 0 || task->domain == 0) {
        __builtin_trap();
    }
    runtime_domain_set_current(task->domain);
    runtime_state_publish_domain(
        task->domain->domain_id,
        task->domain->runtime_handle
    );
    task->state = RUNTIME_TASK_STATE_RUNNING;
    current_task = task;
    runtime_current_task_rpc_arena_slot_publish(task->ipc_packet_mapping);
    runtime_task_arch_set_current(task);
}

static RuntimeTask *runtime_task_select_after_block(RuntimeTask *trapped_task) {
    RuntimeTask *next = ready_count == 0
        ? &idle_task
        : runtime_ready_dequeue();
    if (next == trapped_task || next->state != RUNTIME_TASK_STATE_READY) {
        __builtin_trap();
    }
    runtime_task_publish(next);
    ++switch_count;
    return next;
}

void __attribute__((used, retain)) runtime_task_timer_expired(
    RuntimeTimerEvent *event,
    void *context
) {
    RuntimeTask *task = context;
    if (task == 0 || event != &task->timer_event ||
        task->state != RUNTIME_TASK_STATE_TIMER_WAIT) {
        __builtin_trap();
    }
    task->state = RUNTIME_TASK_STATE_READY;
    runtime_ready_enqueue(task);
}

void __attribute__((used, retain)) runtime_task_event_timeout(
    RuntimeTimerEvent *event,
    void *context
) {
    RuntimeTask *task = context;
    RuntimeEvent *waited_event = task == 0
        ? 0
        : task->wait_binding.event;
    if (task == 0 || event != &task->timer_event || waited_event == 0 ||
        task->state != RUNTIME_TASK_STATE_IPC_WAIT ||
        task->wait_binding.event_lower_bound != (uintptr_t)waited_event ||
        task->wait_binding.event_upper_bound !=
            (uintptr_t)(waited_event + 1)) {
        __builtin_trap();
    }
    runtime_event_waiter_remove(waited_event, task);
    task->wait_binding = (RuntimeTaskWaitBinding){0};
    task->frame.x[0] = 0;
    task->state = RUNTIME_TASK_STATE_READY;
    runtime_ready_enqueue(task);
}

void runtime_scheduler_initialize(void) {
    runtime_state_initialize();
    runtime_capability_initialize();
    runtime_domain_reset(AVP_RUNTIME_DOMAIN_BOOT);
    runtime_event_runtime_initialize();
    runtime_task_clear(&boot_task);
    boot_task.domain = runtime_domain_lookup(AVP_RUNTIME_DOMAIN_BOOT);
    boot_task.domain_lower_bound = (uintptr_t)boot_task.domain;
    boot_task.domain_upper_bound = (uintptr_t)(boot_task.domain + 1);
    boot_task.global_ordinal = 0;
    boot_task.domain_ordinal = 0;
    boot_task.state = RUNTIME_TASK_STATE_RUNNING;
    boot_task.ipc_reply_token = 1;
    runtime_task_arch_initialize_frame(&boot_task.frame);
    runtime_task_clear(&idle_task);
    idle_task.domain = boot_task.domain;
    idle_task.domain_lower_bound = boot_task.domain_lower_bound;
    idle_task.domain_upper_bound = boot_task.domain_upper_bound;
    idle_task.global_ordinal = UINT32_MAX;
    idle_task.domain_ordinal = UINT32_MAX;
    idle_task.state = RUNTIME_TASK_STATE_READY;
    idle_task.frame.x[30] = UINT64_C(0xdeadbeefcafef00d);
    idle_task.frame.signed_elr_el1 =
        runtime_task_arch_sign_exception_return(
            (uintptr_t)runtime_idle_entry
        );
    idle_task.frame.sp_el0 = RUNTIME_IDLE_STACK_TOP;
    idle_task.frame.spsr_el1 = 0;
    idle_task.frame.full_vector_state = 1;
    idle_task.descriptor = (RuntimeTaskDescriptor){
        .entry = runtime_idle_entry,
        .entry_lower_bound = (uintptr_t)runtime_idle_entry,
        .entry_upper_bound = (uintptr_t)runtime_idle_entry + 4,
    };
    idle_task.stack_top = RUNTIME_IDLE_STACK_TOP;
    runtime_task_arch_initialize_frame(&idle_task.frame);
    current_task = &boot_task;
    registered_tasks[0] = &boot_task;
    task_capability_handles[0] = 0;
    for (uint8_t index = 1; index < RUNTIME_TASK_CAPACITY; ++index) {
        registered_tasks[index] = 0;
        task_capability_handles[index] = 0;
    }
    for (uint8_t index = 0;
         index < RUNTIME_DYNAMIC_TASK_CAPACITY;
         ++index) {
        dynamic_slots[index] = false;
    }
    registered_task_count = 1;
    runtime_state_publish_task_count(registered_task_count);
    runtime_state_publish_domain(
        boot_task.domain->domain_id,
        boot_task.domain->runtime_handle
    );
    uint64_t *ready_words = (uint64_t *)&ready_queue_root;
    for (uint32_t index = 0;
         index < sizeof(ready_queue_root) / sizeof(uint64_t);
         ++index) {
        ready_words[index] = 0;
    }
    ready_count = 0;
    switch_count = 0;
    parked_slot = (RuntimeParkedSlot){0};
    runtime_timer_initialize();
    runtime_current_task_rpc_arena_slot_publish(
        boot_task.ipc_packet_mapping
    );
    runtime_task_arch_set_current(&boot_task);
}

AVP_RECOVERED_ENTRY bool runtime_task_register(
    RuntimeTask *task,
    RuntimeDomain *domain,
    uintptr_t stack_top,
    RuntimeTaskEntry entry,
    void *entry_context
) {
    if (entry == 0 || stack_top == 0 ||
        !runtime_task_register_common(
            task,
            domain,
            stack_top,
            RUNTIME_CAPABILITY_RIGHT_WAIT |
                RUNTIME_CAPABILITY_RIGHT_SIGNAL
        )) {
        return false;
    }
    task->frame.x[0] = (uintptr_t)entry_context;
    task->frame.x[30] = UINT64_C(0xdeadbeefcafef00d);
    task->frame.signed_elr_el1 = runtime_task_arch_sign_exception_return(
        (uintptr_t)entry
    );
    task->frame.sp_el0 = stack_top;
    task->frame.spsr_el1 = 0;
    task->frame.full_vector_state = 1;
    task->descriptor = (RuntimeTaskDescriptor){
        .entry = entry,
        .entry_lower_bound = (uintptr_t)entry,
        .entry_upper_bound = (uintptr_t)entry + 4,
        .entry_context = entry_context,
    };
    task->startup_sp = stack_top;
    task->startup_context = (uintptr_t)entry_context;
    runtime_task_arch_initialize_frame(&task->frame);
    return true;
}

bool runtime_task_register_parked(
    RuntimeTask *task,
    RuntimeDomain *domain,
    uintptr_t stack_top,
    RuntimeTaskEntry entry,
    void *entry_context
) {
    if (!runtime_task_register(task, domain, stack_top, entry, entry_context)) {
        return false;
    }
    task->state = RUNTIME_TASK_STATE_IPC_WAIT;
    return true;
}

bool runtime_task_activate(RuntimeTask *task) {
    if (!runtime_task_is_registered(task) ||
        task->state != RUNTIME_TASK_STATE_INITIALIZED) {
        return false;
    }
    task->state = RUNTIME_TASK_STATE_READY;
    runtime_ready_enqueue(task);
    return true;
}

RuntimeTask *runtime_task_schedule_from_exception(RuntimeTask *trapped_task) {
    if (trapped_task == 0 || trapped_task != current_task ||
        trapped_task->state != RUNTIME_TASK_STATE_RUNNING) {
        __builtin_trap();
    }
    if (ready_count == 0) {
        return trapped_task;
    }

    trapped_task->state = RUNTIME_TASK_STATE_READY;
    if (trapped_task != &idle_task) {
        runtime_ready_enqueue(trapped_task);
    }
    RuntimeTask *next = runtime_ready_dequeue();
    if (next == trapped_task || next->state != RUNTIME_TASK_STATE_READY) {
        __builtin_trap();
    }
    runtime_task_publish(next);
    ++switch_count;
    return next;
}

RuntimeTask *runtime_task_wait_until_from_exception(
    RuntimeTask *trapped_task,
    uint64_t deadline
) {
    if (trapped_task == 0 || trapped_task != current_task ||
        trapped_task == &idle_task ||
        trapped_task->state != RUNTIME_TASK_STATE_RUNNING ||
        runtime_timer_is_armed(&trapped_task->timer_event)) {
        __builtin_trap();
    }
    trapped_task->state = RUNTIME_TASK_STATE_TIMER_WAIT;
    if (!runtime_timer_arm(
            &trapped_task->timer_event,
            deadline,
            runtime_task_timer_expired,
            trapped_task
        )) {
        __builtin_trap();
    }
    return runtime_task_select_after_block(trapped_task);
}

RuntimeTask *runtime_task_idle_pump_from_exception(RuntimeTask *trapped_task) {
    if (trapped_task != &idle_task || trapped_task != current_task ||
        trapped_task->state != RUNTIME_TASK_STATE_RUNNING ||
        runtime_current_domain() != AVP_RUNTIME_DOMAIN_BOOT) {
        __builtin_trap();
    }
    trapped_task->state = RUNTIME_TASK_STATE_READY;
    trapped_task->frame.x[0] = 0;
    for (;;) {
        if (runtime_timer_interrupt_pending() ||
            (runtime_timer_next_deadline() != 0 &&
                runtime_timer_next_deadline() <= runtime_timer_counter())) {
            (void)runtime_timer_process_expired();
        }
        if (runtime_timer_irq_pending()) {
            trapped_task->frame.x[0] = 1;
            runtime_task_publish(trapped_task);
            return trapped_task;
        }
        if (parked_slot.task != 0) {
            RuntimeTask *next = parked_slot.task;
            if ((uintptr_t)next < parked_slot.lower_bound ||
                (uintptr_t)next >= parked_slot.upper_bound ||
                next->state != RUNTIME_TASK_STATE_PARKED) {
                __builtin_trap();
            }
            runtime_task_publish(next);
            parked_slot = (RuntimeParkedSlot){0};
            ++switch_count;
            return next;
        }
        if (ready_count != 0) {
            RuntimeTask *next = runtime_ready_dequeue();
            if (next->state != RUNTIME_TASK_STATE_READY) {
                __builtin_trap();
            }
            runtime_task_publish(next);
            ++switch_count;
            return next;
        }
        runtime_timer_wait_for_interrupt();
    }
}

RuntimeTask *runtime_task_timer_fiq_from_exception(RuntimeTask *trapped_task) {
    /* ICC_IAR0 has already authenticated PPI27 and moved it to active state. */
    if (trapped_task == 0 || trapped_task != current_task ||
        trapped_task == &idle_task ||
        trapped_task->state != RUNTIME_TASK_STATE_RUNNING ||
        parked_slot.task != 0) {
        __builtin_trap();
    }
    (void)runtime_timer_process_expired();
    trapped_task->state = RUNTIME_TASK_STATE_PARKED;
    parked_slot = (RuntimeParkedSlot){
        .task = trapped_task,
        .lower_bound = (uintptr_t)trapped_task,
        .upper_bound = (uintptr_t)(trapped_task + 1),
        .cookie = (uintptr_t)&parked_slot,
    };
    idle_task.frame.x[0] = 2;
    runtime_task_publish(&idle_task);
    ++switch_count;
    return &idle_task;
}

uint16_t runtime_task_capability_handle(const RuntimeTask *task) {
    if (!runtime_task_is_registered(task) || task == &boot_task) {
        return 0;
    }
    return task_capability_handles[task->global_ordinal];
}

AVP_RECOVERED_ENTRY static RuntimeTask *runtime_task_resolve_capability(
    uint16_t handle
) {
    RuntimeTask *task = runtime_capability_resolve(
        handle,
        RUNTIME_CAPABILITY_TYPE_TASK,
        RUNTIME_CAPABILITY_RIGHT_SIGNAL
    );
    return runtime_task_is_registered(task) ? task : 0;
}

RuntimeTask *runtime_task_create_from_exception(RuntimeTask *trapped_task) {
    if (trapped_task == 0 || trapped_task != current_task ||
        trapped_task->state != RUNTIME_TASK_STATE_RUNNING) {
        __builtin_trap();
    }
    const uint16_t domain_handle = (uint16_t)trapped_task->frame.x[0];
    const uint8_t rights = (uint8_t)trapped_task->frame.x[1];
    const RuntimeTaskDescriptor *descriptor =
        (const RuntimeTaskDescriptor *)trapped_task->frame.x[2];
    runtime_task_validate_descriptor(
        descriptor,
        trapped_task->frame.x[3],
        trapped_task->frame.x[4]
    );
    RuntimeDomain *domain = runtime_task_resolve_domain_handle(domain_handle);
    if (domain == 0 || rights == 0 || (rights & ~UINT8_C(7)) != 0 ||
        registered_task_count == RUNTIME_TASK_CAPACITY) {
        __builtin_trap();
    }

    RuntimeTask *task = runtime_dynamic_task_allocate();
    if (!runtime_task_register_common(task, domain, 0, rights)) {
        __builtin_trap();
    }
    task->descriptor = *descriptor;
    task->projected_flag = trapped_task->frame.x[5] != 0;
    trapped_task->frame.x[0] = runtime_task_capability_handle(task);
    if (trapped_task->frame.x[0] == 0) {
        __builtin_trap();
    }
    return trapped_task;
}

static void runtime_task_rebuild_frame(RuntimeTask *task) {
    if (task->descriptor.entry == 0 || task->stack_top == 0 ||
        task->stack_top <= task->stack_base) {
        __builtin_trap();
    }
    uint64_t *words = (uint64_t *)&task->frame;
    for (uint32_t index = 0;
         index < sizeof(task->frame) / sizeof(uint64_t);
         ++index) {
        words[index] = 0;
    }
    task->frame.x[0] = (uintptr_t)task->descriptor.entry_context;
    task->frame.x[30] = UINT64_C(0xdeadbeefcafef00d);
    task->frame.signed_elr_el1 = runtime_task_arch_sign_exception_return(
        (uintptr_t)task->descriptor.entry
    );
    task->frame.sp_el0 = task->stack_top;
    task->frame.spsr_el1 = 0;
    task->frame.full_vector_state = 1;
    task->startup_sp = task->stack_top;
    task->startup_context = (uintptr_t)task->descriptor.entry_context;
    runtime_task_arch_initialize_frame(&task->frame);
}

RuntimeTask *runtime_task_activate_from_exception(
    RuntimeTask *trapped_task,
    uint16_t capability_handle,
    uint64_t stack_guard_seed
) {
    RuntimeTask *task = runtime_task_resolve_capability(capability_handle);
    if (trapped_task == 0 || trapped_task != current_task || task == 0 ||
        (task->state != RUNTIME_TASK_STATE_INITIALIZED &&
            task->state != RUNTIME_TASK_STATE_EXITED)) {
        __builtin_trap();
    }
    const bool dynamic = runtime_dynamic_task_index(task) >= 0;
    if (dynamic) {
        runtime_task_acquire_stack(task);
    } else if (task->stack_top == 0) {
        __builtin_trap();
    }
    if (task->state == RUNTIME_TASK_STATE_EXITED || dynamic) {
        if (runtime_timer_is_armed(&task->timer_event) &&
            !runtime_timer_cancel(&task->timer_event)) {
            __builtin_trap();
        }
        runtime_task_rebuild_frame(task);
        task->state = RUNTIME_TASK_STATE_INITIALIZED;
    }
    if (stack_guard_seed == 0) {
        stack_guard_seed = runtime_task_arch_generate_guard_seed();
    }
    task->frame.stack_guard = stack_guard_seed & UINT64_C(0xffffffffffff00ff);
    if (!runtime_task_activate(task)) {
        __builtin_trap();
    }
    return trapped_task;
}

RuntimeTask *runtime_task_exit_from_exception(
    RuntimeTask *trapped_task,
    int32_t result
) {
    if (trapped_task == 0 || trapped_task != current_task ||
        trapped_task == &boot_task || trapped_task == &idle_task ||
        trapped_task->state != RUNTIME_TASK_STATE_RUNNING ||
        runtime_timer_is_armed(&trapped_task->timer_event) ||
        trapped_task->ready_link.next != 0 ||
        trapped_task->ready_link.previous != 0 ||
        trapped_task->join_target != 0) {
        __builtin_trap();
    }
    runtime_task_release_stack(trapped_task);
    trapped_task->exit_result = result;
    trapped_task->state = RUNTIME_TASK_STATE_EXITED;
    runtime_task_wake_joiners(trapped_task);
    return runtime_task_select_after_block(trapped_task);
}

RuntimeTask *runtime_task_join_from_exception(
    RuntimeTask *trapped_task,
    uint16_t capability_handle
) {
    RuntimeTask *target = runtime_capability_resolve(
        capability_handle,
        RUNTIME_CAPABILITY_TYPE_TASK,
        RUNTIME_CAPABILITY_RIGHT_WAIT
    );
    if (trapped_task == 0 || trapped_task != current_task ||
        trapped_task == &idle_task ||
        trapped_task->state != RUNTIME_TASK_STATE_RUNNING || target == 0 ||
        target == trapped_task || target == &boot_task || target == &idle_task ||
        !runtime_task_is_registered(target) ||
        target->state == RUNTIME_TASK_STATE_EMPTY) {
        __builtin_trap();
    }
    if (target->state == RUNTIME_TASK_STATE_EXITED) {
        trapped_task->frame.x[0] = (uint64_t)(int64_t)target->exit_result;
        return trapped_task;
    }
    if ((trapped_task->frame.spsr_el1 & UINT64_C(0xc0)) != 0 ||
        trapped_task->join_target != 0 ||
        trapped_task->ready_link.next != 0 ||
        trapped_task->ready_link.previous != 0) {
        __builtin_trap();
    }
    trapped_task->join_target = target;
    trapped_task->state = RUNTIME_TASK_STATE_IPC_WAIT;
    runtime_join_waiter_enqueue(target, trapped_task);
    return runtime_task_select_after_block(trapped_task);
}

bool __attribute__((used, retain)) runtime_event_wake_one(RuntimeEvent *event) {
    RuntimeTask *task = runtime_event_waiter_head(event);
    if (task == 0) {
        return false;
    }
    if (task->state != RUNTIME_TASK_STATE_IPC_WAIT ||
        task->wait_binding.event != event) {
        __builtin_trap();
    }
    runtime_event_waiter_remove(event, task);
    if (!runtime_timer_cancel(&task->timer_event)) {
        __builtin_trap();
    }
    task->wait_binding = (RuntimeTaskWaitBinding){0};
    task->frame.x[0] = 1;
    task->state = RUNTIME_TASK_STATE_READY;
    runtime_ready_enqueue(task);
    return true;
}

RuntimeTask *runtime_event_wait_from_exception(
    RuntimeTask *trapped_task,
    uint16_t handle,
    uint64_t absolute_deadline
) {
    RuntimeEvent *event = runtime_capability_resolve(
        handle,
        RUNTIME_CAPABILITY_TYPE_EVENT,
        RUNTIME_CAPABILITY_RIGHT_WAIT
    );
    if (trapped_task == 0 || trapped_task != current_task || event == 0 ||
        event->owner_ordinal != 0 ||
        trapped_task->state != RUNTIME_TASK_STATE_RUNNING) {
        __builtin_trap();
    }
    if (absolute_deadline != 0 &&
        (trapped_task == &idle_task ||
            (trapped_task->frame.spsr_el1 & UINT64_C(0xc0)) != 0)) {
        __builtin_trap();
    }
    if (event->signaled != 0) {
        if (event->auto_reset != 0) {
            event->signaled = 0;
        }
        trapped_task->frame.x[0] = 1;
        return trapped_task;
    }
    if (absolute_deadline == 0) {
        trapped_task->frame.x[0] = 0;
        return trapped_task;
    }
    if (runtime_timer_is_armed(&trapped_task->timer_event) ||
        trapped_task->wait_binding.event != 0) {
        __builtin_trap();
    }
    trapped_task->wait_binding = (RuntimeTaskWaitBinding){
        .event = event,
        .event_lower_bound = (uintptr_t)event,
        .event_upper_bound = (uintptr_t)(event + 1),
        .event_cookie = (uintptr_t)&trapped_task->wait_binding,
    };
    trapped_task->state = RUNTIME_TASK_STATE_IPC_WAIT;
    runtime_event_waiter_enqueue(event, trapped_task);
    if (absolute_deadline != UINT64_MAX &&
        !runtime_timer_arm(
            &trapped_task->timer_event,
            absolute_deadline,
            runtime_task_event_timeout,
            trapped_task
        )) {
        __builtin_trap();
    }
    return runtime_task_select_after_block(trapped_task);
}

RuntimeTask *runtime_event_signal_from_exception(
    RuntimeTask *trapped_task,
    uint16_t handle,
    bool signaled
) {
    RuntimeEvent *event = runtime_capability_resolve(
        handle,
        RUNTIME_CAPABILITY_TYPE_EVENT,
        RUNTIME_CAPABILITY_RIGHT_SIGNAL
    );
    if (trapped_task == 0 || trapped_task != current_task || event == 0 ||
        trapped_task->state != RUNTIME_TASK_STATE_RUNNING) {
        __builtin_trap();
    }
    event->signaled = signaled;
    if (!signaled) {
        return trapped_task;
    }
    if (event->auto_reset != 0) {
        if (runtime_event_wake_one(event)) {
            event->signaled = 0;
        }
        return trapped_task;
    }
    while (runtime_event_wake_one(event)) {
    }
    return trapped_task;
}

RuntimeTask *runtime_task_wait_and_wake_from_exception(
    RuntimeTask *trapped_task,
    RuntimeTask *wake_task
) {
    if (trapped_task == 0 || trapped_task != current_task ||
        trapped_task->state != RUNTIME_TASK_STATE_RUNNING ||
        wake_task == 0 || wake_task == trapped_task ||
        wake_task->state != RUNTIME_TASK_STATE_IPC_WAIT) {
        __builtin_trap();
    }
    trapped_task->state = RUNTIME_TASK_STATE_IPC_WAIT;
    wake_task->state = RUNTIME_TASK_STATE_READY;
    runtime_ready_enqueue(wake_task);
    return runtime_task_select_after_block(trapped_task);
}

RuntimeTask *runtime_task_wait_from_exception(RuntimeTask *trapped_task) {
    if (trapped_task == 0 || trapped_task != current_task ||
        trapped_task->state != RUNTIME_TASK_STATE_RUNNING) {
        __builtin_trap();
    }
    trapped_task->state = RUNTIME_TASK_STATE_IPC_WAIT;
    return runtime_task_select_after_block(trapped_task);
}

RuntimeTask *runtime_task_wake_from_exception(
    RuntimeTask *trapped_task,
    RuntimeTask *wake_task
) {
    if (trapped_task == 0 || trapped_task != current_task ||
        trapped_task->state != RUNTIME_TASK_STATE_RUNNING ||
        wake_task == 0 || wake_task == trapped_task ||
        wake_task->state != RUNTIME_TASK_STATE_IPC_WAIT) {
        __builtin_trap();
    }
    wake_task->state = RUNTIME_TASK_STATE_READY;
    runtime_ready_enqueue(wake_task);
    return trapped_task;
}

RuntimeTask *runtime_task_require_exception_context(void)
    RUNTIME_SECTION(".text.runtime_task_context_recovered");

RuntimeTask *runtime_task_require_exception_context(void) {
    if (current_task == 0 || current_task->frame.signed_elr_el1 == 0) {
        __builtin_trap();
    }
    return current_task;
}

const RuntimeTask *runtime_task_current(void) {
    return current_task;
}

const RuntimeTask *runtime_task_idle(void) {
    return &idle_task;
}

uint32_t runtime_task_ready_count(void) {
    return ready_count;
}

uint32_t runtime_task_switch_count(void) {
    return switch_count;
}

RuntimeTask *runtime_task_lookup_ordinal(uint8_t ordinal) {
    return ordinal < registered_task_count ? registered_tasks[ordinal] : 0;
}
