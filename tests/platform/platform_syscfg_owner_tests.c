#include "avp/platform/platform_syscfg_owner.h"

#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>


typedef struct {
    uint8_t bytes[512];
    size_t length;
} Buffer;

static Buffer encoded_configuration;
static size_t acquire_count;
static size_t release_count;
static size_t log_count;
static uint32_t logged_line;
static size_t fetch_count;
static char fetched_name[5];
static uint32_t fetched_selector;
static uint32_t fetch_status;
static jmp_buf panic_environment;
static bool panic_expected;

_Noreturn void runtime_firebloom_panic(void) {
    assert(panic_expected);
    longjmp(panic_environment, 1);
}

static void append(Buffer *buffer, const void *bytes, size_t length) {
    assert(buffer->length + length <= sizeof(buffer->bytes));
    memcpy(buffer->bytes + buffer->length, bytes, length);
    buffer->length += length;
}

static void append_tlv(Buffer *buffer, uint8_t tag, const Buffer *content) {
    assert(content->length < 128);
    const uint8_t header[] = {tag, (uint8_t)content->length};
    append(buffer, header, sizeof(header));
    append(buffer, content->bytes, content->length);
}

static void append_bytes(
    Buffer *buffer,
    uint8_t tag,
    const void *bytes,
    size_t length
) {
    Buffer content = {0};
    append(&content, bytes, length);
    append_tlv(buffer, tag, &content);
}

static void append_word(Buffer *buffer, uint32_t word) {
    const uint8_t bytes[] = {
        (uint8_t)word,
        (uint8_t)(word >> 8),
        (uint8_t)(word >> 16),
        (uint8_t)(word >> 24),
    };
    append_bytes(buffer, 0x04, bytes, sizeof(bytes));
}

static Buffer payload_data_entry(
    uint32_t key,
    const char *key_string,
    const uint8_t *data,
    size_t data_length
) {
    Buffer content = {0};
    append_word(&content, key);
    append_bytes(&content, 0x0c, key_string, strlen(key_string));
    append_bytes(&content, 0x04, data, data_length);
    Buffer encoded = {0};
    append_tlv(&encoded, 0x30, &content);
    return encoded;
}

static Buffer payload_string_entry(
    uint32_t key,
    const char *key_string,
    const char *value
) {
    Buffer content = {0};
    append_word(&content, key);
    append_bytes(&content, 0x0c, key_string, strlen(key_string));
    append_bytes(&content, 0x0c, value, strlen(value));
    Buffer encoded = {0};
    append_tlv(&encoded, 0x30, &content);
    return encoded;
}

static Buffer payload_deleted_entry(uint32_t key, const char *key_string) {
    Buffer content = {0};
    append_word(&content, key);
    append_bytes(&content, 0x0c, key_string, strlen(key_string));
    const uint8_t deleted = 0xff;
    append_bytes(&content, 0x01, &deleted, sizeof(deleted));
    Buffer encoded = {0};
    append_tlv(&encoded, 0x30, &content);
    return encoded;
}

static Buffer valid_sysconfig3(void) {
    const uint8_t data[] = {0xde, 0xad, 0xbe, 0xef};
    const Buffer data_entry = payload_data_entry(
        UINT32_C(0x61746164),
        "data",
        data,
        sizeof(data)
    );
    const Buffer string_entry = payload_string_entry(
        UINT32_C(0x72747373),
        "sstr",
        "board"
    );
    const Buffer deleted_entry = payload_deleted_entry(
        UINT32_C(0x6c656464),
        "ddel"
    );
    Buffer payload_content = {0};
    append(&payload_content, data_entry.bytes, data_entry.length);
    append(&payload_content, string_entry.bytes, string_entry.length);
    append(&payload_content, deleted_entry.bytes, deleted_entry.length);
    Buffer payload = {0};
    append_tlv(&payload, 0x31, &payload_content);

    Buffer metadata_content = {0};
    Buffer metadata = {0};
    append_tlv(&metadata, 0x31, &metadata_content);
    Buffer manifest_content = {0};
    const uint8_t hmac[] = {1, 2, 3};
    append_bytes(&manifest_content, 0x04, hmac, sizeof(hmac));
    append_bytes(&manifest_content, 0x04, hmac, sizeof(hmac));
    append_bytes(&manifest_content, 0x04, hmac, sizeof(hmac));
    Buffer manifest = {0};
    append_tlv(&manifest, 0x30, &manifest_content);

    Buffer top_content = {0};
    append_word(&top_content, UINT32_C(0x33434653));
    append_word(&top_content, SYSCONFIG3_VERSION_2);
    append(&top_content, payload.bytes, payload.length);
    append(&top_content, metadata.bytes, metadata.length);
    append(&top_content, manifest.bytes, manifest.length);
    Buffer encoded = {0};
    append_tlv(&encoded, 0x30, &top_content);
    return encoded;
}

