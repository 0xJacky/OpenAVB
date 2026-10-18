#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "avp/runtime/firebloom/runtime_firebloom.h"
#include "avp/runtime/support/runtime_log.h"
#include "avp/platform/configuration/sysconfig3_compat.h"


#if __has_attribute(retain)
#define PLATFORM_SYSCFG_PUBLIC __attribute__((used, retain))
#else
#define PLATFORM_SYSCFG_PUBLIC __attribute__((used))
#endif

typedef struct PlatformSyscfgSource PlatformSyscfgSource;

typedef int (*PlatformSyscfgCopyEntry)(
    PlatformSyscfgSource *source,
    uint32_t tag,
    void *destination,
    size_t capacity
);
typedef bool (*PlatformSyscfgGetEntryView)(
    PlatformSyscfgSource *source,
    uint32_t tag,
    RuntimeFirebloomSpan *value,
    uint32_t *length,
    bool *deleted
);
typedef void (*PlatformSyscfgDestroy)(PlatformSyscfgSource *source);

typedef struct {
    PlatformSyscfgDestroy destroy;
    PlatformSyscfgCopyEntry copy_entry;
    PlatformSyscfgGetEntryView get_entry_view;
    uint8_t reserved[0x20];
} PlatformSyscfgSourceMethods;

_Static_assert(sizeof(PlatformSyscfgSourceMethods) == 0x38,
    "platform SysCfg method table size");

struct PlatformSyscfgSource {
    PlatformSyscfgSource *next;
    PlatformSyscfgSource *previous;
    uint8_t reserved_10[0x30];
    RuntimeFirebloomSpan methods;
    uint32_t magic;
    uint32_t access_class;
    uint8_t metadata_68[0x28];
    RuntimeFirebloomSpan decoded_config;
    uint64_t encoded_length;
    uint32_t element_count;
    uint8_t source_flag;
    uint8_t reserved_bd[3];
};

_Static_assert(sizeof(PlatformSyscfgSource) == 0xc0,
    "platform SysCfg source size");
_Static_assert(offsetof(PlatformSyscfgSource, methods) == 0x40,
    "platform SysCfg methods offset");
_Static_assert(offsetof(PlatformSyscfgSource, decoded_config) == 0x90,
    "platform SysCfg decoded config offset");
_Static_assert(offsetof(PlatformSyscfgSource, encoded_length) == 0xb0,
    "platform SysCfg encoded length offset");

typedef uint32_t (*PlatformSyscfgFetch)(
    void *context,
    const char name[5],
    uint32_t selector,
    const uint8_t **encoded,
    size_t *encoded_length
);

typedef struct {
    uint32_t tag;
    uint8_t opaque_04[8];
    uint32_t selector;
    PlatformSyscfgFetch fetch;
    void *context;
    uint8_t opaque_20[0x20];
    uint8_t source_flag;
    uint8_t reserved_41[7];
} PlatformSyscfgRegistrationRecord;

_Static_assert(offsetof(PlatformSyscfgRegistrationRecord, selector) == 0x0c,
    "platform SysCfg selector offset");
_Static_assert(offsetof(PlatformSyscfgRegistrationRecord, source_flag) == 0x40,
    "platform SysCfg source flag offset");
_Static_assert(sizeof(PlatformSyscfgRegistrationRecord) == 0x48,
    "platform SysCfg registration record size");

typedef struct {
    const RuntimeFirebloomType **type_slot;
    void **lower_bound_slot;
    void **current_slot;
    void **upper_bound_slot;
} PlatformSyscfgSourceListSlots;

typedef struct {
    void *current;
    const void *lower_bound;
    const void *upper_bound;
} PlatformSyscfgTriplet;

typedef RuntimeLogFunction PlatformSyscfgLogFunction;
typedef RuntimeSecondaryLogFunction PlatformSyscfgSecondaryLogFunction;
typedef void (*PlatformSyscfgFatalFunction)(const char *message);
typedef void (*PlatformSyscfgLockFunction)(void *context);

typedef struct {
    PlatformSyscfgLockFunction acquire;
    PlatformSyscfgLockFunction release;
    PlatformSyscfgFatalFunction fatal;
    void *context;
} PlatformSyscfgTransport;

extern const RuntimeFirebloomType platform_syscfg_byte_type;
extern const RuntimeFirebloomType platform_syscfg_source_type;

void platform_syscfg_bind_transport(PlatformSyscfgTransport transport);
void platform_syscfg_registry_reset(void);
size_t platform_syscfg_registry_count(void);

