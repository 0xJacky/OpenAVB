#include "avp/runtime/firebloom/runtime_firebloom.h"

#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>


static const RuntimeFirebloomType bytewise_type = {
    .identity = 1,
    .element_size = 1,
    .bytewise_copyable = true,
};

static const RuntimeFirebloomType typed_words = {
    .identity = 0x100,
    .element_size = 4,
    .bytewise_copyable = false,
};

static const RuntimeFirebloomType typed_words_alias = {
    .identity = 0x100,
    .element_size = 4,
    .bytewise_copyable = false,
};

static const RuntimeFirebloomType typed_qwords = {
    .identity = 0x200,
    .element_size = 8,
    .bytewise_copyable = false,
};

_Noreturn void pvpanic_trigger(void) {
    raise(SIGABRT);
    __builtin_unreachable();
}

static RuntimeFirebloomSpan span(
    void *current,
    void *lower,
    void *upper,
    const RuntimeFirebloomType *type
) {
    return (RuntimeFirebloomSpan){
        .current = current,
        .lower_bound = lower,
        .upper_bound = upper,
        .type = type,
    };
}

static void expect_fatal(void (*operation)(void)) {
    const pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        operation();
        _Exit(0);
    }
    int status = 0;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status));
}

static void test_bytewise_copy_and_overlap(void) {
    uint8_t source[] = {0, 1, 2, 3, 4, 5, 6, 7};
    uint8_t destination[8] = {0};
    assert(runtime_firebloom_copy_span(
        span(destination, destination, destination + 8, &bytewise_type),
        span(source, source, source + 8, &bytewise_type),
        sizeof(source)
    ) == destination);
    for (size_t index = 0; index < sizeof(source); ++index) {
        assert(destination[index] == source[index]);
    }

    runtime_firebloom_copy_span(
        span(source + 2, source, source + 8, &bytewise_type),
        span(source, source, source + 8, &bytewise_type),
        6
    );
    const uint8_t expected[] = {0, 1, 0, 1, 2, 3, 4, 5};
    for (size_t index = 0; index < sizeof(source); ++index) {
        assert(source[index] == expected[index]);
    }
}

static void test_typed_copy_and_zero_fill(void) {
    uint32_t source[] = {1, 2, 3, 4};
    uint32_t destination[4] = {0};
    runtime_firebloom_copy_span(
        span(
            destination,
            destination,
            destination + 4,
            &typed_words
        ),
        span(source, source, source + 4, &typed_words_alias),
        sizeof(source)
    );
    for (size_t index = 0; index < 4; ++index) {
        assert(destination[index] == source[index]);
    }
    runtime_firebloom_fill_span(
        span(
            destination,
            destination,
            destination + 4,
            &typed_words
        ),
        0,
        sizeof(destination)
    );
    for (size_t index = 0; index < 4; ++index) {
        assert(destination[index] == 0);
    }
}

static void test_zero_size_preserves_original_fast_path(void) {
    assert(runtime_firebloom_copy_span(
        span((void *)1, (void *)2, (void *)0, 0),
        span((void *)3, (void *)4, (void *)0, 0),
        0
    ) == (void *)1);
    assert(runtime_firebloom_fill_span(
        span((void *)5, (void *)6, (void *)0, 0),
        0xff,
        0
    ) == (void *)5);
}

static void fatal_destination_bounds(void) {
    uint8_t bytes[8] = {0};
    runtime_firebloom_fill_span(
        span(bytes + 4, bytes, bytes + 8, &bytewise_type),
        0,
        5
    );
}

static void fatal_source_bounds(void) {
    uint8_t source[4] = {0};
    uint8_t destination[8] = {0};
    runtime_firebloom_copy_span(
        span(destination, destination, destination + 8, &bytewise_type),
        span(source + 1, source, source + 4, &bytewise_type),
        4
    );
}

static void fatal_unaligned_current(void) {
    uint8_t bytes[12] = {0};
    runtime_firebloom_fill_span(
        span(bytes + 1, bytes, bytes + 12, &typed_words),
        0,
        4
    );
}

static void fatal_mismatched_types(void) {
    uint64_t source[2] = {0};
    uint32_t destination[4] = {0};
    runtime_firebloom_copy_span(
        span(
            destination,
            destination,
            destination + 4,
            &typed_words
        ),
        span(source, source, source + 2, &typed_qwords),
        sizeof(destination)
    );
}

static void fatal_partial_element_copy(void) {
    uint32_t source[2] = {0};
    uint32_t destination[2] = {0};
    runtime_firebloom_copy_span(
        span(
            destination,
            destination,
            destination + 2,
            &typed_words
        ),
        span(source, source, source + 2, &typed_words),
        6
    );
}

static void fatal_nonzero_typed_fill(void) {
    uint32_t destination[2] = {0};
    runtime_firebloom_fill_span(
        span(
            destination,
            destination,
            destination + 2,
            &typed_words
        ),
        1,
        sizeof(destination)
    );
}

int main(void) {
    test_bytewise_copy_and_overlap();
    test_typed_copy_and_zero_fill();
    test_zero_size_preserves_original_fast_path();
    expect_fatal(fatal_destination_bounds);
    expect_fatal(fatal_source_bounds);
    expect_fatal(fatal_unaligned_current);
    expect_fatal(fatal_mismatched_types);
    expect_fatal(fatal_partial_element_copy);
    expect_fatal(fatal_nonzero_typed_fill);
}
