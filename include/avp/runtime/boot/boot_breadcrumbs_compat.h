#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    BOOT_BREADCRUMB_ROLLOVER_RESERVE = 8,
};

typedef void (*BootBreadcrumbProgress)(
    void *context,
    uint32_t code,
    uint32_t detail,
    bool has_detail
);
typedef size_t (*BootBreadcrumbRead)(
    void *context,
    const char *key,
    size_t key_length,
    char *output,
    size_t capacity
);
typedef int (*BootBreadcrumbWrite)(
    void *context,
    const char *key,
    size_t key_length,
    const char *text,
    size_t text_length
);

typedef struct {
    void *context;
    BootBreadcrumbProgress progress;
    BootBreadcrumbRead read;
    BootBreadcrumbWrite primary_write;
    BootBreadcrumbWrite fallback_write;
} BootBreadcrumbPlatform;

typedef struct {
    char *buffer;
    char *scratch;
    size_t capacity;
    size_t write_offset;
    bool recording_enabled;
    bool rollover_pending;
    bool retained_tail_pending;
    bool category_present;
    bool commit_pending;
    BootBreadcrumbPlatform platform;
} BootBreadcrumbState;

#define BOOT_BREADCRUMB_PUBLIC __attribute__((used, retain))

BOOT_BREADCRUMB_PUBLIC void boot_breadcrumb_state_initialize(
    BootBreadcrumbState *state,
    char *buffer,
    char *scratch,
    size_t capacity,
    const BootBreadcrumbPlatform *platform
);
BOOT_BREADCRUMB_PUBLIC void boot_breadcrumb_emit_code_hook_noop(uint32_t code);
BOOT_BREADCRUMB_PUBLIC void
boot_breadcrumb_emit_code_with_detail_hook_noop(
    uint32_t code,
    uint32_t detail
);
BOOT_BREADCRUMB_PUBLIC int boot_breadcrumb_primary_sink_write_unsupported(
    void *context,
    const char *key,
    size_t key_length,
    const char *text,
    size_t text_length
);
BOOT_BREADCRUMB_PUBLIC int boot_breadcrumb_fallback_sink_write_unsupported(
    void *context,
    const char *key,
    size_t key_length,
    const char *text,
    size_t text_length
);
BOOT_BREADCRUMB_PUBLIC void boot_breadcrumb_emit_code(
    BootBreadcrumbState *state,
    uint32_t code
);
BOOT_BREADCRUMB_PUBLIC void boot_breadcrumb_buffer_rollover_if_needed(
    BootBreadcrumbState *state);
BOOT_BREADCRUMB_PUBLIC char *
boot_breadcrumb_buffer_write_span(BootBreadcrumbState *state);
BOOT_BREADCRUMB_PUBLIC _Noreturn void
boot_breadcrumb_buffer_write_index_out_of_range_fatal(void);
BOOT_BREADCRUMB_PUBLIC size_t
boot_breadcrumb_buffer_remaining_capacity(const BootBreadcrumbState *state);
BOOT_BREADCRUMB_PUBLIC _Noreturn void
boot_breadcrumb_buffer_exhausted_fatal(void);
BOOT_BREADCRUMB_PUBLIC void boot_breadcrumb_emit_code_with_detail(
    BootBreadcrumbState *state,
    uint32_t code,
    uint32_t detail
);
BOOT_BREADCRUMB_PUBLIC void boot_breadcrumb_append_label_if_enabled(
    BootBreadcrumbState *state,
    const char *label,
    size_t label_length,
    bool mark_pending
);
BOOT_BREADCRUMB_PUBLIC void boot_breadcrumb_append_label(
    BootBreadcrumbState *state,
    const char *label,
    size_t label_length,
    bool mark_pending
);
BOOT_BREADCRUMB_PUBLIC int boot_breadcrumb_finalize_and_commit(
    BootBreadcrumbState *state,
    const char *key,
    size_t key_length
);
BOOT_BREADCRUMB_PUBLIC int boot_breadcrumb_commit(
    BootBreadcrumbState *state,
    const char *key,
    size_t key_length
);
BOOT_BREADCRUMB_PUBLIC void boot_breadcrumb_buffer_compact_after_rollover(
    BootBreadcrumbState *state);
BOOT_BREADCRUMB_PUBLIC size_t boot_breadcrumb_buffer_prepend_tail_tokens(
    BootBreadcrumbState *state,
    const char *history,
    size_t history_length
);
BOOT_BREADCRUMB_PUBLIC bool boot_breadcrumb_validate_byte_tuple(
    const char *base,
    size_t length,
    size_t capacity
);
BOOT_BREADCRUMB_PUBLIC uint64_t boot_breadcrumb_source_token(void);
BOOT_BREADCRUMB_PUBLIC void boot_breadcrumb_buffer_advance_write_offset(
    BootBreadcrumbState *state,
    size_t count
);
BOOT_BREADCRUMB_PUBLIC bool boot_breadcrumb_recording_enabled_compare(
    const BootBreadcrumbState *state);
BOOT_BREADCRUMB_PUBLIC void boot_breadcrumb_record_category_flag(
    BootBreadcrumbState *state,
    uint32_t code
);
BOOT_BREADCRUMB_PUBLIC void
boot_breadcrumb_mark_commit_pending(BootBreadcrumbState *state);
