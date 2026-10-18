#include "avp/runtime/firebloom/runtime_firebloom_metadata.h"
#include "avp/runtime/memory/runtime_memory.h"


#if defined(__ELF__)
#define RUNTIME_FIREBLOOM_METADATA_TEXT \
    __attribute__((noinline, section(".text.runtime_firebloom_metadata")))
#else
#define RUNTIME_FIREBLOOM_METADATA_TEXT __attribute__((noinline))
#endif

#define RUNTIME_FIREBLOOM_MINIMUM_ALLOCATION_ALIGNMENT ((size_t)0x40)

static const RuntimeFirebloomDescriptor *require_descriptor(
    RuntimeFirebloomDescriptorView view,
    size_t field_offset,
    size_t field_size
) {
    const uintptr_t current = (uintptr_t)view.current;
    const uintptr_t lower = (uintptr_t)view.lower_bound;
    const uintptr_t upper = (uintptr_t)view.upper_bound;
    if (current < lower || current >= upper || field_offset > upper - current ||
        field_size > upper - current - field_offset) {
        runtime_firebloom_panic();
    }
    return view.current;
}

RUNTIME_FIREBLOOM_METADATA_TEXT
uint32_t runtime_firebloom_descriptor_instance_size(
    RuntimeFirebloomDescriptorView descriptor
) {
    return require_descriptor(
        descriptor,
        offsetof(RuntimeFirebloomDescriptor, instance_size),
        sizeof(descriptor.current->instance_size)
    )->instance_size;
}

RUNTIME_FIREBLOOM_METADATA_TEXT
uint32_t runtime_firebloom_descriptor_element_shift(
    RuntimeFirebloomDescriptorView descriptor
) {
    return require_descriptor(
        descriptor,
        offsetof(RuntimeFirebloomDescriptor, element_shift),
        sizeof(descriptor.current->element_shift)
    )->element_shift;
}

RUNTIME_FIREBLOOM_METADATA_TEXT
uint64_t runtime_firebloom_descriptor_type_identity(
    RuntimeFirebloomDescriptorView descriptor
) {
    return require_descriptor(
        descriptor,
        offsetof(RuntimeFirebloomDescriptor, type_identity),
        sizeof(descriptor.current->type_identity)
    )->type_identity;
}

RUNTIME_FIREBLOOM_METADATA_TEXT
uint32_t runtime_firebloom_descriptor_instance_alignment(
    RuntimeFirebloomDescriptorView descriptor
) {
    return require_descriptor(
        descriptor,
        offsetof(RuntimeFirebloomDescriptor, instance_alignment),
        sizeof(descriptor.current->instance_alignment)
    )->instance_alignment;
}

RUNTIME_FIREBLOOM_METADATA_TEXT
uint32_t runtime_firebloom_descriptor_variant_shift(
    RuntimeFirebloomDescriptorView descriptor
) {
    return require_descriptor(
        descriptor,
        offsetof(RuntimeFirebloomDescriptor, variant_shift),
        sizeof(descriptor.current->variant_shift)
    )->variant_shift;
}

RUNTIME_FIREBLOOM_METADATA_TEXT
void runtime_firebloom_descriptor_allocation_requirements(
    RuntimeFirebloomDescriptorView descriptor,
    size_t *size,
    size_t *alignment
) {
    if (size == 0 || alignment == 0) {
        runtime_firebloom_panic();
    }
    const size_t descriptor_alignment =
        runtime_firebloom_descriptor_instance_alignment(descriptor);
    const size_t descriptor_size =
        runtime_firebloom_descriptor_instance_size(descriptor);
    *alignment = descriptor_alignment >
            RUNTIME_FIREBLOOM_MINIMUM_ALLOCATION_ALIGNMENT
        ? descriptor_alignment
        : RUNTIME_FIREBLOOM_MINIMUM_ALLOCATION_ALIGNMENT;
    *size = descriptor_size > descriptor_alignment
        ? descriptor_size
        : descriptor_alignment;
}

RUNTIME_FIREBLOOM_METADATA_TEXT
void runtime_firebloom_allocate_typed_storage(
    RuntimeFirebloomDescriptorView descriptor,
    const RuntimeFirebloomAllocationProfile *profile,
    RuntimeFirebloomSpan *allocation
) {
    if (profile == 0 || profile->allocate == 0 ||
        profile->allocation_type == 0 || allocation == 0) {
        runtime_firebloom_panic();
    }
    size_t size = 0;
    size_t alignment = 0;
    runtime_firebloom_descriptor_allocation_requirements(
        descriptor,
        &size,
        &alignment
    );
    RuntimeAllocatorRoute *route = runtime_allocator_select_route(false);
    if (route == 0) {
        runtime_firebloom_panic();
    }
    profile->allocate(
        profile->context,
        route,
        true,
        size,
        alignment,
        allocation
    );
    if (allocation->current == 0 || allocation->lower_bound == 0 ||
        allocation->upper_bound == 0 || allocation->type == 0 ||
        allocation->type != profile->allocation_type) {
        runtime_firebloom_panic();
    }
    runtime_firebloom_validate_span(allocation, size);
}
