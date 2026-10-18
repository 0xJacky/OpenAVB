#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if __has_attribute(retain)
#define RUNTIME_BOOT_HANDOFF_PUBLIC __attribute__((used, retain))
#else
#define RUNTIME_BOOT_HANDOFF_PUBLIC __attribute__((used))
#endif

typedef struct {
    uint8_t *begin;
    uint8_t *end;
} RuntimeBootHandoffRange;

typedef void (*RuntimeBootHandoffEntry)(
    uintptr_t image_base,
    size_t image_size,
    uint32_t image_type,
    uintptr_t context
);

typedef struct {
    RuntimeBootHandoffEntry entry;
    uintptr_t image_base;
    size_t image_size;
    uint32_t image_type;
    uintptr_t context;
    const RuntimeBootHandoffRange *scrub_ranges;
    size_t scrub_range_count;
} RuntimeBootHandoffPlan;

typedef struct {
    void (*sync_code)(const void *address, size_t length, void *context);
    void (*disable_services)(uint32_t image_type, void *context);
    void (*commit_architecture)(void *context);
    void (*failure)(uint32_t source_line, void *context);
    void *context;
} RuntimeBootHandoffTransport;

void runtime_boot_handoff_bind_transport(RuntimeBootHandoffTransport transport);
void runtime_boot_handoff_reset(void);
bool runtime_boot_handoff_is_committed(void);

RUNTIME_BOOT_HANDOFF_PUBLIC void runtime_boot_handoff_zero_range(
    uint8_t *begin,
    uint8_t *end
);
RUNTIME_BOOT_HANDOFF_PUBLIC _Noreturn void
runtime_boot_handoff_zero_range_failure(void);
RUNTIME_BOOT_HANDOFF_PUBLIC uint64_t runtime_boot_source_token(void);
RUNTIME_BOOT_HANDOFF_PUBLIC bool runtime_boot_handoff_install_scrub_trampoline(
    uint8_t *destination,
    size_t destination_capacity,
    const uint8_t *trampoline,
    size_t trampoline_size
);
RUNTIME_BOOT_HANDOFF_PUBLIC bool runtime_boot_handoff_scrub_and_enter_trampoline(
    const RuntimeBootHandoffPlan *plan
);
RUNTIME_BOOT_HANDOFF_PUBLIC bool runtime_boot_handoff_enter_payload(
    const RuntimeBootHandoffPlan *plan
);
RUNTIME_BOOT_HANDOFF_PUBLIC bool boot_finalize_services_and_enter_payload(
    const RuntimeBootHandoffPlan *plan
);
RUNTIME_BOOT_HANDOFF_PUBLIC bool runtime_boot_handoff_commit_and_enter_svc(
    const RuntimeBootHandoffPlan *plan
);
RUNTIME_BOOT_HANDOFF_PUBLIC bool runtime_boot_handoff_commit_and_enter(
    const RuntimeBootHandoffPlan *plan
);

