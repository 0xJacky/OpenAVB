#include "avp/platform/platform_dma.h"
#include "avp/runtime/support/recovered_symbol.h"


static _Noreturn void platform_dma_fatal(void) {
    __builtin_trap();
}

static bool add_overflows_uintptr(
    uintptr_t value,
    size_t increment,
    uintptr_t *result
) {
    if (increment > UINTPTR_MAX - value) {
        return true;
    }
    *result = value + increment;
    return false;
}

static void platform_dma_validate_type(uint8_t type) {
    if (type < PLATFORM_DMA_TYPE_MINIMUM ||
        type > PLATFORM_DMA_TYPE_MAXIMUM) {
        platform_dma_fatal();
    }
}

static void platform_dma_validate_ops(const PlatformDMAOps *ops) {
    if (ops == NULL || ops->translate == NULL || ops->transact == NULL ||
        ops->synchronize_for_device == NULL ||
        ops->synchronize_for_cpu == NULL) {
        platform_dma_fatal();
    }
}

static void platform_dma_validate_translator(const PlatformDMAOps *ops) {
    if (ops == NULL || ops->translate == NULL) {
        platform_dma_fatal();
    }
}

static bool platform_dma_synchronization_completed(uint32_t status) {
    /* The public coherent host reports ENOTSUP after completing the handoff. */
    return status == PLATFORM_DMA_STATUS_SUCCESS ||
        status == PLATFORM_DMA_STATUS_NOT_SUPPORTED;
}

void platform_dma_descriptor_initialize(
    PlatformDMADescriptor *descriptor,
    PlatformDMAOperation operation,
    uint8_t type,
    uintptr_t physical_address,
    size_t length
) {
    if (descriptor == NULL || length == 0 || physical_address == 0 ||
        operation < PLATFORM_DMA_OPERATION_MAP ||
        operation > PLATFORM_DMA_OPERATION_RELEASE) {
        platform_dma_fatal();
    }
    platform_dma_validate_type(type);
    *descriptor = (PlatformDMADescriptor){
        .header = {
            .version = PLATFORM_DMA_DESCRIPTOR_VERSION,
            .type = type,
            .reserved = 0,
            .operation = operation,
        },
        .physical_address = physical_address,
        .length = length,
    };
}

AVP_RECOVERED_ENTRY uintptr_t platform_dma_translate_stage1_address(
    uintptr_t virtual_address
) {
#if defined(__aarch64__)
    uintptr_t translated = virtual_address;
    __asm__ volatile(
        "at s1e1r, %0\n"
        "isb\n"
        "mrs %0, par_el1"
        : "+r"(translated)
        :
        : "memory"
    );
    if ((translated & 1U) != 0) {
        return 0;
    }
    return (translated & UINT64_C(0xfffffffff000)) |
        (virtual_address & (PLATFORM_DMA_PAGE_SIZE - 1));
#else
    (void)virtual_address;
    platform_dma_fatal();
#endif
}

uintptr_t platform_dma_identity_translate(
    void *context,
    uintptr_t virtual_address
) {
    (void)context;
    return virtual_address;
}

AVP_RECOVERED_ENTRY bool platform_dma_translate_contiguous_range(
    const PlatformDMAOps *ops,
    const void *virtual_address,
    size_t length,
    uintptr_t *physical_address
) {
    platform_dma_validate_translator(ops);
    if (virtual_address == NULL || physical_address == NULL || length == 0) {
        platform_dma_fatal();
    }

    const uintptr_t start = (uintptr_t)virtual_address;
    uintptr_t last;
    if (add_overflows_uintptr(start, length - 1, &last)) {
        return false;
    }
    const uintptr_t first_physical = ops->translate(ops->context, start);
    if (first_physical == 0) {
        return false;
    }

    uintptr_t page;
    bool page_overflow = add_overflows_uintptr(
        start & ~(uintptr_t)(PLATFORM_DMA_PAGE_SIZE - 1),
        PLATFORM_DMA_PAGE_SIZE,
        &page
    );
    while (!page_overflow && page <= last) {
        const uintptr_t translated = ops->translate(ops->context, page);
        uintptr_t expected;
        if (translated == 0 ||
            add_overflows_uintptr(first_physical, page - start, &expected) ||
            translated != expected) {
            return false;
        }
        page_overflow = add_overflows_uintptr(
            page,
            PLATFORM_DMA_PAGE_SIZE,
            &page
        );
    }

    const uintptr_t translated_last = ops->translate(ops->context, last);
    uintptr_t expected_last;
    if (translated_last == 0 ||
        add_overflows_uintptr(first_physical, length - 1, &expected_last) ||
        translated_last != expected_last) {
        return false;
    }
    *physical_address = first_physical;
    return true;
}

