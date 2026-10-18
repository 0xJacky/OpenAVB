#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


typedef struct {
    uintptr_t identity;
    size_t element_size;
    bool bytewise_copyable;
} RuntimeFirebloomType;

typedef struct {
    void *current;
    void *lower_bound;
    void *upper_bound;
    const RuntimeFirebloomType *type;
} RuntimeFirebloomSpan;

_Static_assert(
    sizeof(RuntimeFirebloomSpan) == 0x20,
    "RuntimeFirebloomSpan must match the four-register AVPBooter tuple"
);

bool runtime_firebloom_type_is_bytewise(
    const RuntimeFirebloomType *type
);
void runtime_firebloom_validate_span(
    const RuntimeFirebloomSpan *span,
    size_t size
);
void runtime_firebloom_validate_element_span(
    const RuntimeFirebloomType *type,
    size_t size
);
bool runtime_firebloom_types_are_compatible(
    const RuntimeFirebloomType *left,
    const RuntimeFirebloomType *right
);
void *runtime_firebloom_copy_span(
    RuntimeFirebloomSpan destination,
    RuntimeFirebloomSpan source,
    size_t size
);
void *runtime_firebloom_fill_span(
    RuntimeFirebloomSpan destination,
    int value,
    size_t size
);
