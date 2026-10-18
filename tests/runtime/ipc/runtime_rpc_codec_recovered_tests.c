#include "avp/runtime/ipc/runtime_rpc_codec_recovered.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>


static void test_builder_view(void) {
    RuntimeRPCBuilder builder = {
        .arena_type_token = UINT64_C(0x144f30),
    };
    const RuntimeRPCCodecBoundedView view =
        runtime_rpc_builder_bounded_view(&builder);
    assert(view.current == &builder);
    assert(view.lower_bound == &builder);
    assert(view.upper_bound == (const uint8_t *)&builder + sizeof(builder));
    assert(view.type_token == builder.arena_type_token);
}

static void test_register_aggregate_helpers(void) {
    uint8_t storage[24] = {0};
    const RuntimeRPCCodecSavedTriplet triplet =
        runtime_rpc_outlined_save_triplet_x21_x19(
            storage + 8,
            storage,
            storage + sizeof(storage)
        );
    assert(triplet.x21_current == storage + 8);
    assert(triplet.x20_lower_bound == storage);
    assert(triplet.x19_upper_bound == storage + sizeof(storage));

    const RuntimeRPCCodecSavedBounds bounds =
        runtime_rpc_outlined_save_field_bounds_x21_x20(
            storage,
            storage + sizeof(storage)
        );
    assert(bounds.x21_lower_bound == storage);
    assert(bounds.x20_upper_bound == storage + sizeof(storage));
}

static void test_magic_and_tags(void) {
    assert(runtime_rpc_source_token() == AVP_RUNTIME_RPC_SOURCE_TOKEN);
    assert(runtime_rpc_outlined_compare_field_descriptor_magic(
        AVP_RUNTIME_RPC_PACKET_MAGIC
    ));
    assert(!runtime_rpc_outlined_compare_field_descriptor_magic(
        AVP_RUNTIME_RPC_FIELD_MAGIC
    ));
    assert(runtime_rpc_outlined_load_ptr_is_raw_tag_x8() ==
        AVP_RUNTIME_RPC_RAW_POINTER_TOKEN);
    assert(runtime_rpc_outlined_load_ptr_is_not_ptr_tag_x8() ==
        AVP_RUNTIME_RPC_NO_POINTER_TOKEN);
}

static void test_current_task_arena_slot(void) {
    uint64_t arena[8] = {0};
    runtime_current_task_rpc_arena_slot_publish(arena);
    const RuntimeRPCCodecBoundedView view =
        runtime_current_task_rpc_arena_slot_bounded_view();
    assert(view.current == view.lower_bound);
    assert(view.upper_bound == (const uint8_t *)view.lower_bound +
        sizeof(void *));
    assert(view.type_token == AVP_RUNTIME_RPC_ARENA_SLOT_TYPE_TOKEN);
    assert(*(void *const *)view.current == arena);

    runtime_current_task_rpc_arena_slot_publish(0);
    assert(*(void *const *)
        runtime_current_task_rpc_arena_slot_bounded_view().current == 0);
}

int main(void) {
    test_builder_view();
    test_register_aggregate_helpers();
    test_magic_and_tags();
    test_current_task_arena_slot();
    puts("runtime recovered RPC codec tests passed");
    return 0;
}
