#include "avp/platform/platform_syscfg_owner.h"

#include <stdarg.h>

#if __STDC_HOSTED__
#include <stdio.h>
#endif

#include "avp/runtime/support/runtime_format_compat.h"
#include "avp/runtime/memory/runtime_memory.h"


const RuntimeFirebloomType platform_syscfg_byte_type = {
    .identity = 0x00144f18,
    .element_size = 1,
    .bytewise_copyable = true,
};

const RuntimeFirebloomType platform_syscfg_source_type = {
    .identity = 0x00147c90,
    .element_size = sizeof(PlatformSyscfgSource),
    .bytewise_copyable = false,
};

static const RuntimeFirebloomType platform_syscfg_lock_type = {
    .identity = 0x00147d20,
    .element_size = 0x28,
    .bytewise_copyable = false,
};

#if defined(__ELF__)
#define PLATFORM_SYSCFG_STATE \
    __attribute__((section(".runtime_prng.state"), used))
#else
#define PLATFORM_SYSCFG_STATE
#endif

static PlatformSyscfgSource *source_head PLATFORM_SYSCFG_STATE;
static PlatformSyscfgSource *source_tail PLATFORM_SYSCFG_STATE;
static PlatformSyscfgTransport bound_transport PLATFORM_SYSCFG_STATE;
static bool callbacks_initialized PLATFORM_SYSCFG_STATE;
static uint8_t lock_storage[0x28] PLATFORM_SYSCFG_STATE;
static RuntimeFirebloomSpan saved_method_span PLATFORM_SYSCFG_STATE;

static _Noreturn void platform_syscfg_default_fatal(const char *message) {
    (void)message;
    runtime_firebloom_panic();
}

static void platform_syscfg_noop_lock(void *context) {
    (void)context;
}

static PlatformSyscfgTransport platform_syscfg_transport(void) {
    PlatformSyscfgTransport transport = bound_transport;
    if (transport.acquire == 0) {
        transport.acquire = platform_syscfg_noop_lock;
    }
    if (transport.release == 0) {
        transport.release = platform_syscfg_noop_lock;
    }
    if (transport.fatal == 0) {
        transport.fatal = platform_syscfg_default_fatal;
    }
    return transport;
}

static _Noreturn void platform_syscfg_fail(const char *message) {
    PlatformSyscfgTransport transport = platform_syscfg_transport();
    transport.fatal(message);
    runtime_firebloom_panic();
}

static bool platform_syscfg_span_contains(
    RuntimeFirebloomSpan span,
    size_t size
) {
    const uintptr_t current = (uintptr_t)span.current;
    const uintptr_t lower = (uintptr_t)span.lower_bound;
    const uintptr_t upper = (uintptr_t)span.upper_bound;
    return span.type != 0 && current >= lower && current <= upper &&
        size <= upper - current;
}

static void platform_syscfg_copy_bytes(
    uint8_t *destination,
    const uint8_t *source,
    size_t size
) {
    for (size_t index = 0; index < size; ++index) {
        destination[index] = source[index];
    }
}

static size_t platform_syscfg_string_length(const char *string) {
    size_t length = 0;
    while (string[length] != '\0') {
        ++length;
    }
    return length;
}

static uint32_t platform_syscfg_access_class(uint32_t selector) {
    if (selector == 1) {
        return 4;
    }
    if (selector == 4) {
        return 1;
    }
    return 3;
}

static const PlatformSyscfgSourceMethods platform_syscfg_default_methods = {
    .destroy = platform_syscfg_source_destroy,
    .copy_entry = platform_syscfg_source_copy_entry,
    .get_entry_view = platform_syscfg_source_get_entry_view,
};

void platform_syscfg_bind_transport(PlatformSyscfgTransport transport) {
    if (transport.acquire == 0 || transport.release == 0 ||
        transport.fatal == 0) {
        platform_syscfg_fail("invalid platform SysCfg transport");
    }
    bound_transport = transport;
}

void platform_syscfg_registry_reset(void) {
    PlatformSyscfgSource *source = source_head;
    while (source != 0) {
        PlatformSyscfgSource *next = source->next;
        platform_syscfg_source_destroy(source);
        sysconfig3_release_owned(source);
        source = next;
    }
    source_head = 0;
    source_tail = 0;
    bound_transport = (PlatformSyscfgTransport){0};
    (void)runtime_log_set_function(0);
    runtime_log_set_secondary_function(0);
    callbacks_initialized = false;
    saved_method_span = (RuntimeFirebloomSpan){0};
}

size_t platform_syscfg_registry_count(void) {
    size_t count = 0;
    for (PlatformSyscfgSource *source = source_head;
         source != 0; source = source->next) {
        ++count;
    }
    return count;
}

