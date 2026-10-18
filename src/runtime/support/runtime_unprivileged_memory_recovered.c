#include "avp/runtime/support/runtime_unprivileged_memory_recovered.h"


/*
 * Width-specific hooks keep lower-EL translation semantics at the exact
 * access boundary. The host path models the same little-endian byte stream.
 */

#if defined(__aarch64__) && !defined(__APPLE__)
uint64_t runtime_unprivileged_load_u64_arch(const void *source);
uint32_t runtime_unprivileged_load_u32_arch(const void *source);
uint16_t runtime_unprivileged_load_u16_arch(const void *source);
uint8_t runtime_unprivileged_load_u8_arch(const void *source);
void runtime_unprivileged_store_u64_arch(void *destination, uint64_t value);
void runtime_unprivileged_store_u32_arch(void *destination, uint32_t value);
void runtime_unprivileged_store_u16_arch(void *destination, uint16_t value);
void runtime_unprivileged_store_u8_arch(void *destination, uint8_t value);
#else
static uint64_t runtime_unprivileged_load_u64_arch(const void *source) {
    const uint8_t *bytes = source;
    uint64_t value = 0;
    for (size_t index = 0; index < sizeof(value); ++index) {
        value |= (uint64_t)bytes[index] << (index * 8);
    }
    return value;
}

static uint32_t runtime_unprivileged_load_u32_arch(const void *source) {
    const uint8_t *bytes = source;
    uint32_t value = 0;
    for (size_t index = 0; index < sizeof(value); ++index) {
        value |= (uint32_t)bytes[index] << (index * 8);
    }
    return value;
}

static uint16_t runtime_unprivileged_load_u16_arch(const void *source) {
    const uint8_t *bytes = source;
    return (uint16_t)((uint16_t)bytes[0] | (uint16_t)bytes[1] << 8);
}

static uint8_t runtime_unprivileged_load_u8_arch(const void *source) {
    return *(const uint8_t *)source;
}

static void runtime_unprivileged_store_u64_arch(
    void *destination,
    uint64_t value
) {
    uint8_t *bytes = destination;
    for (size_t index = 0; index < sizeof(value); ++index) {
        bytes[index] = (uint8_t)(value >> (index * 8));
    }
}

static void runtime_unprivileged_store_u32_arch(
    void *destination,
    uint32_t value
) {
    uint8_t *bytes = destination;
    for (size_t index = 0; index < sizeof(value); ++index) {
        bytes[index] = (uint8_t)(value >> (index * 8));
    }
}

static void runtime_unprivileged_store_u16_arch(
    void *destination,
    uint16_t value
) {
    uint8_t *bytes = destination;
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
}

static void runtime_unprivileged_store_u8_arch(
    void *destination,
    uint8_t value
) {
    *(uint8_t *)destination = value;
}
#endif

const RuntimeFirebloomType runtime_unprivileged_byte_type = {
    .identity = UINT64_C(0x62797465),
    .element_size = 1,
    .bytewise_copyable = true,
};

const RuntimeFirebloomType runtime_unprivileged_u16_type = {
    .identity = UINT64_C(0x753136),
    .element_size = 2,
    .bytewise_copyable = true,
};

const RuntimeFirebloomType runtime_unprivileged_u32_type = {
    .identity = UINT64_C(0x753332),
    .element_size = 4,
    .bytewise_copyable = true,
};

const RuntimeFirebloomType runtime_unprivileged_u64_type = {
    .identity = UINT64_C(0x753634),
    .element_size = 8,
    .bytewise_copyable = true,
};

static _Noreturn void runtime_unprivileged_memory_fail(void) {
    __builtin_trap();
}

static uint8_t *runtime_unprivileged_require_span(
    RuntimeFirebloomSpan span,
    size_t byte_count
) {
    const uintptr_t current = (uintptr_t)span.current;
    const uintptr_t lower = (uintptr_t)span.lower_bound;
    const uintptr_t upper = (uintptr_t)span.upper_bound;
    if (current < lower || upper < current || byte_count > upper - current) {
        runtime_unprivileged_memory_fail();
    }
    return span.current;
}

static RuntimeFirebloomSpan runtime_unprivileged_validate_width(
    RuntimeFirebloomSpan span,
    const RuntimeFirebloomType *type
) {
    (void)runtime_unprivileged_require_span(span, type->element_size);
    span.type = type;
    return span;
}

RuntimeFirebloomSpan runtime_unprivileged_copy_validate_u64_tuple(
    RuntimeFirebloomSpan span
) {
    return runtime_unprivileged_validate_width(
        span,
        &runtime_unprivileged_u64_type
    );
}

RuntimeFirebloomSpan runtime_unprivileged_copy_validate_u32_tuple(
    RuntimeFirebloomSpan span
) {
    return runtime_unprivileged_validate_width(
        span,
        &runtime_unprivileged_u32_type
    );
}

