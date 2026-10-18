#include "avp/runtime/support/runtime_intrusive_queue.h"


/*
 * Related public algorithms:
 * FreeRTOS-Kernel ce221a8 List_t (MIT):
 * https://github.com/FreeRTOS/FreeRTOS-Kernel/commit/ce221a8bb468e462ca6b435cef66a9636e00baf4
 * Zephyr 0d65ce4 sys_dlist (Apache-2.0):
 * https://github.com/zephyrproject-rtos/zephyr/commit/0d65ce46dda0626164503c50f37e6e1f59d310a9
 * FreeBSD 3aac283 TAILQ (BSD family):
 * https://github.com/freebsd/freebsd-src/commit/3aac283613bd3fd0228a06d6c854ca0bf190ecfb
 * AVP links carry bounds and provenance on both edges, so topology updates
 * copy complete tuples after validating reciprocal links.
 */

static const RuntimeFirebloomType intrusive_queue_position_type = {
    .identity = UINT64_C(0x7175657565706f73),
    .element_size = sizeof(RuntimeIntrusiveQueuePosition),
    .bytewise_copyable = false,
};

static bool runtime_intrusive_queue_position_is_empty(
    RuntimeIntrusiveQueuePosition position
) {
    return position.current == 0 && position.lower_bound == 0 &&
        position.upper_bound == 0 && position.cookie == 0;
}

RuntimeIntrusiveQueueLink *runtime_intrusive_queue_root_tuple_validate(
    RuntimeIntrusiveQueuePosition root
) {
    if (runtime_intrusive_queue_position_is_empty(root)) {
        return 0;
    }
    if (root.current == 0) {
        return 0;
    }
    const uintptr_t current = (uintptr_t)root.current;
    if (root.lower_bound > current || current >= root.upper_bound ||
        sizeof(*root.current) > root.upper_bound - current) {
        return 0;
    }
    return root.current;
}

_Noreturn void runtime_intrusive_queue_root_bounds_panic(void) {
    __builtin_trap();
}

_Noreturn void runtime_task_queue_root_bounds_panic(void) {
    __builtin_trap();
}

_Noreturn void runtime_intrusive_queue_front_bounds_panic(void) {
    __builtin_trap();
}

_Noreturn void runtime_intrusive_queue_link_is_linked_bounds_panic(void) {
    __builtin_trap();
}

void runtime_intrusive_queue_root_initialize(
    RuntimeIntrusiveQueueLink *root,
    uintptr_t lower_bound,
    uintptr_t upper_bound,
    uintptr_t cookie
) {
    const RuntimeIntrusiveQueuePosition self = {
        .current = root,
        .lower_bound = lower_bound,
        .upper_bound = upper_bound,
        .cookie = cookie,
    };
    if (runtime_intrusive_queue_root_tuple_validate(self) == 0) {
        runtime_intrusive_queue_root_bounds_panic();
    }
    root->previous = self;
    root->next = self;
}

void runtime_task_queue_root_initialize(
    RuntimeIntrusiveQueueLink *root,
    uintptr_t lower_bound,
    uintptr_t upper_bound,
    uintptr_t cookie
) {
    const RuntimeIntrusiveQueuePosition self = {
        .current = root,
        .lower_bound = lower_bound,
        .upper_bound = upper_bound,
        .cookie = cookie,
    };
    if (runtime_intrusive_queue_root_tuple_validate(self) == 0) {
        runtime_task_queue_root_bounds_panic();
    }
    runtime_intrusive_queue_root_initialize(
        root,
        lower_bound,
        upper_bound,
        cookie
    );
}

RuntimeIntrusiveQueuePosition runtime_intrusive_queue_front(
    RuntimeIntrusiveQueuePosition root
) {
    RuntimeIntrusiveQueueLink *root_link =
        runtime_intrusive_queue_root_tuple_validate(root);
    if (root_link == 0) {
        runtime_intrusive_queue_front_bounds_panic();
    }
    const RuntimeIntrusiveQueuePosition front = root_link->next;
    if (front.current == root.current) {
        return (RuntimeIntrusiveQueuePosition){0};
    }
    if (runtime_intrusive_queue_root_tuple_validate(front) == 0) {
        runtime_intrusive_queue_front_bounds_panic();
    }
    return front;
}

bool runtime_intrusive_queue_link_is_linked(
    RuntimeIntrusiveQueuePosition link
) {
    RuntimeIntrusiveQueueLink *link_object =
        runtime_intrusive_queue_root_tuple_validate(link);
    if (link_object == 0) {
        runtime_intrusive_queue_link_is_linked_bounds_panic();
    }
    return link_object->previous.current != 0 ||
        link_object->next.current != 0;
}