PLATFORM_SYSCFG_PUBLIC bool
platform_syscfg_status_skips_registration(uint32_t status);
PLATFORM_SYSCFG_PUBLIC int platform_syscfg_get_entry(
    uint32_t tag,
    void *destination,
    size_t destination_capacity
);
PLATFORM_SYSCFG_PUBLIC PlatformSyscfgSource *platform_syscfg_source_create(
    const PlatformSyscfgSourceMethods *methods,
    const PlatformSyscfgRegistrationRecord *record,
    SysConfig3 *configuration,
    size_t encoded_length
);
PLATFORM_SYSCFG_PUBLIC uint32_t
platform_syscfg_source_record_failure_clear_size(
    const PlatformSyscfgSource *source
);
PLATFORM_SYSCFG_PUBLIC _Noreturn void
platform_syscfg_source_record_bounds_panic(void);
PLATFORM_SYSCFG_PUBLIC RuntimeFirebloomSpan
platform_syscfg_validate_source_record_tuple_thunk(RuntimeFirebloomSpan span);
PLATFORM_SYSCFG_PUBLIC PlatformSyscfgSourceListSlots
platform_syscfg_outlined_load_source_list_slots(void);
PLATFORM_SYSCFG_PUBLIC RuntimeFirebloomSpan
platform_syscfg_lock_bounded_view(void);
PLATFORM_SYSCFG_PUBLIC const RuntimeFirebloomType *
platform_syscfg_outlined_load_lock_type_x3(void);
PLATFORM_SYSCFG_PUBLIC PlatformSyscfgTriplet
platform_syscfg_outlined_restore_source_record_bounds_x20_x22(
    void *current,
    const void *lower_bound,
    const void *upper_bound
);
PLATFORM_SYSCFG_PUBLIC RuntimeFirebloomSpan
platform_syscfg_validate_source_record_tuple(RuntimeFirebloomSpan span);
PLATFORM_SYSCFG_PUBLIC void
platform_syscfg_outlined_save_method_span_bounds_x24_x22(
    RuntimeFirebloomSpan methods
);
PLATFORM_SYSCFG_PUBLIC PlatformSyscfgTriplet
platform_syscfg_outlined_load_method_span_and_compare_lower_bound(
    const PlatformSyscfgSource *source
);
PLATFORM_SYSCFG_PUBLIC const void *
platform_syscfg_outlined_load_source_list_sentinel_x10(void);
PLATFORM_SYSCFG_PUBLIC bool
platform_syscfg_validate_source_record_and_compare_lower_bound(
    RuntimeFirebloomSpan span
);
PLATFORM_SYSCFG_PUBLIC PlatformSyscfgTriplet
platform_syscfg_outlined_restore_source_list_bounds_x19_x28(
    void *current,
    const void *lower_bound,
    const void *upper_bound
);
PLATFORM_SYSCFG_PUBLIC void platform_syscfg_lock_release(void);
PLATFORM_SYSCFG_PUBLIC PlatformSyscfgSource *
platform_syscfg_decode_and_register_source(
    const PlatformSyscfgRegistrationRecord *record,
    const uint8_t *encoded,
    size_t encoded_length
);
PLATFORM_SYSCFG_PUBLIC void platform_syscfg_initialize_callbacks_once(
    PlatformSyscfgLogFunction primary_callback,
    PlatformSyscfgSecondaryLogFunction secondary_callback
);
PLATFORM_SYSCFG_PUBLIC RuntimeFirebloomSpan
platform_syscfg_validate_encoded_byte_span(RuntimeFirebloomSpan span);
PLATFORM_SYSCFG_PUBLIC void
platform_syscfg_source_destroy(PlatformSyscfgSource *source);
PLATFORM_SYSCFG_PUBLIC int platform_syscfg_source_copy_entry(
    PlatformSyscfgSource *source,
    uint32_t tag,
    void *destination,
    size_t destination_capacity
);
PLATFORM_SYSCFG_PUBLIC bool platform_syscfg_source_get_entry_view(
    PlatformSyscfgSource *source,
    uint32_t tag,
    RuntimeFirebloomSpan *value,
    uint32_t *length,
    bool *deleted
);
PLATFORM_SYSCFG_PUBLIC bool platform_syscfg_payload_entry_get_value(
    const SysConfig3PayloadEntry *entry,
    RuntimeFirebloomSpan *value,
    uint32_t *length,
    bool *deleted
);
PLATFORM_SYSCFG_PUBLIC uint32_t platform_syscfg_load_and_register_record(
    const PlatformSyscfgRegistrationRecord *record
);
PLATFORM_SYSCFG_PUBLIC void
platform_syscfg_log_generated_location(uint32_t source_line);
PLATFORM_SYSCFG_PUBLIC PlatformSyscfgLogFunction
platform_syscfg_noop_log_callback(void);
PLATFORM_SYSCFG_PUBLIC PlatformSyscfgSource *
platform_syscfg_outlined_create_source_from_decoded_config(
    const PlatformSyscfgRegistrationRecord *record,
    SysConfig3 *configuration,
    size_t encoded_length
);
PLATFORM_SYSCFG_PUBLIC RuntimeFirebloomSpan
platform_syscfg_outlined_save_source_span_x22_x19(RuntimeFirebloomSpan span);
PLATFORM_SYSCFG_PUBLIC bool platform_syscfg_outlined_extract_payload_entry(
    const SysConfig3PayloadEntry *entry,
    RuntimeFirebloomSpan *value,
    uint32_t *length,
    bool *deleted
);
PLATFORM_SYSCFG_PUBLIC RuntimeFirebloomSpan
platform_syscfg_outlined_restore_source_span_x22_x19(RuntimeFirebloomSpan span);
PLATFORM_SYSCFG_PUBLIC void platform_syscfg_noop_log_sink(
    uint32_t level,
    const char *format,
    ...
);
PLATFORM_SYSCFG_PUBLIC _Noreturn void
platform_syscfg_fatal_format_callback(const char *format, ...);
