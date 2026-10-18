#include "avp/runtime/task/runtime_task_remaining_recovered.h"

#include <string.h>

static const RuntimeFirebloomType runtime_task_remaining_byte_type = {
    .identity = 0x7274737461636b75ull,
    .element_size = 1u,
    .bytewise_copyable = true,
};

static void runtime_task_remaining_ready(
    RuntimeTaskWaitQueueRecovered *queue,
    RuntimeTask *task,
    uint64_t result
) {
    task->frame.x[0] = result;
    task->state = RUNTIME_TASK_STATE_READY;
    if (queue->cancel_timer != NULL) {
        queue->cancel_timer(queue->hook_context, &task->timer_event);
    }
    if (queue->ready != NULL) {
        queue->ready(queue->hook_context, task);
    }
}

RuntimeTask *runtime_task_wait_queue_wake(
    RuntimeTaskWaitQueueRecovered *queue,
    uint64_t result
) {
    if (queue == NULL || queue->count == 0u) {
        return NULL;
    }
    RuntimeTask *task = queue->tasks[0];
    for (size_t index = 1; index < queue->count; ++index) {
        queue->tasks[index - 1u] = queue->tasks[index];
        queue->deadlines[index - 1u] = queue->deadlines[index];
    }
    --queue->count;
    runtime_task_remaining_ready(queue, task, result);
    return task;
}

size_t runtime_task_wait_queue_wake_all(
    RuntimeTaskWaitQueueRecovered *queue,
    uint64_t result
) {
    if (queue == NULL) {
        return 0u;
    }
    const size_t count = queue->count;
    while (queue->count != 0u) {
        (void)runtime_task_wait_queue_wake(queue, result);
    }
    return count;
}

bool runtime_task_block_on_wait_queue(
    RuntimeTaskWaitQueueRecovered *queue,
    RuntimeTask *task
) {
    return runtime_task_block_on_wait_queue_until_deadline(
        queue,
        task,
        UINT64_MAX,
        0u
    );
}

bool runtime_task_block_on_wait_queue_until_deadline(
    RuntimeTaskWaitQueueRecovered *queue,
    RuntimeTask *task,
    uint64_t deadline,
    uint64_t now
) {
    if (queue == NULL || task == NULL || queue->count == RUNTIME_TASK_WAIT_LIMIT) {
        return false;
    }
    if (deadline != UINT64_MAX && deadline <= now) {
        task->frame.x[0] = 1u;
        return false;
    }
    queue->tasks[queue->count] = task;
    queue->deadlines[queue->count] = deadline;
    ++queue->count;
    task->state = deadline == UINT64_MAX ? RUNTIME_TASK_STATE_IPC_WAIT
                                        : RUNTIME_TASK_STATE_TIMER_WAIT;
    task->timer_event.deadline = deadline;
    return true;
}

void runtime_task_stack_initialize(
    RuntimeTaskStackPoolRecovered *pool,
    uint8_t *storage,
    size_t page_count
) {
    if (pool == NULL || storage == NULL || page_count == 0u ||
        page_count > RUNTIME_TASK_STACK_PAGE_LIMIT ||
        ((uintptr_t)storage & (RUNTIME_TASK_STACK_PAGE_SIZE - 1u)) != 0u) {
        __builtin_trap();
    }
    *pool = (RuntimeTaskStackPoolRecovered){
        .storage = storage,
        .page_count = page_count,
    };
}

void *runtime_task_stack_allocate_bitmap_page(RuntimeTaskStackPoolRecovered *pool) {
    if (pool == NULL) {
        return NULL;
    }
    for (size_t index = 0; index < pool->page_count; ++index) {
        const uint64_t bit = UINT64_C(1) << index;
        if ((pool->allocation_bitmap & bit) == 0u) {
            pool->allocation_bitmap |= bit;
            uint8_t *page = pool->storage + index * RUNTIME_TASK_STACK_PAGE_SIZE;
            memset(page, 0, RUNTIME_TASK_STACK_PAGE_SIZE);
            return page;
        }
    }
    return NULL;
}

void runtime_task_stack_release_bitmap_page(
    RuntimeTaskStackPoolRecovered *pool,
    void *page
) {
    if (pool == NULL || page == NULL || page < (void *)pool->storage) {
        __builtin_trap();
    }
    const uintptr_t delta = (uintptr_t)page - (uintptr_t)pool->storage;
    if ((delta & (RUNTIME_TASK_STACK_PAGE_SIZE - 1u)) != 0u) {
        __builtin_trap();
    }
    const size_t index = delta / RUNTIME_TASK_STACK_PAGE_SIZE;
    if (index >= pool->page_count ||
        (pool->allocation_bitmap & (UINT64_C(1) << index)) == 0u) {
        __builtin_trap();
    }
    pool->allocation_bitmap &= ~(UINT64_C(1) << index);
}

