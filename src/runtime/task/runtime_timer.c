#include "avp/runtime/task/runtime_timer.h"


#if defined(__ELF__)
#define RUNTIME_SECTION(name) __attribute__((section(name)))
#else
#define RUNTIME_SECTION(name)
#endif

extern uint64_t runtime_timer_arch_counter(void);
extern uint64_t runtime_timer_arch_frequency(void);
extern void runtime_timer_arch_initialize(void);
extern void runtime_timer_arch_program(uint64_t ticks_until_deadline);
extern void runtime_timer_arch_mask(void);
extern bool runtime_timer_arch_interrupt_pending(void);
extern bool runtime_timer_arch_irq_pending(void);
extern void runtime_timer_arch_wait(void);

static RuntimeTaskQueueLink timer_queue_root
    RUNTIME_SECTION(".runtime_timer.queue");
static uint32_t timer_count
    RUNTIME_SECTION(".runtime_task.state");

static RuntimeTimerEvent *runtime_timer_head(void) {
    return timer_queue_root.next;
}

static RuntimeTimerEvent *runtime_timer_tail(void) {
    return timer_queue_root.previous;
}

static bool runtime_timer_is_linked(const RuntimeTimerEvent *event) {
    return event->queue_link.next != 0 || event->queue_link.previous != 0 ||
        event == runtime_timer_head() || event == runtime_timer_tail();
}

static uint64_t runtime_timer_delta(uint64_t deadline) {
    const uint64_t now = runtime_timer_arch_counter();
    if (deadline <= now) {
        return 0;
    }
    const uint64_t delta = deadline - now;
    return delta < UINT64_C(0x7fffffff)
        ? delta
        : UINT64_C(0x7fffffff);
}

void __attribute__((used, retain)) runtime_timer_program_head(void) {
    RuntimeTimerEvent *head = runtime_timer_head();
    if (head == 0) {
        runtime_timer_arch_mask();
        return;
    }
    runtime_timer_arch_program(runtime_timer_delta(head->deadline));
}

static void runtime_timer_unlink(RuntimeTimerEvent *event) {
    RuntimeTimerEvent *previous = event->queue_link.previous;
    RuntimeTimerEvent *next = event->queue_link.next;
    if (previous != 0) {
        previous->queue_link.next = next;
    } else {
        timer_queue_root.next = next;
    }
    if (next != 0) {
        next->queue_link.previous = previous;
    } else {
        timer_queue_root.previous = previous;
    }
    event->queue_link.next = 0;
    event->queue_link.previous = 0;
    --timer_count;
}

void runtime_timer_initialize(void) {
    uint64_t *words = (uint64_t *)&timer_queue_root;
    for (uint32_t index = 0;
         index < sizeof(timer_queue_root) / sizeof(uint64_t);
         ++index) {
        words[index] = 0;
    }
    timer_count = 0;
    runtime_timer_arch_initialize();
    runtime_timer_arch_mask();
}

uint64_t runtime_timer_counter(void) {
    return runtime_timer_arch_counter();
}

uint64_t runtime_timer_frequency(void) {
    return runtime_timer_arch_frequency();
}

bool runtime_timer_arm(
    RuntimeTimerEvent *event,
    uint64_t deadline,
    RuntimeTimerCallback callback,
    RuntimeTask *owner
) {
    if (event == 0 || callback == 0 || owner == 0) {
        return false;
    }
    if (runtime_timer_is_linked(event) && !runtime_timer_cancel(event)) {
        __builtin_trap();
    }
    event->deadline = deadline;
    event->reserved_48 = 0;
    event->callback = callback;
    event->callback_metadata = 0;
    event->owner = owner;
    event->owner_lower_bound = (uintptr_t)owner;
    event->owner_upper_bound = (uintptr_t)(owner + 1);
    event->owner_cookie = 0;
    event->queue_link.next_lower_bound = 0;
    event->queue_link.next_upper_bound = 0;
    event->queue_link.next_cookie = 0;
    event->queue_link.previous_lower_bound = 0;
    event->queue_link.previous_upper_bound = 0;
    event->queue_link.previous_cookie = 0;

    RuntimeTimerEvent *cursor = runtime_timer_head();
    RuntimeTimerEvent *previous = 0;
    /* Equal deadlines retain insertion order, as AVPBooter's queue does. */
    while (cursor != 0 && cursor->deadline <= deadline) {
        previous = cursor;
        cursor = cursor->queue_link.next;
    }
    event->queue_link.previous = previous;
    event->queue_link.next = cursor;
    if (previous != 0) {
        previous->queue_link.next = event;
    } else {
        timer_queue_root.next = event;
    }
    if (cursor != 0) {
        cursor->queue_link.previous = event;
    } else {
        timer_queue_root.previous = event;
    }
    ++timer_count;
    runtime_timer_program_head();
    return true;
}

bool runtime_timer_cancel(RuntimeTimerEvent *event) {
    if (event == 0) {
        return false;
    }
    if (!runtime_timer_is_linked(event)) {
        return true;
    }
    runtime_timer_unlink(event);
    runtime_timer_program_head();
    return true;
}

bool runtime_timer_is_armed(const RuntimeTimerEvent *event) {
    return event != 0 && runtime_timer_is_linked(event);
}

uint32_t runtime_timer_process_expired(void) {
    uint32_t processed = 0;
    runtime_timer_arch_mask();
    for (;;) {
        RuntimeTimerEvent *event = runtime_timer_head();
        if (event == 0 || event->deadline > runtime_timer_arch_counter()) {
            break;
        }
        runtime_timer_unlink(event);
        RuntimeTimerCallback callback = event->callback;
        callback(event, event->owner);
        ++processed;
    }
    runtime_timer_program_head();
    return processed;
}

uint32_t runtime_timer_count(void) {
    return timer_count;
}

uint64_t runtime_timer_next_deadline(void) {
    RuntimeTimerEvent *head = runtime_timer_head();
    return head == 0 ? 0 : head->deadline;
}

bool runtime_timer_interrupt_pending(void) {
    return runtime_timer_arch_interrupt_pending();
}

bool runtime_timer_irq_pending(void) {
    return runtime_timer_arch_irq_pending();
}

void runtime_timer_wait_for_interrupt(void) {
    runtime_timer_arch_wait();
}
