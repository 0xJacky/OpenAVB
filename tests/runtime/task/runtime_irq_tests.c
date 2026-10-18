#include "avp/runtime/task/runtime_irq.h"

#include <assert.h>
#include <string.h>


typedef struct TestProvider {
    uint32_t acknowledged_interrupt_id;
    RuntimeIRQGroup acknowledged_group;
    RuntimeIRQGroup ended_group;
    uint32_t ended_interrupt_id;
    RuntimeIRQGroup enabled_group;
    uint32_t enabled_interrupt_id;
    bool enabled_value;
    uint64_t mask_token;
    uint64_t restored_token;
    RuntimeIRQFatalReason fatal_reason;
    uint32_t fatal_interrupt_id;
    uint32_t initialize_count;
    uint32_t acknowledge_count;
    uint32_t end_count;
    uint32_t enable_count;
    uint32_t mask_count;
    uint32_t restore_count;
    uint32_t fatal_count;
    char trace[32];
    size_t trace_count;
} TestProvider;

typedef struct NestedHandlerContext {
    RuntimeIRQRuntime *runtime;
    RuntimeIRQStatus disable_status;
    RuntimeIRQDispatchResult nested_result;
} NestedHandlerContext;

static void trace(TestProvider *provider, char event) {
    assert(provider->trace_count + 1 < sizeof(provider->trace));
    provider->trace[provider->trace_count++] = event;
    provider->trace[provider->trace_count] = '\0';
}

static void provider_initialize(void *context) {
    TestProvider *provider = context;
    ++provider->initialize_count;
    trace(provider, 'I');
}

static uint32_t provider_acknowledge(
    void *context,
    RuntimeIRQGroup group
) {
    TestProvider *provider = context;
    ++provider->acknowledge_count;
    provider->acknowledged_group = group;
    trace(provider, 'A');
    return provider->acknowledged_interrupt_id;
}

static void provider_end(
    void *context,
    RuntimeIRQGroup group,
    uint32_t interrupt_id
) {
    TestProvider *provider = context;
    ++provider->end_count;
    provider->ended_group = group;
    provider->ended_interrupt_id = interrupt_id;
    trace(provider, 'E');
}

static void provider_set_enabled(
    void *context,
    RuntimeIRQGroup group,
    uint32_t interrupt_id,
    bool enabled
) {
    TestProvider *provider = context;
    ++provider->enable_count;
    provider->enabled_group = group;
    provider->enabled_interrupt_id = interrupt_id;
    provider->enabled_value = enabled;
    trace(provider, enabled ? '+' : '-');
}

static uint64_t provider_mask(void *context) {
    TestProvider *provider = context;
    ++provider->mask_count;
    trace(provider, 'M');
    return provider->mask_token;
}

static void provider_restore(void *context, uint64_t token) {
    TestProvider *provider = context;
    ++provider->restore_count;
    provider->restored_token = token;
    trace(provider, 'R');
}

static void provider_fatal(
    void *context,
    RuntimeIRQFatalReason reason,
    uint32_t interrupt_id
) {
    TestProvider *provider = context;
    ++provider->fatal_count;
    provider->fatal_reason = reason;
    provider->fatal_interrupt_id = interrupt_id;
    trace(provider, 'F');
}

static RuntimeIRQProvider make_provider(TestProvider *provider) {
    return (RuntimeIRQProvider){
        .context = provider,
        .initialize = provider_initialize,
        .acknowledge = provider_acknowledge,
        .end = provider_end,
        .set_enabled = provider_set_enabled,
        .mask = provider_mask,
        .restore = provider_restore,
        .fatal = provider_fatal,
    };
}

static void initialize_runtime(
    RuntimeIRQRuntime *runtime,
    RuntimeIRQHandlerEntry *entries,
    size_t capacity,
    TestProvider *provider
) {
    RuntimeIRQProvider interface = make_provider(provider);
    assert(runtime_irq_initialize(
        runtime,
        &interface,
        entries,
        capacity
    ) == RUNTIME_IRQ_STATUS_OK);
    assert(provider->initialize_count == 1);
}

static RuntimeIRQHandlerResult handled_handler(
    uint32_t interrupt_id,
    void *context
) {
    TestProvider *provider = context;
    assert(interrupt_id == 27);
    trace(provider, 'H');
    return RUNTIME_IRQ_HANDLER_HANDLED;
}

static RuntimeIRQHandlerResult reschedule_handler(
    uint32_t interrupt_id,
    void *context
) {
    TestProvider *provider = context;
    assert(interrupt_id == 27);
    trace(provider, 'S');
    return RUNTIME_IRQ_HANDLER_RESCHEDULE;
}

