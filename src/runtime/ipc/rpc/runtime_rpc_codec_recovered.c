#include "avp/runtime/ipc/runtime_rpc_codec_recovered.h"


/*
 * AVPBooter's helpers preserve register aggregates between larger codec
 * routines. Explicit value types retain that ABI state as ordinary C data.
 */

static void *current_task_rpc_arena;

RuntimeRPCCodecBoundedView runtime_rpc_builder_bounded_view(
    const RuntimeRPCBuilder *builder
) {
    const uint8_t *const lower_bound = (const uint8_t *)builder;
    return (RuntimeRPCCodecBoundedView){
        .current = builder,
        .lower_bound = builder,
        .upper_bound = lower_bound + sizeof(*builder),
        .type_token = builder->arena_type_token,
    };
}

uint64_t runtime_rpc_source_token(void) {
    return AVP_RUNTIME_RPC_SOURCE_TOKEN;
}

RuntimeRPCCodecSavedTriplet runtime_rpc_outlined_save_triplet_x21_x19(
    const void *current,
    const void *lower_bound,
    const void *upper_bound
) {
    return (RuntimeRPCCodecSavedTriplet){
        .x21_current = current,
        .x20_lower_bound = lower_bound,
        .x19_upper_bound = upper_bound,
    };
}

bool runtime_rpc_outlined_compare_field_descriptor_magic(
    uint64_t candidate
) {
    return candidate == AVP_RUNTIME_RPC_PACKET_MAGIC;
}

RuntimeRPCCodecSavedBounds runtime_rpc_outlined_save_field_bounds_x21_x20(
    const void *lower_bound,
    const void *upper_bound
) {
    return (RuntimeRPCCodecSavedBounds){
        .x21_lower_bound = lower_bound,
        .x20_upper_bound = upper_bound,
    };
}

uint64_t runtime_rpc_outlined_load_ptr_is_raw_tag_x8(void) {
    return AVP_RUNTIME_RPC_RAW_POINTER_TOKEN;
}

uint64_t runtime_rpc_outlined_load_ptr_is_not_ptr_tag_x8(void) {
    return AVP_RUNTIME_RPC_NO_POINTER_TOKEN;
}

void runtime_current_task_rpc_arena_slot_publish(void *arena) {
    /* A task switch owns this slot, matching the single-core scheduler state. */
    current_task_rpc_arena = arena;
}

RuntimeRPCCodecBoundedView
runtime_current_task_rpc_arena_slot_bounded_view(void) {
    return (RuntimeRPCCodecBoundedView){
        .current = &current_task_rpc_arena,
        .lower_bound = &current_task_rpc_arena,
        .upper_bound = &current_task_rpc_arena + 1,
        .type_token = AVP_RUNTIME_RPC_ARENA_SLOT_TYPE_TOKEN,
    };
}