void platform_dma_cacheline_clean_invalidate(const void *address) {
#if defined(__aarch64__)
    __asm__ volatile("dc civac, %0" : : "r"(address) : "memory");
#else
    (void)address;
    __asm__ volatile("" : : : "memory");
#endif
}

void platform_dma_cache_range_clean_invalidate(
    void *context,
    void *virtual_address,
    size_t length
) {
    (void)context;
    if (virtual_address == NULL || length == 0) {
        platform_dma_fatal();
    }
    const uintptr_t start = (uintptr_t)virtual_address;
    uintptr_t end;
    if (add_overflows_uintptr(start, length, &end)) {
        platform_dma_fatal();
    }
    uintptr_t line = start & ~(uintptr_t)(PLATFORM_DMA_CACHE_LINE_SIZE - 1);
    while (line < end) {
        platform_dma_cacheline_clean_invalidate((const void *)line);
        if (end - line <= PLATFORM_DMA_CACHE_LINE_SIZE) {
            break;
        }
        line += PLATFORM_DMA_CACHE_LINE_SIZE;
    }
#if defined(__aarch64__)
    __asm__ volatile("dsb sy" : : : "memory");
#else
    __asm__ volatile("" : : : "memory");
#endif
}

uint32_t platform_dma_transact_mmio(
    void *context,
    PlatformDMADescriptor *descriptor,
    uintptr_t descriptor_ipa
) {
    (void)context;
#if defined(__aarch64__)
    if (descriptor == NULL || descriptor_ipa == 0) {
        platform_dma_fatal();
    }
    __asm__ volatile("dsb sy" : : : "memory");
    *(volatile uint64_t *)(uintptr_t)PLATFORM_DMA_DOORBELL = descriptor_ipa;
    __asm__ volatile("dsb sy" : : : "memory");
    if (descriptor->header.version != PLATFORM_DMA_DESCRIPTOR_VERSION ||
        descriptor->header.type != 0 ||
        descriptor->header.reserved != 1 ||
        descriptor->header.operation != PLATFORM_DMA_OPERATION_RESPONSE) {
        platform_dma_fatal();
    }
    return (uint32_t)descriptor->physical_address;
#else
    (void)descriptor;
    (void)descriptor_ipa;
    platform_dma_fatal();
#endif
}

static uint32_t platform_dma_transact_mapping(
    PlatformDMAMapping *mapping,
    PlatformDMAOperation operation
) {
    PlatformDMADescriptor descriptor;
    platform_dma_descriptor_initialize(
        &descriptor,
        operation,
        mapping->type,
        mapping->physical_address,
        mapping->length
    );
    uintptr_t descriptor_ipa;
    if (!platform_dma_translate_contiguous_range(
            &mapping->ops,
            &descriptor,
            sizeof(descriptor),
            &descriptor_ipa
        )) {
        platform_dma_fatal();
    }
    const uint32_t status = mapping->ops.transact(
        mapping->ops.context,
        &descriptor,
        descriptor_ipa
    );
    mapping->last_transport_status = status;
    return status;
}

static uint32_t platform_dma_mapping_initialize_internal(
    PlatformDMAMapping *mapping,
    const PlatformDMAOps *ops,
    void *virtual_address,
    size_t length,
    size_t alignment,
    uint8_t type,
    bool owns_allocation
) {
    if (mapping == NULL || mapping->state != PLATFORM_DMA_MAPPING_EMPTY ||
        virtual_address == NULL || length == 0) {
        platform_dma_fatal();
    }
    platform_dma_validate_ops(ops);
    platform_dma_validate_type(type);

    uintptr_t physical_address;
    if (!platform_dma_translate_contiguous_range(
            ops,
            virtual_address,
            length,
            &physical_address
        )) {
        platform_dma_fatal();
    }
    *mapping = (PlatformDMAMapping){
        .ops = *ops,
        .virtual_address = virtual_address,
        .physical_address = physical_address,
        .length = length,
        .alignment = alignment,
        .last_transport_status = PLATFORM_DMA_STATUS_SUCCESS,
        .type = type,
        .owns_allocation = owns_allocation,
        .state = PLATFORM_DMA_MAPPING_CPU_OWNED,
    };
    const uint32_t status = platform_dma_transact_mapping(
        mapping,
        PLATFORM_DMA_OPERATION_MAP
    );
    if (status != PLATFORM_DMA_STATUS_SUCCESS) {
        mapping->state = PLATFORM_DMA_MAPPING_EMPTY;
    }
    return status;
}