static RuntimeIRQHandlerResult invalid_handler(
    uint32_t interrupt_id,
    void *context
) {
    TestProvider *provider = context;
    assert(interrupt_id == 27);
    trace(provider, 'X');
    return (RuntimeIRQHandlerResult)99;
}

static RuntimeIRQHandlerResult nested_handler(
    uint32_t interrupt_id,
    void *context
) {
    NestedHandlerContext *nested = context;
    assert(interrupt_id == 27);
    nested->disable_status = runtime_irq_disable(nested->runtime, interrupt_id);
    nested->nested_result = runtime_irq_dispatch(
        nested->runtime,
        RUNTIME_IRQ_GROUP0
    );
    return RUNTIME_IRQ_HANDLER_HANDLED;
}

static void test_registry_lifecycle(void) {
    RuntimeIRQRuntime runtime;
    RuntimeIRQHandlerEntry entries[2];
    TestProvider provider = {0};
    initialize_runtime(&runtime, entries, 2, &provider);

    assert(runtime_irq_register(
        &runtime,
        RUNTIME_IRQ_GROUP0,
        RUNTIME_IRQ_VIRTUAL_TIMER_ID,
        handled_handler,
        &provider
    ) == RUNTIME_IRQ_STATUS_OK);
    assert(runtime_irq_register(
        &runtime,
        RUNTIME_IRQ_GROUP0,
        RUNTIME_IRQ_VIRTUAL_TIMER_ID,
        handled_handler,
        &provider
    ) == RUNTIME_IRQ_STATUS_DUPLICATE);
    assert(runtime_irq_register(
        &runtime,
        RUNTIME_IRQ_GROUP0,
        RUNTIME_IRQ_ARCHITECTURAL_LIMIT,
        handled_handler,
        &provider
    ) == RUNTIME_IRQ_STATUS_INVALID_ARGUMENT);
    assert(runtime_irq_register(
        &runtime,
        RUNTIME_IRQ_GROUP1,
        4,
        handled_handler,
        &provider
    ) == RUNTIME_IRQ_STATUS_OK);
    assert(runtime_irq_register(
        &runtime,
        RUNTIME_IRQ_GROUP0,
        5,
        handled_handler,
        &provider
    ) == RUNTIME_IRQ_STATUS_CAPACITY);
    assert(runtime_irq_is_registered(&runtime, 27));
    assert(!runtime_irq_is_enabled(&runtime, 27));

    assert(runtime_irq_enable(&runtime, 27) == RUNTIME_IRQ_STATUS_OK);
    assert(provider.enabled_group == RUNTIME_IRQ_GROUP0);
    assert(provider.enabled_interrupt_id == 27);
    assert(provider.enabled_value);
    assert(runtime_irq_is_enabled(&runtime, 27));
    assert(runtime_irq_enable(&runtime, 27) ==
        RUNTIME_IRQ_STATUS_INVALID_STATE);
    assert(runtime_irq_unregister(&runtime, 27) ==
        RUNTIME_IRQ_STATUS_INVALID_STATE);
    assert(runtime_irq_disable(&runtime, 27) == RUNTIME_IRQ_STATUS_OK);
    assert(!provider.enabled_value);
    assert(runtime_irq_unregister(&runtime, 27) == RUNTIME_IRQ_STATUS_OK);
    assert(!runtime_irq_is_registered(&runtime, 27));
    assert(runtime_irq_unregister(&runtime, 27) ==
        RUNTIME_IRQ_STATUS_NOT_FOUND);
    assert(runtime_irq_enable(&runtime, 99) == RUNTIME_IRQ_STATUS_NOT_FOUND);
}