static void acquire_lock(void *context) {
    assert(context == &encoded_configuration);
    ++acquire_count;
}

static void release_lock(void *context) {
    assert(context == &encoded_configuration);
    ++release_count;
}

static _Noreturn void fatal_callback(const char *message) {
    assert(message != 0);
    assert(panic_expected);
    longjmp(panic_environment, 1);
}

static void log_callback(uint32_t level, const char *format, ...) {
    assert(level == 0);
    assert(strcmp(format, "platform/syscfg.generated:%u") == 0);
    va_list arguments;
    va_start(arguments, format);
    logged_line = va_arg(arguments, uint32_t);
    va_end(arguments);
    ++log_count;
}

static void secondary_log_callback(const char *format, ...) {
    (void)format;
}

static uint32_t fetch_configuration(
    void *context,
    const char name[5],
    uint32_t selector,
    const uint8_t **encoded,
    size_t *encoded_length
) {
    assert(context == &encoded_configuration);
    ++fetch_count;
    memcpy(fetched_name, name, sizeof(fetched_name));
    fetched_selector = selector;
    *encoded = encoded_configuration.bytes;
    *encoded_length = encoded_configuration.length;
    return fetch_status;
}

static PlatformSyscfgRegistrationRecord registration_record(
    uint32_t selector
) {
    return (PlatformSyscfgRegistrationRecord){
        .tag = UINT32_C(0x53434647),
        .selector = selector,
        .fetch = fetch_configuration,
        .context = &encoded_configuration,
        .source_flag = 1,
    };
}

static void reset_fixture(void) {
    platform_syscfg_registry_reset();
    encoded_configuration = valid_sysconfig3();
    acquire_count = 0;
    release_count = 0;
    log_count = 0;
    logged_line = 0;
    fetch_count = 0;
    memset(fetched_name, 0, sizeof(fetched_name));
    fetched_selector = 0;
    fetch_status = 0;
    panic_expected = false;
    platform_syscfg_bind_transport((PlatformSyscfgTransport){
        .acquire = acquire_lock,
        .release = release_lock,
        .fatal = fatal_callback,
        .context = &encoded_configuration,
    });
    platform_syscfg_initialize_callbacks_once(
        log_callback,
        secondary_log_callback);
}

