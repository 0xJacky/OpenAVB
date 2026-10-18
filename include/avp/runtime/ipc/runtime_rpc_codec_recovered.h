#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "avp/runtime/ipc/runtime_rpc.h"


#define AVP_RUNTIME_RPC_SOURCE_TOKEN UINT64_C(0x070b7ca7d373f8bb)
#define AVP_RUNTIME_RPC_ARENA_SLOT_TYPE_TOKEN UINT64_C(0x144f30)

#define RUNTIME_RPC_CODEC_RECOVERED_PUBLIC __attribute__((used, retain))

typedef struct {
    const void *current;
    const void *lower_bound;
    const void *upper_bound;
    uintptr_t type_token;
} RuntimeRPCCodecBoundedView;

typedef struct {
    const void *x21_current;
    const void *x20_lower_bound;
    const void *x19_upper_bound;
} RuntimeRPCCodecSavedTriplet;

typedef struct {
    const void *x21_lower_bound;
    const void *x20_upper_bound;
} RuntimeRPCCodecSavedBounds;

_Static_assert(
    sizeof(RuntimeRPCCodecBoundedView) == 0x20,
    "RPC codec bounded view size"
);

RUNTIME_RPC_CODEC_RECOVERED_PUBLIC RuntimeRPCCodecBoundedView
runtime_rpc_builder_bounded_view(const RuntimeRPCBuilder *builder);
RUNTIME_RPC_CODEC_RECOVERED_PUBLIC uint64_t runtime_rpc_source_token(void);
RUNTIME_RPC_CODEC_RECOVERED_PUBLIC RuntimeRPCCodecSavedTriplet
runtime_rpc_outlined_save_triplet_x21_x19(
    const void *current,
    const void *lower_bound,
    const void *upper_bound
);
RUNTIME_RPC_CODEC_RECOVERED_PUBLIC bool
runtime_rpc_outlined_compare_field_descriptor_magic(uint64_t candidate);
RUNTIME_RPC_CODEC_RECOVERED_PUBLIC RuntimeRPCCodecSavedBounds
runtime_rpc_outlined_save_field_bounds_x21_x20(
    const void *lower_bound,
    const void *upper_bound
);
RUNTIME_RPC_CODEC_RECOVERED_PUBLIC uint64_t
runtime_rpc_outlined_load_ptr_is_raw_tag_x8(void);
RUNTIME_RPC_CODEC_RECOVERED_PUBLIC uint64_t
runtime_rpc_outlined_load_ptr_is_not_ptr_tag_x8(void);

void runtime_current_task_rpc_arena_slot_publish(void *arena);
RUNTIME_RPC_CODEC_RECOVERED_PUBLIC RuntimeRPCCodecBoundedView
runtime_current_task_rpc_arena_slot_bounded_view(void);
