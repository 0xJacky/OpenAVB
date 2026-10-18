#include "avp/runtime/firebloom/runtime_firebloom.h"
#include "avp/runtime/memory/runtime_memory.h"


#if defined(__ELF__)
#define RUNTIME_FIREBLOOM_TEXT \
    __attribute__((section(".text.runtime_firebloom_ranges")))
#else
#define RUNTIME_FIREBLOOM_TEXT
#endif

static uintptr_t address_of(const void *pointer) {
    return (uintptr_t)pointer;
}

RUNTIME_FIREBLOOM_TEXT bool runtime_firebloom_type_is_bytewise(
    const RuntimeFirebloomType *type
) {
    if (type == 0) {
        runtime_firebloom_panic();
    }
    return type->bytewise_copyable;
}

RUNTIME_FIREBLOOM_TEXT void runtime_firebloom_validate_span(
    const RuntimeFirebloomSpan *span,
    size_t size
) {
    if (span == 0 || span->type == 0) {
        runtime_firebloom_panic();
    }
    const uintptr_t current = address_of(span->current);
    const uintptr_t lower = address_of(span->lower_bound);
    const uintptr_t upper = address_of(span->upper_bound);
    if (current < lower || current > upper || size > upper - current) {
        runtime_firebloom_panic();
    }
    if (!runtime_firebloom_type_is_bytewise(span->type)) {
        const size_t element_size = span->type->element_size;
        if (element_size == 0 || (current - lower) % element_size != 0) {
            runtime_firebloom_panic();
        }
    }
}

RUNTIME_FIREBLOOM_TEXT void runtime_firebloom_validate_element_span(
    const RuntimeFirebloomType *type,
    size_t size
) {
    if (type == 0 || type->element_size == 0 ||
        size % type->element_size != 0) {
        runtime_firebloom_panic();
    }
}

RUNTIME_FIREBLOOM_TEXT bool runtime_firebloom_types_are_compatible(
    const RuntimeFirebloomType *left,
    const RuntimeFirebloomType *right
) {
    if (left == 0 || right == 0) {
        runtime_firebloom_panic();
    }
    return left == right ||
        (left->identity != 0 && left->identity == right->identity);
}

RUNTIME_FIREBLOOM_TEXT void *runtime_firebloom_copy_span(
    RuntimeFirebloomSpan destination,
    RuntimeFirebloomSpan source,
    size_t size
) {
    if (size == 0) {
        return destination.current;
    }
    runtime_firebloom_validate_span(&destination, size);
    runtime_firebloom_validate_span(&source, size);
    const bool destination_is_bytewise =
        runtime_firebloom_type_is_bytewise(destination.type);
    const bool source_is_bytewise =
        runtime_firebloom_type_is_bytewise(source.type);
    if (!(destination_is_bytewise && source_is_bytewise)) {
        if (!runtime_firebloom_types_are_compatible(
                destination.type,
                source.type
            )) {
            runtime_firebloom_panic();
        }
        runtime_firebloom_validate_element_span(destination.type, size);
    }
    return runtime_memmove(destination.current, source.current, size);
}

RUNTIME_FIREBLOOM_TEXT void *runtime_firebloom_fill_span(
    RuntimeFirebloomSpan destination,
    int value,
    size_t size
) {
    if (size == 0) {
        return destination.current;
    }
    runtime_firebloom_validate_span(&destination, size);
    if (!runtime_firebloom_type_is_bytewise(destination.type)) {
        if (value != 0) {
            runtime_firebloom_panic();
        }
        runtime_firebloom_validate_element_span(destination.type, size);
    }
    return runtime_memset(destination.current, value, size);
}
