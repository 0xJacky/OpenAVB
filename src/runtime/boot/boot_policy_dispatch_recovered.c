#include "avp/runtime/boot/boot_policy_dispatch_recovered.h"


extern void firmware_main(void);

static BootDispatchRecoveredHooks recovered_hooks;

void boot_dispatch_recovered_bind_hooks(const BootDispatchRecoveredHooks *hooks) {
    recovered_hooks = hooks == 0 ? (BootDispatchRecoveredHooks){0} : *hooks;
}

static void call_stage(BootDispatchRecoveredEntry entry) {
    if (entry != 0) {
        entry(recovered_hooks.context);
    }
}

void boot_dispatch_main(void) {
    if (recovered_hooks.initialize_queues == 0 &&
        recovered_hooks.preflight == 0 &&
        recovered_hooks.initialize_core_services == 0 &&
        recovered_hooks.initialize_firmware_services == 0 &&
        recovered_hooks.dispatch_sources == 0 &&
        recovered_hooks.finalize_handoff == 0) {
        firmware_main();
        return;
    }
    call_stage(recovered_hooks.initialize_queues);
    call_stage(recovered_hooks.preflight);
    call_stage(recovered_hooks.initialize_core_services);
    call_stage(recovered_hooks.initialize_firmware_services);
    call_stage(recovered_hooks.dispatch_sources);
    call_stage(recovered_hooks.finalize_handoff);
}

void boot_dispatch_task_entry(void) {
    boot_dispatch_main();
}

