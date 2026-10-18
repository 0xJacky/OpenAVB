#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#define RUNTIME_IRQ_ARCHITECTURAL_LIMIT UINT32_C(1020)
#define RUNTIME_IRQ_VIRTUAL_TIMER_ID UINT32_C(27)
#define RUNTIME_IRQ_SPURIOUS_ID UINT32_C(1023)
#define RUNTIME_IRQ_NO_ACTIVE_ID UINT32_MAX
#define RUNTIME_IRQ_MASK_DEPTH_LIMIT UINT32_C(10000)

typedef enum RuntimeIRQGroup {
    RUNTIME_IRQ_GROUP0 = 0,
    RUNTIME_IRQ_GROUP1 = 1,
} RuntimeIRQGroup;

typedef enum RuntimeIRQHandlerResult {
    RUNTIME_IRQ_HANDLER_HANDLED = 0,
    RUNTIME_IRQ_HANDLER_RESCHEDULE = 1,
} RuntimeIRQHandlerResult;

typedef enum RuntimeIRQStatus {
    RUNTIME_IRQ_STATUS_OK = 0,
    RUNTIME_IRQ_STATUS_INVALID_ARGUMENT,
    RUNTIME_IRQ_STATUS_CAPACITY,
    RUNTIME_IRQ_STATUS_DUPLICATE,
    RUNTIME_IRQ_STATUS_NOT_FOUND,
    RUNTIME_IRQ_STATUS_INVALID_STATE,
    RUNTIME_IRQ_STATUS_FAULTED,
} RuntimeIRQStatus;

typedef enum RuntimeIRQDispatchResult {
    RUNTIME_IRQ_DISPATCH_SPURIOUS = 0,
    RUNTIME_IRQ_DISPATCH_HANDLED,
    RUNTIME_IRQ_DISPATCH_RESCHEDULE,
    RUNTIME_IRQ_DISPATCH_FATAL,
} RuntimeIRQDispatchResult;

typedef enum RuntimeIRQFatalReason {
    RUNTIME_IRQ_FATAL_NONE = 0,
    RUNTIME_IRQ_FATAL_INVALID_GROUP,
    RUNTIME_IRQ_FATAL_INVALID_ACKNOWLEDGE,
    RUNTIME_IRQ_FATAL_NESTED_DISPATCH,
    RUNTIME_IRQ_FATAL_UNREGISTERED,
    RUNTIME_IRQ_FATAL_DISABLED,
    RUNTIME_IRQ_FATAL_GROUP_MISMATCH,
    RUNTIME_IRQ_FATAL_INVALID_HANDLER_RESULT,
    RUNTIME_IRQ_FATAL_MASK_OVERFLOW,
    RUNTIME_IRQ_FATAL_MASK_UNDERFLOW,
} RuntimeIRQFatalReason;

typedef RuntimeIRQHandlerResult (*RuntimeIRQHandler)(
    uint32_t interrupt_id,
    void *context
);

typedef struct RuntimeIRQProvider {
    void *context;
    void (*initialize)(void *context);
    uint32_t (*acknowledge)(void *context, RuntimeIRQGroup group);
    void (*end)(
        void *context,
        RuntimeIRQGroup group,
        uint32_t interrupt_id
    );
    void (*set_enabled)(
        void *context,
        RuntimeIRQGroup group,
        uint32_t interrupt_id,
        bool enabled
    );
    uint64_t (*mask)(void *context);
    void (*restore)(void *context, uint64_t token);
    void (*fatal)(
        void *context,
        RuntimeIRQFatalReason reason,
        uint32_t interrupt_id
    );
} RuntimeIRQProvider;

typedef struct RuntimeIRQHandlerEntry {
    uint32_t interrupt_id;
    RuntimeIRQGroup group;
    RuntimeIRQHandler handler;
    void *context;
    bool enabled;
} RuntimeIRQHandlerEntry;

typedef struct RuntimeIRQRuntime {
    RuntimeIRQProvider provider;
    RuntimeIRQHandlerEntry *entries;
    size_t entry_capacity;
    size_t entry_count;
    uint32_t active_interrupt_id;
    uint32_t mask_depth;
    uint64_t outer_mask_token;
    RuntimeIRQFatalReason fatal_reason;
    uint32_t fatal_interrupt_id;
    RuntimeIRQGroup active_group;
    bool dispatching;
    bool faulted;
} RuntimeIRQRuntime;

struct RuntimeTask;

RuntimeIRQStatus runtime_irq_initialize(
    RuntimeIRQRuntime *runtime,
    const RuntimeIRQProvider *provider,
    RuntimeIRQHandlerEntry *entries,
    size_t entry_capacity
);
RuntimeIRQStatus runtime_irq_register(
    RuntimeIRQRuntime *runtime,
    RuntimeIRQGroup group,
    uint32_t interrupt_id,
    RuntimeIRQHandler handler,
    void *context
);
RuntimeIRQStatus runtime_irq_unregister(
    RuntimeIRQRuntime *runtime,
    uint32_t interrupt_id
);
RuntimeIRQStatus runtime_irq_enable(
    RuntimeIRQRuntime *runtime,
    uint32_t interrupt_id
);
RuntimeIRQStatus runtime_irq_disable(
    RuntimeIRQRuntime *runtime,
    uint32_t interrupt_id
);
RuntimeIRQDispatchResult runtime_irq_dispatch(
    RuntimeIRQRuntime *runtime,
    RuntimeIRQGroup group
);
RuntimeIRQStatus runtime_irq_mask_enter(RuntimeIRQRuntime *runtime);
RuntimeIRQStatus runtime_irq_mask_leave(RuntimeIRQRuntime *runtime);
bool runtime_irq_is_registered(
    const RuntimeIRQRuntime *runtime,
    uint32_t interrupt_id
);
bool runtime_irq_is_enabled(
    const RuntimeIRQRuntime *runtime,
    uint32_t interrupt_id
);

void runtime_irq_arch_controller_initialize(void);
void runtime_irq_arch_virtual_timer_initialize(void);
uint32_t runtime_irq_arch_acknowledge_group0(void);
void runtime_irq_arch_end_group0(uint32_t interrupt_id);
void runtime_irq_arch_set_virtual_timer_enabled(bool enabled);
uint64_t runtime_irq_arch_mask(void);
void runtime_irq_arch_restore(uint64_t token);
void runtime_irq_system_initialize(void);
struct RuntimeTask *runtime_irq_system_timer_fiq_from_exception(
    struct RuntimeTask *trapped_task
);
