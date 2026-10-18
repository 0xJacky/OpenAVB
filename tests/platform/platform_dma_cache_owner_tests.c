#include "avp/platform/platform_dma_cache_owner.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>


static void *current_task;
static unsigned push_count;
static unsigned pop_count;
static unsigned barrier_count;
static unsigned runtime_invalidate_count;
static uintptr_t maintained_lines[8];
static size_t maintained_line_count;

RuntimeFirebloomSpan runtime_current_task_bounded_view(void) {
    void *upper_bound = current_task;
    if ((uintptr_t)current_task != UINTPTR_MAX) {
        upper_bound = (uint8_t *)current_task + 0x70;
    }
    return (RuntimeFirebloomSpan){
        .current = current_task,
        .lower_bound = current_task,
        .upper_bound = upper_bound,
        .type = 0,
    };
}

bool runtime_current_task_is_bootstrap_sentinel(const void *task) {
    return (uintptr_t)task == UINTPTR_MAX;
}

void runtime_interrupt_mask_push_svc(void) {
    ++push_count;
}

void runtime_interrupt_mask_pop_svc(void) {
    ++pop_count;
}

void runtime_data_memory_barrier_system(void) {
    ++barrier_count;
}

void platform_dma_cacheline_clean_invalidate(const void *address) {
    assert(maintained_line_count < 8);
    maintained_lines[maintained_line_count++] = (uintptr_t)address;
}

static void runtime_invalidate(void *context) {
    assert(context == &runtime_invalidate_count);
    ++runtime_invalidate_count;
}

static void reset_fixture(void) {
    static uint8_t task;
    current_task = &task;
    push_count = 0;
    pop_count = 0;
    barrier_count = 0;
    runtime_invalidate_count = 0;
    maintained_line_count = 0;
    platform_dma_cache_bind_runtime((PlatformDMACacheRuntimeBinding){
        .invalidate_all = runtime_invalidate,
        .context = &runtime_invalidate_count,
    });
}

static void test_noop_and_full_cache_routes(void) {
    reset_fixture();
    uint8_t byte = 0xa5;
    platform_dma_cacheline_invalidate_noop(&byte);
    platform_dma_cacheline_clean_noop(&byte);
    platform_dma_cache_invalidate_all_bootstrap_noop();
    platform_dma_cache_clean_all_noop();
    j_platform_dma_cache_invalidate_all_bootstrap_noop();
    assert(byte == 0xa5);

    current_task = (void *)UINTPTR_MAX;
    platform_dma_cache_invalidate_all();
    assert(runtime_invalidate_count == 0);

    static uint8_t task;
    current_task = &task;
    platform_dma_cache_invalidate_all();
    assert(runtime_invalidate_count == 1);
}

static void test_bounded_line_operations(void) {
    _Alignas(PLATFORM_DMA_CACHE_OWNER_LINE_SIZE) uint8_t buffer[192] = {0};

    reset_fixture();
    platform_dma_cache_maintain_bounded_range(
        PLATFORM_DMA_CACHE_OPERATION_CLEAN,
        buffer + 3,
        buffer,
        buffer + sizeof(buffer),
        0,
        65
    );
    assert(push_count == 1 && pop_count == 1 && barrier_count == 1);
    assert(maintained_line_count == 0);

    reset_fixture();
    platform_dma_cache_maintain_bounded_range(
        PLATFORM_DMA_CACHE_OPERATION_INVALIDATE,
        buffer,
        buffer,
        buffer + sizeof(buffer),
        0,
        128
    );
    assert(push_count == 1 && pop_count == 1 && barrier_count == 1);
    assert(maintained_line_count == 0);

    reset_fixture();
    platform_dma_cache_maintain_bounded_range(
        PLATFORM_DMA_CACHE_OPERATION_CLEAN_INVALIDATE,
        buffer + 3,
        buffer,
        buffer + sizeof(buffer),
        0,
        65
    );
    assert(push_count == 1 && pop_count == 1 && barrier_count == 1);
    assert(maintained_line_count == 2);
    assert(maintained_lines[0] == (uintptr_t)buffer);
    assert(maintained_lines[1] == (uintptr_t)(buffer + 64));

    reset_fixture();
    platform_dma_cache_maintain_bounded_range(
        PLATFORM_DMA_CACHE_OPERATION_NONE,
        buffer,
        buffer,
        buffer + sizeof(buffer),
        0,
        sizeof(buffer)
    );
    assert(push_count == 1 && pop_count == 1 && barrier_count == 1);
    assert(maintained_line_count == 0);
}

static void test_whole_cache_and_unlocked_routes(void) {
    reset_fixture();
    platform_dma_cache_maintain_bounded_range(
        PLATFORM_DMA_CACHE_OPERATION_CLEAN,
        0,
        0,
        0,
        0,
        0
    );
    assert(push_count == 1 && pop_count == 1 && barrier_count == 1);
    assert(runtime_invalidate_count == 0);

    reset_fixture();
    platform_dma_cache_maintain_bounded_range(
        PLATFORM_DMA_CACHE_OPERATION_NONE,
        0,
        0,
        0,
        0,
        0
    );
    assert(push_count == 1 && pop_count == 1 && barrier_count == 1);
    assert(runtime_invalidate_count == 1);

    reset_fixture();
    platform_dma_cache_clean_all_unlocked();
    assert(push_count == 0 && pop_count == 0 && barrier_count == 0);
    assert(runtime_invalidate_count == 0);
}

int main(void) {
    test_noop_and_full_cache_routes();
    test_bounded_line_operations();
    test_whole_cache_and_unlocked_routes();
    platform_dma_cache_reset_runtime();
    puts("platform dma cache owner tests passed");
}
