#pragma once

#include <stdint.h>


void avp_mailbox_load_pair(
    uintptr_t slot_address,
    uint64_t *low,
    uint64_t *high
);
void avp_mailbox_store_pair(
    uintptr_t slot_address,
    uint64_t low,
    uint64_t high
);