static void test_dispatch_and_scheduler_boundary(void) {
    RuntimeIRQRuntime runtime;
    RuntimeIRQHandlerEntry entries[1];
    TestProvider provider = {
        .acknowledged_interrupt_id = RUNTIME_IRQ_SPURIOUS_ID,
    };
    initialize_runtime(&runtime, entries, 1, &provider);
    provider.trace_count = 0;

    assert(runtime_irq_dispatch(&runtime, RUNTIME_IRQ_GROUP0) ==
        RUNTIME_IRQ_DISPATCH_SPURIOUS);
    assert(strcmp(provider.trace, "A") == 0);
    assert(provider.end_count == 0);

    assert(runtime_irq_register(
        &runtime,
        RUNTIME_IRQ_GROUP0,
        RUNTIME_IRQ_VIRTUAL_TIMER_ID,
        handled_handler,
        &provider
    ) == RUNTIME_IRQ_STATUS_OK);
    assert(runtime_irq_enable(&runtime, 27) == RUNTIME_IRQ_STATUS_OK);
    provider.acknowledged_interrupt_id = 27;
    provider.trace_count = 0;
    provider.trace[0] = '\0';
    assert(runtime_irq_dispatch(&runtime, RUNTIME_IRQ_GROUP0) ==
        RUNTIME_IRQ_DISPATCH_HANDLED);
    assert(strcmp(provider.trace, "AHE") == 0);
    assert(provider.ended_interrupt_id == 27);
    assert(provider.ended_group == RUNTIME_IRQ_GROUP0);
    assert(runtime.active_interrupt_id == RUNTIME_IRQ_NO_ACTIVE_ID);

    assert(runtime_irq_disable(&runtime, 27) == RUNTIME_IRQ_STATUS_OK);
    assert(runtime_irq_unregister(&runtime, 27) == RUNTIME_IRQ_STATUS_OK);
    assert(runtime_irq_register(
        &runtime,
        RUNTIME_IRQ_GROUP0,
        27,
        reschedule_handler,
        &provider
    ) == RUNTIME_IRQ_STATUS_OK);
    assert(runtime_irq_enable(&runtime, 27) == RUNTIME_IRQ_STATUS_OK);
    provider.trace_count = 0;
    provider.trace[0] = '\0';
    assert(runtime_irq_dispatch(&runtime, RUNTIME_IRQ_GROUP0) ==
        RUNTIME_IRQ_DISPATCH_RESCHEDULE);
    assert(strcmp(provider.trace, "ASE") == 0);
}

static void assert_dispatch_fatal(
    RuntimeIRQRuntime *runtime,
    TestProvider *provider,
    RuntimeIRQGroup group,
    RuntimeIRQFatalReason reason,
    uint32_t interrupt_id
) {
    assert(runtime_irq_dispatch(runtime, group) ==
        RUNTIME_IRQ_DISPATCH_FATAL);
    assert(runtime->faulted);
    assert(runtime->fatal_reason == reason);
    assert(runtime->fatal_interrupt_id == interrupt_id);
    assert(provider->fatal_reason == reason);
    assert(provider->fatal_interrupt_id == interrupt_id);
    assert(provider->fatal_count == 1);
}

static void test_fatal_dispatch_contracts(void) {
    RuntimeIRQRuntime runtime;
    RuntimeIRQHandlerEntry entries[1];
    TestProvider provider = {.acknowledged_interrupt_id = 1022};
    initialize_runtime(&runtime, entries, 1, &provider);
    assert_dispatch_fatal(
        &runtime,
        &provider,
        RUNTIME_IRQ_GROUP0,
        RUNTIME_IRQ_FATAL_INVALID_ACKNOWLEDGE,
        1022
    );
    assert(runtime_irq_enable(&runtime, 27) == RUNTIME_IRQ_STATUS_FAULTED);

    memset(&provider, 0, sizeof(provider));
    provider.acknowledged_interrupt_id = 27;
    initialize_runtime(&runtime, entries, 1, &provider);
    assert_dispatch_fatal(
        &runtime,
        &provider,
        RUNTIME_IRQ_GROUP0,
        RUNTIME_IRQ_FATAL_UNREGISTERED,
        27
    );

    memset(&provider, 0, sizeof(provider));
    provider.acknowledged_interrupt_id = 27;
    initialize_runtime(&runtime, entries, 1, &provider);
    assert(runtime_irq_register(
        &runtime,
        RUNTIME_IRQ_GROUP0,
        27,
        handled_handler,
        &provider
    ) == RUNTIME_IRQ_STATUS_OK);
    assert_dispatch_fatal(
        &runtime,
        &provider,
        RUNTIME_IRQ_GROUP0,
        RUNTIME_IRQ_FATAL_DISABLED,
        27
    );

    memset(&provider, 0, sizeof(provider));
    provider.acknowledged_interrupt_id = 27;
    initialize_runtime(&runtime, entries, 1, &provider);
    assert(runtime_irq_register(
        &runtime,
        RUNTIME_IRQ_GROUP1,
        27,
        handled_handler,
        &provider
    ) == RUNTIME_IRQ_STATUS_OK);
    assert(runtime_irq_enable(&runtime, 27) == RUNTIME_IRQ_STATUS_OK);
    assert_dispatch_fatal(
        &runtime,
        &provider,
        RUNTIME_IRQ_GROUP0,
        RUNTIME_IRQ_FATAL_GROUP_MISMATCH,
        27
    );

    memset(&provider, 0, sizeof(provider));
    provider.acknowledged_interrupt_id = 27;
    initialize_runtime(&runtime, entries, 1, &provider);
    assert(runtime_irq_register(
        &runtime,
        RUNTIME_IRQ_GROUP0,
        27,
        invalid_handler,
        &provider
    ) == RUNTIME_IRQ_STATUS_OK);
    assert(runtime_irq_enable(&runtime, 27) == RUNTIME_IRQ_STATUS_OK);
    assert_dispatch_fatal(
        &runtime,
        &provider,
        RUNTIME_IRQ_GROUP0,
        RUNTIME_IRQ_FATAL_INVALID_HANDLER_RESULT,
        27
    );
    assert(provider.end_count == 0);
}