static RuntimeIntrusiveQueueLink *runtime_intrusive_queue_require_position(
    RuntimeIntrusiveQueuePosition position
) {
    RuntimeIntrusiveQueueLink *link =
        runtime_intrusive_queue_root_tuple_validate(position);
    if (link == 0) {
        runtime_intrusive_queue_root_bounds_panic();
    }
    return link;
}

static void runtime_intrusive_queue_require_reciprocal(
    RuntimeIntrusiveQueuePosition position
) {
    RuntimeIntrusiveQueueLink *link =
        runtime_intrusive_queue_require_position(position);
    RuntimeIntrusiveQueueLink *previous =
        runtime_intrusive_queue_require_position(link->previous);
    RuntimeIntrusiveQueueLink *next =
        runtime_intrusive_queue_require_position(link->next);
    if (previous->next.current != position.current ||
        next->previous.current != position.current) {
        runtime_intrusive_queue_root_bounds_panic();
    }
}

static void runtime_intrusive_queue_require_unlinked(
    RuntimeIntrusiveQueuePosition link
) {
    RuntimeIntrusiveQueueLink *link_object =
        runtime_intrusive_queue_require_position(link);
    if (link_object->previous.current != 0 ||
        link_object->next.current != 0) {
        runtime_intrusive_queue_root_bounds_panic();
    }
}

void runtime_intrusive_queue_link_insert_after(
    RuntimeIntrusiveQueuePosition position,
    RuntimeIntrusiveQueuePosition link
) {
    runtime_intrusive_queue_require_reciprocal(position);
    runtime_intrusive_queue_require_unlinked(link);
    RuntimeIntrusiveQueueLink *position_object = position.current;
    RuntimeIntrusiveQueueLink *link_object = link.current;
    RuntimeIntrusiveQueueLink *next_object = position_object->next.current;

    link_object->previous = position;
    link_object->next = position_object->next;
    next_object->previous = link;
    position_object->next = link;
}

void runtime_intrusive_queue_link_insert_before(
    RuntimeIntrusiveQueuePosition position,
    RuntimeIntrusiveQueuePosition link
) {
    runtime_intrusive_queue_require_reciprocal(position);
    runtime_intrusive_queue_require_unlinked(link);
    RuntimeIntrusiveQueueLink *position_object = position.current;
    RuntimeIntrusiveQueueLink *link_object = link.current;
    RuntimeIntrusiveQueueLink *previous_object =
        position_object->previous.current;

    link_object->previous = position_object->previous;
    link_object->next = position;
    previous_object->next = link;
    position_object->previous = link;
}

void runtime_intrusive_queue_link_remove(
    RuntimeIntrusiveQueuePosition link
) {
    runtime_intrusive_queue_require_reciprocal(link);
    RuntimeIntrusiveQueueLink *link_object = link.current;
    if (link_object->previous.current == link.current &&
        link_object->next.current == link.current) {
        runtime_intrusive_queue_root_bounds_panic();
    }
    RuntimeIntrusiveQueueLink *previous = link_object->previous.current;
    RuntimeIntrusiveQueueLink *next = link_object->next.current;
    previous->next = link_object->next;
    next->previous = link_object->previous;
    link_object->previous = (RuntimeIntrusiveQueuePosition){0};
    link_object->next = (RuntimeIntrusiveQueuePosition){0};
}

const RuntimeFirebloomType *runtime_intrusive_queue_load_position_type_to_x3(
    void
) {
    return &intrusive_queue_position_type;
}

RuntimeIntrusiveQueueCurrentLower
runtime_intrusive_queue_duplicate_x19_link_current_lower(
    RuntimeIntrusiveQueueLink *link
) {
    return (RuntimeIntrusiveQueueCurrentLower){
        .current = link,
        .lower_bound = (uintptr_t)link,
    };
}

void runtime_static_intrusive_queue_roots_initialize(
    const RuntimeIntrusiveQueuePosition *roots,
    size_t root_count
) {
    if (roots == 0 && root_count != 0) {
        runtime_intrusive_queue_root_bounds_panic();
    }
    for (size_t index = 0; index < root_count; ++index) {
        const RuntimeIntrusiveQueuePosition root = roots[index];
        if (runtime_intrusive_queue_root_tuple_validate(root) == 0) {
            runtime_intrusive_queue_root_bounds_panic();
        }
        runtime_intrusive_queue_root_initialize(
            root.current,
            root.lower_bound,
            root.upper_bound,
            root.cookie
        );
    }
}
