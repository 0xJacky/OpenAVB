#include "avp/runtime/boot/boot_arguments_compat.h"

#include "avp/runtime/memory/runtime_memory_recovered.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint8_t allocation[512];
    const char *property;
    size_t load_count;
    size_t allocation_count;
    size_t release_count;
} TestPlatform;

_Noreturn void pvpanic_trigger(void) { __builtin_trap(); }

static RuntimeFirebloomSpan byte_span(void *storage, size_t size) {
    return (RuntimeFirebloomSpan){
        .current = storage,
        .lower_bound = storage,
        .upper_bound = (uint8_t *)storage + size,
        .type = &runtime_memory_byte_type,
    };
}

static RuntimeFirebloomSpan text_span(const char *text) {
    return byte_span((void *)text, strlen(text) + 1);
}

static RuntimeFirebloomSpan key_span(const char *key) {
    return byte_span((void *)key, strlen(key));
}

static RuntimeFirebloomSpan test_load_property(void *context,
                                               RuntimeFirebloomSpan name) {
    TestPlatform *platform = context;
    ++platform->load_count;
    assert((size_t)((uint8_t *)name.upper_bound - (uint8_t *)name.current) ==
           sizeof("boot-args"));
    assert(memcmp(name.current, "boot-args", sizeof("boot-args")) == 0);
    return text_span(platform->property);
}

static RuntimeFirebloomSpan test_allocate(void *context, size_t size,
                                          size_t alignment) {
    TestPlatform *platform = context;
    ++platform->allocation_count;
    assert(alignment == 1);
    assert(size <= sizeof(platform->allocation));
    return byte_span(platform->allocation, size);
}

static void test_release(void *context, RuntimeFirebloomSpan allocation) {
    TestPlatform *platform = context;
    ++platform->release_count;
    assert(allocation.current == platform->allocation);
}

static bool oracle_separator(char character) {
    return character == '\t' || character == '\n' || character == '\r' ||
           character == ' ';
}

static bool oracle_lookup(const char *arguments, const char *key,
                          const char **value, size_t *value_length,
                          bool *has_value) {
    const size_t key_length = strlen(key);
    const char *cursor = arguments;
    while (*cursor != 0) {
        while (oracle_separator(*cursor)) {
            ++cursor;
        }
        if (*cursor == 0) {
            break;
        }
        const char *token_begin = cursor;
        while (*cursor != 0 && !oracle_separator(*cursor)) {
            ++cursor;
        }
        const char *const token_end = cursor;
        const char *equals = token_begin;
        while (equals < token_end && *equals != '=') {
            ++equals;
        }
        if ((size_t)(equals - token_begin) == key_length &&
            memcmp(token_begin, key, key_length) == 0) {
            *has_value = equals < token_end;
            *value = *has_value ? equals + 1 : token_end;
            *value_length = *has_value ? (size_t)(token_end - equals - 1) : 0;
            return true;
        }
    }
    *value = 0;
    *value_length = 0;
    *has_value = false;
    return false;
}

static void test_xnu_vectors(void) {
    struct Vector {
        const char *arguments;
        const char *key;
        const char *value;
        bool found;
        bool has_value;
    };
    static const struct Vector vectors[] = {
        {"-x -a b=3 y=42", "-a", 0, true, false},
        {"-x -a b=3 y=42", "b", "3", true, true},
        {"-x -a b=2 ba=3 y=42", "b", "2", true, true},
        {"-x -a ba=3 b=2 y=42", "b", "2", true, true},
        {"-x -a b=2 ba=3 y=42", "ba", "3", true, true},
        {"-x -a ba=3 b=2 y=42", "ba", "3", true, true},
        {"-x -ab -aa y=42", "-a", 0, false, false},
        {"-x b=96 y=42", "bx", 0, false, false},
        {"-x ab=96 y=42", "a", 0, false, false},
        {"hello=world -foobar abc debug=0xBAADF00D", "hello", "world", true,
         true},
        {"hello=world -foobar abc debug=0xBAADF00D", "debug", "0xBAADF00D",
         true, true},
        {"hello=world -foobar abc debug=0xBAADF00D", "-foobar", 0, true, false},
        {"hello=world -foobar abc debug=0xBAADF00D", "abc", 0, true, false},
        {"line=1\ncarriage=2\rtab=3", "carriage", "2", true, true},
    };

    for (size_t index = 0; index < sizeof(vectors) / sizeof(vectors[0]);
         ++index) {
        BootArgumentLookup actual;
        const bool found =
            boot_arguments_lookup_value(text_span(vectors[index].arguments),
                                        key_span(vectors[index].key), &actual);
        assert(found == vectors[index].found);
        if (!found) {
            continue;
        }
        assert(actual.has_value == vectors[index].has_value);
        if (actual.has_value) {
            const size_t length = (size_t)((uint8_t *)actual.value.upper_bound -
                                           (uint8_t *)actual.value.current);
            assert(length == strlen(vectors[index].value));
            assert(memcmp(actual.value.current, vectors[index].value, length) ==
                   0);
        }
    }
}

static uint64_t next_word(uint64_t *state) {
    uint64_t value = *state;
    value ^= value << 13;
    value ^= value >> 7;
    value ^= value << 17;
    *state = value;
    return value;
}

