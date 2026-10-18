#pragma once

#include "avp/runtime/memory/runtime_bootstrap_memory_recovered.h"


typedef struct {
    RuntimeBootstrapMemoryRecoveredConfiguration configuration;
    RuntimeBootstrapMemoryBitmap published_region_bitmap;
    uint8_t complete_flag;
    uint8_t active_flag;
    uint8_t sealed_flag;
    uint8_t storage_alias_flag;
    uintptr_t shared_storage_cursor;
} RuntimeBootstrapMemoryRecoveredState;

RuntimeBootstrapMemoryRecoveredState *
runtime_bootstrap_memory_recovered_state(void);
RuntimeBootstrapMemoryRecoveredConfiguration
runtime_bootstrap_memory_recovered_configuration_snapshot(void);
_Noreturn void runtime_bootstrap_memory_recovered_panic(void);
RuntimeFirebloomSpan runtime_bootstrap_memory_recovered_lookup_region(
    uint16_t region_id
);
bool runtime_bootstrap_memory_recovered_map(
    RuntimeBootstrapMemoryMappingRequest request
);
bool runtime_bootstrap_memory_recovered_boot_owner(void);
