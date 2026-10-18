#include "avp/runtime/boot/boot_breadcrumbs_compat.h"

#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

enum {
    TEST_CAPACITY = 96,
};

typedef struct {
    char persisted[TEST_CAPACITY];
    char written[TEST_CAPACITY];
    size_t written_length;
    uint32_t progress_code;
    uint32_t progress_detail;
    bool progress_has_detail;
    size_t progress_count;
    int primary_result;
    int fallback_result;
    size_t primary_count;
    size_t fallback_count;
} TestPlatform;

static void progress(void *context, uint32_t code, uint32_t detail,
                     bool has_detail) {
    TestPlatform *platform = context;
    platform->progress_code = code;
    platform->progress_detail = detail;
    platform->progress_has_detail = has_detail;
    ++platform->progress_count;
}

static size_t read_history(void *context, const char *key, size_t key_length,
                           char *output, size_t capacity) {
    TestPlatform *platform = context;
    assert(key_length == 4);
    assert(memcmp(key, "boot", 4) == 0);
    const size_t length = strlen(platform->persisted);
    assert(length < capacity);
    memcpy(output, platform->persisted, length + 1);
    return length;
}

static int primary_write(void *context, const char *key, size_t key_length,
                         const char *text, size_t text_length) {
    TestPlatform *platform = context;
    assert(key_length == 4);
    assert(memcmp(key, "boot", 4) == 0);
    assert(text_length < sizeof(platform->written));
    memcpy(platform->written, text, text_length);
    platform->written[text_length] = '\0';
    platform->written_length = text_length;
    ++platform->primary_count;
    return platform->primary_result;
}

static int fallback_write(void *context, const char *key, size_t key_length,
                          const char *text, size_t text_length) {
    TestPlatform *platform = context;
    assert(key_length == 4);
    assert(memcmp(key, "boot", 4) == 0);
    assert(text_length < sizeof(platform->written));
    memcpy(platform->written, text, text_length);
    platform->written[text_length] = '\0';
    platform->written_length = text_length;
    ++platform->fallback_count;
    return platform->fallback_result;
}

static BootBreadcrumbState make_state(TestPlatform *platform, char *buffer,
                                      char *scratch, size_t capacity) {
    BootBreadcrumbState state;
    const BootBreadcrumbPlatform binding = {
        .context = platform,
        .progress = progress,
        .read = read_history,
        .primary_write = primary_write,
        .fallback_write = fallback_write,
    };
    boot_breadcrumb_state_initialize(&state, buffer, scratch, capacity,
                                     &binding);
    return state;
}

static void test_emit_and_progress_contract(void) {
    TestPlatform platform = {0};
    char buffer[TEST_CAPACITY];
    char scratch[TEST_CAPACITY];
    BootBreadcrumbState state =
        make_state(&platform, buffer, scratch, sizeof(buffer));

    boot_breadcrumb_emit_code(&state, UINT32_C(0x40040052));
    assert(strcmp(state.buffer, "40040052 ") == 0);
    assert(state.write_offset == 9);
    assert(state.commit_pending);
    assert(state.category_present);
    assert(platform.progress_code == UINT32_C(0x40040052));
    assert(platform.progress_detail == 0);
    assert(!platform.progress_has_detail);

    boot_breadcrumb_emit_code_with_detail(&state, UINT32_C(0x130008),
                                          UINT32_C(0xfeed));
    assert(strcmp(state.buffer, "40040052 130008(feed) ") == 0);
    assert(platform.progress_code == UINT32_C(0x130008));
    assert(platform.progress_detail == UINT32_C(0xfeed));
    assert(platform.progress_has_detail);
    assert(platform.progress_count == 2);
}

static void test_disabled_recording_still_publishes_progress(void) {
    TestPlatform platform = {0};
    char buffer[TEST_CAPACITY];
    char scratch[TEST_CAPACITY];
    BootBreadcrumbState state =
        make_state(&platform, buffer, scratch, sizeof(buffer));
    state.recording_enabled = false;

    boot_breadcrumb_emit_code(&state, UINT32_C(0xc001));
    boot_breadcrumb_emit_code_with_detail(&state, UINT32_C(0xc002), 7);
    boot_breadcrumb_append_label_if_enabled(&state, "SKIP", 4, true);
    assert(state.write_offset == 0);
    assert(strcmp(state.buffer, "") == 0);
    assert(platform.progress_count == 2);
    assert(platform.progress_code == UINT32_C(0xc002));
}

static size_t oracle_suffix_start(const char *history, size_t history_length,
                                  size_t available) {
    size_t suffix = history_length < available ? history_length : available;
    size_t start = history_length - suffix;
    if (start != 0) {
        while (start < history_length && history[start - 1] != ' ') {
            ++start;
        }
    }
    return start;
}

static void test_tail_token_oracle(void) {
    uint64_t random = UINT64_C(0x6272656164637275);
    for (size_t iteration = 0; iteration < 1024; ++iteration) {
        TestPlatform platform = {0};
        char buffer[TEST_CAPACITY];
        char scratch[TEST_CAPACITY];
        BootBreadcrumbState state =
            make_state(&platform, buffer, scratch, sizeof(buffer));
        memcpy(state.buffer, "NEW ", 5);
        state.write_offset = 4;

        char history[192];
        size_t length = 0;
        for (size_t token = 0; token < 24; ++token) {
            random ^= random << 13;
            random ^= random >> 7;
            random ^= random << 17;
            const size_t digits = 1 + (random & 3);
            for (size_t digit = 0; digit < digits; ++digit) {
                history[length++] =
                    "0123456789abcdef"[(random >> (digit * 4)) & 15];
            }
            history[length++] = ' ';
        }
        history[length] = '\0';

        const size_t start = oracle_suffix_start(
            history, length, TEST_CAPACITY - state.write_offset - 1);
        const size_t copied = boot_breadcrumb_buffer_prepend_tail_tokens(
            &state, history, length);
        assert(copied == length - start);
        assert(memcmp(state.buffer, history + start, copied) == 0);
        assert(strcmp(state.buffer + copied, "NEW ") == 0);
        assert(state.write_offset == copied + 4);
        if (start != 0) {
            assert(history[start - 1] == ' ');
        }
    }
}

