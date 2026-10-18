#include "avp/runtime/support/runtime_recursive_lock.h"


/*
 * Related public implementations of this state machine:
 * FreeRTOS-Kernel ce221a8 (MIT), queue.c recursive mutex paths:
 * https://github.com/FreeRTOS/FreeRTOS-Kernel/commit/ce221a8bb468e462ca6b435cef66a9636e00baf4
 * Zephyr 0d65ce4 (Apache-2.0), kernel/mutex.c:
 * https://github.com/zephyrproject-rtos/zephyr/commit/0d65ce46dda0626164503c50f37e6e1f59d310a9
 *
 * The AVP lock keeps a bounded capability tuple and reserves ownership for a
 * single event waiter. Kernel-native mutexes cannot preserve that ABI or its
 * handoff invariant, so no third-party implementation is incorporated here.
 */

static RuntimeRecursiveLockPlatform recursive_lock_platform;

static _Noreturn void runtime_recursive_lock_fail(void) {
    __builtin_trap();
}

static bool runtime_recursive_lock_platform_is_bound(void) {
    return recursive_lock_platform.current_owner != 0 &&
        recursive_lock_platform.current_domain != 0 &&
        recursive_lock_platform.create_event != 0 &&
        recursive_lock_platform.wait_event != 0 &&
        recursive_lock_platform.signal_event != 0 &&
        recursive_lock_platform.enter_critical != 0 &&
        recursive_lock_platform.leave_critical != 0;
}

static bool runtime_recursive_lock_bounds_contain(
    const RuntimeRecursiveLock *lock,
    RuntimeRecursiveLockBounds bounds
) {
    const uintptr_t address = (uintptr_t)lock;
    return lock != 0 && bounds.lower_bound <= address &&
        address < bounds.upper_bound &&
        sizeof(*lock) <= bounds.upper_bound - address;
}

static bool runtime_recursive_lock_tuple_equal(
    RuntimeRecursiveLockOwnerTuple left,
    RuntimeRecursiveLockOwnerTuple right
) {
    return left.object == right.object &&
        left.lower_bound == right.lower_bound &&
        left.upper_bound == right.upper_bound && left.cookie == right.cookie;
}

static void runtime_recursive_lock_clear_owner(RuntimeRecursiveLock *lock) {
    lock->owner.object = 0;
    lock->owner.lower_bound = 0;
    lock->owner.upper_bound = 0;
    lock->owner.cookie = 0;
}

static uint8_t runtime_recursive_lock_event_access(uint16_t audience) {
    if (audience == 0) {
        return UINT8_C(0xb);
    }
    const uint8_t domain = recursive_lock_platform.current_domain(
        recursive_lock_platform.context
    );
    if (domain >= 16) {
        runtime_recursive_lock_fail();
    }
    const uint16_t local_audience = (uint16_t)(UINT16_C(1) << domain);
    return audience == local_audience ? UINT8_C(3) : UINT8_C(7);
}

void runtime_recursive_lock_bind_platform(
    const RuntimeRecursiveLockPlatform *platform
) {
    if (platform == 0) {
        runtime_recursive_lock_fail();
    }
    recursive_lock_platform = *platform;
    if (!runtime_recursive_lock_platform_is_bound()) {
        runtime_recursive_lock_fail();
    }
}

bool runtime_recursive_lock_initialize(
    RuntimeRecursiveLock *lock,
    uintptr_t lower_bound,
    uintptr_t upper_bound,
    uintptr_t cookie,
    bool recursive,
    uint16_t audience
) {
    const RuntimeRecursiveLockBounds bounds =
        runtime_recursive_lock_save_bounds(lower_bound, upper_bound, cookie);
    if (!runtime_recursive_lock_platform_is_bound() ||
        !runtime_recursive_lock_bounds_contain(lock, bounds) ||
        lock->event_handle != 0 || runtime_recursive_lock_owner(lock) != 0) {
        return false;
    }

    runtime_recursive_lock_clear_owner(lock);
    lock->depth = 0;
    lock->waiter_count = 0;
    lock->recursive = recursive ? UINT8_C(1) : UINT8_C(0);
    lock->reserved_27 = 0;
    lock->event_handle = recursive_lock_platform.create_event(
        recursive_lock_platform.context,
        true,
        false,
        runtime_recursive_lock_event_access(audience)
    );
    return lock->event_handle != 0;
}

bool runtime_recursive_lock_acquire(
    RuntimeRecursiveLock *lock,
    uintptr_t lower_bound,
    uintptr_t upper_bound,
    uintptr_t cookie
) {
    return runtime_recursive_lock_acquire_with_timeout(
        lock,
        lower_bound,
        upper_bound,
        cookie,
        UINT64_MAX
    );
}

