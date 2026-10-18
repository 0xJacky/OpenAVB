#pragma once

#include <stddef.h>
#include <stdint.h>


typedef struct {
    uint8_t task_count;
    uint8_t reserved_01[3];
    uint8_t lifecycle_flags;
    uint8_t reserved_05;
    uint8_t current_domain;
    uint8_t reserved_07;
    uint16_t runtime_handle;
    uint16_t object_count;
    uint16_t capability_count;
    uint16_t reserved_0e;
} RuntimeGlobalState;

_Static_assert(sizeof(RuntimeGlobalState) == 0x10, "runtime state size");
_Static_assert(
    offsetof(RuntimeGlobalState, lifecycle_flags) == 0x04,
    "runtime lifecycle offset"
);
_Static_assert(
    offsetof(RuntimeGlobalState, current_domain) == 0x06,
    "runtime domain offset"
);
_Static_assert(
    offsetof(RuntimeGlobalState, runtime_handle) == 0x08,
    "runtime handle offset"
);
_Static_assert(
    offsetof(RuntimeGlobalState, object_count) == 0x0a,
    "runtime object count offset"
);
_Static_assert(
    offsetof(RuntimeGlobalState, capability_count) == 0x0c,
    "runtime capability count offset"
);

void runtime_state_initialize(void);
void runtime_state_publish_task_count(uint8_t task_count);
void runtime_state_publish_domain(uint8_t domain, uint16_t runtime_handle);
void runtime_state_publish_registry_counts(
    uint16_t object_count,
    uint16_t capability_count
);
volatile RuntimeGlobalState *runtime_state_get(void);
