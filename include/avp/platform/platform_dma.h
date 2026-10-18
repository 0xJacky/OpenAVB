#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


enum {
    PLATFORM_DMA_DOORBELL = 0x004ffff8,
    PLATFORM_DMA_DESCRIPTOR_VERSION = 1,
    PLATFORM_DMA_TYPE_MINIMUM = 1,
    PLATFORM_DMA_TYPE_MAXIMUM = 4,
    PLATFORM_DMA_PAGE_SIZE = 0x1000,
    PLATFORM_DMA_CACHE_LINE_SIZE = 64,
    PLATFORM_DMA_STATUS_SUCCESS = 0,
    PLATFORM_DMA_STATUS_NOT_SUPPORTED = 45,
};

/* The response reuses the request storage with a distinct fixed header. */
#define PLATFORM_DMA_RESPONSE_HEADER UINT64_C(0x0000000001000001)

typedef enum {
    PLATFORM_DMA_OPERATION_RESPONSE = 0,
    PLATFORM_DMA_OPERATION_MAP = 1,
    PLATFORM_DMA_OPERATION_SYNCHRONIZE = 2,
    PLATFORM_DMA_OPERATION_RELEASE = 3,
} PlatformDMAOperation;

typedef struct {
    uint16_t version;
    uint8_t type;
    uint8_t reserved;
    uint32_t operation;
} PlatformDMAHeader;

_Static_assert(sizeof(PlatformDMAHeader) == 8, "PlatformDMAHeader size");

typedef struct {
    PlatformDMAHeader header;
    uint64_t physical_address;
    uint64_t length;
} PlatformDMADescriptor;

_Static_assert(
    sizeof(PlatformDMADescriptor) == 24,
    "PlatformDMADescriptor size"
);
_Static_assert(
    offsetof(PlatformDMADescriptor, physical_address) == 8,
    "PlatformDMADescriptor physical address offset"
);
_Static_assert(
    offsetof(PlatformDMADescriptor, length) == 16,
    "PlatformDMADescriptor length offset"
);

typedef uintptr_t (*PlatformDMATranslate)(
    void *context,
    uintptr_t virtual_address
);
typedef uint32_t (*PlatformDMATransact)(
    void *context,
    PlatformDMADescriptor *descriptor,
    uintptr_t descriptor_ipa
);
typedef void (*PlatformDMACacheSynchronize)(
    void *context,
    void *virtual_address,
    size_t length
);
typedef void *(*PlatformDMAAllocate)(
    void *context,
    size_t length,
    size_t alignment
);
typedef void (*PlatformDMADeallocate)(
    void *context,
    void *allocation,
    size_t length,
    size_t alignment
);

typedef struct {
    PlatformDMATranslate translate;
    PlatformDMATransact transact;
    PlatformDMACacheSynchronize synchronize_for_device;
    PlatformDMACacheSynchronize synchronize_for_cpu;
    PlatformDMAAllocate allocate;
    PlatformDMADeallocate deallocate;
    void *context;
} PlatformDMAOps;

typedef enum {
    PLATFORM_DMA_MAPPING_EMPTY = 0,
    PLATFORM_DMA_MAPPING_CPU_OWNED = 1,
    PLATFORM_DMA_MAPPING_DEVICE_OWNED = 2,
    PLATFORM_DMA_MAPPING_RELEASED = 3,
} PlatformDMAMappingState;

typedef struct {
    PlatformDMAOps ops;
    void *virtual_address;
    uintptr_t physical_address;
    size_t length;
    size_t alignment;
    uint32_t last_transport_status;
    uint8_t type;
    uint8_t owns_allocation;
    PlatformDMAMappingState state;
} PlatformDMAMapping;

void platform_dma_descriptor_initialize(
    PlatformDMADescriptor *descriptor,
    PlatformDMAOperation operation,
    uint8_t type,
    uintptr_t physical_address,
    size_t length
);
uintptr_t platform_dma_translate_stage1_address(uintptr_t virtual_address);
uintptr_t platform_dma_identity_translate(
    void *context,
    uintptr_t virtual_address
);
bool platform_dma_translate_contiguous_range(
    const PlatformDMAOps *ops,
    const void *virtual_address,
    size_t length,
    uintptr_t *physical_address
);
void platform_dma_cacheline_clean_invalidate(const void *address);
void platform_dma_cache_range_clean_invalidate(
    void *context,
    void *virtual_address,
    size_t length
);
uint32_t platform_dma_transact_mmio(
    void *context,
    PlatformDMADescriptor *descriptor,
    uintptr_t descriptor_ipa
);
uint32_t platform_dma_mapping_initialize(
    PlatformDMAMapping *mapping,
    const PlatformDMAOps *ops,
    void *virtual_address,
    size_t length,
    uint8_t type
);
uint32_t platform_dma_owned_buffer_initialize(
    PlatformDMAMapping *mapping,
    const PlatformDMAOps *ops,
    size_t length,
    size_t alignment,
    uint8_t type
);
uint32_t platform_dma_mapping_synchronize_for_device(
    PlatformDMAMapping *mapping
);
uint32_t platform_dma_mapping_synchronize_for_cpu(
    PlatformDMAMapping *mapping
);
uint32_t platform_dma_mapping_release(PlatformDMAMapping *mapping);
