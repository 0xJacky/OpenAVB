#pragma once

#include "avp/runtime/memory/runtime_address_space_recovered.h"

RuntimeAddressSpaceRecoveredStatus runtime_address_space_recovered_status(
    RuntimeAddressSpaceRecoveredStatus status);
_Noreturn void runtime_address_space_recovered_fatal(uint32_t line);
RuntimeAddressSpaceRecoveredMappingSlot *
runtime_address_space_recovered_slot(uint16_t mapping_handle);
RuntimeAddressSpaceRecoveredMappingSlot *
runtime_address_space_recovered_allocate_slot(void);
RuntimeAddressSpaceMapResult runtime_address_space_recovered_publish_slot(
    RuntimeAddressSpaceRecoveredMappingSlot *slot,
    uint8_t rights
);
bool runtime_address_space_recovered_range_valid(
    uintptr_t address,
    size_t size
);
