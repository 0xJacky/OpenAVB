#include "avp/runtime/task/runtime_irq.h"
#include "avp/runtime/support/recovered_symbol.h"


static bool runtime_irq_group_is_valid(RuntimeIRQGroup group) {
    return group == RUNTIME_IRQ_GROUP0 || group == RUNTIME_IRQ_GROUP1;
}

static RuntimeIRQHandlerEntry *runtime_irq_find(
    RuntimeIRQRuntime *runtime,
    uint32_t interrupt_id
) {
    for (size_t index = 0; index < runtime->entry_count; ++index) {
        if (runtime->entries[index].interrupt_id == interrupt_id) {
            return &runtime->entries[index];
        }
    }
    return 0;
}

static const RuntimeIRQHandlerEntry *runtime_irq_find_const(
    const RuntimeIRQRuntime *runtime,
    uint32_t interrupt_id
) {
    for (size_t index = 0; index < runtime->entry_count; ++index) {
        if (runtime->entries[index].interrupt_id == interrupt_id) {
            return &runtime->entries[index];
        }
    }
    return 0;
}

static void runtime_irq_fatal(
    RuntimeIRQRuntime *runtime,
    RuntimeIRQFatalReason reason,
    uint32_t interrupt_id
) {
    /* The fatal path is terminal and deliberately leaves an
     * acknowledged interrupt active instead of issuing a speculative EOI. */
    runtime->fatal_reason = reason;
    runtime->fatal_interrupt_id = interrupt_id;
    runtime->faulted = true;
    runtime->provider.fatal(runtime->provider.context, reason, interrupt_id);
}

static RuntimeIRQStatus runtime_irq_ready(const RuntimeIRQRuntime *runtime) {
    if (runtime == 0) {
        return RUNTIME_IRQ_STATUS_INVALID_ARGUMENT;
    }
    if (runtime->faulted) {
        return RUNTIME_IRQ_STATUS_FAULTED;
    }
    return RUNTIME_IRQ_STATUS_OK;
}

RuntimeIRQStatus runtime_irq_initialize(
    RuntimeIRQRuntime *runtime,
    const RuntimeIRQProvider *provider,
    RuntimeIRQHandlerEntry *entries,
    size_t entry_capacity
) {
    if (runtime == 0 || provider == 0 || entries == 0 ||
        entry_capacity == 0 || provider->initialize == 0 ||
        provider->acknowledge == 0 || provider->end == 0 ||
        provider->set_enabled == 0 || provider->mask == 0 ||
        provider->restore == 0 || provider->fatal == 0) {
        return RUNTIME_IRQ_STATUS_INVALID_ARGUMENT;
    }

    runtime->provider = *provider;
    runtime->entries = entries;
    runtime->entry_capacity = entry_capacity;
    runtime->entry_count = 0;
    runtime->active_interrupt_id = RUNTIME_IRQ_NO_ACTIVE_ID;
    runtime->mask_depth = 0;
    runtime->outer_mask_token = 0;
    runtime->fatal_reason = RUNTIME_IRQ_FATAL_NONE;
    runtime->fatal_interrupt_id = RUNTIME_IRQ_NO_ACTIVE_ID;
    runtime->active_group = RUNTIME_IRQ_GROUP0;
    runtime->dispatching = false;
    runtime->faulted = false;
    provider->initialize(provider->context);
    return RUNTIME_IRQ_STATUS_OK;
}

RuntimeIRQStatus runtime_irq_register(
    RuntimeIRQRuntime *runtime,
    RuntimeIRQGroup group,
    uint32_t interrupt_id,
    RuntimeIRQHandler handler,
    void *context
) {
    RuntimeIRQStatus status = runtime_irq_ready(runtime);
    if (status != RUNTIME_IRQ_STATUS_OK) {
        return status;
    }
    if (!runtime_irq_group_is_valid(group) ||
        interrupt_id >= RUNTIME_IRQ_ARCHITECTURAL_LIMIT || handler == 0) {
        return RUNTIME_IRQ_STATUS_INVALID_ARGUMENT;
    }
    if (runtime_irq_find(runtime, interrupt_id) != 0) {
        return RUNTIME_IRQ_STATUS_DUPLICATE;
    }
    if (runtime->entry_count == runtime->entry_capacity) {
        return RUNTIME_IRQ_STATUS_CAPACITY;
    }

    RuntimeIRQHandlerEntry *entry = &runtime->entries[runtime->entry_count++];
    entry->interrupt_id = interrupt_id;
    entry->group = group;
    entry->handler = handler;
    entry->context = context;
    entry->enabled = false;
    return RUNTIME_IRQ_STATUS_OK;
}

