#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


enum {
    RUNTIME_RECURSIVE_LOCK_MAX_DEPTH = 4,
    RUNTIME_RECURSIVE_LOCK_MAX_WAITERS = 3,
};

typedef struct {
    void *object;
    uintptr_t lower_bound;
    uintptr_t upper_bound;
    uintptr_t cookie;
} RuntimeRecursiveLockOwnerTuple;

_Static_assert(
    sizeof(RuntimeRecursiveLockOwnerTuple) == 0x20,
    "recursive lock owner tuple size"
);

typedef struct {
    uintptr_t lower_bound;
    uintptr_t upper_bound;
    uintptr_t cookie;
} RuntimeRecursiveLockBounds;

typedef struct {
    RuntimeRecursiveLockOwnerTuple owner;
    uint16_t depth;
    uint16_t waiter_count;
    uint16_t event_handle;
    uint8_t recursive;
    uint8_t reserved_27;
} RuntimeRecursiveLock;

_Static_assert(sizeof(RuntimeRecursiveLock) == 0x28, "recursive lock size");
_Static_assert(
    offsetof(RuntimeRecursiveLock, depth) == 0x20,
    "recursive lock depth offset"
);
_Static_assert(
    offsetof(RuntimeRecursiveLock, waiter_count) == 0x22,
    "recursive lock waiter offset"
);
_Static_assert(
    offsetof(RuntimeRecursiveLock, event_handle) == 0x24,
    "recursive lock event offset"
);
_Static_assert(
    offsetof(RuntimeRecursiveLock, recursive) == 0x26,
    "recursive lock mode offset"
);

typedef struct {
    void *context;
    RuntimeRecursiveLockOwnerTuple (*current_owner)(void *context);
    uint8_t (*current_domain)(void *context);
    uint16_t (*create_event)(
        void *context,
        bool auto_reset,
        bool initially_signaled,
        uint8_t access_options
    );
    bool (*wait_event)(
        void *context,
        uint16_t event_handle,
        uint64_t absolute_deadline
    );
    void (*signal_event)(void *context, uint16_t event_handle);
    void (*enter_critical)(void *context);
    void (*leave_critical)(void *context);
} RuntimeRecursiveLockPlatform;

#define RUNTIME_RECURSIVE_LOCK_PUBLIC __attribute__((used, retain))

void runtime_recursive_lock_bind_platform(
    const RuntimeRecursiveLockPlatform *platform
);
RUNTIME_RECURSIVE_LOCK_PUBLIC bool runtime_recursive_lock_initialize(
    RuntimeRecursiveLock *lock,
    uintptr_t lower_bound,
    uintptr_t upper_bound,
    uintptr_t cookie,
    bool recursive,
    uint16_t audience
);
RUNTIME_RECURSIVE_LOCK_PUBLIC bool runtime_recursive_lock_acquire(
    RuntimeRecursiveLock *lock,
    uintptr_t lower_bound,
    uintptr_t upper_bound,
    uintptr_t cookie
);
RUNTIME_RECURSIVE_LOCK_PUBLIC bool runtime_recursive_lock_acquire_with_timeout(
    RuntimeRecursiveLock *lock,
    uintptr_t lower_bound,
    uintptr_t upper_bound,
    uintptr_t cookie,
    uint64_t absolute_deadline
);
RUNTIME_RECURSIVE_LOCK_PUBLIC void runtime_recursive_lock_release(
    RuntimeRecursiveLock *lock,
    uintptr_t lower_bound,
    uintptr_t upper_bound,
    uintptr_t cookie
);
RUNTIME_RECURSIVE_LOCK_PUBLIC void *runtime_recursive_lock_owner_tuple_validate(
    const RuntimeRecursiveLockOwnerTuple *owner
);
RUNTIME_RECURSIVE_LOCK_PUBLIC uint64_t runtime_recursive_lock_source_token(void);
RUNTIME_RECURSIVE_LOCK_PUBLIC RuntimeRecursiveLockOwnerTuple
runtime_recursive_lock_restore_task_tuple(
    const RuntimeRecursiveLockOwnerTuple *owner
);
RUNTIME_RECURSIVE_LOCK_PUBLIC RuntimeRecursiveLockBounds
runtime_recursive_lock_save_bounds(
    uintptr_t lower_bound,
    uintptr_t upper_bound,
    uintptr_t cookie
);
RUNTIME_RECURSIVE_LOCK_PUBLIC bool runtime_recursive_lock_initialize_default(
    RuntimeRecursiveLock *lock,
    uintptr_t lower_bound,
    uintptr_t upper_bound,
    uintptr_t cookie
);
RUNTIME_RECURSIVE_LOCK_PUBLIC void *runtime_recursive_lock_owner(
    const RuntimeRecursiveLock *lock
);
RUNTIME_RECURSIVE_LOCK_PUBLIC bool runtime_recursive_lock_increment_depth(
    RuntimeRecursiveLock *lock
);
