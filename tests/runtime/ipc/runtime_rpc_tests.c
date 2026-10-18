#include "avp/runtime/ipc/runtime_rpc.h"

#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include <sys/wait.h>
#include <unistd.h>


static RuntimeRPCBuilder death_builder;
static uint64_t death_arena[32];

_Noreturn void pvpanic_trigger(void) {
    raise(SIGABRT);
    __builtin_unreachable();
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

static RuntimeRPCPayloadSpan payload_span(const void *bytes, size_t size) {
    return (RuntimeRPCPayloadSpan){
        .current = bytes,
        .lower_bound = bytes,
        .upper_bound = (const uint8_t *)bytes + size,
        .type_token = UINT64_C(0x144f00),
    };
}

static void append_after_finalize(void) {
    uint32_t value = 1;
    (void)runtime_rpc_builder_append_field(
        &death_builder,
        (RuntimeRPCFieldSpec){
            .alignment = sizeof(value),
            .size = sizeof(value),
            .type_token = AVP_RUNTIME_RPC_NO_POINTER_TOKEN,
        },
        payload_span(&value, sizeof(value))
    );
}

static void append_misaligned_pointer(void) {
    uint32_t value = 1;
    (void)runtime_rpc_builder_append_field(
        &death_builder,
        (RuntimeRPCFieldSpec){
            .alignment = sizeof(value),
            .size = sizeof(value),
            .pointer_offset = 1,
            .type_token = AVP_RUNTIME_RPC_NO_POINTER_TOKEN,
        },
        payload_span(&value, sizeof(value))
    );
}

static void traverse_corrupt_tail(void) {
    const RuntimeRPCPacketView packet = runtime_rpc_builder_view(
        &death_builder
    );
    const RuntimeRPCFieldHeader *field = runtime_rpc_field_find(packet, 0);
    assert(field != 0);
    uint64_t *tail = (uint64_t *)runtime_rpc_field_payload_const(field) + 1;
    *tail = 0;
    (void)runtime_rpc_field_find(packet, 0);
}

static void test_storage_packet(void) {
    static const uint64_t root_expected[] = {
        0xaca7f00dbeefcafe, 0xfeedf00d70ca7500,
        0x0000000000000004, 0x0000000000000000,
        0x0000000000000000, 0x0000000000000000,
        0xea7badca75f00d00, 0xfeedf00d70ca7501,
        0x8000000000000005, 0x0000000000000000,
        0x0000000000144f18, 0x00000000746f6f72,
        0xea7badca75f00d01, 0xfeedf00d70ca7502,
        0x0000000000000004, 0x0000000000000000,
        0x49734e6f50747472, 0x0000000000000000,
        0xea7badca75f00d02, 0xbeefcafefed9ca75,
    };
    RuntimeStorageInitializeRPC packet;
    runtime_rpc_build_storage_initialize(&packet, "root", 0);
    const uint64_t *words = (const uint64_t *)&packet;
    for (uint32_t index = 0;
         index < sizeof(root_expected) / sizeof(root_expected[0]);
         ++index) {
        assert(words[index] == root_expected[index]);
    }
    assert(runtime_rpc_validate_storage_initialize(&packet));
    runtime_rpc_build_storage_initialize(&packet, "util", 1);
    assert(words[11] == 0x000000006c697475);
    assert(words[17] == 1);
    assert(runtime_rpc_validate_storage_initialize(&packet));
}

static void test_builder_and_parser(void) {
    uint64_t arena[64];
    RuntimeRPCBuilder builder;
    runtime_rpc_builder_initialize(&builder, arena, sizeof(arena));

    RuntimeRPCFieldHeader *output = runtime_rpc_builder_append_field(
        &builder,
        (RuntimeRPCFieldSpec){
            .alignment = sizeof(uint32_t),
            .size = sizeof(uint32_t),
        },
        (RuntimeRPCPayloadSpan){0}
    );
    const uint8_t bytes[] = {1, 2, 3, 4, 5};
    RuntimeRPCFieldHeader *input = runtime_rpc_builder_append_field(
        &builder,
        (RuntimeRPCFieldSpec){
            .alignment = 1,
            .size = sizeof(bytes),
            .type_token = AVP_RUNTIME_RPC_DOMAIN_TYPE_TOKEN,
            .span = true,
        },
        payload_span(bytes, sizeof(bytes))
    );
    assert(runtime_rpc_builder_finalize(&builder) == 0x70);

    const RuntimeRPCPacketView packet = runtime_rpc_builder_view(&builder);
    assert(packet.byte_size == 0x70);
    assert(runtime_rpc_field_find(packet, 0) == output);
    assert(runtime_rpc_field_find(packet, 1) == input);
    assert(runtime_rpc_field_find(packet, 2) == 0);
    assert(runtime_rpc_field_next(packet, output) == input);
    assert(runtime_rpc_field_next(packet, input) == 0);
    assert(runtime_rpc_field_require(packet, input, 1) == input);
    assert(runtime_rpc_field_payload_size(output) == sizeof(uint32_t));
    assert(*(const uint32_t *)runtime_rpc_field_payload_const(output) == 0);
    const uint8_t *copy = runtime_rpc_field_payload_const(input);
    for (size_t index = 0; index < sizeof(bytes); ++index) {
        assert(copy[index] == bytes[index]);
    }
    assert(copy[5] == 0 && copy[6] == 0 && copy[7] == 0);
}

static void test_raw_pointer_tuple(void) {
    uint64_t arena[32];
    RuntimeRPCBuilder builder;
    runtime_rpc_builder_initialize(&builder, arena, sizeof(arena));
    uint64_t seed = 0;
    (void)runtime_rpc_builder_append_field(
        &builder,
        (RuntimeRPCFieldSpec){
            .alignment = sizeof(seed),
            .size = sizeof(seed),
        },
        (RuntimeRPCPayloadSpan){0}
    );
    RuntimeRPCFieldHeader *field = runtime_rpc_builder_append_field(
        &builder,
        (RuntimeRPCFieldSpec){
            .alignment = sizeof(RuntimeRPCPayloadSpan),
            .size = sizeof(RuntimeRPCPayloadSpan),
            .type_token = AVP_RUNTIME_RPC_RAW_POINTER_TOKEN,
        },
        (RuntimeRPCPayloadSpan){
            .current = (const void *)(uintptr_t)
                AVP_RUNTIME_RPC_RAW_POINTER_TOKEN,
            .lower_bound = (const void *)(uintptr_t)0x1000,
            .upper_bound = (const void *)(uintptr_t)0x2000,
            .type_token = 0x3000,
        }
    );
    assert(runtime_rpc_builder_finalize(&builder) == 0x88);
    const uint64_t *tuple = runtime_rpc_field_payload_const(field);
    assert(tuple[0] == AVP_RUNTIME_RPC_RAW_POINTER_TOKEN);
    assert(tuple[1] == 0x1000);
    assert(tuple[2] == 0x2000);
    assert(tuple[3] == 0x3000);
}

static void test_fatal_contracts(void) {
    uint32_t seed = 0;
    runtime_rpc_builder_initialize(
        &death_builder,
        death_arena,
        sizeof(death_arena)
    );
    (void)runtime_rpc_builder_append_field(
        &death_builder,
        (RuntimeRPCFieldSpec){
            .alignment = sizeof(seed),
            .size = sizeof(seed),
        },
        (RuntimeRPCPayloadSpan){0}
    );
    (void)runtime_rpc_builder_finalize(&death_builder);
    expect_fatal(append_after_finalize);
    expect_fatal(traverse_corrupt_tail);

    runtime_rpc_builder_initialize(
        &death_builder,
        death_arena,
        sizeof(death_arena)
    );
    (void)runtime_rpc_builder_append_field(
        &death_builder,
        (RuntimeRPCFieldSpec){
            .alignment = sizeof(seed),
            .size = sizeof(seed),
        },
        (RuntimeRPCPayloadSpan){0}
    );
    expect_fatal(append_misaligned_pointer);
}

int main(void) {
    test_storage_packet();
    test_builder_and_parser();
    test_raw_pointer_tuple();
    test_fatal_contracts();
    return 0;
}
