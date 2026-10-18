#include "avp/platform/platform_dma.h"

#include <assert.h>
#include <stdalign.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>


typedef struct {
    PlatformDMADescriptor transactions[8];
    uintptr_t descriptor_ipas[8];
    uint32_t statuses[8];
    size_t transaction_count;
    size_t device_sync_count;
    size_t cpu_sync_count;
    size_t allocation_count;
    size_t deallocation_count;
    uintptr_t translation_bias;
    uintptr_t discontinuity;
} TestContext;

static uintptr_t translate(void *opaque, uintptr_t virtual_address) {
    TestContext *context = opaque;
    uintptr_t physical = virtual_address + context->translation_bias;
    if (context->discontinuity != 0 &&
        virtual_address >= context->discontinuity) {
        physical += PLATFORM_DMA_PAGE_SIZE;
    }
    return physical;
}

static uint32_t transact(
    void *opaque,
    PlatformDMADescriptor *descriptor,
    uintptr_t descriptor_ipa
) {
    TestContext *context = opaque;
    const size_t index = context->transaction_count++;
    assert(index < 8);
    context->transactions[index] = *descriptor;
    context->descriptor_ipas[index] = descriptor_ipa;
    return context->statuses[index];
}

static void synchronize_for_device(
    void *opaque,
    void *virtual_address,
    size_t length
) {
    TestContext *context = opaque;
    assert(virtual_address != NULL);
    assert(length != 0);
    ++context->device_sync_count;
}

static void synchronize_for_cpu(
    void *opaque,
    void *virtual_address,
    size_t length
) {
    TestContext *context = opaque;
    assert(virtual_address != NULL);
    assert(length != 0);
    ++context->cpu_sync_count;
}

static void *allocate_buffer(
    void *opaque,
    size_t length,
    size_t alignment
) {
    TestContext *context = opaque;
    ++context->allocation_count;
    return aligned_alloc(alignment, length);
}

static void deallocate_buffer(
    void *opaque,
    void *allocation,
    size_t length,
    size_t alignment
) {
    TestContext *context = opaque;
    assert(allocation != NULL);
    assert(length != 0);
    assert(alignment != 0);
    ++context->deallocation_count;
    free(allocation);
}

static PlatformDMAOps test_ops(TestContext *context) {
    return (PlatformDMAOps){
        .translate = translate,
        .transact = transact,
        .synchronize_for_device = synchronize_for_device,
        .synchronize_for_cpu = synchronize_for_cpu,
        .allocate = allocate_buffer,
        .deallocate = deallocate_buffer,
        .context = context,
    };
}

static void fatal_invalid_transition(void) {
    PlatformDMAMapping mapping = {
        .state = PLATFORM_DMA_MAPPING_CPU_OWNED,
    };
    (void)platform_dma_mapping_synchronize_for_cpu(&mapping);
}

static void test_invalid_transition_is_fatal(void) {
    const pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        fatal_invalid_transition();
        _Exit(0);
    }
    int status = 0;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status));
}

static void test_descriptor_wire_layout(void) {
    PlatformDMADescriptor descriptor;
    platform_dma_descriptor_initialize(
        &descriptor,
        PLATFORM_DMA_OPERATION_SYNCHRONIZE,
        1,
        UINT64_C(0xb40ef000),
        0x500000
    );
    union {
        PlatformDMAHeader header;
        uint64_t word;
    } header = {.header = descriptor.header};
    assert(header.word == UINT64_C(0x0000000200010001));
    assert(descriptor.physical_address == UINT64_C(0xb40ef000));
    assert(descriptor.length == 0x500000);

    union {
        uint64_t word;
        PlatformDMAHeader header;
    } response = {.word = PLATFORM_DMA_RESPONSE_HEADER};
    assert(response.header.version == PLATFORM_DMA_DESCRIPTOR_VERSION);
    assert(response.header.type == 0);
    assert(response.header.reserved == 1);
    assert(response.header.operation == PLATFORM_DMA_OPERATION_RESPONSE);
    assert((uint64_t)PLATFORM_DMA_STATUS_NOT_SUPPORTED == 45);
}

