#pragma once

#include <stddef.h>
#include <stdint.h>

#include "avp/runtime/firebloom/runtime_firebloom.h"


#if __has_attribute(retain)
#define PLATFORM_DMA_CACHE_PUBLIC __attribute__((used, retain))
#else
#define PLATFORM_DMA_CACHE_PUBLIC __attribute__((used))
#endif

enum {
    PLATFORM_DMA_CACHE_OPERATION_NONE = 0,
    PLATFORM_DMA_CACHE_OPERATION_CLEAN = 1,
    PLATFORM_DMA_CACHE_OPERATION_INVALIDATE = 2,
    PLATFORM_DMA_CACHE_OPERATION_CLEAN_INVALIDATE = 3,
    PLATFORM_DMA_CACHE_OPERATION_UNLOCKED = 1u << 3,
    PLATFORM_DMA_CACHE_OWNER_LINE_SIZE = 64,
};

typedef void (*PlatformDMACacheRuntimeInvalidate)(void *context);

typedef struct {
    PlatformDMACacheRuntimeInvalidate invalidate_all;
    void *context;
} PlatformDMACacheRuntimeBinding;

void platform_dma_cache_bind_runtime(
    PlatformDMACacheRuntimeBinding binding
);
void platform_dma_cache_reset_runtime(void);

PLATFORM_DMA_CACHE_PUBLIC void
platform_dma_cacheline_invalidate_noop(void *address);
PLATFORM_DMA_CACHE_PUBLIC void
platform_dma_cacheline_clean_noop(void *address);
PLATFORM_DMA_CACHE_PUBLIC void
platform_dma_cache_invalidate_all_bootstrap_noop(void);
PLATFORM_DMA_CACHE_PUBLIC void platform_dma_cache_clean_all_noop(void);
PLATFORM_DMA_CACHE_PUBLIC void
j_platform_dma_cache_invalidate_all_bootstrap_noop(void);
PLATFORM_DMA_CACHE_PUBLIC void platform_dma_cache_invalidate_all(void);
PLATFORM_DMA_CACHE_PUBLIC void platform_dma_cache_maintain_bounded_range(
    uint32_t operation,
    void *current,
    const void *lower_bound,
    const void *upper_bound,
    const RuntimeFirebloomType *type_context,
    size_t byte_count
);
PLATFORM_DMA_CACHE_PUBLIC void platform_dma_cache_clean_all_unlocked(void);
