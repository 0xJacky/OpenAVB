#include "avp/runtime/boot/boot_breadcrumbs_compat.h"

static _Noreturn void boot_breadcrumb_contract_failure(void) {
    __builtin_trap();
    __builtin_unreachable();
}

static size_t bounded_length(const char *text, size_t capacity) {
    size_t length = 0;
    while (length < capacity && text[length] != '\0') {
        ++length;
    }
    return length;
}

static void copy_bytes(char *output, const char *input, size_t length) {
    for (size_t index = 0; index < length; ++index) {
        output[index] = input[index];
    }
}

static size_t append_hex(char *output, uint32_t value) {
    static const char digits[] = "0123456789abcdef";
    char reversed[8];
    size_t length = 0;
    do {
        reversed[length++] = digits[value & 15u];
        value >>= 4;
    } while (value != 0);
    for (size_t index = 0; index < length; ++index) {
        output[index] = reversed[length - index - 1];
    }
    return length;
}

static void append_bytes(
    BootBreadcrumbState *state,
    const char *bytes,
    size_t length
) {
    if (length >= boot_breadcrumb_buffer_remaining_capacity(state)) {
        boot_breadcrumb_contract_failure();
    }
    copy_bytes(boot_breadcrumb_buffer_write_span(state), bytes, length);
    boot_breadcrumb_buffer_advance_write_offset(state, length);
    state->buffer[state->write_offset] = '\0';
}

static void validate_state(const BootBreadcrumbState *state) {
    if (state == 0 || state->buffer == 0 || state->scratch == 0 ||
        state->capacity <= BOOT_BREADCRUMB_ROLLOVER_RESERVE ||
        state->buffer == state->scratch) {
        boot_breadcrumb_contract_failure();
    }
}

void BOOT_BREADCRUMB_PUBLIC boot_breadcrumb_state_initialize(
    BootBreadcrumbState *state,
    char *buffer,
    char *scratch,
    size_t capacity,
    const BootBreadcrumbPlatform *platform
) {
    if (state == 0 || buffer == 0 || scratch == 0 || platform == 0 ||
        capacity <= BOOT_BREADCRUMB_ROLLOVER_RESERVE || buffer == scratch) {
        boot_breadcrumb_contract_failure();
    }
    *state = (BootBreadcrumbState){
        .buffer = buffer,
        .scratch = scratch,
        .capacity = capacity,
        .recording_enabled = true,
        .platform = *platform,
    };
    buffer[0] = '\0';
    scratch[0] = '\0';
}

void BOOT_BREADCRUMB_PUBLIC
boot_breadcrumb_emit_code_hook_noop(uint32_t code) {
    (void)code;
}

void BOOT_BREADCRUMB_PUBLIC
boot_breadcrumb_emit_code_with_detail_hook_noop(
    uint32_t code,
    uint32_t detail
) {
    (void)code;
    (void)detail;
}

int BOOT_BREADCRUMB_PUBLIC boot_breadcrumb_primary_sink_write_unsupported(
    void *context,
    const char *key,
    size_t key_length,
    const char *text,
    size_t text_length
) {
    (void)context;
    (void)key;
    (void)key_length;
    (void)text;
    (void)text_length;
    return -1;
}

int BOOT_BREADCRUMB_PUBLIC boot_breadcrumb_fallback_sink_write_unsupported(
    void *context,
    const char *key,
    size_t key_length,
    const char *text,
    size_t text_length
) {
    (void)context;
    (void)key;
    (void)key_length;
    (void)text;
    (void)text_length;
    return -1;
}

bool BOOT_BREADCRUMB_PUBLIC boot_breadcrumb_validate_byte_tuple(
    const char *base,
    size_t length,
    size_t capacity
) {
    return base != 0 && length < capacity;
}

uint64_t BOOT_BREADCRUMB_PUBLIC boot_breadcrumb_source_token(void) {
    return UINT64_C(0x0490dd6a1de0c147);
}

_Noreturn void BOOT_BREADCRUMB_PUBLIC
boot_breadcrumb_buffer_write_index_out_of_range_fatal(void) {
    boot_breadcrumb_contract_failure();
}

_Noreturn void BOOT_BREADCRUMB_PUBLIC
boot_breadcrumb_buffer_exhausted_fatal(void) {
    boot_breadcrumb_contract_failure();
}

char BOOT_BREADCRUMB_PUBLIC *
boot_breadcrumb_buffer_write_span(BootBreadcrumbState *state) {
    validate_state(state);
    if (state->write_offset >= state->capacity) {
        boot_breadcrumb_buffer_write_index_out_of_range_fatal();
    }
    return state->buffer + state->write_offset;
}