static void test_contiguous_translation(void) {
    alignas(PLATFORM_DMA_PAGE_SIZE) uint8_t storage[0x3000];
    TestContext context = {.translation_bias = 0x200000};
    const PlatformDMAOps ops = test_ops(&context);
    uintptr_t physical = 0;
    assert(platform_dma_translate_contiguous_range(
        &ops,
        storage + 0x321,
        0x2100,
        &physical
    ));
    assert(physical == (uintptr_t)(storage + 0x321) + 0x200000);

    context.discontinuity = (uintptr_t)storage + PLATFORM_DMA_PAGE_SIZE;
    assert(!platform_dma_translate_contiguous_range(
        &ops,
        storage + 0x321,
        0x2100,
        &physical
    ));
}

static void test_external_mapping_lifecycle(void) {
    alignas(PLATFORM_DMA_PAGE_SIZE) uint8_t storage[0x2000];
    TestContext context = {
        .statuses = {0, PLATFORM_DMA_STATUS_NOT_SUPPORTED, 0, 0},
        .translation_bias = 0x300000,
    };
    const PlatformDMAOps ops = test_ops(&context);
    PlatformDMAMapping mapping = {0};

    assert(platform_dma_mapping_initialize(
        &mapping,
        &ops,
        storage,
        sizeof(storage),
        3
    ) == PLATFORM_DMA_STATUS_SUCCESS);
    assert(mapping.state == PLATFORM_DMA_MAPPING_CPU_OWNED);
    assert(mapping.physical_address == (uintptr_t)storage + 0x300000);
    assert(context.transactions[0].header.operation ==
        PLATFORM_DMA_OPERATION_MAP);

    assert(platform_dma_mapping_synchronize_for_device(&mapping) ==
        PLATFORM_DMA_STATUS_NOT_SUPPORTED);
    assert(mapping.state == PLATFORM_DMA_MAPPING_DEVICE_OWNED);
    assert(mapping.last_transport_status == PLATFORM_DMA_STATUS_NOT_SUPPORTED);
    assert(context.device_sync_count == 1);
    assert(context.transactions[1].header.operation ==
        PLATFORM_DMA_OPERATION_SYNCHRONIZE);

    assert(platform_dma_mapping_synchronize_for_cpu(&mapping) ==
        PLATFORM_DMA_STATUS_SUCCESS);
    assert(mapping.state == PLATFORM_DMA_MAPPING_CPU_OWNED);
    assert(context.cpu_sync_count == 1);
    assert(context.transactions[2].header.operation ==
        PLATFORM_DMA_OPERATION_SYNCHRONIZE);

    assert(platform_dma_mapping_release(&mapping) ==
        PLATFORM_DMA_STATUS_SUCCESS);
    assert(mapping.state == PLATFORM_DMA_MAPPING_RELEASED);
    assert(context.transactions[3].header.operation ==
        PLATFORM_DMA_OPERATION_RELEASE);
    assert(context.deallocation_count == 0);
}

static void test_owned_mapping_releases_once(void) {
    TestContext context = {.translation_bias = 0x400000};
    const PlatformDMAOps ops = test_ops(&context);
    PlatformDMAMapping mapping = {0};

    assert(platform_dma_owned_buffer_initialize(
        &mapping,
        &ops,
        0x3000,
        PLATFORM_DMA_PAGE_SIZE,
        4
    ) == PLATFORM_DMA_STATUS_SUCCESS);
    assert(context.allocation_count == 1);
    assert(mapping.owns_allocation == 1);
    assert(platform_dma_mapping_release(&mapping) ==
        PLATFORM_DMA_STATUS_SUCCESS);
    assert(context.deallocation_count == 1);
    assert(mapping.state == PLATFORM_DMA_MAPPING_RELEASED);
}

static void test_failed_owned_map_releases_allocation(void) {
    TestContext context = {
        .statuses = {17},
        .translation_bias = 0x500000,
    };
    const PlatformDMAOps ops = test_ops(&context);
    PlatformDMAMapping mapping = {0};

    assert(platform_dma_owned_buffer_initialize(
        &mapping,
        &ops,
        0x2000,
        PLATFORM_DMA_PAGE_SIZE,
        2
    ) == 17);
    assert(context.allocation_count == 1);
    assert(context.deallocation_count == 1);
    assert(mapping.state == PLATFORM_DMA_MAPPING_EMPTY);
    assert(mapping.last_transport_status == 17);
}

int main(void) {
    test_descriptor_wire_layout();
    test_contiguous_translation();
    test_external_mapping_lifecycle();
    test_owned_mapping_releases_once();
    test_failed_owned_map_releases_allocation();
    test_invalid_transition_is_fatal();
}
