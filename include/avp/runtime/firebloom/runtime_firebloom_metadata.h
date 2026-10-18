#pragma once

#include "avp/runtime/memory/allocator/runtime_allocator.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


typedef struct {
    uint8_t opaque_00[0x08];
    uint32_t instance_size;
    uint8_t opaque_0c[0x0c];
    uint32_t element_shift;
    uint32_t opaque_1c;
    uint64_t type_identity;
    uint32_t instance_alignment;
    uint32_t variant_shift;
} RuntimeFirebloomDescriptor;

_Static_assert(
    offsetof(RuntimeFirebloomDescriptor, instance_size) == 0x08,
    "RuntimeFirebloomDescriptor instance size offset"
);
_Static_assert(
    offsetof(RuntimeFirebloomDescriptor, element_shift) == 0x18,
    "RuntimeFirebloomDescriptor element shift offset"
);
_Static_assert(
    offsetof(RuntimeFirebloomDescriptor, type_identity) == 0x20,
    "RuntimeFirebloomDescriptor type identity offset"
);
_Static_assert(
    offsetof(RuntimeFirebloomDescriptor, instance_alignment) == 0x28,
    "RuntimeFirebloomDescriptor instance alignment offset"
);
_Static_assert(
    offsetof(RuntimeFirebloomDescriptor, variant_shift) == 0x2c,
    "RuntimeFirebloomDescriptor variant shift offset"
);
_Static_assert(
    sizeof(RuntimeFirebloomDescriptor) == 0x30,
    "RuntimeFirebloomDescriptor size"
);

typedef struct {
    const RuntimeFirebloomDescriptor *current;
    const void *lower_bound;
    const void *upper_bound;
    uintptr_t context;
} RuntimeFirebloomDescriptorView;

_Static_assert(
    sizeof(RuntimeFirebloomDescriptorView) == 0x20,
    "RuntimeFirebloomDescriptorView tuple size"
);

typedef void (*RuntimeFirebloomAllocateFunction)(
    void *context,
    RuntimeAllocatorRoute *route,
    bool clear,
    size_t size,
    size_t alignment,
    RuntimeFirebloomSpan *allocation
);

typedef struct {
    RuntimeFirebloomAllocateFunction allocate;
    void *context;
    const RuntimeFirebloomType *allocation_type;
} RuntimeFirebloomAllocationProfile;

uint32_t runtime_firebloom_descriptor_instance_size(
    RuntimeFirebloomDescriptorView descriptor
);
uint32_t runtime_firebloom_descriptor_element_shift(
    RuntimeFirebloomDescriptorView descriptor
);
uint64_t runtime_firebloom_descriptor_type_identity(
    RuntimeFirebloomDescriptorView descriptor
);
uint32_t runtime_firebloom_descriptor_instance_alignment(
    RuntimeFirebloomDescriptorView descriptor
);
uint32_t runtime_firebloom_descriptor_variant_shift(
    RuntimeFirebloomDescriptorView descriptor
);
void runtime_firebloom_descriptor_allocation_requirements(
    RuntimeFirebloomDescriptorView descriptor,
    size_t *size,
    size_t *alignment
);
void runtime_firebloom_allocate_typed_storage(
    RuntimeFirebloomDescriptorView descriptor,
    const RuntimeFirebloomAllocationProfile *profile,
    RuntimeFirebloomSpan *allocation
);
