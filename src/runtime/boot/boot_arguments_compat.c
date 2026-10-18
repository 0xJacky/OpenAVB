#include "avp/runtime/boot/boot_arguments_compat.h"

#include "avp/runtime/memory/runtime_memory.h"
#include "avp/runtime/memory/runtime_memory_recovered.h"

static BootArgumentsPlatform boot_arguments_platform;
static bool boot_arguments_platform_bound;

static _Noreturn void boot_arguments_contract_failure(void) {
    __builtin_trap();
}

static size_t boot_arguments_span_size(RuntimeFirebloomSpan span) {
    const uintptr_t current = (uintptr_t)span.current;
    const uintptr_t lower = (uintptr_t)span.lower_bound;
    const uintptr_t upper = (uintptr_t)span.upper_bound;
    if (span.type == 0 || span.type->element_size != 1 || current < lower ||
        current > upper) {
        boot_arguments_contract_failure();
    }
    return upper - current;
}

static bool boot_arguments_is_separator(uint8_t character) {
    /* AVPBooter extends XNU's boot-argument grammar with line separators. */
    return character == '\t' || character == '\n' || character == '\r' ||
           character == ' ';
}

static RuntimeFirebloomSpan
boot_arguments_empty_span(RuntimeFirebloomSpan input) {
    input.current = input.upper_bound;
    input.lower_bound = input.upper_bound;
    return input;
}

void boot_arguments_bind_platform(const BootArgumentsPlatform *platform) {
    if (platform == 0 || platform->load_property == 0 ||
        platform->allocate == 0 || platform->release == 0) {
        boot_arguments_contract_failure();
    }
    boot_arguments_platform = *platform;
    boot_arguments_platform_bound = true;
}

RuntimeFirebloomSpan BOOT_ARGUMENTS_PUBLIC boot_arguments_parse_next_token(
    RuntimeFirebloomSpan input,
    BootArgumentToken *token
) {
    if (token == 0) {
        boot_arguments_contract_failure();
    }
    const size_t available = boot_arguments_span_size(input);
    const uint8_t *const begin = input.current;
    size_t cursor = 0;
    while (cursor < available && boot_arguments_is_separator(begin[cursor])) {
        ++cursor;
    }

    const size_t token_begin = cursor;
    size_t key_length = 0;
    bool key_complete = false;
    while (cursor < available && begin[cursor] != 0 &&
           !boot_arguments_is_separator(begin[cursor])) {
        if (!key_complete && begin[cursor] == '=') {
            key_complete = true;
        } else if (!key_complete) {
            ++key_length;
        }
        ++cursor;
    }

    token->span = input;
    token->span.current = (void *)(begin + token_begin);
    token->key_length = key_length;
    token->token_length = cursor - token_begin;
    input.current = (void *)(begin + cursor);
    return input;
}

bool BOOT_ARGUMENTS_PUBLIC boot_arguments_lookup_value(
    RuntimeFirebloomSpan input,
    RuntimeFirebloomSpan key,
    BootArgumentLookup *result
) {
    if (result == 0) {
        boot_arguments_contract_failure();
    }
    const size_t key_length = boot_arguments_span_size(key);
    const uint8_t *const key_bytes = key.current;
    result->token = (BootArgumentToken){0};
    result->value = boot_arguments_empty_span(input);
    result->has_value = false;

    while (boot_arguments_span_size(input) != 0) {
        const uint8_t *current = input.current;
        if (*current == 0) {
            break;
        }
        BootArgumentToken token;
        input = boot_arguments_parse_next_token(input, &token);
        if (token.token_length == 0) {
            break;
        }
        if (token.key_length != key_length) {
            continue;
        }

        bool equal = true;
        const uint8_t *token_bytes = token.span.current;
        for (size_t index = 0; index < key_length; ++index) {
            equal &= token_bytes[index] == key_bytes[index];
        }
        if (!equal) {
            continue;
        }

        result->token = token;
        if (token.token_length > token.key_length &&
            token_bytes[token.key_length] == '=') {
            result->has_value = true;
            result->value = token.span;
            result->value.current =
                (void *)(token_bytes + token.key_length + 1);
            result->value.lower_bound = result->value.current;
            result->value.upper_bound =
                (void *)(token_bytes + token.token_length);
        }
        return true;
    }
    return false;
}