size_t BOOT_BREADCRUMB_PUBLIC boot_breadcrumb_buffer_remaining_capacity(
    const BootBreadcrumbState *state) {
    validate_state(state);
    if (state->write_offset >= state->capacity) {
        boot_breadcrumb_buffer_exhausted_fatal();
    }
    return state->capacity - state->write_offset;
}

void BOOT_BREADCRUMB_PUBLIC boot_breadcrumb_buffer_advance_write_offset(
    BootBreadcrumbState *state,
    size_t count
) {
    validate_state(state);
    if (count >= state->capacity - state->write_offset) {
        boot_breadcrumb_contract_failure();
    }
    state->write_offset += count;
}

bool BOOT_BREADCRUMB_PUBLIC boot_breadcrumb_recording_enabled_compare(
    const BootBreadcrumbState *state) {
    validate_state(state);
    return state->recording_enabled;
}

void BOOT_BREADCRUMB_PUBLIC boot_breadcrumb_record_category_flag(
    BootBreadcrumbState *state,
    uint32_t code
) {
    validate_state(state);
    state->category_present |= (code & UINT32_C(0xc0000000)) != 0;
}

void BOOT_BREADCRUMB_PUBLIC
boot_breadcrumb_mark_commit_pending(BootBreadcrumbState *state) {
    validate_state(state);
    state->commit_pending = true;
}

void BOOT_BREADCRUMB_PUBLIC boot_breadcrumb_buffer_rollover_if_needed(
    BootBreadcrumbState *state) {
    if (boot_breadcrumb_buffer_remaining_capacity(state) >
        BOOT_BREADCRUMB_ROLLOVER_RESERVE) {
        return;
    }
    char *old_buffer = state->buffer;
    state->buffer = state->scratch;
    state->scratch = old_buffer;
    state->write_offset = 0;
    state->buffer[0] = '\0';
    state->rollover_pending = true;
    state->retained_tail_pending = true;
}

void BOOT_BREADCRUMB_PUBLIC boot_breadcrumb_emit_code(
    BootBreadcrumbState *state,
    uint32_t code
) {
    validate_state(state);
    if (state->recording_enabled) {
        boot_breadcrumb_buffer_rollover_if_needed(state);
        char formatted[9];
        size_t length = append_hex(formatted, code);
        formatted[length++] = ' ';
        append_bytes(state, formatted, length);
        boot_breadcrumb_mark_commit_pending(state);
        boot_breadcrumb_record_category_flag(state, code);
    }
    boot_breadcrumb_emit_code_hook_noop(code);
    if (state->platform.progress != 0) {
        state->platform.progress(state->platform.context, code, 0, false);
    }
}

void BOOT_BREADCRUMB_PUBLIC boot_breadcrumb_emit_code_with_detail(
    BootBreadcrumbState *state,
    uint32_t code,
    uint32_t detail
) {
    validate_state(state);
    if (state->recording_enabled) {
        boot_breadcrumb_buffer_rollover_if_needed(state);
        char formatted[20];
        size_t length = append_hex(formatted, code);
        formatted[length++] = '(';
        length += append_hex(formatted + length, detail);
        formatted[length++] = ')';
        formatted[length++] = ' ';
        append_bytes(state, formatted, length);
        boot_breadcrumb_mark_commit_pending(state);
        boot_breadcrumb_record_category_flag(state, code);
    }
    boot_breadcrumb_emit_code_with_detail_hook_noop(code, detail);
    if (state->platform.progress != 0) {
        state->platform.progress(state->platform.context, code, detail, true);
    }
}

void BOOT_BREADCRUMB_PUBLIC boot_breadcrumb_append_label(
    BootBreadcrumbState *state,
    const char *label,
    size_t label_length,
    bool mark_pending
) {
    validate_state(state);
    if (label == 0 || bounded_length(label, label_length) != label_length) {
        boot_breadcrumb_contract_failure();
    }
    boot_breadcrumb_buffer_rollover_if_needed(state);
    if (label_length + 4 >=
        boot_breadcrumb_buffer_remaining_capacity(state)) {
        boot_breadcrumb_contract_failure();
    }
    append_bytes(state, "<", 1);
    append_bytes(state, label, label_length);
    append_bytes(state, "> ", 2);
    if (mark_pending) {
        boot_breadcrumb_mark_commit_pending(state);
    }
}