bool platform_syscfg_status_skips_registration(uint32_t status) {
    return status > 20 || ((UINT32_C(0x3fffe) >> status) & 1u) != 0;
}

int platform_syscfg_get_entry(
    uint32_t tag,
    void *destination,
    size_t destination_capacity
) {
    if (destination == 0 && destination_capacity != 0) {
        platform_syscfg_fail("invalid platform SysCfg destination");
    }
    PlatformSyscfgTransport transport = platform_syscfg_transport();
    transport.acquire(transport.context);
    int result = -1;
    for (PlatformSyscfgSource *source = source_head;
         source != 0; source = source->next) {
        const PlatformSyscfgSourceMethods *methods = source->methods.current;
        if (methods == 0 || methods->copy_entry == 0) {
            platform_syscfg_fail("invalid platform SysCfg source methods");
        }
        result = methods->copy_entry(
            source, tag, destination, destination_capacity);
        if (result >= 0) {
            break;
        }
        uint32_t clear_size =
            platform_syscfg_source_record_failure_clear_size(source);
        if (clear_size > destination_capacity) {
            clear_size = (uint32_t)destination_capacity;
        }
        for (uint32_t index = 0; index < clear_size; ++index) {
            ((uint8_t *)destination)[index] = 0;
        }
    }
    transport.release(transport.context);
    return result;
}

PlatformSyscfgSource *platform_syscfg_source_create(
    const PlatformSyscfgSourceMethods *methods,
    const PlatformSyscfgRegistrationRecord *record,
    SysConfig3 *configuration,
    size_t encoded_length
) {
    if (methods == 0 || record == 0 || configuration == 0) {
        platform_syscfg_fail("invalid platform SysCfg source arguments");
    }
    PlatformSyscfgSource *source =
        sysconfig3_allocate_owned(sizeof(*source));
    if (source == 0) {
        return 0;
    }
    for (size_t index = 0; index < sizeof(*source); ++index) {
        ((uint8_t *)source)[index] = 0;
    }
    source->methods = (RuntimeFirebloomSpan){
        .current = (void *)methods,
        .lower_bound = (void *)methods,
        .upper_bound = (void *)(methods + 1),
        .type = &platform_syscfg_byte_type,
    };
    source->magic = record->tag;
    source->access_class = platform_syscfg_access_class(record->selector);
    source->decoded_config = (RuntimeFirebloomSpan){
        .current = configuration,
        .lower_bound = configuration,
        .upper_bound = configuration + 1,
        .type = &platform_syscfg_byte_type,
    };
    source->encoded_length = encoded_length;
    source->element_count = record->tag == 8 ?
        (uint32_t)(encoded_length / 0x50) : 0;
    source->source_flag = record->source_flag;
    return source;
}

uint32_t platform_syscfg_source_record_failure_clear_size(
    const PlatformSyscfgSource *source
) {
    if (source == 0) {
        platform_syscfg_source_record_bounds_panic();
    }
    if (source->access_class == 3 || source->access_class == 4) {
        return 0;
    }
    return source->access_class == 1 ? 9 : 8;
}

_Noreturn void platform_syscfg_source_record_bounds_panic(void) {
    platform_syscfg_fail("platform SysCfg source record bounds");
}

RuntimeFirebloomSpan platform_syscfg_validate_source_record_tuple_thunk(
    RuntimeFirebloomSpan span
) {
    return platform_syscfg_validate_source_record_tuple(span);
}

PlatformSyscfgSourceListSlots
platform_syscfg_outlined_load_source_list_slots(void) {
    static const RuntimeFirebloomType *type = &platform_syscfg_source_type;
    return (PlatformSyscfgSourceListSlots){
        .type_slot = &type,
        .lower_bound_slot = (void **)&source_head,
        .current_slot = (void **)&source_head,
        .upper_bound_slot = (void **)&source_tail,
    };
}

RuntimeFirebloomSpan platform_syscfg_lock_bounded_view(void) {
    return (RuntimeFirebloomSpan){
        .current = lock_storage,
        .lower_bound = lock_storage,
        .upper_bound = lock_storage + sizeof(lock_storage),
        .type = &platform_syscfg_lock_type,
    };
}

const RuntimeFirebloomType *platform_syscfg_outlined_load_lock_type_x3(void) {
    return &platform_syscfg_lock_type;
}

PlatformSyscfgTriplet
platform_syscfg_outlined_restore_source_record_bounds_x20_x22(
    void *current,
    const void *lower_bound,
    const void *upper_bound
) {
    return (PlatformSyscfgTriplet){current, lower_bound, upper_bound};
}

RuntimeFirebloomSpan platform_syscfg_validate_source_record_tuple(
    RuntimeFirebloomSpan span
) {
    if (span.type != &platform_syscfg_source_type ||
        !platform_syscfg_span_contains(span, sizeof(PlatformSyscfgSource))) {
        platform_syscfg_source_record_bounds_panic();
    }
    return span;
}

