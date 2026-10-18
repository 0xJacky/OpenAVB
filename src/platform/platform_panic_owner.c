#include "avp/platform/platform_panic_owner.h"

#include "avp/runtime/boot/boot_progress_recovered.h"
#include "avp/runtime/boot/boot_reset_recovered.h"
#include "avp/platform/platform_dma_cache_owner.h"
#include "avp/runtime/exception/pvpanic.h"
#include "avp/runtime/support/runtime_console_recovered.h"
#include "avp/runtime/task/runtime_irq_recovered.h"
#include "avp/runtime/task/runtime_task_context_recovered.h"
#include "avp/runtime/support/runtime_time.h"


static const RuntimeFirebloomType panic_byte_type = {
    .identity = 0x00144f18,
    .element_size = 1,
    .bytewise_copyable = true,
};

#if defined(__ELF__)
#define PLATFORM_PANIC_STATE __attribute__((section(".runtime_prng.state"), used))
#else
#define PLATFORM_PANIC_STATE
#endif

static PlatformPanicBinding bound_binding PLATFORM_PANIC_STATE;
static PlatformPanicRecord panic_record PLATFORM_PANIC_STATE;

static _Noreturn void platform_panic_contract_failure(void) {
    __builtin_trap();
}

static __attribute__((minsize)) size_t
platform_panic_cstring_length(const char *text) {
    size_t length = 0;
    while (text[length] != '\0') {
        ++length;
    }
    return length;
}

static __attribute__((minsize)) RuntimeFirebloomSpan
platform_panic_cstring_span(const char *text) {
    static const char empty[] = "";
    if (text == 0) {
        text = empty;
    }
    const size_t length = platform_panic_cstring_length(text);
    return (RuntimeFirebloomSpan){
        .current = (void *)text,
        .lower_bound = (void *)text,
        .upper_bound = (void *)(text + length + 1),
        .type = &panic_byte_type,
    };
}

static __attribute__((minsize)) const char *platform_panic_span_cstring(
    const RuntimeFirebloomSpan *span
) {
    if (span == 0 || span->current == 0 || span->lower_bound == 0 ||
        span->upper_bound == 0 || span->type == 0 ||
        (uintptr_t)span->current < (uintptr_t)span->lower_bound ||
        (uintptr_t)span->current >= (uintptr_t)span->upper_bound) {
        platform_panic_contract_failure();
    }
    const char *current = span->current;
    const size_t capacity =
        (uintptr_t)span->upper_bound - (uintptr_t)span->current;
    for (size_t index = 0; index < capacity; ++index) {
        if (current[index] == '\0') {
            return current;
        }
    }
    platform_panic_contract_failure();
}

static __attribute__((minsize)) void platform_panic_append(const char *text) {
    if (panic_record.buffer == 0 || panic_record.buffer_capacity == 0) {
        platform_panic_contract_failure();
    }
    const size_t used = platform_panic_cstring_length(panic_record.buffer);
    const size_t length = platform_panic_cstring_length(text);
    if (length >= panic_record.buffer_capacity - used) {
        platform_panic_contract_failure();
    }
    for (size_t index = 0; index <= length; ++index) {
        panic_record.buffer[used + index] = text[index];
    }
}

static void default_interrupt_mask_push(void *context) {
    (void)context;
    runtime_interrupt_mask_push_svc();
}

static void default_cache_clean_all(void *context) {
    (void)context;
    platform_dma_cache_clean_all_unlocked();
}

static int default_console_write(const char *message, void *context) {
    (void)context;
    return runtime_console_write_cstring_status(
        platform_panic_cstring_span(message)
    );
}

static uint64_t default_counter_read(void *context) {
    (void)context;
    return avp_virtual_counter_read_thunk();
}

static uint32_t default_progress_read(void *context) {
    (void)context;
    return platform_progress_read_code();
}

static void default_progress_publish(
    uint32_t code,
    uint32_t detail,
    uint32_t auxiliary0,
    uint32_t auxiliary1,
    void *context
) {
    (void)context;
    platform_progress_publish_words(
        code,
        detail,
        auxiliary0,
        auxiliary1
    );
}

static void default_panic_event_raise(void *context) {
    (void)context;
    pvpanic_raise_panic_event();
}

static void default_reset(uint32_t reason, void *context) {
    (void)context;
    platform_system_reset(reason);
}

static bool default_current_task_is_bootstrap(void *context) {
    (void)context;
    RuntimeFirebloomSpan task = runtime_current_task_bounded_view();
    return runtime_current_task_is_bootstrap_sentinel(task.current);
}

