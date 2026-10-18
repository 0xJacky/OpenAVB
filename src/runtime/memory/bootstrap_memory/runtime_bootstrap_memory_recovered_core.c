#include "runtime_bootstrap_memory_recovered_private.h"

#include "avp/runtime/memory/runtime_memory.h"
#include "avp/runtime/memory/runtime_page_table_recovered.h"


_Noreturn void runtime_bootstrap_memory_recovered_panic(void) {
    runtime_firebloom_panic();
}

static uint8_t *configured_byte(uint8_t *configured, uint8_t *fallback) {
    return configured != 0 ? configured : fallback;
}

RuntimeBootstrapMemoryRecoveredConfiguration
runtime_bootstrap_memory_recovered_configuration_snapshot(void) {
    RuntimeBootstrapMemoryRecoveredState *state =
        runtime_bootstrap_memory_recovered_state();
    RuntimeBootstrapMemoryRecoveredConfiguration configuration =
        state->configuration;
    if (configuration.platform_boot_base == 0) {
        configuration.platform_boot_base = UINT64_C(0x70074000);
    }
    if (configuration.fixed_root_region_id == 0) {
        configuration.fixed_root_region_id = UINT16_C(0x00a6);
    }
    if (configuration.fixed_auxiliary_region_id == 0) {
        configuration.fixed_auxiliary_region_id = UINT16_C(0x00a8);
    }
    configuration.complete_flag = configured_byte(
        configuration.complete_flag,
        &state->complete_flag
    );
    configuration.active_flag = configured_byte(
        configuration.active_flag,
        &state->active_flag
    );
    configuration.sealed_flag = configured_byte(
        configuration.sealed_flag,
        &state->sealed_flag
    );
    configuration.storage_alias_flag = configured_byte(
        configuration.storage_alias_flag,
        &state->storage_alias_flag
    );
    if (configuration.shared_storage_cursor == 0) {
        configuration.shared_storage_cursor = &state->shared_storage_cursor;
    }
    return configuration;
}

void runtime_bootstrap_memory_recovered_configure(
    const RuntimeBootstrapMemoryRecoveredConfiguration *configuration
) {
    if (configuration == 0 ||
        (configuration->shared_mapping_count != 0 &&
         configuration->shared_mappings == 0) ||
        (configuration->capability_metadata_capacity != 0 &&
         configuration->capability_metadata == 0) ||
        (configuration->fixed_descriptor_size != 0 &&
         configuration->fixed_descriptor == 0)) {
        runtime_bootstrap_memory_recovered_panic();
    }
    runtime_bootstrap_memory_recovered_state()->configuration = *configuration;
}

void runtime_bootstrap_memory_recovered_reset(void) {
    *runtime_bootstrap_memory_recovered_state() =
        (RuntimeBootstrapMemoryRecoveredState){0};
}

RuntimeFirebloomSpan runtime_bootstrap_memory_recovered_lookup_region(
    uint16_t region_id
) {
    RuntimeBootstrapMemoryRecoveredConfiguration configuration =
        runtime_bootstrap_memory_recovered_configuration_snapshot();
    if (configuration.lookup_region != 0) {
        return configuration.lookup_region(region_id, configuration.context);
    }
#if defined(__aarch64__) && defined(__ELF__)
    return runtime_physical_memory_region_lookup(region_id);
#else
    runtime_bootstrap_memory_recovered_panic();
#endif
}

bool runtime_bootstrap_memory_recovered_map(
    RuntimeBootstrapMemoryMappingRequest request
) {
    if (request.size == 0 ||
        request.virtual_address > UINTPTR_MAX - request.size ||
        request.physical_address > UINTPTR_MAX - request.size) {
        runtime_bootstrap_memory_recovered_panic();
    }
    RuntimeBootstrapMemoryRecoveredConfiguration configuration =
        runtime_bootstrap_memory_recovered_configuration_snapshot();
    if (configuration.map_range != 0) {
        return configuration.map_range(&request, configuration.context);
    }
#if defined(__aarch64__) && defined(__ELF__)
    return runtime_page_table_update_range_unowned(
        request.virtual_address,
        request.physical_address,
        request.size,
        request.regime,
        request.memory_attribute,
        request.flags
    ) == RUNTIME_PAGE_TABLE_STATUS_OK;
#else
    runtime_bootstrap_memory_recovered_panic();
#endif
}