void platform_syscfg_outlined_save_method_span_bounds_x24_x22(
    RuntimeFirebloomSpan methods
) {
    saved_method_span = methods;
}

PlatformSyscfgTriplet
platform_syscfg_outlined_load_method_span_and_compare_lower_bound(
    const PlatformSyscfgSource *source
) {
    if (source == 0) {
        platform_syscfg_source_record_bounds_panic();
    }
    return (PlatformSyscfgTriplet){
        source->methods.current,
        source->methods.lower_bound,
        source->methods.upper_bound,
    };
}

const void *platform_syscfg_outlined_load_source_list_sentinel_x10(void) {
    return 0;
}

bool platform_syscfg_validate_source_record_and_compare_lower_bound(
    RuntimeFirebloomSpan span
) {
    span = platform_syscfg_validate_source_record_tuple(span);
    return span.current == span.lower_bound;
}

PlatformSyscfgTriplet
platform_syscfg_outlined_restore_source_list_bounds_x19_x28(
    void *current,
    const void *lower_bound,
    const void *upper_bound
) {
    return (PlatformSyscfgTriplet){current, lower_bound, upper_bound};
}

void platform_syscfg_lock_release(void) {
    PlatformSyscfgTransport transport = platform_syscfg_transport();
    transport.release(transport.context);
}

PlatformSyscfgSource *platform_syscfg_decode_and_register_source(
    const PlatformSyscfgRegistrationRecord *record,
    const uint8_t *encoded,
    size_t encoded_length
) {
    if (record == 0 || encoded == 0 || encoded_length == 0 ||
        encoded_length > UINT32_MAX) {
        platform_syscfg_fail("invalid encoded platform SysCfg source");
    }
    platform_syscfg_initialize_callbacks_once(
        platform_syscfg_noop_log_callback(),
        platform_syscfg_fatal_format_callback);
    SysConfig3 *configuration =
        DERDecodeSysConfig3(encoded, (uint32_t)encoded_length);
    if (configuration == 0) {
        return 0;
    }
    PlatformSyscfgSource *source =
        platform_syscfg_outlined_create_source_from_decoded_config(
            record, configuration, encoded_length);
    if (source == 0) {
        freeSysConfig3(configuration);
        return 0;
    }
    source->previous = source_tail;
    if (source_tail != 0) {
        source_tail->next = source;
    } else {
        source_head = source;
    }
    source_tail = source;
    return source;
}

void platform_syscfg_initialize_callbacks_once(
    PlatformSyscfgLogFunction primary_callback,
    PlatformSyscfgSecondaryLogFunction secondary_callback
) {
    if (!callbacks_initialized) {
        (void)runtime_log_set_function(primary_callback);
        runtime_log_set_secondary_function(secondary_callback);
        callbacks_initialized = true;
    }
}

RuntimeFirebloomSpan platform_syscfg_validate_encoded_byte_span(
    RuntimeFirebloomSpan span
) {
    if (span.type == 0 || !span.type->bytewise_copyable ||
        !platform_syscfg_span_contains(span, 0)) {
        platform_syscfg_fail("invalid encoded platform SysCfg span");
    }
    return span;
}

void platform_syscfg_source_destroy(PlatformSyscfgSource *source) {
    if (source == 0) {
        platform_syscfg_source_record_bounds_panic();
    }
    if (source->decoded_config.current != 0) {
        freeSysConfig3(source->decoded_config.current);
    }
    source->decoded_config = (RuntimeFirebloomSpan){0};
    source->encoded_length = 0;
}

int platform_syscfg_source_copy_entry(
    PlatformSyscfgSource *source,
    uint32_t tag,
    void *destination,
    size_t destination_capacity
) {
    if (source == 0 || (destination == 0 && destination_capacity != 0)) {
        platform_syscfg_source_record_bounds_panic();
    }
    SysConfig3 *configuration = source->decoded_config.current;
    SysConfig3PayloadEntry *entry =
        sysConfig3GetPayloadEntryForKey(configuration, tag);
    RuntimeFirebloomSpan value = {0};
    uint32_t length = 0;
    bool deleted = false;
    if (!platform_syscfg_payload_entry_get_value(
            entry, &value, &length, &deleted) || deleted) {
        return -1;
    }
    size_t copied = length;
    if (copied > destination_capacity) {
        copied = destination_capacity;
    }
    platform_syscfg_copy_bytes(destination, value.current, copied);
    return (int)copied;
}