static void test_status_and_record_contracts(void) {
    reset_fixture();
    assert(!platform_syscfg_status_skips_registration(0));
    for (uint32_t status = 1; status <= 17; ++status) {
        assert(platform_syscfg_status_skips_registration(status));
    }
    assert(!platform_syscfg_status_skips_registration(18));
    assert(!platform_syscfg_status_skips_registration(19));
    assert(!platform_syscfg_status_skips_registration(20));
    assert(platform_syscfg_status_skips_registration(21));
    assert(platform_syscfg_status_skips_registration(UINT32_MAX));

    PlatformSyscfgRegistrationRecord record = registration_record(4);
    SysConfig3 *configuration = DERDecodeSysConfig3(
        encoded_configuration.bytes,
        (uint32_t)encoded_configuration.length);
    assert(configuration != 0);
    PlatformSyscfgSource *source = platform_syscfg_source_create(
        &(PlatformSyscfgSourceMethods){
            .destroy = platform_syscfg_source_destroy,
            .copy_entry = platform_syscfg_source_copy_entry,
            .get_entry_view = platform_syscfg_source_get_entry_view,
        },
        &record,
        configuration,
        0xa0);
    assert(source != 0);
    assert(source->magic == UINT32_C(0x53434647));
    assert(source->access_class == 1);
    assert(source->source_flag == 1);
    assert(platform_syscfg_source_record_failure_clear_size(source) == 9);
    source->access_class = 2;
    assert(platform_syscfg_source_record_failure_clear_size(source) == 8);
    source->access_class = 3;
    assert(platform_syscfg_source_record_failure_clear_size(source) == 0);
    source->access_class = 4;
    assert(platform_syscfg_source_record_failure_clear_size(source) == 0);
    platform_syscfg_source_destroy(source);
    assert(source->decoded_config.current == 0);
    assert(source->encoded_length == 0);
    sysconfig3_release_owned(source);
}

static void test_payload_entry_contract(void) {
    reset_fixture();
    SysConfig3 *configuration = DERDecodeSysConfig3(
        encoded_configuration.bytes,
        (uint32_t)encoded_configuration.length);
    assert(configuration != 0);
    RuntimeFirebloomSpan value = {0};
    uint32_t length = 0;
    bool deleted = false;

    SysConfig3PayloadEntry *entry = sysConfig3GetPayloadEntryForKey(
        configuration,
        UINT32_C(0x61746164)
    );
    assert(platform_syscfg_payload_entry_get_value(
        entry, &value, &length, &deleted));
    assert(length == 4 && !deleted);
    assert(memcmp(value.current, "\xde\xad\xbe\xef", 4) == 0);

    entry = sysConfig3GetPayloadEntryForKey(
        configuration,
        UINT32_C(0x72747373)
    );
    assert(platform_syscfg_outlined_extract_payload_entry(
        entry, &value, &length, &deleted));
    assert(length == 5 && !deleted);
    assert(memcmp(value.current, "board", 5) == 0);
    assert((uintptr_t)value.upper_bound - (uintptr_t)value.current == 6);

    entry = sysConfig3GetPayloadEntryForKey(
        configuration,
        UINT32_C(0x6c656464)
    );
    assert(!platform_syscfg_payload_entry_get_value(
        entry, &value, &length, &deleted));
    assert(deleted);
    freeSysConfig3(configuration);
}

static void test_decode_registry_and_lookup(void) {
    reset_fixture();
    PlatformSyscfgRegistrationRecord record = registration_record(1);
    PlatformSyscfgSource *source = platform_syscfg_decode_and_register_source(
        &record,
        encoded_configuration.bytes,
        encoded_configuration.length);
    assert(source != 0);
    assert(source->access_class == 4);
    assert(platform_syscfg_registry_count() == 1);

    uint8_t output[8];
    memset(output, 0xcc, sizeof(output));
    assert(platform_syscfg_source_copy_entry(
        source, UINT32_C(0x61746164), output, 3) == 3);
    assert(memcmp(output, "\xde\xad\xbe", 3) == 0);
    RuntimeFirebloomSpan value = {0};
    uint32_t length = 0;
    bool deleted = false;
    assert(platform_syscfg_source_get_entry_view(
        source, UINT32_C(0x72747373), &value, &length, &deleted));
    assert(length == 5 && !deleted);
    assert(platform_syscfg_get_entry(
        UINT32_C(0x61746164), output, sizeof(output)) == 4);
    assert(acquire_count == 1 && release_count == 1);
    assert(memcmp(output, "\xde\xad\xbe\xef", 4) == 0);

    memset(output, 0xaa, sizeof(output));
    assert(platform_syscfg_get_entry(
        UINT32_C(0x6c656464), output, sizeof(output)) == -1);
    assert(acquire_count == 2 && release_count == 2);
    for (size_t index = 0; index < sizeof(output); ++index) {
        assert(output[index] == 0xaa);
    }
}

