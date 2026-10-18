#include "avp/runtime/ipc/runtime_rpc.h"
#include "avp/runtime/support/recovered_symbol.h"
#include "avp/runtime/memory/runtime_memory.h"


#if defined(__ELF__)
#define RUNTIME_RPC_TEXT __attribute__((section(".text.runtime_rpc_ranges")))
#else
#define RUNTIME_RPC_TEXT
#endif

static uintptr_t address_of(const void *pointer) {
    return (uintptr_t)pointer;
}

static size_t align_to_qword(size_t size) {
    if (size > SIZE_MAX - 7) {
        runtime_firebloom_panic();
    }
    return (size + 7) & ~(size_t)7;
}

static void validate_builder(const RuntimeRPCBuilder *builder) {
    if (builder == 0 || builder->base == 0 ||
        builder->base != builder->lower_bound ||
        builder->upper_bound <= builder->lower_bound ||
        (address_of(builder->base) & 7) != 0 ||
        builder->word_count == 0 ||
        builder->word_count >= AVP_RUNTIME_RPC_MAX_WORDS ||
        builder->field_index > AVP_RUNTIME_RPC_MAX_FIELDS ||
        builder->base[0] != AVP_RUNTIME_RPC_PACKET_MAGIC) {
        runtime_firebloom_panic();
    }
}

AVP_RECOVERED_ENTRY static void append_word(
    RuntimeRPCBuilder *builder,
    uint64_t value
) {
    validate_builder(builder);
    if (builder->finalized_word_count != 0 ||
        builder->word_count >= AVP_RUNTIME_RPC_MAX_WORDS) {
        runtime_firebloom_panic();
    }
    uint64_t *output = builder->base + builder->word_count;
    if (output < builder->lower_bound || output >= builder->upper_bound) {
        runtime_firebloom_panic();
    }
    builder->word_count += 1;
    *output = value;
}

static size_t packet_word_count(RuntimeRPCPacketView packet) {
    if (packet.base == 0 || packet.byte_size < 2 * sizeof(uint64_t) ||
        packet.byte_size > AVP_RUNTIME_RPC_ARENA_SIZE ||
        (packet.byte_size & 7) != 0 ||
        packet.base[0] != AVP_RUNTIME_RPC_PACKET_MAGIC ||
        packet.base[packet.byte_size / sizeof(uint64_t) - 1] !=
            AVP_RUNTIME_RPC_PACKET_TAIL) {
        runtime_firebloom_panic();
    }
    return packet.byte_size / sizeof(uint64_t);
}

static size_t field_payload_size_checked(
    const RuntimeRPCFieldHeader *field
) {
    if (field == 0 ||
        (field->magic & UINT64_C(0xffffffffffffff00)) !=
            AVP_RUNTIME_RPC_FIELD_MAGIC) {
        runtime_firebloom_panic();
    }
    return (size_t)(field->size_and_flags & ~AVP_RUNTIME_RPC_SPAN_FLAG);
}

RUNTIME_RPC_TEXT void runtime_rpc_builder_initialize(
    RuntimeRPCBuilder *builder,
    void *arena,
    size_t arena_size
) {
    if (builder == 0 || arena == 0 || arena_size < 2 * sizeof(uint64_t) ||
        arena_size > AVP_RUNTIME_RPC_ARENA_SIZE ||
        (address_of(arena) & 7) != 0 || (arena_size & 7) != 0) {
        runtime_firebloom_panic();
    }
    runtime_bzero(arena, arena_size);
    *builder = (RuntimeRPCBuilder){
        .base = arena,
        .lower_bound = arena,
        .upper_bound = (uint64_t *)((uint8_t *)arena + arena_size),
        .arena_type_token = UINT64_C(0x144f30),
    };
    builder->word_count = 1;
    builder->base[0] = AVP_RUNTIME_RPC_PACKET_MAGIC;
}

