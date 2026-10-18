#include "avp/runtime/ipc/runtime_registry_recovered.h"


static void *index_root_table;
static size_t index_root_table_size;
static void *type_tag_table;
static size_t type_tag_table_size;

void runtime_registry_recovered_bind_tables(
    void *bound_index_root_table,
    size_t bound_index_root_table_size,
    void *bound_type_tag_table,
    size_t bound_type_tag_table_size
) {
    if ((bound_index_root_table == 0) !=
            (bound_index_root_table_size == 0) ||
        (bound_type_tag_table == 0) != (bound_type_tag_table_size == 0)) {
        __builtin_trap();
    }
    index_root_table = bound_index_root_table;
    index_root_table_size = bound_index_root_table_size;
    type_tag_table = bound_type_tag_table;
    type_tag_table_size = bound_type_tag_table_size;
}

_Noreturn void runtime_registry_copy_typed_value_unsupported_tag_fatal(void) {
    __builtin_trap();
}

uint64_t runtime_registry_source_token(void) {
    return UINT64_C(0x0fa351f01526260a);
}

RuntimeRegistryUpperBoundResult
runtime_registry_outlined_compute_table_upper_bound_checked(
    uintptr_t lower_bound,
    size_t record_count
) {
    const bool multiplication_overflow =
        record_count > SIZE_MAX / sizeof(AVPRuntimeRegistryRecord);
    size_t byte_size = multiplication_overflow
        ? 0
        : record_count * sizeof(AVPRuntimeRegistryRecord);
    const bool addition_succeeded = UINTPTR_MAX - lower_bound >= byte_size;
    return (RuntimeRegistryUpperBoundResult){
        .upper_bound = addition_succeeded ? lower_bound + byte_size : 0,
        .multiplication_overflow = multiplication_overflow,
        .addition_succeeded = addition_succeeded,
    };
}

bool runtime_registry_outlined_compare_cursor_to_table_end(
    const AVPRuntimeRegistryRecord *cursor,
    const AVPRuntimeRegistryRecord *table_end
) {
    return (uintptr_t)cursor >= (uintptr_t)table_end;
}

RuntimeRegistryCursorAdvanceResult
runtime_registry_outlined_advance_record_cursor_checked(
    const AVPRuntimeRegistryRecord *cursor
) {
    const uintptr_t address = (uintptr_t)cursor;
    const bool succeeded = UINTPTR_MAX - address >= sizeof(*cursor);
    return (RuntimeRegistryCursorAdvanceResult){
        .cursor = succeeded
            ? (const AVPRuntimeRegistryRecord *)(address + sizeof(*cursor))
            : 0,
        .addition_succeeded = succeeded,
    };
}

static RuntimeFirebloomSpan table_span(void *table, size_t size) {
    if (table == 0 || size == 0 || UINTPTR_MAX - (uintptr_t)table < size) {
        __builtin_trap();
    }
    return (RuntimeFirebloomSpan){
        .current = table,
        .lower_bound = table,
        .upper_bound = (uint8_t *)table + size,
        .type = 0,
    };
}

RuntimeFirebloomSpan runtime_registry_index_root_table_span(void) {
    return table_span(index_root_table, index_root_table_size);
}

RuntimeFirebloomSpan runtime_registry_type_tag_table_span(void) {
    return table_span(type_tag_table, type_tag_table_size);
}