static void test_nested_dispatch_contract(void) {
    RuntimeIRQRuntime runtime;
    RuntimeIRQHandlerEntry entries[1];
    TestProvider provider = {.acknowledged_interrupt_id = 27};
    NestedHandlerContext nested = {.runtime = &runtime};
    initialize_runtime(&runtime, entries, 1, &provider);
    assert(runtime_irq_register(
        &runtime,
        RUNTIME_IRQ_GROUP0,
        27,
        nested_handler,
        &nested
    ) == RUNTIME_IRQ_STATUS_OK);
    assert(runtime_irq_enable(&runtime, 27) == RUNTIME_IRQ_STATUS_OK);

    assert(runtime_irq_dispatch(&runtime, RUNTIME_IRQ_GROUP0) ==
        RUNTIME_IRQ_DISPATCH_FATAL);
    assert(nested.disable_status == RUNTIME_IRQ_STATUS_INVALID_STATE);
    assert(nested.nested_result == RUNTIME_IRQ_DISPATCH_FATAL);
    assert(runtime.fatal_reason == RUNTIME_IRQ_FATAL_NESTED_DISPATCH);
    assert(provider.acknowledge_count == 1);
    assert(provider.end_count == 0);
}

static void test_nested_mask_state(void) {
    RuntimeIRQRuntime runtime;
    RuntimeIRQHandlerEntry entries[1];
    TestProvider provider = {.mask_token = UINT64_C(0x80)};
    initialize_runtime(&runtime, entries, 1, &provider);
    provider.trace_count = 0;

    assert(runtime_irq_mask_enter(&runtime) == RUNTIME_IRQ_STATUS_OK);
    assert(runtime_irq_mask_enter(&runtime) == RUNTIME_IRQ_STATUS_OK);
    assert(runtime.mask_depth == 2);
    assert(provider.mask_count == 1);
    assert(runtime_irq_mask_leave(&runtime) == RUNTIME_IRQ_STATUS_OK);
    assert(provider.restore_count == 0);
    assert(runtime_irq_mask_leave(&runtime) == RUNTIME_IRQ_STATUS_OK);
    assert(provider.restore_count == 1);
    assert(provider.restored_token == UINT64_C(0x80));
    assert(strcmp(provider.trace, "MR") == 0);

    assert(runtime_irq_mask_leave(&runtime) == RUNTIME_IRQ_STATUS_FAULTED);
    assert(runtime.fatal_reason == RUNTIME_IRQ_FATAL_MASK_UNDERFLOW);

    memset(&provider, 0, sizeof(provider));
    initialize_runtime(&runtime, entries, 1, &provider);
    runtime.mask_depth = RUNTIME_IRQ_MASK_DEPTH_LIMIT;
    assert(runtime_irq_mask_enter(&runtime) == RUNTIME_IRQ_STATUS_FAULTED);
    assert(runtime.fatal_reason == RUNTIME_IRQ_FATAL_MASK_OVERFLOW);
}

static void test_invalid_inputs(void) {
    RuntimeIRQRuntime runtime;
    RuntimeIRQHandlerEntry entries[1];
    TestProvider provider = {0};
    RuntimeIRQProvider interface = make_provider(&provider);
    assert(runtime_irq_initialize(0, &interface, entries, 1) ==
        RUNTIME_IRQ_STATUS_INVALID_ARGUMENT);
    interface.end = 0;
    assert(runtime_irq_initialize(&runtime, &interface, entries, 1) ==
        RUNTIME_IRQ_STATUS_INVALID_ARGUMENT);

    interface = make_provider(&provider);
    assert(runtime_irq_initialize(&runtime, &interface, entries, 1) ==
        RUNTIME_IRQ_STATUS_OK);
    assert(runtime_irq_register(
        &runtime,
        (RuntimeIRQGroup)2,
        27,
        handled_handler,
        &provider
    ) == RUNTIME_IRQ_STATUS_INVALID_ARGUMENT);
    assert(runtime_irq_dispatch(&runtime, (RuntimeIRQGroup)2) ==
        RUNTIME_IRQ_DISPATCH_FATAL);
    assert(runtime.fatal_reason == RUNTIME_IRQ_FATAL_INVALID_GROUP);
}

int main(void) {
    test_registry_lifecycle();
    test_dispatch_and_scheduler_boundary();
    test_fatal_dispatch_contracts();
    test_nested_dispatch_contract();
    test_nested_mask_state();
    test_invalid_inputs();
    return 0;
}
