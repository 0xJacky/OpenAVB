#include "avp/runtime/support/runtime_recursive_lock.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>


typedef struct {
    RuntimeRecursiveLockOwnerTuple owner_a;
    RuntimeRecursiveLockOwnerTuple owner_b;
    RuntimeRecursiveLockOwnerTuple current;
    RuntimeRecursiveLock *release_during_wait;
    RuntimeRecursiveLockBounds release_bounds;
    uint16_t next_event;
    uint16_t signaled_event;
    uint64_t observed_deadline;
    uint32_t create_count;
    uint32_t wait_count;
    uint32_t signal_count;
    uint32_t critical_depth;
    uint8_t domain;
    uint8_t observed_access;
    bool wait_result;
} MockRuntime;

static RuntimeRecursiveLockOwnerTuple mock_current_owner(void *context) {
    return ((MockRuntime *)context)->current;
}

static uint8_t mock_current_domain(void *context) {
    return ((MockRuntime *)context)->domain;
}

static uint16_t mock_create_event(
    void *context,
    bool auto_reset,
    bool initially_signaled,
    uint8_t access_options
) {
    MockRuntime *runtime = context;
    assert(auto_reset);
    assert(!initially_signaled);
    ++runtime->create_count;
    runtime->observed_access = access_options;
    return ++runtime->next_event;
}

static bool mock_wait_event(
    void *context,
    uint16_t event_handle,
    uint64_t absolute_deadline
) {
    MockRuntime *runtime = context;
    ++runtime->wait_count;
    runtime->observed_deadline = absolute_deadline;
    if (runtime->release_during_wait != 0) {
        runtime->current = runtime->owner_a;
        runtime_recursive_lock_release(
            runtime->release_during_wait,
            runtime->release_bounds.lower_bound,
            runtime->release_bounds.upper_bound,
            runtime->release_bounds.cookie
        );
        runtime->release_during_wait = 0;
        runtime->current = runtime->owner_b;
        assert(runtime->signaled_event == event_handle);
    }
    return runtime->wait_result;
}

static void mock_signal_event(void *context, uint16_t event_handle) {
    MockRuntime *runtime = context;
    ++runtime->signal_count;
    runtime->signaled_event = event_handle;
}

static void mock_enter_critical(void *context) {
    MockRuntime *runtime = context;
    assert(runtime->critical_depth == 0);
    runtime->critical_depth = 1;
}

static void mock_leave_critical(void *context) {
    MockRuntime *runtime = context;
    assert(runtime->critical_depth == 1);
    runtime->critical_depth = 0;
}

static RuntimeRecursiveLockBounds lock_bounds(RuntimeRecursiveLock *lock) {
    return runtime_recursive_lock_save_bounds(
        (uintptr_t)lock,
        (uintptr_t)lock + sizeof(*lock),
        (uintptr_t)UINT64_C(0x51a7)
    );
}

static void initialize_lock(
    RuntimeRecursiveLock *lock,
    bool recursive,
    uint16_t audience
) {
    const RuntimeRecursiveLockBounds bounds = lock_bounds(lock);
    assert(runtime_recursive_lock_initialize(
        lock,
        bounds.lower_bound,
        bounds.upper_bound,
        bounds.cookie,
        recursive,
        audience
    ));
}

static void test_initialize_and_helpers(MockRuntime *runtime) {
    RuntimeRecursiveLock lock = {0};
    initialize_lock(&lock, true, 0);
    assert(lock.event_handle == 2);
    assert(lock.recursive == 1);
    assert(runtime->observed_access == 0xb);
    assert(runtime_recursive_lock_owner(&lock) == 0);
    assert(runtime_recursive_lock_source_token() ==
        UINT64_C(0x02e40f17907c8149));

    const RuntimeRecursiveLockOwnerTuple restored =
        runtime_recursive_lock_restore_task_tuple(&runtime->owner_a);
    assert(restored.object == runtime->owner_a.object);
    assert(runtime_recursive_lock_owner_tuple_validate(&restored) ==
        runtime->owner_a.object);

    RuntimeRecursiveLock local = {0};
    assert(runtime_recursive_lock_increment_depth(&local));
    assert(local.depth == 1);
}

