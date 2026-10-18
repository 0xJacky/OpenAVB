#pragma once

#include "avp/runtime/firebloom/runtime_firebloom.h"
#include "avp/runtime/ipc/runtime_registry.h"

#include <stdbool.h>
#include <stdint.h>


#define RUNTIME_REGISTRY_RECOVERED_PUBLIC __attribute__((used, retain))

typedef struct {
    uintptr_t upper_bound;
    bool multiplication_overflow;
    bool addition_succeeded;
} RuntimeRegistryUpperBoundResult;

typedef struct {
    const AVPRuntimeRegistryRecord *cursor;
    bool addition_succeeded;
} RuntimeRegistryCursorAdvanceResult;

void runtime_registry_recovered_bind_tables(
    void *index_root_table,
    size_t index_root_table_size,
    void *type_tag_table,
    size_t type_tag_table_size
);

RUNTIME_REGISTRY_RECOVERED_PUBLIC _Noreturn void
runtime_registry_copy_typed_value_unsupported_tag_fatal(void);
RUNTIME_REGISTRY_RECOVERED_PUBLIC uint64_t runtime_registry_source_token(void);
RUNTIME_REGISTRY_RECOVERED_PUBLIC RuntimeRegistryUpperBoundResult
runtime_registry_outlined_compute_table_upper_bound_checked(
    uintptr_t lower_bound,
    size_t record_count
);
RUNTIME_REGISTRY_RECOVERED_PUBLIC bool
runtime_registry_outlined_compare_cursor_to_table_end(
    const AVPRuntimeRegistryRecord *cursor,
    const AVPRuntimeRegistryRecord *table_end
);
RUNTIME_REGISTRY_RECOVERED_PUBLIC RuntimeRegistryCursorAdvanceResult
runtime_registry_outlined_advance_record_cursor_checked(
    const AVPRuntimeRegistryRecord *cursor
);
RUNTIME_REGISTRY_RECOVERED_PUBLIC RuntimeFirebloomSpan
runtime_registry_index_root_table_span(void);
RUNTIME_REGISTRY_RECOVERED_PUBLIC RuntimeFirebloomSpan
runtime_registry_type_tag_table_span(void);