RUNTIME_RPC_TEXT RuntimeRPCFieldHeader *runtime_rpc_builder_append_field(
    RuntimeRPCBuilder *builder,
    RuntimeRPCFieldSpec spec,
    RuntimeRPCPayloadSpan payload
) {
    validate_builder(builder);
    if (builder->finalized_word_count != 0 ||
        builder->field_index >= AVP_RUNTIME_RPC_MAX_FIELDS) {
        runtime_firebloom_panic();
    }

    size_t aligned_size = 0;
    if (spec.alignment != 0) {
        aligned_size = align_to_qword(spec.size);
        if (spec.size < spec.alignment ||
            spec.size % spec.alignment != 0) {
            runtime_firebloom_panic();
        }
    } else if (payload.current != 0) {
        runtime_firebloom_panic();
    }
    if (builder->field_index != 0) {
        if (spec.alignment == 0 || spec.type_token == 0 ||
            spec.pointer_offset >= spec.size ||
            spec.pointer_offset % spec.alignment != 0) {
            runtime_firebloom_panic();
        }
    }
    if (spec.span && (builder->field_index == 0 ||
        spec.type_token == 0 ||
        spec.type_token == AVP_RUNTIME_RPC_NO_POINTER_TOKEN)) {
        runtime_firebloom_panic();
    }

    const size_t payload_words = aligned_size / sizeof(uint64_t);
    const size_t required_words = 5 + payload_words;
    if (builder->word_count > UINT16_MAX - required_words ||
        builder->word_count + required_words >= 0x7ff ||
        builder->word_count + required_words >
            (size_t)(builder->upper_bound - builder->base)) {
        runtime_firebloom_panic();
    }

    const uint8_t field_index = builder->field_index;
    RuntimeRPCFieldHeader *field =
        (RuntimeRPCFieldHeader *)(builder->base + builder->word_count);
    append_word(builder, AVP_RUNTIME_RPC_FIELD_MAGIC | field_index);
    append_word(
        builder,
        spec.size | (spec.span ? AVP_RUNTIME_RPC_SPAN_FLAG : 0)
    );
    append_word(builder, spec.pointer_offset);
    append_word(builder, spec.type_token);

    uint8_t *output = (uint8_t *)(builder->base + builder->word_count);
    builder->word_count += payload_words;
    if (payload.current ==
        (const void *)(uintptr_t)AVP_RUNTIME_RPC_RAW_POINTER_TOKEN) {
        if (spec.size != sizeof(RuntimeRPCPayloadSpan) ||
            spec.alignment != sizeof(RuntimeRPCPayloadSpan) ||
            aligned_size != sizeof(RuntimeRPCPayloadSpan)) {
            runtime_firebloom_panic();
        }
        const uintptr_t tuple[] = {
            address_of(payload.current),
            address_of(payload.lower_bound),
            address_of(payload.upper_bound),
            payload.type_token,
        };
        runtime_memmove(output, tuple, sizeof(tuple));
    } else if (payload.current != 0) {
        const uintptr_t current = address_of(payload.current);
        const uintptr_t lower = address_of(payload.lower_bound);
        const uintptr_t upper = address_of(payload.upper_bound);
        if (current < lower || current > upper || spec.size > upper - current) {
            runtime_firebloom_panic();
        }
        runtime_memmove(output, payload.current, spec.size);
    }
    append_word(builder, AVP_RUNTIME_RPC_FIELD_TAIL | field_index);
    builder->field_index += 1;
    return field;
}

RUNTIME_RPC_TEXT size_t runtime_rpc_builder_finalize(
    RuntimeRPCBuilder *builder
) {
    validate_builder(builder);
    if (builder->finalized_word_count != 0 || builder->field_index == 0 ||
        builder->word_count <= 5) {
        runtime_firebloom_panic();
    }
    append_word(builder, AVP_RUNTIME_RPC_PACKET_TAIL);
    builder->finalized_word_count = builder->word_count;
    return (size_t)builder->finalized_word_count * sizeof(uint64_t);
}

RUNTIME_RPC_TEXT RuntimeRPCPacketView runtime_rpc_builder_view(
    const RuntimeRPCBuilder *builder
) {
    validate_builder(builder);
    if (builder->finalized_word_count != builder->word_count) {
        runtime_firebloom_panic();
    }
    return (RuntimeRPCPacketView){
        .base = builder->base,
        .byte_size = (size_t)builder->finalized_word_count *
            sizeof(uint64_t),
    };
}

RUNTIME_RPC_TEXT const RuntimeRPCFieldHeader *runtime_rpc_field_next(
    RuntimeRPCPacketView packet,
    const RuntimeRPCFieldHeader *field
) {
    const size_t words = packet_word_count(packet);
    const uintptr_t packet_start = address_of(packet.base);
    const uintptr_t packet_end = packet_start + packet.byte_size;
    const uintptr_t field_address = address_of(field);
    if (field == 0 || field_address <= packet_start ||
        field_address >= packet_end - sizeof(uint64_t) ||
        (field_address - packet_start) % sizeof(uint64_t) != 0) {
        runtime_firebloom_panic();
    }
    const size_t payload_size = field_payload_size_checked(field);
    const size_t field_words = 5 + align_to_qword(payload_size) /
        sizeof(uint64_t);
    const size_t field_index =
        (field_address - packet_start) / sizeof(uint64_t);
    if (field_index + field_words >= words) {
        runtime_firebloom_panic();
    }
    const uint64_t expected_tail = AVP_RUNTIME_RPC_FIELD_TAIL |
        (field->magic & UINT64_C(0xff));
    if (packet.base[field_index + field_words - 1] != expected_tail) {
        runtime_firebloom_panic();
    }
    const uint64_t *next = packet.base + field_index + field_words;
    if (*next == AVP_RUNTIME_RPC_PACKET_TAIL) {
        return 0;
    }
    if ((*next & UINT64_C(0xffffffffffffff00)) !=
        AVP_RUNTIME_RPC_FIELD_MAGIC) {
        runtime_firebloom_panic();
    }
    return (const RuntimeRPCFieldHeader *)next;
}

