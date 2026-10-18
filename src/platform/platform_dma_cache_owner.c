#include "avp/platform/platform_dma_cache_owner.h"

#include "avp/platform/platform_dma.h"
#include "avp/runtime/support/runtime_cpu_recovered.h"
#include "avp/runtime/task/runtime_irq_recovered.h"
#include "avp/runtime/task/runtime_task_context_recovered.h"


static PlatformDMACacheRuntimeBinding runtime_binding;

static _Noreturn void platform_dma_cache_fatal(void) {
    __builtin_trap();
}

static void platform_dma_cache_invalidate_all_runtime(void) {
    if (runtime_binding.invalidate_all != 0) {
        runtime_binding.invalidate_all(runtime_binding.context);
        return;
    }
#if defined(__aarch64__)
    __asm__ volatile("svc #0xf" : : : "memory");
#else
    platform_dma_cache_fatal();
#endif
}

void platform_dma_cache_bind_runtime(
    PlatformDMACacheRuntimeBinding binding
) {
    if (binding.invalidate_all == 0) {
        platform_dma_cache_fatal();
    }
    runtime_binding = binding;
}

void platform_dma_cache_reset_runtime(void) {
    runtime_binding = (PlatformDMACacheRuntimeBinding){0};
}

void platform_dma_cacheline_invalidate_noop(void *address) {
    (void)address;
}

void platform_dma_cacheline_clean_noop(void *address) {
    (void)address;
}

void platform_dma_cache_invalidate_all_bootstrap_noop(void) {
}

void platform_dma_cache_clean_all_noop(void) {
}

void j_platform_dma_cache_invalidate_all_bootstrap_noop(void) {
    platform_dma_cache_invalidate_all_bootstrap_noop();
}

void platform_dma_cache_invalidate_all(void) {
    RuntimeFirebloomSpan task = runtime_current_task_bounded_view();
    if (runtime_current_task_is_bootstrap_sentinel(task.current)) {
        platform_dma_cache_invalidate_all_bootstrap_noop();
        return;
    }
    platform_dma_cache_invalidate_all_runtime();
}

static void platform_dma_cache_maintain_lines(
    uint32_t operation,
    uintptr_t current,
    uintptr_t end
) {
    void (*maintain)(void *) = 0;
    switch (operation & 3u) {
        case PLATFORM_DMA_CACHE_OPERATION_NONE:
            return;
        case PLATFORM_DMA_CACHE_OPERATION_CLEAN:
            current &= ~(uintptr_t)(PLATFORM_DMA_CACHE_OWNER_LINE_SIZE - 1u);
            maintain = platform_dma_cacheline_clean_noop;
            break;
        case PLATFORM_DMA_CACHE_OPERATION_INVALIDATE:
            if ((current & (PLATFORM_DMA_CACHE_OWNER_LINE_SIZE - 1u)) != 0 ||
                (end & (PLATFORM_DMA_CACHE_OWNER_LINE_SIZE - 1u)) != 0) {
                platform_dma_cache_fatal();
            }
            maintain = platform_dma_cacheline_invalidate_noop;
            break;
        case PLATFORM_DMA_CACHE_OPERATION_CLEAN_INVALIDATE:
            current &= ~(uintptr_t)(PLATFORM_DMA_CACHE_OWNER_LINE_SIZE - 1u);
            break;
    }

    while (current < end) {
        if (maintain != 0) {
            maintain((void *)current);
        } else {
            platform_dma_cacheline_clean_invalidate((const void *)current);
        }
        current += PLATFORM_DMA_CACHE_OWNER_LINE_SIZE;
    }
}

void platform_dma_cache_maintain_bounded_range(
    uint32_t operation,
    void *current,
    const void *lower_bound,
    const void *upper_bound,
    const RuntimeFirebloomType *type_context,
    size_t byte_count
) {
    (void)type_context;
    if ((operation & PLATFORM_DMA_CACHE_OPERATION_UNLOCKED) != 0) {
        if ((operation & PLATFORM_DMA_CACHE_OPERATION_CLEAN) != 0 &&
            current == 0 && byte_count == 0) {
            platform_dma_cache_clean_all_noop();
        }
        return;
    }

    runtime_interrupt_mask_push_svc();
    if (current == 0 && byte_count == 0) {
        switch (operation & 3u) {
            case PLATFORM_DMA_CACHE_OPERATION_CLEAN:
                platform_dma_cache_clean_all_noop();
                break;
            case PLATFORM_DMA_CACHE_OPERATION_INVALIDATE:
                platform_dma_cache_fatal();
            default:
                platform_dma_cache_invalidate_all();
                break;
        }
    } else {
        const uintptr_t address = (uintptr_t)current;
        const uintptr_t lower = (uintptr_t)lower_bound;
        const uintptr_t upper = (uintptr_t)upper_bound;
        if (current == 0 || lower_bound == 0 || upper_bound == 0 ||
            address < lower || address >= upper ||
            byte_count > upper - address) {
            platform_dma_cache_fatal();
        }
        platform_dma_cache_maintain_lines(
            operation,
            address,
            address + byte_count
        );
    }
    runtime_data_memory_barrier_system();
    runtime_interrupt_mask_pop_svc();
}

void platform_dma_cache_clean_all_unlocked(void) {
    platform_dma_cache_maintain_bounded_range(
        PLATFORM_DMA_CACHE_OPERATION_UNLOCKED |
            PLATFORM_DMA_CACHE_OPERATION_CLEAN,
        0,
        0,
        0,
        0,
        0
    );
}
