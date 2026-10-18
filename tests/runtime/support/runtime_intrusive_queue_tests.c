#include "avp/runtime/support/runtime_intrusive_queue.h"

#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>


static RuntimeIntrusiveQueuePosition position(
    RuntimeIntrusiveQueueLink *link,
    uintptr_t cookie
) {
    return (RuntimeIntrusiveQueuePosition){
        .current = link,
        .lower_bound = (uintptr_t)link,
        .upper_bound = (uintptr_t)link + sizeof(*link),
        .cookie = cookie,
    };
}

static void expect_fatal(void (*operation)(void)) {
    const pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        operation();
        _Exit(0);
    }
    int status = 0;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status));
}

static void root_bounds_panic(void) {
    runtime_intrusive_queue_root_bounds_panic();
}

static void task_bounds_panic(void) {
    runtime_task_queue_root_bounds_panic();
}

static void front_bounds_panic(void) {
    runtime_intrusive_queue_front_bounds_panic();
}

static void linked_bounds_panic(void) {
    runtime_intrusive_queue_link_is_linked_bounds_panic();
}

static void test_queue_operations(void) {
    RuntimeIntrusiveQueueLink root = {0};
    RuntimeIntrusiveQueueLink first = {0};
    RuntimeIntrusiveQueueLink second = {0};
    RuntimeIntrusiveQueueLink third = {0};
    const RuntimeIntrusiveQueuePosition root_position = position(&root, 1);
    const RuntimeIntrusiveQueuePosition first_position = position(&first, 2);
    const RuntimeIntrusiveQueuePosition second_position =
        position(&second, 3);
    const RuntimeIntrusiveQueuePosition third_position = position(&third, 4);

    runtime_intrusive_queue_root_initialize(
        &root,
        root_position.lower_bound,
        root_position.upper_bound,
        root_position.cookie
    );
    assert(runtime_intrusive_queue_front(root_position).current == 0);
    assert(runtime_intrusive_queue_link_is_linked(root_position));
    assert(!runtime_intrusive_queue_link_is_linked(first_position));

    runtime_intrusive_queue_link_insert_after(
        root_position,
        first_position
    );
    runtime_intrusive_queue_link_insert_after(
        first_position,
        second_position
    );
    runtime_intrusive_queue_link_insert_before(
        first_position,
        third_position
    );
    assert(runtime_intrusive_queue_front(root_position).current == &third);
    assert(third.next.current == &first);
    assert(first.next.current == &second);
    assert(second.next.current == &root);

    runtime_intrusive_queue_link_remove(first_position);
    assert(!runtime_intrusive_queue_link_is_linked(first_position));
    assert(third.next.current == &second);
    assert(second.previous.current == &third);
    runtime_intrusive_queue_link_remove(third_position);
    runtime_intrusive_queue_link_remove(second_position);
    assert(runtime_intrusive_queue_front(root_position).current == 0);
}

static void test_root_helpers(void) {
    RuntimeIntrusiveQueueLink roots[3] = {0};
    RuntimeIntrusiveQueuePosition positions[2] = {
        position(&roots[0], 10),
        position(&roots[1], 11),
    };
    runtime_static_intrusive_queue_roots_initialize(positions, 2);
    assert(roots[0].next.current == &roots[0]);
    assert(roots[1].previous.current == &roots[1]);

    const RuntimeIntrusiveQueuePosition task_root = position(&roots[2], 12);
    runtime_task_queue_root_initialize(
        &roots[2],
        task_root.lower_bound,
        task_root.upper_bound,
        task_root.cookie
    );
    assert(runtime_intrusive_queue_root_tuple_validate(task_root) ==
        &roots[2]);

    const RuntimeIntrusiveQueueCurrentLower duplicate =
        runtime_intrusive_queue_duplicate_x19_link_current_lower(&roots[2]);
    assert(duplicate.current == &roots[2]);
    assert(duplicate.lower_bound == (uintptr_t)&roots[2]);
    const RuntimeFirebloomType *type =
        runtime_intrusive_queue_load_position_type_to_x3();
    assert(type->element_size == sizeof(RuntimeIntrusiveQueuePosition));
}

int main(void) {
    test_queue_operations();
    test_root_helpers();
    expect_fatal(root_bounds_panic);
    expect_fatal(task_bounds_panic);
    expect_fatal(front_bounds_panic);
    expect_fatal(linked_bounds_panic);
    puts("runtime intrusive queue tests passed");
    return 0;
}