void BOOT_BREADCRUMB_PUBLIC boot_breadcrumb_append_label_if_enabled(
    BootBreadcrumbState *state,
    const char *label,
    size_t label_length,
    bool mark_pending
) {
    if (boot_breadcrumb_recording_enabled_compare(state)) {
        boot_breadcrumb_append_label(state, label, label_length, mark_pending);
    }
}

size_t BOOT_BREADCRUMB_PUBLIC boot_breadcrumb_buffer_prepend_tail_tokens(
    BootBreadcrumbState *state,
    const char *history,
    size_t history_length
) {
    validate_state(state);
    if (history == 0 || history_length == 0) {
        return 0;
    }
    if (bounded_length(history, history_length) != history_length) {
        boot_breadcrumb_contract_failure();
    }

    const size_t current_length = state->write_offset;
    if (current_length >= state->capacity - 1) {
        boot_breadcrumb_contract_failure();
    }
    const bool add_separator = current_length != 0 &&
                               history[history_length - 1] != ' ' &&
                               state->buffer[0] != ' ';
    size_t available = state->capacity - current_length - 1;
    if (add_separator) {
        --available;
    }
    size_t suffix = history_length;
    if (suffix > available) {
        suffix = available;
    }
    size_t start = history_length - suffix;
    if (start != 0) {
        while (start < history_length && history[start - 1] != ' ') {
            ++start;
        }
    }
    suffix = history_length - start;

    copy_bytes(state->scratch, history + start, suffix);
    size_t output_offset = suffix;
    if (add_separator) {
        state->scratch[output_offset++] = ' ';
    }
    copy_bytes(
        state->scratch + output_offset,
        state->buffer,
        current_length + 1
    );
    char *old_buffer = state->buffer;
    state->buffer = state->scratch;
    state->scratch = old_buffer;
    state->write_offset = output_offset + current_length;
    return suffix;
}

void BOOT_BREADCRUMB_PUBLIC boot_breadcrumb_buffer_compact_after_rollover(
    BootBreadcrumbState *state) {
    validate_state(state);
    if (!state->rollover_pending) {
        return;
    }
    const size_t retained_length = bounded_length(
        state->scratch,
        state->capacity
    );
    if (retained_length == state->capacity) {
        boot_breadcrumb_contract_failure();
    }
    boot_breadcrumb_buffer_prepend_tail_tokens(
        state,
        state->scratch,
        retained_length
    );
    state->rollover_pending = false;
    state->retained_tail_pending = false;
}

int BOOT_BREADCRUMB_PUBLIC boot_breadcrumb_commit(
    BootBreadcrumbState *state,
    const char *key,
    size_t key_length
) {
    validate_state(state);
    if (key == 0 || bounded_length(key, key_length) != key_length) {
        boot_breadcrumb_contract_failure();
    }
    if (!state->recording_enabled) {
        return 0;
    }
    boot_breadcrumb_buffer_compact_after_rollover(state);

    if (state->category_present && state->commit_pending) {
        boot_breadcrumb_append_label(state, "COMMIT", 6, false);
        boot_breadcrumb_buffer_compact_after_rollover(state);
    }

    if (state->platform.read != 0) {
        const size_t history_length = state->platform.read(
            state->platform.context,
            key,
            key_length,
            state->scratch,
            state->capacity
        );
        if (history_length >= state->capacity ||
            bounded_length(state->scratch, history_length) != history_length) {
            boot_breadcrumb_contract_failure();
        }
        boot_breadcrumb_buffer_prepend_tail_tokens(
            state,
            state->scratch,
            history_length
        );
    }

    if (!state->category_present || !state->commit_pending ||
        state->buffer == 0) {
        return 0;
    }
    int result = -1;
    if (state->platform.primary_write != 0) {
        result = state->platform.primary_write(
            state->platform.context,
            key,
            key_length,
            state->buffer,
            state->write_offset
        );
    }
    if (result == 0 && state->platform.fallback_write != 0) {
        result = state->platform.fallback_write(
            state->platform.context,
            key,
            key_length,
            state->buffer,
            state->write_offset
        );
    }
    if (result >= 0) {
        state->commit_pending = false;
    }
    return result;
}

int BOOT_BREADCRUMB_PUBLIC boot_breadcrumb_finalize_and_commit(
    BootBreadcrumbState *state,
    const char *key,
    size_t key_length
) {
    if (!boot_breadcrumb_recording_enabled_compare(state)) {
        return 0;
    }
    boot_breadcrumb_append_label(state, "DONE", 4, true);
    const int result = boot_breadcrumb_commit(state, key, key_length);
    state->buffer = 0;
    state->scratch = 0;
    state->capacity = 0;
    state->write_offset = 0;
    state->recording_enabled = false;
    return result;
}