static void test_load_record_callbacks_and_skip_status(void) {
    reset_fixture();
    PlatformSyscfgRegistrationRecord record = registration_record(7);
    assert(platform_syscfg_load_and_register_record(&record) == 0);
    assert(fetch_count == 1);
    assert(strcmp(fetched_name, "SCFG") == 0);
    assert(fetched_selector == 7);
    assert(platform_syscfg_registry_count() == 1);
    assert(log_count == 1 && logged_line == 0x70);

    fetch_status = 1;
    assert(platform_syscfg_load_and_register_record(&record) == 1);
    assert(platform_syscfg_registry_count() == 1);
    assert(log_count == 1);
    platform_syscfg_log_generated_location(123);
    assert(log_count == 2 && logged_line == 123);
    platform_syscfg_initialize_callbacks_once(
        platform_syscfg_noop_log_callback(),
        secondary_log_callback);
    platform_syscfg_log_generated_location(124);
    assert(log_count == 3 && logged_line == 124);
}

static void test_views_outliners_and_fatal(void) {
    reset_fixture();
    PlatformSyscfgSource source = {0};
    RuntimeFirebloomSpan source_span = {
        .current = &source,
        .lower_bound = &source,
        .upper_bound = &source + 1,
        .type = &platform_syscfg_source_type,
    };
    assert(platform_syscfg_validate_source_record_tuple(source_span).current ==
        &source);
    assert(platform_syscfg_validate_source_record_tuple_thunk(source_span).type ==
        &platform_syscfg_source_type);
    assert(platform_syscfg_validate_source_record_and_compare_lower_bound(
        source_span));
    RuntimeFirebloomSpan encoded_span = {
        .current = encoded_configuration.bytes,
        .lower_bound = encoded_configuration.bytes,
        .upper_bound = encoded_configuration.bytes +
            encoded_configuration.length,
        .type = &platform_syscfg_byte_type,
    };
    assert(platform_syscfg_validate_encoded_byte_span(encoded_span).current ==
        encoded_configuration.bytes);
    assert(platform_syscfg_outlined_save_source_span_x22_x19(
        encoded_span).current == encoded_configuration.bytes);
    assert(platform_syscfg_outlined_restore_source_span_x22_x19(
        encoded_span).upper_bound == encoded_span.upper_bound);

    PlatformSyscfgTriplet triplet =
        platform_syscfg_outlined_restore_source_record_bounds_x20_x22(
            &source,
            &source,
            &source + 1
        );
    assert(triplet.current == &source && triplet.lower_bound == &source);
    triplet = platform_syscfg_outlined_restore_source_list_bounds_x19_x28(
        &source,
        &source,
        &source + 1
    );
    assert(triplet.upper_bound == &source + 1);
    assert(platform_syscfg_outlined_load_source_list_sentinel_x10() == 0);
    assert(platform_syscfg_lock_bounded_view().type ==
        platform_syscfg_outlined_load_lock_type_x3());
    PlatformSyscfgSourceListSlots slots =
        platform_syscfg_outlined_load_source_list_slots();
    assert(slots.type_slot != 0 && *slots.type_slot ==
        &platform_syscfg_source_type);
    platform_syscfg_lock_release();
    assert(release_count == 1);

    panic_expected = true;
    if (setjmp(panic_environment) == 0) {
        platform_syscfg_source_record_bounds_panic();
        assert(false);
    }
    if (setjmp(panic_environment) == 0) {
        platform_syscfg_fatal_format_callback("failure %u", 7u);
        assert(false);
    }
    panic_expected = false;
}

int main(void) {
    test_status_and_record_contracts();
    test_payload_entry_contract();
    test_decode_registry_and_lookup();
    test_load_record_callbacks_and_skip_status();
    test_views_outliners_and_fatal();
    platform_syscfg_registry_reset();
    puts("platform SysCfg owner tests passed");
}
