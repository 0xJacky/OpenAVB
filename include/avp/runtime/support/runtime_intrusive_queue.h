#pragma once

#include "avp/runtime/firebloom/runtime_firebloom.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


typedef struct RuntimeIntrusiveQueueLink RuntimeIntrusiveQueueLink;

typedef struct {
    RuntimeIntrusiveQueueLink *current;
    uintptr_t lower_bound;
    uintptr_t upper_bound;
    uintptr_t cookie;
} RuntimeIntrusiveQueuePosition;

_Static_assert(
    sizeof(RuntimeIntrusiveQueuePosition) == 0x20,
    "intrusive queue position size"
);

struct RuntimeIntrusiveQueueLink {
    RuntimeIntrusiveQueuePosition previous;
    RuntimeIntrusiveQueuePosition next;
};

_Static_assert(
    sizeof(RuntimeIntrusiveQueueLink) == 0x40,
    "intrusive queue link size"
);
_Static_assert(
    offsetof(RuntimeIntrusiveQueueLink, next) == 0x20,
    "intrusive queue next offset"
);

typedef struct {
    RuntimeIntrusiveQueueLink *current;
    uintptr_t lower_bound;
} RuntimeIntrusiveQueueCurrentLower;

#define RUNTIME_INTRUSIVE_QUEUE_PUBLIC __attribute__((used, retain))

RUNTIME_INTRUSIVE_QUEUE_PUBLIC void runtime_intrusive_queue_root_initialize(
    RuntimeIntrusiveQueueLink *root,
    uintptr_t lower_bound,
    uintptr_t upper_bound,
    uintptr_t cookie
);
RUNTIME_INTRUSIVE_QUEUE_PUBLIC _Noreturn void
runtime_intrusive_queue_root_bounds_panic(void);
RUNTIME_INTRUSIVE_QUEUE_PUBLIC void runtime_task_queue_root_initialize(
    RuntimeIntrusiveQueueLink *root,
    uintptr_t lower_bound,
    uintptr_t upper_bound,
    uintptr_t cookie
);
RUNTIME_INTRUSIVE_QUEUE_PUBLIC _Noreturn void
runtime_task_queue_root_bounds_panic(void);
RUNTIME_INTRUSIVE_QUEUE_PUBLIC RuntimeIntrusiveQueuePosition
runtime_intrusive_queue_front(
    RuntimeIntrusiveQueuePosition root
);
RUNTIME_INTRUSIVE_QUEUE_PUBLIC _Noreturn void
runtime_intrusive_queue_front_bounds_panic(void);
RUNTIME_INTRUSIVE_QUEUE_PUBLIC bool runtime_intrusive_queue_link_is_linked(
    RuntimeIntrusiveQueuePosition link
);
RUNTIME_INTRUSIVE_QUEUE_PUBLIC _Noreturn void
runtime_intrusive_queue_link_is_linked_bounds_panic(void);
RUNTIME_INTRUSIVE_QUEUE_PUBLIC void runtime_intrusive_queue_link_insert_after(
    RuntimeIntrusiveQueuePosition position,
    RuntimeIntrusiveQueuePosition link
);
RUNTIME_INTRUSIVE_QUEUE_PUBLIC void runtime_intrusive_queue_link_insert_before(
    RuntimeIntrusiveQueuePosition position,
    RuntimeIntrusiveQueuePosition link
);
RUNTIME_INTRUSIVE_QUEUE_PUBLIC void runtime_intrusive_queue_link_remove(
    RuntimeIntrusiveQueuePosition link
);
RUNTIME_INTRUSIVE_QUEUE_PUBLIC const RuntimeFirebloomType *
runtime_intrusive_queue_load_position_type_to_x3(
    void
);
RUNTIME_INTRUSIVE_QUEUE_PUBLIC RuntimeIntrusiveQueueCurrentLower
runtime_intrusive_queue_duplicate_x19_link_current_lower(
    RuntimeIntrusiveQueueLink *link
);
RUNTIME_INTRUSIVE_QUEUE_PUBLIC void
runtime_static_intrusive_queue_roots_initialize(
    const RuntimeIntrusiveQueuePosition *roots,
    size_t root_count
);
RUNTIME_INTRUSIVE_QUEUE_PUBLIC RuntimeIntrusiveQueueLink *
runtime_intrusive_queue_root_tuple_validate(
    RuntimeIntrusiveQueuePosition root
);