static __attribute__((minsize)) PlatformPanicBinding
platform_panic_binding(void) {
    if (bound_binding.reset != 0) {
        return bound_binding;
    }
    return (PlatformPanicBinding){
        .interrupt_mask_push = default_interrupt_mask_push,
        .cache_clean_all = default_cache_clean_all,
        .console_write = default_console_write,
        .counter_read = default_counter_read,
        .progress_read = default_progress_read,
        .progress_publish = default_progress_publish,
        .panic_event_raise = default_panic_event_raise,
        .reset = default_reset,
        .current_task_is_bootstrap = default_current_task_is_bootstrap,
        .panic_buffer = (char *)(uintptr_t)0x7002a44a,
        .panic_buffer_capacity = PLATFORM_PANIC_BUFFER_SIZE,
    };
}

void platform_panic_bind(PlatformPanicBinding binding) {
    if (binding.interrupt_mask_push == 0 || binding.cache_clean_all == 0 ||
        binding.console_write == 0 || binding.counter_read == 0 ||
        binding.progress_read == 0 || binding.progress_publish == 0 ||
        binding.panic_event_raise == 0 || binding.reset == 0 ||
        binding.current_task_is_bootstrap == 0 ||
        binding.panic_buffer == 0 || binding.panic_buffer_capacity == 0) {
        platform_panic_contract_failure();
    }
    bound_binding = binding;
}

void platform_panic_reset_binding(void) {
    bound_binding = (PlatformPanicBinding){0};
    panic_record = (PlatformPanicRecord){0};
}

const PlatformPanicRecord *platform_panic_last_record(void) {
    return &panic_record;
}

int platform_panic_outlined_write_separator(
    const char *separator,
    const RuntimeFirebloomType *type_context
) {
    if (separator == 0 || type_context == 0) {
        platform_panic_contract_failure();
    }
    const PlatformPanicBinding binding = platform_panic_binding();
    return binding.console_write(separator, binding.context);
}

_Noreturn void platform_panic_reset(void) {
    PlatformPanicBinding binding = platform_panic_binding();
    binding.reset(PLATFORM_PANIC_RESET_REASON, binding.context);
    platform_panic_contract_failure();
}

static __attribute__((minsize)) void platform_panic_begin_record(
    const PlatformPanicBinding *binding,
    const char *source
) {
    panic_record = (PlatformPanicRecord){
        .buffer = binding->panic_buffer,
        .buffer_capacity = binding->panic_buffer_capacity,
    };
    panic_record.buffer[0] = '\0';
    panic_record.source = platform_panic_cstring_span(source);
}

static __attribute__((minsize)) void platform_panic_emit_record(
    const PlatformPanicBinding *binding
) {
    binding->cache_clean_all(binding->context);
    (void)platform_panic_outlined_write_separator("\n\n", &panic_byte_type);
    (void)binding->console_write(panic_record.buffer, binding->context);
    (void)platform_panic_outlined_write_separator("\n\n", &panic_byte_type);
}

_Noreturn void platform_publish_double_panic(const char *source) {
    PlatformPanicBinding binding = platform_panic_binding();
    binding.interrupt_mask_push(binding.context);
    platform_panic_begin_record(&binding, source);
    platform_panic_append("double panic in ");
    platform_panic_append(panic_record.source.current);
    platform_panic_emit_record(&binding);
    platform_panic_reset();
}

_Noreturn void platform_publish_iboot_panic(
    const char *source,
    const RuntimeFirebloomSpan *message
) {
    PlatformPanicBinding binding = platform_panic_binding();
    if (!binding.current_task_is_bootstrap(binding.context)) {
        binding.interrupt_mask_push(binding.context);
    }
    platform_panic_begin_record(&binding, source);
    const char *message_text = platform_panic_span_cstring(message);
    panic_record.message = *message;
    panic_record.caller = binding.counter_read(binding.context);
    panic_record.progress_code = binding.progress_read(binding.context);
    binding.progress_publish(
        PLATFORM_PANIC_PROGRESS_CODE,
        (uint32_t)panic_record.caller,
        panic_record.progress_code,
        0,
        binding.context
    );
    platform_panic_append("iBoot Panic: ");
    platform_panic_append(panic_record.source.current);
    platform_panic_append(": ");
    platform_panic_append(message_text);
    platform_panic_emit_record(&binding);
    binding.cache_clean_all(binding.context);
    binding.panic_event_raise(binding.context);
    platform_panic_reset();
}
