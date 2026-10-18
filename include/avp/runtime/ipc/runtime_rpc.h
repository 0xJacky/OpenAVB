#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#define AVP_RUNTIME_RPC_PACKET_MAGIC UINT64_C(0xaca7f00dbeefcafe)
#define AVP_RUNTIME_RPC_PACKET_TAIL UINT64_C(0xbeefcafefed9ca75)
#define AVP_RUNTIME_RPC_FIELD_MAGIC UINT64_C(0xfeedf00d70ca7500)
#define AVP_RUNTIME_RPC_FIELD_TAIL UINT64_C(0xea7badca75f00d00)
#define AVP_RUNTIME_RPC_SPAN_FLAG UINT64_C(0x8000000000000000)
#define AVP_RUNTIME_RPC_RAW_POINTER_TOKEN UINT64_C(0x5074724973526177)
#define AVP_RUNTIME_RPC_DOMAIN_TYPE_TOKEN UINT64_C(0x144f18)
#define AVP_RUNTIME_RPC_NO_POINTER_TOKEN UINT64_C(0x49734e6f50747472)

enum {
    AVP_RUNTIME_RPC_ARENA_SIZE = 0x4000,
    AVP_RUNTIME_RPC_MAX_WORDS = 0x800,
    AVP_RUNTIME_RPC_MAX_FIELDS = 0x10,
};

typedef struct {
    uint64_t magic;
    uint64_t size_and_flags;
    uint64_t pointer_offset;
    uint64_t type_token;
} RuntimeRPCFieldHeader;

typedef struct {
    const void *current;
    const void *lower_bound;
    const void *upper_bound;
    uintptr_t type_token;
} RuntimeRPCPayloadSpan;

typedef struct {
    size_t alignment;
    size_t size;
    size_t pointer_offset;
    uint64_t type_token;
    bool span;
} RuntimeRPCFieldSpec;

typedef struct {
    uint64_t *base;
    uint64_t *lower_bound;
    uint64_t *upper_bound;
    uintptr_t arena_type_token;
    uint16_t word_count;
    uint16_t finalized_word_count;
    uint8_t field_index;
    uint8_t reserved[3];
} RuntimeRPCBuilder;

typedef struct {
    const uint64_t *base;
    size_t byte_size;
} RuntimeRPCPacketView;

typedef struct {
    uint64_t packet_magic;
    RuntimeRPCFieldHeader result_header;
    uint32_t result;
    uint32_t result_padding;
    uint64_t result_tail;
    RuntimeRPCFieldHeader domain_header;
    char domain[8];
    uint64_t domain_tail;
    RuntimeRPCFieldHeader slot_header;
    uint32_t slot;
    uint32_t slot_padding;
    uint64_t slot_tail;
    uint64_t packet_tail;
} RuntimeStorageInitializeRPC;

_Static_assert(sizeof(RuntimeRPCFieldHeader) == 0x20, "RPC field header size");
_Static_assert(sizeof(RuntimeRPCPayloadSpan) == 0x20, "RPC span size");
_Static_assert(sizeof(RuntimeRPCBuilder) == 0x28, "RPC builder size");
_Static_assert(
    offsetof(RuntimeRPCBuilder, word_count) == 0x20,
    "RPC builder word-count offset"
);
_Static_assert(
    offsetof(RuntimeRPCBuilder, finalized_word_count) == 0x22,
    "RPC builder finalized-count offset"
);
_Static_assert(
    offsetof(RuntimeRPCBuilder, field_index) == 0x24,
    "RPC builder field-index offset"
);
_Static_assert(
    sizeof(RuntimeStorageInitializeRPC) == 0xa0,
    "storage initialization RPC size"
);

void runtime_rpc_builder_initialize(
    RuntimeRPCBuilder *builder,
    void *arena,
    size_t arena_size
);
RuntimeRPCFieldHeader *runtime_rpc_builder_append_field(
    RuntimeRPCBuilder *builder,
    RuntimeRPCFieldSpec spec,
    RuntimeRPCPayloadSpan payload
);
size_t runtime_rpc_builder_finalize(RuntimeRPCBuilder *builder);
RuntimeRPCPacketView runtime_rpc_builder_view(
    const RuntimeRPCBuilder *builder
);
const RuntimeRPCFieldHeader *runtime_rpc_field_next(
    RuntimeRPCPacketView packet,
    const RuntimeRPCFieldHeader *field
);
const RuntimeRPCFieldHeader *runtime_rpc_field_find(
    RuntimeRPCPacketView packet,
    uint8_t field_index
);
const RuntimeRPCFieldHeader *runtime_rpc_field_require(
    RuntimeRPCPacketView packet,
    const RuntimeRPCFieldHeader *field,
    uint8_t field_index
);
void *runtime_rpc_field_payload(RuntimeRPCFieldHeader *field);
const void *runtime_rpc_field_payload_const(
    const RuntimeRPCFieldHeader *field
);
size_t runtime_rpc_field_payload_size(
    const RuntimeRPCFieldHeader *field
);

void runtime_rpc_build_storage_initialize(
    RuntimeStorageInitializeRPC *packet,
    const char domain[4],
    uint32_t slot
);
bool runtime_rpc_validate_storage_initialize(
    const RuntimeStorageInitializeRPC *packet
);