RuntimeIRQStatus runtime_irq_unregister(
    RuntimeIRQRuntime *runtime,
    uint32_t interrupt_id
) {
    RuntimeIRQStatus status = runtime_irq_ready(runtime);
    if (status != RUNTIME_IRQ_STATUS_OK) {
        return status;
    }
    RuntimeIRQHandlerEntry *entry = runtime_irq_find(runtime, interrupt_id);
    if (entry == 0) {
        return RUNTIME_IRQ_STATUS_NOT_FOUND;
    }
    if (entry->enabled || runtime->active_interrupt_id == interrupt_id) {
        return RUNTIME_IRQ_STATUS_INVALID_STATE;
    }

    const size_t index = (size_t)(entry - runtime->entries);
    for (size_t cursor = index + 1; cursor < runtime->entry_count; ++cursor) {
        runtime->entries[cursor - 1] = runtime->entries[cursor];
    }
    --runtime->entry_count;
    return RUNTIME_IRQ_STATUS_OK;
}

RuntimeIRQStatus runtime_irq_enable(
    RuntimeIRQRuntime *runtime,
    uint32_t interrupt_id
) {
    RuntimeIRQStatus status = runtime_irq_ready(runtime);
    if (status != RUNTIME_IRQ_STATUS_OK) {
        return status;
    }
    RuntimeIRQHandlerEntry *entry = runtime_irq_find(runtime, interrupt_id);
    if (entry == 0) {
        return RUNTIME_IRQ_STATUS_NOT_FOUND;
    }
    if (entry->enabled) {
        return RUNTIME_IRQ_STATUS_INVALID_STATE;
    }
    runtime->provider.set_enabled(
        runtime->provider.context,
        entry->group,
        entry->interrupt_id,
        true
    );
    entry->enabled = true;
    return RUNTIME_IRQ_STATUS_OK;
}

RuntimeIRQStatus runtime_irq_disable(
    RuntimeIRQRuntime *runtime,
    uint32_t interrupt_id
) {
    RuntimeIRQStatus status = runtime_irq_ready(runtime);
    if (status != RUNTIME_IRQ_STATUS_OK) {
        return status;
    }
    RuntimeIRQHandlerEntry *entry = runtime_irq_find(runtime, interrupt_id);
    if (entry == 0) {
        return RUNTIME_IRQ_STATUS_NOT_FOUND;
    }
    if (!entry->enabled || runtime->active_interrupt_id == interrupt_id) {
        return RUNTIME_IRQ_STATUS_INVALID_STATE;
    }
    runtime->provider.set_enabled(
        runtime->provider.context,
        entry->group,
        entry->interrupt_id,
        false
    );
    entry->enabled = false;
    return RUNTIME_IRQ_STATUS_OK;
}

