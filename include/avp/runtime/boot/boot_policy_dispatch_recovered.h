#pragma once

#include <stdint.h>


typedef void (*BootDispatchRecoveredEntry)(void *context);

typedef struct {
    BootDispatchRecoveredEntry initialize_queues;
    BootDispatchRecoveredEntry preflight;
    BootDispatchRecoveredEntry initialize_core_services;
    BootDispatchRecoveredEntry initialize_firmware_services;
    BootDispatchRecoveredEntry dispatch_sources;
    BootDispatchRecoveredEntry finalize_handoff;
    void *context;
} BootDispatchRecoveredHooks;

#define BOOT_POLICY_DISPATCH_PUBLIC __attribute__((used, retain))

void boot_dispatch_recovered_bind_hooks(const BootDispatchRecoveredHooks *hooks);
BOOT_POLICY_DISPATCH_PUBLIC void boot_dispatch_main(void);
BOOT_POLICY_DISPATCH_PUBLIC void boot_dispatch_task_entry(void);

