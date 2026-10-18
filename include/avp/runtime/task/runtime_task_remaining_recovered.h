#pragma once

#include "avp/runtime/firebloom/runtime_firebloom.h"
#include "avp/runtime/task/runtime_task.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(__ELF__) && __has_attribute(retain)
#define RUNTIME_TASK_REMAINING_PUBLIC \
    __attribute__((used, retain, noinline, section(".text.runtime_support_recovered")))
#elif defined(__ELF__)
#define RUNTIME_TASK_REMAINING_PUBLIC \
    __attribute__((used, noinline, section(".text.runtime_support_recovered")))
#else
#define RUNTIME_TASK_REMAINING_PUBLIC __attribute__((used, noinline))
#endif

enum {
    RUNTIME_TASK_WAIT_LIMIT = 16,
    RUNTIME_TASK_STACK_PAGE_SIZE = 0x4000,
    RUNTIME_TASK_STACK_PAGE_LIMIT = 64,
};

typedef void (*RuntimeTaskRemainingReady)(void *context, RuntimeTask *task);
typedef void (*RuntimeTaskRemainingCancelTimer)(void *context,
                                                RuntimeTimerEvent *event);

typedef struct {
    RuntimeTask *tasks[RUNTIME_TASK_WAIT_LIMIT];
    uint64_t deadlines[RUNTIME_TASK_WAIT_LIMIT];
    size_t count;
    RuntimeTaskRemainingReady ready;
    RuntimeTaskRemainingCancelTimer cancel_timer;
    void *hook_context;
} RuntimeTaskWaitQueueRecovered;

typedef struct {
    uint8_t *storage;
    size_t page_count;
    uint64_t allocation_bitmap;
} RuntimeTaskStackPoolRecovered;

typedef struct {
    RuntimeTaskStackPoolRecovered *stack_pool;
    RuntimeTask *root_task;
    RuntimeTask *main_task;
    RuntimeTask *interrupt_task;
    RuntimeTaskDescriptor main_descriptor;
    RuntimeTaskDescriptor interrupt_descriptor;
} RuntimeSchedulerBootstrapRecovered;

RUNTIME_TASK_REMAINING_PUBLIC size_t
runtime_task_wait_queue_wake_all(RuntimeTaskWaitQueueRecovered *queue,
                                 uint64_t result);
RUNTIME_TASK_REMAINING_PUBLIC RuntimeTask *
runtime_task_wait_queue_wake(RuntimeTaskWaitQueueRecovered *queue,
                             uint64_t result);
RUNTIME_TASK_REMAINING_PUBLIC bool
runtime_task_block_on_wait_queue(RuntimeTaskWaitQueueRecovered *queue,
                                 RuntimeTask *task);
RUNTIME_TASK_REMAINING_PUBLIC bool
runtime_task_block_on_wait_queue_until_deadline(
    RuntimeTaskWaitQueueRecovered *queue,
    RuntimeTask *task,
    uint64_t deadline,
    uint64_t now);
RUNTIME_TASK_REMAINING_PUBLIC void
runtime_task_stack_initialize(RuntimeTaskStackPoolRecovered *pool,
                              uint8_t *storage,
                              size_t page_count);
RUNTIME_TASK_REMAINING_PUBLIC void *
runtime_task_stack_allocate_bitmap_page(RuntimeTaskStackPoolRecovered *pool);
RUNTIME_TASK_REMAINING_PUBLIC void
runtime_task_stack_release_bitmap_page(RuntimeTaskStackPoolRecovered *pool,
                                       void *page);
RUNTIME_TASK_REMAINING_PUBLIC bool
runtime_task_record_initialize(RuntimeTask *task,
                               RuntimeDomain *domain,
                               const RuntimeTaskDescriptor *descriptor,
                               void *stack_page,
                               size_t stack_size,
                               uint32_t global_ordinal,
                               uint32_t domain_ordinal);
RUNTIME_TASK_REMAINING_PUBLIC bool
runtime_scheduler_bootstrap(RuntimeSchedulerBootstrapRecovered *bootstrap);
RUNTIME_TASK_REMAINING_PUBLIC RuntimeFirebloomSpan
runtime_scheduler_bootstrap_outlined_prepare_task_initialize_arguments(
    RuntimeTask *task,
    const RuntimeFirebloomType *type);
RUNTIME_TASK_REMAINING_PUBLIC RuntimeFirebloomSpan
runtime_scheduler_interrupt_task_stack_storage_span(void);
RUNTIME_TASK_REMAINING_PUBLIC RuntimeFirebloomSpan
runtime_scheduler_bootstrap_task_stack_storage_span(void);