RuntimeIRQDispatchResult runtime_irq_dispatch(
    RuntimeIRQRuntime *runtime,
    RuntimeIRQGroup group
) {
    if (runtime == 0) {
        return RUNTIME_IRQ_DISPATCH_FATAL;
    }
    if (runtime->faulted) {
        return RUNTIME_IRQ_DISPATCH_FATAL;
    }
    if (!runtime_irq_group_is_valid(group)) {
        runtime_irq_fatal(
            runtime,
            RUNTIME_IRQ_FATAL_INVALID_GROUP,
            RUNTIME_IRQ_NO_ACTIVE_ID
        );
        return RUNTIME_IRQ_DISPATCH_FATAL;
    }
    if (runtime->dispatching) {
        runtime_irq_fatal(
            runtime,
            RUNTIME_IRQ_FATAL_NESTED_DISPATCH,
            runtime->active_interrupt_id
        );
        return RUNTIME_IRQ_DISPATCH_FATAL;
    }

    const uint32_t interrupt_id = runtime->provider.acknowledge(
        runtime->provider.context,
        group
    );
    /* GIC INTID 1023 is spurious and has no matching EOI transaction. */
    if (interrupt_id == RUNTIME_IRQ_SPURIOUS_ID) {
        return RUNTIME_IRQ_DISPATCH_SPURIOUS;
    }
    if (interrupt_id >= RUNTIME_IRQ_ARCHITECTURAL_LIMIT) {
        runtime_irq_fatal(
            runtime,
            RUNTIME_IRQ_FATAL_INVALID_ACKNOWLEDGE,
            interrupt_id
        );
        return RUNTIME_IRQ_DISPATCH_FATAL;
    }

    RuntimeIRQHandlerEntry *entry = runtime_irq_find(runtime, interrupt_id);
    runtime->active_interrupt_id = interrupt_id;
    runtime->active_group = group;
    runtime->dispatching = true;
    if (entry == 0) {
        runtime_irq_fatal(
            runtime,
            RUNTIME_IRQ_FATAL_UNREGISTERED,
            interrupt_id
        );
        return RUNTIME_IRQ_DISPATCH_FATAL;
    }
    if (!entry->enabled) {
        runtime_irq_fatal(
            runtime,
            RUNTIME_IRQ_FATAL_DISABLED,
            interrupt_id
        );
        return RUNTIME_IRQ_DISPATCH_FATAL;
    }
    if (entry->group != group) {
        runtime_irq_fatal(
            runtime,
            RUNTIME_IRQ_FATAL_GROUP_MISMATCH,
            interrupt_id
        );
        return RUNTIME_IRQ_DISPATCH_FATAL;
    }

    const RuntimeIRQHandlerResult result = entry->handler(
        interrupt_id,
        entry->context
    );
    if (runtime->faulted) {
        return RUNTIME_IRQ_DISPATCH_FATAL;
    }
    if (result != RUNTIME_IRQ_HANDLER_HANDLED &&
        result != RUNTIME_IRQ_HANDLER_RESCHEDULE) {
        runtime_irq_fatal(
            runtime,
            RUNTIME_IRQ_FATAL_INVALID_HANDLER_RESULT,
            interrupt_id
        );
        return RUNTIME_IRQ_DISPATCH_FATAL;
    }

    runtime->provider.end(runtime->provider.context, group, interrupt_id);
    runtime->active_interrupt_id = RUNTIME_IRQ_NO_ACTIVE_ID;
    runtime->dispatching = false;
    /* Scheduling remains outside the IRQ provider, matching the expected
     * tail branch from timer dispatch into the task scheduler. */
    return result == RUNTIME_IRQ_HANDLER_RESCHEDULE
        ? RUNTIME_IRQ_DISPATCH_RESCHEDULE
        : RUNTIME_IRQ_DISPATCH_HANDLED;
}

AVP_RECOVERED_ENTRY RuntimeIRQStatus runtime_irq_mask_enter(
    RuntimeIRQRuntime *runtime
) {
    RuntimeIRQStatus status = runtime_irq_ready(runtime);
    if (status != RUNTIME_IRQ_STATUS_OK) {
        return status;
    }
    if (runtime->mask_depth == RUNTIME_IRQ_MASK_DEPTH_LIMIT) {
        runtime_irq_fatal(
            runtime,
            RUNTIME_IRQ_FATAL_MASK_OVERFLOW,
            RUNTIME_IRQ_NO_ACTIVE_ID
        );
        return RUNTIME_IRQ_STATUS_FAULTED;
    }
    if (runtime->mask_depth == 0) {
        /* Only the outer transition captures hardware state; the matching
         * final leave restores the exact saved token. */
        runtime->outer_mask_token = runtime->provider.mask(
            runtime->provider.context
        );
    }
    ++runtime->mask_depth;
    return RUNTIME_IRQ_STATUS_OK;
}

AVP_RECOVERED_ENTRY RuntimeIRQStatus runtime_irq_mask_leave(
    RuntimeIRQRuntime *runtime
) {
    RuntimeIRQStatus status = runtime_irq_ready(runtime);
    if (status != RUNTIME_IRQ_STATUS_OK) {
        return status;
    }
    if (runtime->mask_depth == 0) {
        runtime_irq_fatal(
            runtime,
            RUNTIME_IRQ_FATAL_MASK_UNDERFLOW,
            RUNTIME_IRQ_NO_ACTIVE_ID
        );
        return RUNTIME_IRQ_STATUS_FAULTED;
    }
    --runtime->mask_depth;
    if (runtime->mask_depth == 0) {
        runtime->provider.restore(
            runtime->provider.context,
            runtime->outer_mask_token
        );
    }
    return RUNTIME_IRQ_STATUS_OK;
}

bool runtime_irq_is_registered(
    const RuntimeIRQRuntime *runtime,
    uint32_t interrupt_id
) {
    return runtime != 0 && runtime_irq_find_const(runtime, interrupt_id) != 0;
}

bool runtime_irq_is_enabled(
    const RuntimeIRQRuntime *runtime,
    uint32_t interrupt_id
) {
    const RuntimeIRQHandlerEntry *entry = runtime == 0
        ? 0
        : runtime_irq_find_const(runtime, interrupt_id);
    return entry != 0 && entry->enabled;
}
