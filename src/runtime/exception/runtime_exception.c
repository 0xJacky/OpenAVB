#include "avp/runtime/task/runtime_event.h"
#include "avp/runtime/ipc/runtime_service.h"
#include "avp/runtime/ipc/runtime_svc.h"
#include "avp/runtime/task/runtime_task.h"


RuntimeTask *runtime_exception_dispatch(
    RuntimeTask *trapped_task,
    uint16_t immediate
) {
    switch (immediate) {
        case 0x20:
            return runtime_task_idle_pump_from_exception(trapped_task);
        case 0x22: {
            const uint64_t previous = trapped_task->frame.spsr_el1 & 0xc0;
            trapped_task->frame.spsr_el1 |= 0xc0;
            trapped_task->frame.x[0] = previous;
            return trapped_task;
        }
        case 0x23: {
            const uint64_t previous = trapped_task->frame.spsr_el1 & 0xc0;
            trapped_task->frame.spsr_el1 &= ~UINT64_C(0xc0);
            trapped_task->frame.x[0] = previous;
            return trapped_task;
        }
        case 0x40:
            return runtime_task_create_from_exception(trapped_task);
        case 0x41:
            return runtime_task_activate_from_exception(
                trapped_task,
                (uint16_t)trapped_task->frame.x[0],
                trapped_task->frame.x[1]
            );
        case 0x42:
            return runtime_task_schedule_from_exception(trapped_task);
        case 0x43:
            return runtime_task_wait_until_from_exception(
                trapped_task,
                trapped_task->frame.x[0]
            );
        case 0x44:
            return runtime_task_exit_from_exception(
                trapped_task,
                (int32_t)trapped_task->frame.x[0]
            );
        case 0x45:
            return runtime_task_join_from_exception(
                trapped_task,
                (uint16_t)trapped_task->frame.x[0]
            );
        case 0x50:
            return runtime_event_create_from_exception(trapped_task);
        case 0x51:
            return runtime_event_wait_from_exception(
                trapped_task,
                (uint16_t)trapped_task->frame.x[0],
                trapped_task->frame.x[1]
            );
        case 0x52:
            return runtime_event_signal_from_exception(
                trapped_task,
                (uint16_t)trapped_task->frame.x[0],
                trapped_task->frame.x[1] != 0
            );
        case 0xc2:
            return runtime_service_call_from_exception(trapped_task);
        case 0xc3:
            return runtime_service_receive_from_exception(trapped_task);
        case 0xc4:
            return runtime_service_complete_from_exception(trapped_task);
        case 0xc5:
            trapped_task->frame.x[0] = runtime_domain_resolve_locality(
                (uint8_t)trapped_task->frame.x[0]
            );
            return trapped_task;
        default:
            __builtin_trap();
    }
}
