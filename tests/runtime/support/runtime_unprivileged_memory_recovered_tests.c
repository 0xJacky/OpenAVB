#include "avp/runtime/support/runtime_unprivileged_memory_recovered.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>


static uint8_t fatal_storage[8];

static RuntimeFirebloomSpan span(void *storage, size_t size) {
    return (RuntimeFirebloomSpan){
        .current = storage,
        .lower_bound = storage,
        .upper_bound = (uint8_t *)storage + size,
        .type = &runtime_unprivileged_byte_type,
    };
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
}

static void invoke_copy_from_overflow(void) {
    runtime_copy_from_unprivileged(
        span(fatal_storage, sizeof(fatal_storage)),
        fatal_storage,
        sizeof(fatal_storage) + 1
    );
}

static void invoke_copy_to_invalid_lower(void) {
    RuntimeFirebloomSpan source = span(fatal_storage, sizeof(fatal_storage));
    source.lower_bound = fatal_storage + 1;
    runtime_copy_to_unprivileged(fatal_storage, source, 1);
}

static void invoke_short_u64_tuple(void) {
    (void)runtime_unprivileged_copy_validate_u64_tuple(
        span(fatal_storage, sizeof(fatal_storage) - 1)
    );
}

static void test_copy_widths_and_alignment(void) {
    uint8_t source[96];
    uint8_t destination[96];
    for (size_t index = 0; index < sizeof(source); ++index) {
        source[index] = (uint8_t)(index * 37U + 11U);
    }
    for (size_t source_offset = 0; source_offset < 8; ++source_offset) {
        for (size_t destination_offset = 0;
             destination_offset < 8;
             ++destination_offset) {
            for (size_t size = 0; size <= 64; ++size) {
                for (size_t index = 0; index < sizeof(destination); ++index) {
                    destination[index] = UINT8_C(0xa5);
                }
                runtime_copy_from_unprivileged(
                    span(destination + destination_offset, size),
                    source + source_offset,
                    size
                );
                for (size_t index = 0; index < size; ++index) {
                    assert(destination[destination_offset + index] ==
                        source[source_offset + index]);
                }

                for (size_t index = 0; index < sizeof(destination); ++index) {
                    destination[index] = UINT8_C(0x5a);
                }
                runtime_copy_to_unprivileged(
                    destination + destination_offset,
                    span(source + source_offset, size),
                    size
                );
                for (size_t index = 0; index < size; ++index) {
                    assert(destination[destination_offset + index] ==
                        source[source_offset + index]);
                }
            }
        }
    }
    runtime_copy_from_unprivileged((RuntimeFirebloomSpan){0}, 0, 0);
    runtime_copy_to_unprivileged(0, (RuntimeFirebloomSpan){0}, 0);
}

static void test_tuple_validation(void) {
    uint8_t storage[16] = {0};
    RuntimeFirebloomSpan input = span(storage, sizeof(storage));
    RuntimeFirebloomSpan validated =
        runtime_unprivileged_copy_validate_u64_tuple(input);
    assert(validated.type == &runtime_unprivileged_u64_type);
    validated = runtime_unprivileged_copy_validate_u32_tuple(input);
    assert(validated.type == &runtime_unprivileged_u32_type);
    validated = runtime_unprivileged_copy_validate_u16_tuple(input);
    assert(validated.type == &runtime_unprivileged_u16_type);
}

static void test_byte_span_materialization(void) {
    uint8_t storage[16] = {0};
    const RuntimeFirebloomSpan destination =
        runtime_unprivileged_copy_make_destination_byte_span(
            storage,
            storage + sizeof(storage),
            5
        );
    assert(destination.current == storage + 5);
    assert(destination.lower_bound == storage);
    assert(destination.upper_bound == storage + sizeof(storage));
    assert(destination.type == &runtime_unprivileged_byte_type);

    const RuntimeFirebloomSpan source =
        runtime_unprivileged_copy_make_source_byte_span(
            storage,
            storage + sizeof(storage),
            7
        );
    assert(source.current == storage + 7);
    assert(source.lower_bound == storage);
    assert(source.upper_bound == storage + sizeof(storage));
    assert(source.type == &runtime_unprivileged_byte_type);
}

int main(void) {
    test_copy_widths_and_alignment();
    test_tuple_validation();
    test_byte_span_materialization();
    expect_trap(invoke_copy_from_overflow);
    expect_trap(invoke_copy_to_invalid_lower);
    expect_trap(invoke_short_u64_tuple);
    puts("runtime recovered unprivileged memory tests passed");
    return 0;
}
