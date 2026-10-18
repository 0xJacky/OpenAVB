#pragma once

#include <stddef.h>
#include <stdint.h>

#include "avp/runtime/memory/runtime_page_table.h"


void runtime_mmu_initialize(void);
RuntimePageTable *runtime_mmu_page_table(void);
