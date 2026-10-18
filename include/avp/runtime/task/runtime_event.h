#pragma once

#include "avp/runtime/task/runtime_task.h"

#include <stdbool.h>
#include <stdint.h>


typedef struct RuntimeEvent {
    RuntimeTaskQueueLink waiters;
    uint8_t signaled;
    uint8_t auto_reset;
    uint8_t owner_ordinal;
    uint8_t reserved_43[5];
} RuntimeEvent;

_Static_assert(sizeof(RuntimeEvent) == 0x48, "RuntimeEvent size");
_Static_assert(offsetof(RuntimeEvent, signaled) == 0x40, "event state");
_Static_assert(offsetof(RuntimeEvent, auto_reset) == 0x41, "event mode");
_Static_assert(offsetof(RuntimeEvent, owner_ordinal) == 0x42, "event owner");

uint16_t runtime_event_initialize(
    RuntimeEvent *event,
    bool auto_reset,
    bool initially_signaled
);
void runtime_event_runtime_initialize(void);
uint16_t runtime_event_create(bool auto_reset, bool initially_signaled);
bool runtime_event_wait(uint16_t handle, uint64_t absolute_deadline);
void runtime_event_set(uint16_t handle);
void runtime_event_clear(uint16_t handle);

RuntimeTask *runtime_event_wait_from_exception(
    RuntimeTask *trapped_task,
    uint16_t handle,
    uint64_t absolute_deadline
);
RuntimeTask *runtime_event_create_from_exception(RuntimeTask *trapped_task);
RuntimeTask *runtime_event_signal_from_exception(
    RuntimeTask *trapped_task,
    uint16_t handle,
    bool signaled
);