bool runtime_task_record_initialize(
    RuntimeTask *task,
    RuntimeDomain *domain,
    const RuntimeTaskDescriptor *descriptor,
    void *stack_page,
    size_t stack_size,
    uint32_t global_ordinal,
    uint32_t domain_ordinal
) {
    if (task == NULL || descriptor == NULL || descriptor->entry == NULL ||
        stack_page == NULL || stack_size < 256u) {
        return false;
    }
    memset(task, 0, sizeof(*task));
    task->global_ordinal = global_ordinal;
    task->domain_ordinal = domain_ordinal;
    task->state = RUNTIME_TASK_STATE_INITIALIZED;
    task->domain = domain;
    task->descriptor = *descriptor;
    task->stack_allocation = stack_page;
    task->stack_mapping = stack_page;
    task->stack_base = (uintptr_t)stack_page;
    task->stack_top = task->stack_base + stack_size;
    task->startup_sp = task->stack_top & ~(uintptr_t)0xfu;
    task->startup_context = (uintptr_t)descriptor->entry_context;
    task->frame.sp_el0 = task->startup_sp;
    task->frame.x[0] = task->startup_context;
    task->frame.signed_elr_el1 = (uintptr_t)descriptor->entry;
    return true;
}

RuntimeFirebloomSpan
runtime_scheduler_bootstrap_outlined_prepare_task_initialize_arguments(
    RuntimeTask *task,
    const RuntimeFirebloomType *type
) {
    if (task == NULL || type == NULL) {
        __builtin_trap();
    }
    return (RuntimeFirebloomSpan){
        .current = task,
        .lower_bound = task,
        .upper_bound = (uint8_t *)task + sizeof(*task),
        .type = type,
    };
}

RuntimeFirebloomSpan runtime_scheduler_interrupt_task_stack_storage_span(void) {
    return (RuntimeFirebloomSpan){
        .current = (void *)(uintptr_t)0x70029b90u,
        .lower_bound = (void *)(uintptr_t)0x70029b90u,
        .upper_bound = (void *)(uintptr_t)0x70029c00u,
        .type = &runtime_task_remaining_byte_type,
    };
}

RuntimeFirebloomSpan runtime_scheduler_bootstrap_task_stack_storage_span(void) {
    return (RuntimeFirebloomSpan){
        .current = (void *)(uintptr_t)0x70029c00u,
        .lower_bound = (void *)(uintptr_t)0x70029c00u,
        .upper_bound = (void *)(uintptr_t)0x70029c70u,
        .type = &runtime_task_remaining_byte_type,
    };
}

bool runtime_scheduler_bootstrap(RuntimeSchedulerBootstrapRecovered *bootstrap) {
    if (bootstrap == NULL || bootstrap->stack_pool == NULL ||
        bootstrap->root_task == NULL || bootstrap->main_task == NULL ||
        bootstrap->interrupt_task == NULL) {
        return false;
    }
    void *main_stack =
        runtime_task_stack_allocate_bitmap_page(bootstrap->stack_pool);
    void *interrupt_stack =
        runtime_task_stack_allocate_bitmap_page(bootstrap->stack_pool);
    if (main_stack == NULL || interrupt_stack == NULL) {
        if (main_stack != NULL) {
            runtime_task_stack_release_bitmap_page(
                bootstrap->stack_pool,
                main_stack
            );
        }
        return false;
    }
    memset(bootstrap->root_task, 0, sizeof(*bootstrap->root_task));
    bootstrap->root_task->state = RUNTIME_TASK_STATE_RUNNING;
    if (!runtime_task_record_initialize(
            bootstrap->main_task, NULL, &bootstrap->main_descriptor, main_stack,
            RUNTIME_TASK_STACK_PAGE_SIZE, 1u, 1u) ||
        !runtime_task_record_initialize(
            bootstrap->interrupt_task, NULL, &bootstrap->interrupt_descriptor,
            interrupt_stack, RUNTIME_TASK_STACK_PAGE_SIZE, 2u, 2u)) {
        runtime_task_stack_release_bitmap_page(bootstrap->stack_pool, main_stack);
        runtime_task_stack_release_bitmap_page(
            bootstrap->stack_pool,
            interrupt_stack
        );
        return false;
    }
    bootstrap->main_task->state = RUNTIME_TASK_STATE_READY;
    bootstrap->interrupt_task->state = RUNTIME_TASK_STATE_PARKED;
    return true;
}

