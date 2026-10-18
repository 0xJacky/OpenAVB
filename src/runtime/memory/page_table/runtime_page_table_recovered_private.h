#pragma once

#include "avp/runtime/memory/runtime_page_table_recovered.h"

RuntimePageTableStatus
runtime_page_table_recovered_record_status(RuntimePageTableStatus status);
RuntimePageTableAttributes runtime_page_table_recovered_attributes(
    uint8_t regime,
    uint8_t memory_attribute,
    uint32_t flags
);
_Noreturn void runtime_page_table_recovered_fatal(uint32_t line);