bool runtime_bootstrap_memory_recovered_boot_owner(void) {
    RuntimeBootstrapMemoryRecoveredConfiguration configuration =
        runtime_bootstrap_memory_recovered_configuration_snapshot();
    if (configuration.is_boot_owner != 0) {
        return configuration.is_boot_owner(configuration.context);
    }
#if defined(__aarch64__) && defined(__ELF__)
    return runtime_bootstrap_memory_is_boot_owner();
#else
    runtime_bootstrap_memory_recovered_panic();
#endif
}

size_t platform_boot_memory_size(void) {
    return RUNTIME_BOOTSTRAP_MEMORY_PLATFORM_SIZE;
}

size_t j_platform_boot_memory_size(void) {
    return platform_boot_memory_size();
}

void runtime_bootstrap_memory_platform_hook_noop(void) {
}

void runtime_bootstrap_memory_publish_region_hook_noop(void) {
}

RuntimeFirebloomSpan
runtime_bootstrap_outlined_clear_u32_and_return_null_span(
    uint32_t *current,
    uint32_t *lower_bound,
    uint32_t *upper_bound
) {
    if (current == 0 || lower_bound == 0 || upper_bound == 0 ||
        current < lower_bound || current >= upper_bound) {
        runtime_bootstrap_clear_u32_bounds_panic();
    }
    *current = 0;
    return (RuntimeFirebloomSpan){0};
}

_Noreturn void runtime_bootstrap_clear_u32_bounds_panic(void) {
    runtime_bootstrap_memory_recovered_panic();
}

static RuntimeBootstrapMemoryByteView byte_view(uint8_t *value) {
    return (RuntimeBootstrapMemoryByteView){
        .current = value,
        .lower_bound = value,
        .upper_bound = value + 1,
    };
}

RuntimeBootstrapMemoryByteView
runtime_bootstrap_complete_flag_bounded_view(void) {
    return byte_view(
        runtime_bootstrap_memory_recovered_configuration_snapshot()
            .complete_flag
    );
}

RuntimeBootstrapMemoryByteView
runtime_bootstrap_memory_active_flag_bounded_view(void) {
    return byte_view(
        runtime_bootstrap_memory_recovered_configuration_snapshot().active_flag
    );
}

RuntimeBootstrapMemoryByteView
runtime_bootstrap_memory_sealed_flag_bounded_view(void) {
    return byte_view(
        runtime_bootstrap_memory_recovered_configuration_snapshot().sealed_flag
    );
}

RuntimeBootstrapMemoryByteView
runtime_bootstrap_memory_storage_alias_flag_bounded_view(void) {
    return byte_view(
        runtime_bootstrap_memory_recovered_configuration_snapshot()
            .storage_alias_flag
    );
}

RuntimeBootstrapMemoryByteView
runtime_bootstrap_complete_flag_prepare_result_tuple(void) {
    return runtime_bootstrap_complete_flag_bounded_view();
}

RuntimeBootstrapMemoryBitmapArguments
runtime_bootstrap_memory_outlined_prepare_region_bitmap_arguments(
    uint16_t region_id
) {
    RuntimeBootstrapMemoryBitmap *bitmap =
        &runtime_bootstrap_memory_recovered_state()->published_region_bitmap;
    return (RuntimeBootstrapMemoryBitmapArguments){
        .bitmap = bitmap,
        .lower_bound = bitmap,
        .upper_bound = bitmap + 1,
        .region_id = region_id,
    };
}

_Noreturn void runtime_bootstrap_memory_bitmap_test_bounds_panic(void) {
    runtime_bootstrap_memory_recovered_panic();
}

_Noreturn void runtime_bootstrap_memory_bitmap_set_bounds_panic(void) {
    runtime_bootstrap_memory_recovered_panic();
}