static void test_recursive_depth_and_release(MockRuntime *runtime) {
    RuntimeRecursiveLock lock = {0};
    initialize_lock(&lock, true, UINT16_C(1) << 2);
    const RuntimeRecursiveLockBounds bounds = lock_bounds(&lock);
    assert(runtime->observed_access == 3);
    runtime->current = runtime->owner_a;

    assert(runtime_recursive_lock_acquire(
        &lock, bounds.lower_bound, bounds.upper_bound, bounds.cookie
    ));
    assert(runtime_recursive_lock_acquire(
        &lock, bounds.lower_bound, bounds.upper_bound, bounds.cookie
    ));
    assert(runtime_recursive_lock_acquire(
        &lock, bounds.lower_bound, bounds.upper_bound, bounds.cookie
    ));
    assert(runtime_recursive_lock_acquire(
        &lock, bounds.lower_bound, bounds.upper_bound, bounds.cookie
    ));
    assert(lock.depth == RUNTIME_RECURSIVE_LOCK_MAX_DEPTH);
    assert(runtime_recursive_lock_owner(&lock) == runtime->owner_a.object);
    assert(!runtime_recursive_lock_acquire_with_timeout(
        &lock, bounds.lower_bound, bounds.upper_bound, bounds.cookie, 0
    ));

    for (uint32_t index = 0;
         index < RUNTIME_RECURSIVE_LOCK_MAX_DEPTH;
         ++index) {
        runtime_recursive_lock_release(
            &lock, bounds.lower_bound, bounds.upper_bound, bounds.cookie
        );
    }
    assert(lock.depth == 0);
    assert(runtime_recursive_lock_owner(&lock) == 0);
}

static void test_timeout_and_reserved_handoff(MockRuntime *runtime) {
    RuntimeRecursiveLock lock = {0};
    initialize_lock(&lock, false, UINT16_C(1));
    const RuntimeRecursiveLockBounds bounds = lock_bounds(&lock);
    assert(runtime->observed_access == 7);
    runtime->current = runtime->owner_a;
    assert(runtime_recursive_lock_acquire(
        &lock, bounds.lower_bound, bounds.upper_bound, bounds.cookie
    ));

    runtime->current = runtime->owner_b;
    runtime->wait_result = false;
    assert(!runtime_recursive_lock_acquire_with_timeout(
        &lock, bounds.lower_bound, bounds.upper_bound, bounds.cookie, 91
    ));
    assert(runtime->observed_deadline == 91);
    assert(lock.waiter_count == 0);
    assert(lock.depth == 1);

    runtime->wait_result = true;
    runtime->release_during_wait = &lock;
    runtime->release_bounds = bounds;
    assert(runtime_recursive_lock_acquire_with_timeout(
        &lock, bounds.lower_bound, bounds.upper_bound, bounds.cookie, 127
    ));
    assert(runtime->signal_count == 1);
    assert(lock.waiter_count == 0);
    assert(lock.depth == 1);
    assert(runtime_recursive_lock_owner(&lock) == runtime->owner_b.object);

    runtime_recursive_lock_release(
        &lock, bounds.lower_bound, bounds.upper_bound, bounds.cookie
    );
    assert(lock.depth == 0);
}

static void test_default_nonrecursive(MockRuntime *runtime) {
    RuntimeRecursiveLock lock = {0};
    const RuntimeRecursiveLockBounds bounds = lock_bounds(&lock);
    assert(runtime_recursive_lock_initialize_default(
        &lock, bounds.lower_bound, bounds.upper_bound, bounds.cookie
    ));
    runtime->current = runtime->owner_a;
    assert(runtime_recursive_lock_acquire(
        &lock, bounds.lower_bound, bounds.upper_bound, bounds.cookie
    ));
    assert(!runtime_recursive_lock_acquire_with_timeout(
        &lock, bounds.lower_bound, bounds.upper_bound, bounds.cookie, 0
    ));
    assert(lock.depth == 1);
    runtime_recursive_lock_release(
        &lock, bounds.lower_bound, bounds.upper_bound, bounds.cookie
    );
}

int main(void) {
    uint64_t owner_a_storage = 0;
    uint64_t owner_b_storage = 0;
    MockRuntime runtime = {
        .owner_a = {
            .object = &owner_a_storage,
            .lower_bound = (uintptr_t)&owner_a_storage,
            .upper_bound = (uintptr_t)&owner_a_storage +
                sizeof(owner_a_storage),
            .cookie = (uintptr_t)UINT64_C(0xa1),
        },
        .owner_b = {
            .object = &owner_b_storage,
            .lower_bound = (uintptr_t)&owner_b_storage,
            .upper_bound = (uintptr_t)&owner_b_storage +
                sizeof(owner_b_storage),
            .cookie = (uintptr_t)UINT64_C(0xb2),
        },
        .next_event = 1,
        .domain = 2,
    };
    runtime.current = runtime.owner_a;
    const RuntimeRecursiveLockPlatform platform = {
        .context = &runtime,
        .current_owner = mock_current_owner,
        .current_domain = mock_current_domain,
        .create_event = mock_create_event,
        .wait_event = mock_wait_event,
        .signal_event = mock_signal_event,
        .enter_critical = mock_enter_critical,
        .leave_critical = mock_leave_critical,
    };
    runtime_recursive_lock_bind_platform(&platform);

    test_initialize_and_helpers(&runtime);
    test_recursive_depth_and_release(&runtime);
    test_timeout_and_reserved_handoff(&runtime);
    test_default_nonrecursive(&runtime);
    assert(runtime.critical_depth == 0);
    assert(runtime.create_count == 4);
    assert(runtime.wait_count == 2);
    puts("runtime recursive lock tests passed");
    return 0;
}