uint32_t platform_dma_mapping_initialize(
    PlatformDMAMapping *mapping,
    const PlatformDMAOps *ops,
    void *virtual_address,
    size_t length,
    uint8_t type
) {
    return platform_dma_mapping_initialize_internal(
        mapping,
        ops,
        virtual_address,
        length,
        0,
        type,
        false
    );
}

uint32_t platform_dma_owned_buffer_initialize(
    PlatformDMAMapping *mapping,
    const PlatformDMAOps *ops,
    size_t length,
    size_t alignment,
    uint8_t type
) {
    platform_dma_validate_ops(ops);
    if (mapping == NULL || mapping->state != PLATFORM_DMA_MAPPING_EMPTY ||
        ops->allocate == NULL || ops->deallocate == NULL || length == 0 ||
        alignment < sizeof(void *) ||
        (alignment & (alignment - 1)) != 0) {
        platform_dma_fatal();
    }
    void *allocation = ops->allocate(ops->context, length, alignment);
    if (allocation == NULL) {
        platform_dma_fatal();
    }
    const uint32_t status = platform_dma_mapping_initialize_internal(
        mapping,
        ops,
        allocation,
        length,
        alignment,
        type,
        true
    );
    if (status != PLATFORM_DMA_STATUS_SUCCESS) {
        ops->deallocate(ops->context, allocation, length, alignment);
        *mapping = (PlatformDMAMapping){0};
        mapping->last_transport_status = status;
    }
    return status;
}

uint32_t platform_dma_mapping_synchronize_for_device(
    PlatformDMAMapping *mapping
) {
    if (mapping == NULL ||
        mapping->state != PLATFORM_DMA_MAPPING_CPU_OWNED) {
        platform_dma_fatal();
    }
    mapping->ops.synchronize_for_device(
        mapping->ops.context,
        mapping->virtual_address,
        mapping->length
    );
    const uint32_t status = platform_dma_transact_mapping(
        mapping,
        PLATFORM_DMA_OPERATION_SYNCHRONIZE
    );
    if (platform_dma_synchronization_completed(status)) {
        mapping->state = PLATFORM_DMA_MAPPING_DEVICE_OWNED;
    }
    return status;
}

uint32_t platform_dma_mapping_synchronize_for_cpu(
    PlatformDMAMapping *mapping
) {
    if (mapping == NULL ||
        mapping->state != PLATFORM_DMA_MAPPING_DEVICE_OWNED) {
        platform_dma_fatal();
    }
    const uint32_t status = platform_dma_transact_mapping(
        mapping,
        PLATFORM_DMA_OPERATION_SYNCHRONIZE
    );
    if (platform_dma_synchronization_completed(status)) {
        mapping->ops.synchronize_for_cpu(
            mapping->ops.context,
            mapping->virtual_address,
            mapping->length
        );
        mapping->state = PLATFORM_DMA_MAPPING_CPU_OWNED;
    }
    return status;
}

uint32_t platform_dma_mapping_release(PlatformDMAMapping *mapping) {
    if (mapping == NULL ||
        mapping->state != PLATFORM_DMA_MAPPING_CPU_OWNED) {
        platform_dma_fatal();
    }
    const uint32_t status = platform_dma_transact_mapping(
        mapping,
        PLATFORM_DMA_OPERATION_RELEASE
    );
    if (status != PLATFORM_DMA_STATUS_SUCCESS) {
        return status;
    }
    if (mapping->owns_allocation != 0) {
        if (mapping->ops.deallocate == NULL) {
            platform_dma_fatal();
        }
        mapping->ops.deallocate(
            mapping->ops.context,
            mapping->virtual_address,
            mapping->length,
            mapping->alignment
        );
    }
    mapping->virtual_address = NULL;
    mapping->physical_address = 0;
    mapping->length = 0;
    mapping->state = PLATFORM_DMA_MAPPING_RELEASED;
    return status;
}
