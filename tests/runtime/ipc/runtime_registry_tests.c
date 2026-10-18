#include "avp/runtime/ipc/runtime_registry.h"

#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include <sys/wait.h>
#include <unistd.h>


static const AVPRuntimeRegistryRecord *death_records[2];

static void expect_trap(void (*operation)(void)) {
    pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        operation();
        _exit(0);
    }
    int status = 0;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status));
}

static void callback_value(
    uint32_t key,
    uintptr_t context,
    void *output
) {
    *(uint64_t *)output = context + key;
}

static void read_mismatched_type(void) {
    uint64_t value = 0;
    (void)runtime_registry_read_typed_optional(
        0x100,
        AVP_RUNTIME_TYPE_U32,
        &value
    );
}

static void read_missing_required(void) {
    uint64_t value = 0;
    runtime_registry_read_typed_required(
        0xffff,
        AVP_RUNTIME_TYPE_U64,
        &value
    );
}

static void build_duplicate_index(void) {
    runtime_registry_initialize(death_records, 2);
    (void)runtime_registry_lookup_entry(death_records[0]->key);
}

int main(void) {
    uint64_t deadline = 0;
    assert(runtime_registry_read_u64(
        AVP_RUNTIME_KEY_SEP_PREFLIGHT_DEADLINE,
        AVP_RUNTIME_TYPE_ABSOLUTE_MICROSECOND_DEADLINE,
        &deadline
    ));
    assert(deadline == 210000);
    assert(!runtime_registry_read_u64(
        AVP_RUNTIME_KEY_SEP_PREFLIGHT_DEADLINE + 1,
        AVP_RUNTIME_TYPE_ABSOLUTE_MICROSECOND_DEADLINE,
        &deadline
    ));

    const uint32_t indirect_value = UINT32_C(0x12345678);
    const AVPRuntimeRegistryRecord inline_record = {
        .key = 0x100,
        .type = AVP_RUNTIME_TYPE_U64 >> 16,
        .flags = AVP_RUNTIME_REGISTRY_REPRESENTATION_INLINE,
        .payload.scalar = UINT64_C(0x1122334455667788),
    };
    const AVPRuntimeRegistryRecord indirect_record = {
        .key = 0x101,
        .type = AVP_RUNTIME_TYPE_U32 >> 16,
        .flags = AVP_RUNTIME_REGISTRY_REPRESENTATION_INDIRECT,
        .payload.indirect = &indirect_value,
    };
    const AVPRuntimeRegistryRecord callback_record = {
        .key = 0x102,
        .type = AVP_RUNTIME_TYPE_U64 >> 16,
        .flags = AVP_RUNTIME_REGISTRY_REPRESENTATION_CALLBACK,
        .payload.callback = callback_value,
        .words = {[1] = 0x9000},
    };
    const AVPRuntimeRegistryRecord wide_record = {
        .key = 0x103,
        .type = AVP_RUNTIME_TYPE_BYTES_32 >> 16,
        .flags = AVP_RUNTIME_REGISTRY_FLAG_WIDE,
        .words = {
            [1] = UINT64_C(0x0102030405060708),
            [2] = UINT64_C(0x1112131415161718),
            [3] = UINT64_C(0x2122232425262728),
            [4] = UINT64_C(0x3132333435363738),
        },
    };
    const AVPRuntimeRegistryRecord alias_target = {
        .key = 0x900,
        .type = AVP_RUNTIME_TYPE_U64 >> 16,
        .payload.scalar = 0xa5a5,
    };
    const AVPRuntimeRegistryRecord alias_record = {
        .key = 0x104,
        .type = AVP_RUNTIME_TYPE_U64 >> 16,
        .flags = AVP_RUNTIME_REGISTRY_REPRESENTATION_ALIAS |
            AVP_RUNTIME_REGISTRY_FLAG_ALIAS,
        .payload.alias = &alias_target,
    };
    const AVPRuntimeRegistryRecord alias_fallback = {
        .key = 0x105,
        .type = AVP_RUNTIME_TYPE_U64 >> 16,
        .flags = AVP_RUNTIME_REGISTRY_FLAG_ALIAS,
        .payload.scalar = 0x1111,
    };
    const AVPRuntimeRegistryRecord normal_override = {
        .key = 0x105,
        .type = AVP_RUNTIME_TYPE_U64 >> 16,
        .payload.scalar = 0x2222,
    };
    const AVPRuntimeRegistryRecord *const records[] = {
        &inline_record,
        &indirect_record,
        &callback_record,
        &wide_record,
        &alias_record,
        &alias_fallback,
        &normal_override,
    };
    runtime_registry_initialize(records, sizeof(records) / sizeof(records[0]));

    uint64_t u64 = 0;
    uint32_t u32 = 0;
    uint64_t wide[4] = {0};
    assert(runtime_registry_read_typed_optional(
        0x100,
        AVP_RUNTIME_TYPE_U64,
        &u64
    ));
    assert(u64 == UINT64_C(0x1122334455667788));
    assert(runtime_registry_read_typed_optional(
        0x101,
        AVP_RUNTIME_TYPE_U32,
        &u32
    ));
    assert(u32 == indirect_value);
    assert(runtime_registry_read_typed_optional(
        0x102,
        AVP_RUNTIME_TYPE_U64,
        &u64
    ));
    assert(u64 == 0x9102);
    assert(runtime_registry_read_typed_optional(
        0x103,
        AVP_RUNTIME_TYPE_BYTES_32,
        wide
    ));
    assert(wide[0] == UINT64_C(0x0102030405060708));
    assert(wide[3] == UINT64_C(0x3132333435363738));
    assert(runtime_registry_read_typed_optional(
        0x104,
        AVP_RUNTIME_TYPE_U64,
        &u64
    ));
    assert(u64 == 0xa5a5);
    assert(runtime_registry_read_typed_optional(
        0x105,
        AVP_RUNTIME_TYPE_U64,
        &u64
    ));
    assert(u64 == 0x2222);

    expect_trap(read_mismatched_type);
    expect_trap(read_missing_required);

    const AVPRuntimeRegistryRecord duplicate_a = {
        .key = 0x200,
        .type = AVP_RUNTIME_TYPE_U64 >> 16,
    };
    const AVPRuntimeRegistryRecord duplicate_b = {
        .key = 0x200,
        .type = AVP_RUNTIME_TYPE_U64 >> 16,
    };
    death_records[0] = &duplicate_a;
    death_records[1] = &duplicate_b;
    expect_trap(build_duplicate_index);

    runtime_registry_initialize(0, 0);
    return 0;
}