bool BOOT_ARGUMENTS_PUBLIC
boot_arguments_copy_value(
    RuntimeFirebloomSpan input,
    RuntimeFirebloomSpan key,
    RuntimeFirebloomSpan *owned_value
) {
    if (owned_value == 0 || !boot_arguments_platform_bound) {
        boot_arguments_contract_failure();
    }
    *owned_value = (RuntimeFirebloomSpan){0};
    /* A null current lane is the expected platform-property sentinel. */
    if (input.current == 0) {
        input = boot_arguments_load_platform_property();
    }
    if (boot_arguments_span_size(input) == 0) {
        return false;
    }

    BootArgumentLookup lookup;
    if (!boot_arguments_lookup_value(input, key, &lookup) ||
        !lookup.has_value) {
        return false;
    }
    const size_t value_length = boot_arguments_span_size(lookup.value);
    if (value_length == 0) {
        return false;
    }

    RuntimeFirebloomSpan allocation = boot_arguments_platform.allocate(
        boot_arguments_platform.context,
        value_length + 1,
        1
    );
    allocation = boot_arguments_validate_byte_span(allocation);
    if (boot_arguments_span_size(allocation) < value_length + 1) {
        boot_arguments_contract_failure();
    }
    runtime_memmove(allocation.current, lookup.value.current, value_length);
    ((uint8_t *)allocation.current)[value_length] = 0;
    allocation.upper_bound = (uint8_t *)allocation.current + value_length + 1;
    *owned_value = allocation;
    return true;
}

uint64_t BOOT_ARGUMENTS_PUBLIC
boot_arguments_get_uint64(
    RuntimeFirebloomSpan input,
    RuntimeFirebloomSpan key,
    uint64_t default_value
) {
    if (boot_arguments_span_size(key) == 0) {
        boot_arguments_contract_failure();
    }
    RuntimeFirebloomSpan owned;
    if (!boot_arguments_copy_value(input, key, &owned)) {
        return default_value;
    }

    RuntimeFirebloomSpan end = {0};
    const uint64_t parsed = runtime_bounded_strtoull(owned, &end, 0);
    const uint8_t *const end_byte = end.current;
    const uint8_t *const upper = owned.upper_bound;
    /* Policy values are accepted only when the parser reaches the terminator. */
    const bool complete = end_byte < upper && *end_byte == 0;
    boot_arguments_platform.release(boot_arguments_platform.context, owned);
    return complete ? parsed : default_value;
}

RuntimeFirebloomSpan BOOT_ARGUMENTS_PUBLIC
boot_arguments_validate_byte_span(RuntimeFirebloomSpan input) {
    return runtime_firebloom_validate_byte_span(input);
}

RuntimeFirebloomSpan BOOT_ARGUMENTS_PUBLIC
boot_arguments_load_platform_property(void) {
    if (!boot_arguments_platform_bound) {
        boot_arguments_contract_failure();
    }
    static const uint8_t name[] = "boot-args";
    const RuntimeFirebloomSpan name_span = {
        .current = (void *)name,
        .lower_bound = (void *)name,
        .upper_bound = (void *)(name + sizeof(name)),
        .type = &runtime_memory_byte_type,
    };
    return boot_arguments_platform.load_property(
        boot_arguments_platform.context,
        name_span
    );
}

uint64_t BOOT_ARGUMENTS_PUBLIC boot_arguments_source_token(void) {
    return UINT64_C(0x0d0268f320fd83b5);
}
