#include "avp/runtime/boot/runtime_boot_handoff_owner.h"

#include <limits.h>

static RuntimeBootHandoffTransport handoff_transport;
static bool handoff_committed;

/*
 * Published by start.S once the .bss wipe is done: the core was entered at
 * EL2, so the payload can be handed control there. The stock t8110 firmware
 * family expects that, and its first instruction is an EL2 register access.
 */

static _Noreturn void handoff_fail(uint32_t source_line) {
    if (handoff_transport.failure != 0) {
        handoff_transport.failure(source_line, handoff_transport.context);
    }
    __builtin_trap();
}

static bool handoff_plan_is_valid(const RuntimeBootHandoffPlan *plan) {
    if (plan == 0 || plan->entry == 0 || plan->image_size == 0) {
        return false;
    }
    if (plan->image_base > UINTPTR_MAX - plan->image_size) {
        return false;
    }
    return plan->scrub_range_count == 0 || plan->scrub_ranges != 0;
}

void runtime_boot_handoff_bind_transport(RuntimeBootHandoffTransport transport) {
    handoff_transport = transport;
}

void runtime_boot_handoff_reset(void) {
    handoff_committed = false;
}

bool runtime_boot_handoff_is_committed(void) {
    return handoff_committed;
}

void runtime_boot_handoff_zero_range(uint8_t *begin, uint8_t *end) {
    if ((begin == 0) != (end == 0) || end < begin) {
        runtime_boot_handoff_zero_range_failure();
    }
    while (begin != end) {
        *begin++ = 0;
    }
}

_Noreturn void runtime_boot_handoff_zero_range_failure(void) {
    handoff_fail(0x22e3u);
}

uint64_t runtime_boot_source_token(void) {
    return UINT64_C(0x08dc9d2b26e32122);
}

bool runtime_boot_handoff_install_scrub_trampoline(
    uint8_t *destination,
    size_t destination_capacity,
    const uint8_t *trampoline,
    size_t trampoline_size
) {
    if (destination == 0 || trampoline == 0 || trampoline_size == 0 ||
        trampoline_size > destination_capacity) {
        handoff_fail(0x0190u);
    }
    for (size_t index = 0; index < trampoline_size; ++index) {
        destination[index] = trampoline[index];
    }
    for (size_t index = trampoline_size; index < destination_capacity; ++index) {
        destination[index] = 0;
    }
    if (handoff_transport.sync_code != 0) {
        handoff_transport.sync_code(
            destination,
            destination_capacity,
            handoff_transport.context
        );
    }
    return true;
}

bool runtime_boot_handoff_scrub_and_enter_trampoline(
    const RuntimeBootHandoffPlan *plan
) {
    if (!handoff_plan_is_valid(plan)) {
        handoff_fail(0x2306u);
    }
    for (size_t index = 0; index < plan->scrub_range_count; ++index) {
        runtime_boot_handoff_zero_range(
            plan->scrub_ranges[index].begin,
            plan->scrub_ranges[index].end
        );
    }
    if (handoff_transport.commit_architecture != 0) {
        handoff_transport.commit_architecture(handoff_transport.context);
    }
    plan->entry(
        plan->image_base,
        plan->image_size,
        plan->image_type,
        plan->context
    );
    return true;
}

bool runtime_boot_handoff_enter_payload(const RuntimeBootHandoffPlan *plan) {
    return runtime_boot_handoff_scrub_and_enter_trampoline(plan);
}

bool boot_finalize_services_and_enter_payload(
    const RuntimeBootHandoffPlan *plan
) {
    if (!handoff_plan_is_valid(plan)) {
        handoff_fail(0x2352u);
    }
    if (handoff_transport.disable_services != 0) {
        handoff_transport.disable_services(
            plan->image_type,
            handoff_transport.context
        );
    }
    return runtime_boot_handoff_commit_and_enter_svc(plan);
}

bool runtime_boot_handoff_commit_and_enter_svc(
    const RuntimeBootHandoffPlan *plan
) {
    return runtime_boot_handoff_commit_and_enter(plan);
}

bool runtime_boot_handoff_commit_and_enter(const RuntimeBootHandoffPlan *plan) {
    if (!handoff_plan_is_valid(plan) || handoff_committed) {
        handoff_fail(0x233bu);
    }
    handoff_committed = true;
    return runtime_boot_handoff_enter_payload(plan);
}

