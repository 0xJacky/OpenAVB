#include "avp/runtime/support/runtime_string_compat.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static RuntimeFirebloomSpan span(void *bytes, size_t capacity) {
    return (RuntimeFirebloomSpan){
        .current = bytes,
        .lower_bound = bytes,
        .upper_bound = (uint8_t *)bytes + capacity,
        .type = NULL,
    };
}

static void test_compare_and_search(void) {
    const unsigned char left[] = {0, 1, 2, 255};
    const unsigned char right[] = {0, 1, 3, 0};
    assert(runtime_memcmp(left, right, 2) == 0);
    assert(runtime_memcmp(left, right, 4) < 0);

    char text[] = "bounded";
    RuntimeFirebloomSpan found = runtime_bounded_strchr(
        span(text, sizeof(text)),
        'd'
    );
    assert(found.current == text + 4);
    assert(found.lower_bound == text);
    assert(runtime_bounded_strchr(span(text, sizeof(text)), 'x').current == 0);

    char equal[] = "bounded";
    char later[] = "boundee";
    assert(
        runtime_bounded_strcmp(
            span(text, sizeof(text)),
            span(equal, sizeof(equal))
        ) == 0
    );
    assert(
        runtime_bounded_strcmp(
            span(text, sizeof(text)),
            span(later, sizeof(later))
        ) < 0
    );
    assert(
        runtime_bounded_strncmp(
            span(text, sizeof(text)),
            span(later, sizeof(later)),
            6
        ) == 0
    );
    assert(
        runtime_bounded_strncmp(
            (RuntimeFirebloomSpan){0},
            (RuntimeFirebloomSpan){0},
            0
        ) == 0
    );
}

static void test_copy_and_append(void) {
    char source[] = "abcdef";
    char output[5] = {0};
    assert(
        runtime_bounded_strlcpy(
            span(output, sizeof(output)),
            span(source, sizeof(source))
        ) == 6
    );
    assert(strcmp(output, "abcd") == 0);

    char appended[6] = "ab";
    char suffix[] = "cdef";
    assert(
        runtime_bounded_strlcat(
            span(appended, sizeof(appended)),
            span(suffix, sizeof(suffix))
        ) == 6
    );
    assert(strcmp(appended, "abcde") == 0);

    memset(output, 0, sizeof(output));
    assert(runtime_strlcpy(output, source, sizeof(output)) == 6);
    assert(strcmp(output, "abcd") == 0);
    assert(runtime_strlen(source) == 6);
    assert(runtime_strncmp("abcdef", "abcxyz", 3) == 0);
    assert(runtime_strncmp("abcdef", "abcxyz", 4) < 0);
}

static void test_move_and_fill(void) {
    char overlap[] = "abcdef";
    assert(
        runtime_memmove_source_first(overlap, overlap + 2, 4) == overlap + 2
    );
    assert(memcmp(overlap, "ababcd", 6) == 0);

    char bytes[8] = "1234567";
    assert(runtime_bounded_memmove(bytes, sizeof(bytes), bytes + 2, 5) == bytes);
    assert(memcmp(bytes, "3456767", 7) == 0);
    assert(runtime_bounded_memset(bytes, sizeof(bytes), 0xa5, 4) == bytes);
    for (size_t index = 0; index < 4; ++index) {
        assert((unsigned char)bytes[index] == 0xa5);
    }
}

int main(void) {
    test_compare_and_search();
    test_copy_and_append();
    test_move_and_fill();
    puts("runtime_string_compat_tests: ok");
}