RuntimeFirebloomSpan runtime_unprivileged_copy_validate_u16_tuple(
    RuntimeFirebloomSpan span
) {
    return runtime_unprivileged_validate_width(
        span,
        &runtime_unprivileged_u16_type
    );
}

RuntimeFirebloomSpan runtime_unprivileged_copy_make_destination_byte_span(
    void *base,
    const void *upper_bound,
    size_t byte_offset
) {
    return (RuntimeFirebloomSpan){
        .current = (void *)((uintptr_t)base + byte_offset),
        .lower_bound = base,
        .upper_bound = (void *)upper_bound,
        .type = &runtime_unprivileged_byte_type,
    };
}

RuntimeFirebloomSpan runtime_unprivileged_copy_make_source_byte_span(
    const void *base,
    const void *upper_bound,
    size_t byte_offset
) {
    return (RuntimeFirebloomSpan){
        .current = (void *)((uintptr_t)base + byte_offset),
        .lower_bound = (void *)base,
        .upper_bound = (void *)upper_bound,
        .type = &runtime_unprivileged_byte_type,
    };
}

static void runtime_store_native_u64(void *destination, uint64_t value) {
    uint8_t *bytes = destination;
    for (size_t index = 0; index < sizeof(value); ++index) {
        bytes[index] = (uint8_t)(value >> (index * 8));
    }
}

static void runtime_store_native_u32(void *destination, uint32_t value) {
    uint8_t *bytes = destination;
    for (size_t index = 0; index < sizeof(value); ++index) {
        bytes[index] = (uint8_t)(value >> (index * 8));
    }
}

static void runtime_store_native_u16(void *destination, uint16_t value) {
    uint8_t *bytes = destination;
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
}

static uint64_t runtime_load_native_u64(const void *source) {
    const uint8_t *bytes = source;
    uint64_t value = 0;
    for (size_t index = 0; index < sizeof(value); ++index) {
        value |= (uint64_t)bytes[index] << (index * 8);
    }
    return value;
}

static uint32_t runtime_load_native_u32(const void *source) {
    const uint8_t *bytes = source;
    uint32_t value = 0;
    for (size_t index = 0; index < sizeof(value); ++index) {
        value |= (uint32_t)bytes[index] << (index * 8);
    }
    return value;
}

static uint16_t runtime_load_native_u16(const void *source) {
    const uint8_t *bytes = source;
    return (uint16_t)((uint16_t)bytes[0] | (uint16_t)bytes[1] << 8);
}

void runtime_copy_from_unprivileged(
    RuntimeFirebloomSpan destination,
    const void *unprivileged_source,
    size_t byte_count
) {
    uint8_t *output = runtime_unprivileged_require_span(
        destination,
        byte_count
    );
    const uint8_t *input = unprivileged_source;
    size_t offset = 0;
    const size_t prefix = (0U - (uintptr_t)input) & 7U;
    while (offset < byte_count && offset < prefix) {
        output[offset] = runtime_unprivileged_load_u8_arch(input + offset);
        ++offset;
    }
    while (byte_count - offset >= 8) {
        runtime_store_native_u64(
            output + offset,
            runtime_unprivileged_load_u64_arch(input + offset)
        );
        offset += 8;
    }
    if (byte_count - offset >= 4) {
        runtime_store_native_u32(
            output + offset,
            runtime_unprivileged_load_u32_arch(input + offset)
        );
        offset += 4;
    }
    if (byte_count - offset >= 2) {
        runtime_store_native_u16(
            output + offset,
            runtime_unprivileged_load_u16_arch(input + offset)
        );
        offset += 2;
    }
    if (offset < byte_count) {
        output[offset] = runtime_unprivileged_load_u8_arch(input + offset);
    }
}

void runtime_copy_to_unprivileged(
    void *unprivileged_destination,
    RuntimeFirebloomSpan source,
    size_t byte_count
) {
    uint8_t *output = unprivileged_destination;
    const uint8_t *input = runtime_unprivileged_require_span(
        source,
        byte_count
    );
    size_t offset = 0;
    const size_t prefix = (0U - (uintptr_t)output) & 7U;
    while (offset < byte_count && offset < prefix) {
        runtime_unprivileged_store_u8_arch(output + offset, input[offset]);
        ++offset;
    }
    while (byte_count - offset >= 8) {
        runtime_unprivileged_store_u64_arch(
            output + offset,
            runtime_load_native_u64(input + offset)
        );
        offset += 8;
    }
    if (byte_count - offset >= 4) {
        runtime_unprivileged_store_u32_arch(
            output + offset,
            runtime_load_native_u32(input + offset)
        );
        offset += 4;
    }
    if (byte_count - offset >= 2) {
        runtime_unprivileged_store_u16_arch(
            output + offset,
            runtime_load_native_u16(input + offset)
        );
        offset += 2;
    }
    if (offset < byte_count) {
        runtime_unprivileged_store_u8_arch(output + offset, input[offset]);
    }
}