static void test_randomized_lookup_oracle(void) {
    static const char *const keys[] = {
        "alpha", "beta", "gamma", "delta", "epsilon", "zeta",
    };
    static const char separators[] = {' ', '\t', '\n', '\r'};
    uint64_t state = UINT64_C(0x626f6f742d617267);
    char arguments[512];
    for (size_t iteration = 0; iteration < 512; ++iteration) {
        size_t used = 0;
        for (size_t index = 0; index < sizeof(keys) / sizeof(keys[0]);
             ++index) {
            const unsigned long long value =
                (unsigned long long)next_word(&state);
            const int written = snprintf(
                arguments + used, sizeof(arguments) - used, "%s=%llx%c",
                keys[(index + iteration) % (sizeof(keys) / sizeof(keys[0]))],
                value, separators[next_word(&state) & 3]);
            assert(written > 0 && (size_t)written < sizeof(arguments) - used);
            used += (size_t)written;
        }

        for (size_t key_index = 0; key_index < sizeof(keys) / sizeof(keys[0]);
             ++key_index) {
            const char *expected_value = 0;
            size_t expected_length = 0;
            bool expected_has_value = false;
            const bool expected_found =
                oracle_lookup(arguments, keys[key_index], &expected_value,
                              &expected_length, &expected_has_value);
            BootArgumentLookup actual;
            const bool actual_found = boot_arguments_lookup_value(
                text_span(arguments), key_span(keys[key_index]), &actual);
            assert(actual_found == expected_found);
            assert(actual.has_value == expected_has_value);
            if (actual_found && actual.has_value) {
                const size_t actual_length =
                    (size_t)((uint8_t *)actual.value.upper_bound -
                             (uint8_t *)actual.value.current);
                assert(actual_length == expected_length);
                assert(memcmp(actual.value.current, expected_value,
                              expected_length) == 0);
            }
        }
    }
}

static uint64_t libc_numeric_oracle(const char *text, uint64_t default_value) {
    errno = 0;
    char *end = 0;
    const unsigned long long result = strtoull(text, &end, 0);
    return end != text && *end == 0 ? (uint64_t)result : default_value;
}

static void test_owned_and_numeric_values(TestPlatform *platform) {
    const char arguments[] =
        "decimal=42 hex=0xBAADF00D octal=077 negative=-1 "
        "partial=12x empty= flag maximum=18446744073709551615 "
        "overflow=18446744073709551616 duplicate=7 duplicate=8";
    static const char *const numeric_keys[] = {
        "decimal", "hex",     "octal",    "negative",
        "partial", "maximum", "overflow", "duplicate",
    };
    static const char *const numeric_text[] = {
        "42",
        "0xBAADF00D",
        "077",
        "-1",
        "12x",
        "18446744073709551615",
        "18446744073709551616",
        "7",
    };
    const uint64_t fallback = UINT64_C(0xfeedfacecafebeef);
    for (size_t index = 0;
         index < sizeof(numeric_keys) / sizeof(numeric_keys[0]); ++index) {
        assert(boot_arguments_get_uint64(text_span(arguments),
                                         key_span(numeric_keys[index]),
                                         fallback) ==
               libc_numeric_oracle(numeric_text[index], fallback));
    }
    assert(boot_arguments_get_uint64(text_span(arguments), key_span("empty"),
                                     fallback) == fallback);
    assert(boot_arguments_get_uint64(text_span(arguments), key_span("flag"),
                                     fallback) == fallback);
    assert(boot_arguments_get_uint64(text_span(arguments), key_span("missing"),
                                     fallback) == fallback);

    RuntimeFirebloomSpan owned;
    assert(boot_arguments_copy_value(text_span(arguments), key_span("hex"),
                                     &owned));
    assert(strcmp(owned.current, "0xBAADF00D") == 0);
    test_release(platform, owned);
}

static void test_platform_property_fallback(TestPlatform *platform) {
    platform->property = "debug=0x1234 serial=enabled";
    const RuntimeFirebloomSpan absent_input = {
        .current = 0,
        .lower_bound = 0,
        .upper_bound = 0,
        .type = &runtime_memory_byte_type,
    };
    assert(boot_arguments_get_uint64(absent_input, key_span("debug"), 0) ==
           UINT64_C(0x1234));
    assert(platform->load_count == 1);

    RuntimeFirebloomSpan owned;
    assert(boot_arguments_copy_value(absent_input, key_span("serial"), &owned));
    assert(strcmp(owned.current, "enabled") == 0);
    test_release(platform, owned);
    assert(platform->load_count == 2);
}

static void test_validation_and_source_token(void) {
    static const RuntimeFirebloomType compatible_byte_type = {
        .identity = UINT64_C(0x74657374),
        .element_size = 1,
        .bytewise_copyable = true,
    };
    uint8_t storage[4] = {0};
    RuntimeFirebloomSpan span = byte_span(storage, sizeof(storage));
    span.type = &compatible_byte_type;
    span = boot_arguments_validate_byte_span(span);
    assert(span.type == &runtime_memory_byte_type);
    assert(boot_arguments_source_token() == UINT64_C(0x0d0268f320fd83b5));
}

int main(void) {
    TestPlatform platform = {0};
    const BootArgumentsPlatform binding = {
        .context = &platform,
        .load_property = test_load_property,
        .allocate = test_allocate,
        .release = test_release,
    };
    boot_arguments_bind_platform(&binding);
    test_xnu_vectors();
    test_randomized_lookup_oracle();
    test_owned_and_numeric_values(&platform);
    test_platform_property_fallback(&platform);
    test_validation_and_source_token();
    assert(platform.allocation_count == platform.release_count);
    puts("boot argument compatibility tests passed");
    return 0;
}