static void test_tail_tokens_insert_missing_separator(void) {
    TestPlatform platform = {0};
    char buffer[TEST_CAPACITY];
    char scratch[TEST_CAPACITY];
    BootBreadcrumbState state =
        make_state(&platform, buffer, scratch, sizeof(buffer));
    memcpy(state.buffer, "NEW ", 5);
    state.write_offset = 4;
    assert(boot_breadcrumb_buffer_prepend_tail_tokens(&state, "OLD", 3) == 3);
    assert(strcmp(state.buffer, "OLD NEW ") == 0);
}

static void test_rollover_preserves_complete_tail(void) {
    TestPlatform platform = {0};
    char buffer[32];
    char scratch[32];
    BootBreadcrumbState state =
        make_state(&platform, buffer, scratch, sizeof(buffer));
    memcpy(state.buffer, "one two three four five ", 25);
    state.write_offset = 24;
    boot_breadcrumb_buffer_rollover_if_needed(&state);
    assert(state.write_offset == 0);
    assert(state.rollover_pending);
    assert(state.retained_tail_pending);
    boot_breadcrumb_emit_code(&state, UINT32_C(0xc1));
    boot_breadcrumb_buffer_compact_after_rollover(&state);
    assert(strcmp(state.buffer, "one two three four five c1 ") == 0);
    assert(!state.rollover_pending);
    assert(!state.retained_tail_pending);
}

static void test_commit_routing_and_history(void) {
    TestPlatform platform = {
        .persisted = "OLD1 OLD2 ",
        .primary_result = 0,
        .fallback_result = 1,
    };
    char buffer[TEST_CAPACITY];
    char scratch[TEST_CAPACITY];
    BootBreadcrumbState state =
        make_state(&platform, buffer, scratch, sizeof(buffer));
    boot_breadcrumb_emit_code(&state, UINT32_C(0x80000003));
    boot_breadcrumb_append_label(&state, "READY", 5, false);
    assert(boot_breadcrumb_commit(&state, "boot", 4) == 1);
    assert(strcmp(platform.written,
                  "OLD1 OLD2 80000003 <READY> <COMMIT> ") == 0);
    assert(platform.primary_count == 1);
    assert(platform.fallback_count == 1);
    assert(!state.commit_pending);
}

static void test_failed_commit_remains_pending(void) {
    TestPlatform platform = {
        .primary_result = -1,
        .fallback_result = 1,
    };
    char buffer[TEST_CAPACITY];
    char scratch[TEST_CAPACITY];
    BootBreadcrumbState state =
        make_state(&platform, buffer, scratch, sizeof(buffer));
    boot_breadcrumb_emit_code(&state, UINT32_C(0x40000001));
    assert(boot_breadcrumb_commit(&state, "boot", 4) == -1);
    assert(state.commit_pending);
    assert(platform.primary_count == 1);
    assert(platform.fallback_count == 0);
}

static void test_finalize_contract(void) {
    TestPlatform platform = {.primary_result = 1};
    char buffer[TEST_CAPACITY];
    char scratch[TEST_CAPACITY];
    BootBreadcrumbState state =
        make_state(&platform, buffer, scratch, sizeof(buffer));
    boot_breadcrumb_emit_code(&state, UINT32_C(0xc0000010));
    assert(boot_breadcrumb_finalize_and_commit(&state, "boot", 4) == 1);
    assert(strcmp(platform.written, "c0000010 <DONE> <COMMIT> ") == 0);
    assert(state.buffer == 0);
    assert(state.capacity == 0);
    assert(!state.recording_enabled);
}

static void expect_trap(void (*operation)(void)) {
    const pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        operation();
        _exit(0);
    }
    int status = 0;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status));
    assert(WTERMSIG(status) == SIGILL || WTERMSIG(status) == SIGTRAP);
}

static void trap_write_index(void) {
    boot_breadcrumb_buffer_write_index_out_of_range_fatal();
}

static void trap_exhausted(void) {
    boot_breadcrumb_buffer_exhausted_fatal();
}

static void test_helpers_and_contract_failures(void) {
    assert(boot_breadcrumb_validate_byte_tuple("x", 1, 2));
    assert(!boot_breadcrumb_validate_byte_tuple(0, 0, 1));
    assert(!boot_breadcrumb_validate_byte_tuple("x", 1, 1));
    assert(boot_breadcrumb_source_token() == UINT64_C(0x0490dd6a1de0c147));
    assert(boot_breadcrumb_primary_sink_write_unsupported(0, 0, 0, 0, 0) ==
           -1);
    assert(boot_breadcrumb_fallback_sink_write_unsupported(0, 0, 0, 0, 0) ==
           -1);
    boot_breadcrumb_emit_code_hook_noop(0);
    boot_breadcrumb_emit_code_with_detail_hook_noop(0, 0);
    expect_trap(trap_write_index);
    expect_trap(trap_exhausted);
}

int main(void) {
    test_emit_and_progress_contract();
    test_disabled_recording_still_publishes_progress();
    test_tail_token_oracle();
    test_tail_tokens_insert_missing_separator();
    test_rollover_preserves_complete_tail();
    test_commit_routing_and_history();
    test_failed_commit_remains_pending();
    test_finalize_contract();
    test_helpers_and_contract_failures();
    return 0;
}