bool runtime_recursive_lock_acquire_with_timeout(
    RuntimeRecursiveLock *lock,
    uintptr_t lower_bound,
    uintptr_t upper_bound,
    uintptr_t cookie,
    uint64_t absolute_deadline
) {
    const RuntimeRecursiveLockBounds bounds =
        runtime_recursive_lock_save_bounds(lower_bound, upper_bound, cookie);
    if (!runtime_recursive_lock_platform_is_bound() ||
        !runtime_recursive_lock_bounds_contain(lock, bounds) ||
        lock->event_handle == 0) {
        return false;
    }
    const RuntimeRecursiveLockOwnerTuple current =
        recursive_lock_platform.current_owner(
            recursive_lock_platform.context
        );
    if (runtime_recursive_lock_owner_tuple_validate(&current) == 0) {
        runtime_recursive_lock_fail();
    }

    recursive_lock_platform.enter_critical(recursive_lock_platform.context);
    if (lock->depth == 0) {
        if (runtime_recursive_lock_owner(lock) != 0 ||
            lock->waiter_count != 0) {
            runtime_recursive_lock_fail();
        }
        lock->owner = current;
        if (!runtime_recursive_lock_increment_depth(lock)) {
            runtime_recursive_lock_fail();
        }
        recursive_lock_platform.leave_critical(
            recursive_lock_platform.context
        );
        return true;
    }

    if (runtime_recursive_lock_tuple_equal(lock->owner, current)) {
        if (lock->recursive == 0 ||
            !runtime_recursive_lock_increment_depth(lock)) {
            recursive_lock_platform.leave_critical(
                recursive_lock_platform.context
            );
            return false;
        }
        recursive_lock_platform.leave_critical(
            recursive_lock_platform.context
        );
        return true;
    }

    if (absolute_deadline == 0) {
        recursive_lock_platform.leave_critical(
            recursive_lock_platform.context
        );
        return false;
    }
    if (lock->waiter_count == RUNTIME_RECURSIVE_LOCK_MAX_WAITERS) {
        runtime_recursive_lock_fail();
    }
    ++lock->waiter_count;
    recursive_lock_platform.leave_critical(recursive_lock_platform.context);

    const bool signaled = recursive_lock_platform.wait_event(
        recursive_lock_platform.context,
        lock->event_handle,
        absolute_deadline
    );

    recursive_lock_platform.enter_critical(recursive_lock_platform.context);
    if (!signaled) {
        if (lock->waiter_count == 0) {
            runtime_recursive_lock_fail();
        }
        --lock->waiter_count;
        recursive_lock_platform.leave_critical(
            recursive_lock_platform.context
        );
        return false;
    }
    if (lock->depth != 1 || runtime_recursive_lock_owner(lock) != 0) {
        runtime_recursive_lock_fail();
    }
    lock->owner = current;
    recursive_lock_platform.leave_critical(recursive_lock_platform.context);
    return true;
}

void runtime_recursive_lock_release(
    RuntimeRecursiveLock *lock,
    uintptr_t lower_bound,
    uintptr_t upper_bound,
    uintptr_t cookie
) {
    const RuntimeRecursiveLockBounds bounds =
        runtime_recursive_lock_save_bounds(lower_bound, upper_bound, cookie);
    if (!runtime_recursive_lock_platform_is_bound() ||
        !runtime_recursive_lock_bounds_contain(lock, bounds) ||
        lock->event_handle == 0) {
        runtime_recursive_lock_fail();
    }
    const RuntimeRecursiveLockOwnerTuple current =
        recursive_lock_platform.current_owner(
            recursive_lock_platform.context
        );

    recursive_lock_platform.enter_critical(recursive_lock_platform.context);
    if (lock->depth == 0 ||
        !runtime_recursive_lock_tuple_equal(lock->owner, current)) {
        runtime_recursive_lock_fail();
    }
    --lock->depth;
    if (lock->depth != 0) {
        recursive_lock_platform.leave_critical(
            recursive_lock_platform.context
        );
        return;
    }

    runtime_recursive_lock_clear_owner(lock);
    if (lock->waiter_count != 0) {
        --lock->waiter_count;
        lock->depth = 1;
        recursive_lock_platform.signal_event(
            recursive_lock_platform.context,
            lock->event_handle
        );
    }
    recursive_lock_platform.leave_critical(recursive_lock_platform.context);
}

void *runtime_recursive_lock_owner_tuple_validate(
    const RuntimeRecursiveLockOwnerTuple *owner
) {
    if (owner == 0 || owner->object == 0) {
        return 0;
    }
    const uintptr_t address = (uintptr_t)owner->object;
    if (owner->lower_bound > address || address >= owner->upper_bound ||
        sizeof(uintptr_t) > owner->upper_bound - address) {
        return 0;
    }
    return owner->object;
}

uint64_t runtime_recursive_lock_source_token(void) {
    return UINT64_C(0x02e40f17907c8149);
}

RuntimeRecursiveLockOwnerTuple runtime_recursive_lock_restore_task_tuple(
    const RuntimeRecursiveLockOwnerTuple *owner
) {
    if (owner == 0) {
        runtime_recursive_lock_fail();
    }
    return *owner;
}

RuntimeRecursiveLockBounds runtime_recursive_lock_save_bounds(
    uintptr_t lower_bound,
    uintptr_t upper_bound,
    uintptr_t cookie
) {
    return (RuntimeRecursiveLockBounds){
        .lower_bound = lower_bound,
        .upper_bound = upper_bound,
        .cookie = cookie,
    };
}

bool runtime_recursive_lock_initialize_default(
    RuntimeRecursiveLock *lock,
    uintptr_t lower_bound,
    uintptr_t upper_bound,
    uintptr_t cookie
) {
    return runtime_recursive_lock_initialize(
        lock,
        lower_bound,
        upper_bound,
        cookie,
        false,
        0
    );
}

void *runtime_recursive_lock_owner(const RuntimeRecursiveLock *lock) {
    return lock == 0
        ? 0
        : runtime_recursive_lock_owner_tuple_validate(&lock->owner);
}

bool runtime_recursive_lock_increment_depth(RuntimeRecursiveLock *lock) {
    if (lock == 0 || lock->depth == RUNTIME_RECURSIVE_LOCK_MAX_DEPTH) {
        return false;
    }
    ++lock->depth;
    return true;
}
