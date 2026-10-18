#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "avp/runtime/firebloom/runtime_firebloom.h"


#if __has_attribute(retain)
#define PLATFORM_PANIC_PUBLIC __attribute__((used, retain, minsize))
#else
#define PLATFORM_PANIC_PUBLIC __attribute__((used, minsize))
#endif

enum {
    PLATFORM_PANIC_BUFFER_SIZE = 0x800,
    PLATFORM_PANIC_PROGRESS_CODE = 0x80010001,
    PLATFORM_PANIC_RESET_REASON = 1,
};

typedef struct {
    void (*interrupt_mask_push)(void *context);
    void (*cache_clean_all)(void *context);
    int (*console_write)(const char *message, void *context);
    uint64_t (*counter_read)(void *context);
    uint32_t (*progress_read)(void *context);
    void (*progress_publish)(
        uint32_t code,
        uint32_t detail,
        uint32_t auxiliary0,
        uint32_t auxiliary1,
        void *context
    );
    void (*panic_event_raise)(void *context);
    void (*reset)(uint32_t reason, void *context);
    bool (*current_task_is_bootstrap)(void *context);
    char *panic_buffer;
    size_t panic_buffer_capacity;
    void *context;
} PlatformPanicBinding;

typedef struct {
    RuntimeFirebloomSpan source;
    RuntimeFirebloomSpan message;
    uint64_t caller;
    uint32_t progress_code;
    char *buffer;
    size_t buffer_capacity;
} PlatformPanicRecord;

void platform_panic_bind(PlatformPanicBinding binding);
void platform_panic_reset_binding(void);
const PlatformPanicRecord *platform_panic_last_record(void);

PLATFORM_PANIC_PUBLIC _Noreturn void
platform_publish_double_panic(const char *source);
PLATFORM_PANIC_PUBLIC _Noreturn void platform_panic_reset(void);
PLATFORM_PANIC_PUBLIC _Noreturn void platform_publish_iboot_panic(
    const char *source,
    const RuntimeFirebloomSpan *message
);
PLATFORM_PANIC_PUBLIC int platform_panic_outlined_write_separator(
    const char *separator,
    const RuntimeFirebloomType *type_context
);
