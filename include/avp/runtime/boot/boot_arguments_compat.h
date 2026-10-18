#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "avp/runtime/firebloom/runtime_firebloom.h"

typedef struct {
    RuntimeFirebloomSpan span;
    size_t key_length;
    size_t token_length;
} BootArgumentToken;

typedef struct {
    BootArgumentToken token;
    RuntimeFirebloomSpan value;
    bool has_value;
} BootArgumentLookup;

typedef RuntimeFirebloomSpan (*BootArgumentsLoadProperty)(
    void *context,
    RuntimeFirebloomSpan name
);

typedef RuntimeFirebloomSpan (*BootArgumentsAllocate)(
    void *context,
    size_t size,
    size_t alignment
);

typedef void (*BootArgumentsRelease)(
    void *context,
    RuntimeFirebloomSpan allocation
);

typedef struct {
    void *context;
    BootArgumentsLoadProperty load_property;
    BootArgumentsAllocate allocate;
    BootArgumentsRelease release;
} BootArgumentsPlatform;

#define BOOT_ARGUMENTS_PUBLIC __attribute__((used, retain))

void boot_arguments_bind_platform(const BootArgumentsPlatform *platform);
BOOT_ARGUMENTS_PUBLIC RuntimeFirebloomSpan boot_arguments_parse_next_token(
    RuntimeFirebloomSpan input,
    BootArgumentToken *token
);
BOOT_ARGUMENTS_PUBLIC bool
boot_arguments_lookup_value(
    RuntimeFirebloomSpan input,
    RuntimeFirebloomSpan key,
    BootArgumentLookup *result
);
BOOT_ARGUMENTS_PUBLIC bool
boot_arguments_copy_value(
    RuntimeFirebloomSpan input,
    RuntimeFirebloomSpan key,
    RuntimeFirebloomSpan *owned_value
);
BOOT_ARGUMENTS_PUBLIC uint64_t
boot_arguments_get_uint64(
    RuntimeFirebloomSpan input,
    RuntimeFirebloomSpan key,
    uint64_t default_value
);
BOOT_ARGUMENTS_PUBLIC RuntimeFirebloomSpan
boot_arguments_validate_byte_span(RuntimeFirebloomSpan input);
BOOT_ARGUMENTS_PUBLIC RuntimeFirebloomSpan
boot_arguments_load_platform_property(void);
BOOT_ARGUMENTS_PUBLIC uint64_t boot_arguments_source_token(void);