RUNTIME_RPC_TEXT const RuntimeRPCFieldHeader *runtime_rpc_field_find(
    RuntimeRPCPacketView packet,
    uint8_t field_index
) {
    (void)packet_word_count(packet);
    const RuntimeRPCFieldHeader *field =
        (const RuntimeRPCFieldHeader *)(packet.base + 1);
    while ((const uint64_t *)field <
        packet.base + packet.byte_size / sizeof(uint64_t) - 1) {
        const RuntimeRPCFieldHeader *next = runtime_rpc_field_next(
            packet,
            field
        );
        if ((uint8_t)field->magic == field_index) {
            return field;
        }
        if (next == 0) {
            return 0;
        }
        field = next;
    }
    return 0;
}

RUNTIME_RPC_TEXT const RuntimeRPCFieldHeader *runtime_rpc_field_require(
    RuntimeRPCPacketView packet,
    const RuntimeRPCFieldHeader *field,
    uint8_t field_index
) {
    (void)runtime_rpc_field_next(packet, field);
    if ((uint8_t)field->magic != field_index) {
        runtime_firebloom_panic();
    }
    return field;
}

RUNTIME_RPC_TEXT void *runtime_rpc_field_payload(
    RuntimeRPCFieldHeader *field
) {
    (void)field_payload_size_checked(field);
    return field + 1;
}

RUNTIME_RPC_TEXT const void *runtime_rpc_field_payload_const(
    const RuntimeRPCFieldHeader *field
) {
    (void)field_payload_size_checked(field);
    return field + 1;
}

RUNTIME_RPC_TEXT size_t runtime_rpc_field_payload_size(
    const RuntimeRPCFieldHeader *field
) {
    return field_payload_size_checked(field);
}

void runtime_rpc_build_storage_initialize(
    RuntimeStorageInitializeRPC *packet,
    const char domain[4],
    uint32_t slot
) {
    RuntimeRPCBuilder builder;
    runtime_rpc_builder_initialize(&builder, packet, sizeof(*packet));

    (void)runtime_rpc_builder_append_field(
        &builder,
        (RuntimeRPCFieldSpec){
            .alignment = _Alignof(uint32_t),
            .size = sizeof(uint32_t),
        },
        (RuntimeRPCPayloadSpan){0}
    );

    char domain_value[5] = {
        domain[0], domain[1], domain[2], domain[3], '\0',
    };
    (void)runtime_rpc_builder_append_field(
        &builder,
        (RuntimeRPCFieldSpec){
            .alignment = 1,
            .size = sizeof(domain_value),
            .type_token = AVP_RUNTIME_RPC_DOMAIN_TYPE_TOKEN,
            .span = true,
        },
        (RuntimeRPCPayloadSpan){
            .current = domain_value,
            .lower_bound = domain_value,
            .upper_bound = domain_value + sizeof(domain_value),
            .type_token = UINT64_C(0x144f00),
        }
    );

    (void)runtime_rpc_builder_append_field(
        &builder,
        (RuntimeRPCFieldSpec){
            .alignment = _Alignof(uint32_t),
            .size = sizeof(slot),
            .type_token = AVP_RUNTIME_RPC_NO_POINTER_TOKEN,
        },
        (RuntimeRPCPayloadSpan){
            .current = &slot,
            .lower_bound = &slot,
            .upper_bound = (const uint8_t *)&slot + sizeof(slot),
            .type_token = UINT64_C(0x144f00),
        }
    );
    if (runtime_rpc_builder_finalize(&builder) != sizeof(*packet)) {
        runtime_firebloom_panic();
    }
}

bool runtime_rpc_validate_storage_initialize(
    const RuntimeStorageInitializeRPC *packet
) {
    return packet->packet_magic == AVP_RUNTIME_RPC_PACKET_MAGIC &&
        packet->result_header.magic == AVP_RUNTIME_RPC_FIELD_MAGIC &&
        packet->result_header.size_and_flags == sizeof(uint32_t) &&
        packet->result_header.pointer_offset == 0 &&
        packet->result_header.type_token == 0 &&
        packet->result_tail == AVP_RUNTIME_RPC_FIELD_TAIL &&
        packet->domain_header.magic == (AVP_RUNTIME_RPC_FIELD_MAGIC | 1) &&
        packet->domain_header.size_and_flags ==
            (AVP_RUNTIME_RPC_SPAN_FLAG | 5) &&
        packet->domain_header.pointer_offset == 0 &&
        packet->domain_header.type_token ==
            AVP_RUNTIME_RPC_DOMAIN_TYPE_TOKEN &&
        packet->domain[4] == 0 && packet->domain[5] == 0 &&
        packet->domain[6] == 0 && packet->domain[7] == 0 &&
        packet->domain_tail == (AVP_RUNTIME_RPC_FIELD_TAIL | 1) &&
        packet->slot_header.magic == (AVP_RUNTIME_RPC_FIELD_MAGIC | 2) &&
        packet->slot_header.size_and_flags == sizeof(uint32_t) &&
        packet->slot_header.pointer_offset == 0 &&
        packet->slot_header.type_token == AVP_RUNTIME_RPC_NO_POINTER_TOKEN &&
        packet->slot_padding == 0 &&
        packet->slot_tail == (AVP_RUNTIME_RPC_FIELD_TAIL | 2) &&
        packet->packet_tail == AVP_RUNTIME_RPC_PACKET_TAIL;
}