bool platform_syscfg_source_get_entry_view(
    PlatformSyscfgSource *source,
    uint32_t tag,
    RuntimeFirebloomSpan *value,
    uint32_t *length,
    bool *deleted
) {
    if (source == 0) {
        platform_syscfg_source_record_bounds_panic();
    }
    SysConfig3 *configuration = source->decoded_config.current;
    SysConfig3PayloadEntry *entry =
        sysConfig3GetPayloadEntryForKey(configuration, tag);
    return platform_syscfg_payload_entry_get_value(
        entry, value, length, deleted);
}

bool platform_syscfg_payload_entry_get_value(
    const SysConfig3PayloadEntry *entry,
    RuntimeFirebloomSpan *value,
    uint32_t *length,
    bool *deleted
) {
    if (deleted != 0) {
        *deleted = false;
    }
    if (entry == 0) {
        return false;
    }
    if (sysConfig3PayloadEntryIsDeleted(entry)) {
        if (deleted != 0) {
            *deleted = true;
        }
        return false;
    }
    const char *string = sysConfig3PayloadEntryGetString(entry);
    if (string != 0) {
        const size_t string_length = platform_syscfg_string_length(string);
        if (string_length > UINT32_MAX) {
            platform_syscfg_fail("platform SysCfg string is too long");
        }
        if (value != 0) {
            *value = (RuntimeFirebloomSpan){
                .current = (void *)string,
                .lower_bound = (void *)string,
                .upper_bound = (void *)(string + string_length + 1),
                .type = &platform_syscfg_byte_type,
            };
        }
        if (length != 0) {
            *length = (uint32_t)string_length;
        }
        return true;
    }
    SysConfigData data = {0};
    if (!sysConfig3PayloadEntryGetData(entry, &data)) {
        return false;
    }
    if (value != 0) {
        *value = (RuntimeFirebloomSpan){
            .current = data.data,
            .lower_bound = data.data,
            .upper_bound = data.data + data.length,
            .type = &platform_syscfg_byte_type,
        };
    }
    if (length != 0) {
        *length = data.length;
    }
    return true;
}

uint32_t platform_syscfg_load_and_register_record(
    const PlatformSyscfgRegistrationRecord *record
) {
    if (record == 0 || record->fetch == 0) {
        platform_syscfg_fail("invalid platform SysCfg registration record");
    }
    char name[5] = {
        (char)((record->tag >> 24) & 0x7f),
        (char)((record->tag >> 16) & 0x7f),
        (char)((record->tag >> 8) & 0x7f),
        (char)(record->tag & 0x7f),
        '\0',
    };
    const uint8_t *encoded = 0;
    size_t encoded_length = 0;
    uint32_t status = record->fetch(
        record->context,
        name,
        record->selector,
        &encoded,
        &encoded_length
    );
    if (!platform_syscfg_status_skips_registration(status)) {
        (void)platform_syscfg_decode_and_register_source(
            record, encoded, encoded_length);
        platform_syscfg_log_generated_location(0x70);
    }
    return status;
}

void platform_syscfg_log_generated_location(uint32_t source_line) {
    PlatformSyscfgLogFunction log = runtime_log_get_function();
    if (log == 0) {
        log = platform_syscfg_noop_log_sink;
    }
    log(0, "platform/syscfg.generated:%u", source_line);
}

PlatformSyscfgLogFunction platform_syscfg_noop_log_callback(void) {
    return platform_syscfg_noop_log_sink;
}

PlatformSyscfgSource *
platform_syscfg_outlined_create_source_from_decoded_config(
    const PlatformSyscfgRegistrationRecord *record,
    SysConfig3 *configuration,
    size_t encoded_length
) {
    return platform_syscfg_source_create(
        &platform_syscfg_default_methods,
        record,
        configuration,
        encoded_length);
}

RuntimeFirebloomSpan platform_syscfg_outlined_save_source_span_x22_x19(
    RuntimeFirebloomSpan span
) {
    return span;
}

bool platform_syscfg_outlined_extract_payload_entry(
    const SysConfig3PayloadEntry *entry,
    RuntimeFirebloomSpan *value,
    uint32_t *length,
    bool *deleted
) {
    return platform_syscfg_payload_entry_get_value(
        entry, value, length, deleted);
}

RuntimeFirebloomSpan platform_syscfg_outlined_restore_source_span_x22_x19(
    RuntimeFirebloomSpan span
) {
    return span;
}

void platform_syscfg_noop_log_sink(
    uint32_t level,
    const char *format,
    ...
) {
    (void)level;
    (void)format;
}

_Noreturn void platform_syscfg_fatal_format_callback(
    const char *format,
    ...
) {
    char message[0x400];
    va_list arguments;
    va_start(arguments, format);
#if __STDC_HOSTED__
    (void)vsnprintf(message, sizeof(message), format, arguments);
#else
    (void)runtime_vsnprintf(message, sizeof(message), format, arguments);
#endif
    va_end(arguments);
    platform_syscfg_fail(message);
}
