#include "avp/runtime/boot/runtime_bootstrap_recovered.h"

#include "avp/runtime/support/stack_guard.h"
#include "mbedtls/platform_util.h"


static RuntimeBootstrapRecoveredHooks recovered_hooks;
void runtime_bootstrap_recovered_bind_hooks(
    const RuntimeBootstrapRecoveredHooks *hooks
) {
    recovered_hooks = hooks == 0
        ? (RuntimeBootstrapRecoveredHooks){0}
        : *hooks;
}

RuntimePRNGRecoveredSpan runtime_bootstrap_secret_stack_span(
    RuntimeBootstrapSecretFrame *frame
) {
    return (RuntimePRNGRecoveredSpan){
        .lower = frame->secret,
        .cursor = frame->secret,
        .upper = frame->secret + sizeof(frame->secret),
        .type = (uintptr_t)runtime_prng_byte_type(),
    };
}

void runtime_bootstrap_secret_stack_initialize(
    RuntimeBootstrapSecretFrame *frame
) {
    frame->guard = runtime_stack_guard();
    mbedtls_platform_zeroize(frame->secret, sizeof(frame->secret));
}

bool runtime_bootstrap_secret_stack_guard_compare(
    const RuntimeBootstrapSecretFrame *frame
) {
    return frame->guard == runtime_stack_guard();
}

static void publish_secret(
    uint32_t secret_class,
    uint64_t first,
    uint64_t second
) {
    if (recovered_hooks.publish_secret != 0) {
        recovered_hooks.publish_secret(
            secret_class,
            first,
            second,
            recovered_hooks.context
        );
        return;
    }
#if defined(__aarch64__)
    register uint64_t x0 __asm__("x0") = secret_class;
    register uint64_t x1 __asm__("x1") = first;
    register uint64_t x2 __asm__("x2") = second;
    __asm__ volatile("svc #7" : "+r"(x0) : "r"(x1), "r"(x2) : "memory");
#else
    (void)secret_class;
    (void)first;
    (void)second;
#endif
}

bool runtime_bootstrap_generate_and_publish_secret(uint32_t secret_class) {
    if (secret_class == 0 || secret_class > 16 ||
        (secret_class & (secret_class - 1u)) != 0) {
        __builtin_trap();
    }
    RuntimeBootstrapSecretFrame frame;
    runtime_bootstrap_secret_stack_initialize(&frame);
    RuntimePRNGRecoveredSpan span = runtime_bootstrap_secret_stack_span(&frame);
    const RuntimePRNGStatus status = runtime_prng_generate(
        runtime_prng_global(),
        span.cursor,
        sizeof(frame.secret)
    );
    uint64_t first = 0;
    uint64_t second = 0;
    __builtin_memcpy(&first, frame.secret, sizeof(first));
    __builtin_memcpy(&second, frame.secret + sizeof(first), sizeof(second));
    if (status == RUNTIME_PRNG_STATUS_OK) {
        publish_secret(secret_class, first, second);
    }
    mbedtls_platform_zeroize(frame.secret, sizeof(frame.secret));
    if (!runtime_bootstrap_secret_stack_guard_compare(&frame)) {
        __builtin_trap();
    }
    return status == RUNTIME_PRNG_STATUS_OK;
}

void runtime_bootstrap_initialize_prng_trace_and_events(void) {
    if (recovered_hooks.initialize_prng != 0) {
        recovered_hooks.initialize_prng(recovered_hooks.context);
    } else {
        runtime_prng_platform_initialize();
    }
    if (recovered_hooks.register_trace != 0) {
        recovered_hooks.register_trace(
            RUNTIME_BOOTSTRAP_TRACE_PRNG_BEGIN,
            0,
            recovered_hooks.context
        );
        recovered_hooks.register_trace(
            RUNTIME_BOOTSTRAP_TRACE_PRNG_END,
            0,
            recovered_hooks.context
        );
    }
    if (recovered_hooks.initialize_events != 0) {
        recovered_hooks.initialize_events(recovered_hooks.context);
    }
}

bool runtime_bootstrap_initialize_core_services(void) {
    runtime_bootstrap_initialize_prng_trace_and_events();
    if (!runtime_prng_global()->initialized) {
        return false;
    }
    if (recovered_hooks.initialize_memory != 0) {
        recovered_hooks.initialize_memory(recovered_hooks.context);
    }
    return runtime_bootstrap_generate_and_publish_secret(1);
}

RuntimePRNGRecoveredSpan
runtime_bootstrap_outlined_prepare_stack_secret_span(
    RuntimeBootstrapSecretFrame *frame
) {
    return runtime_bootstrap_secret_stack_span(frame);
}

size_t runtime_bootstrap_outlined_compute_stack_secret_length_x1(
    const uint8_t *lower,
    const uint8_t *upper
) {
    if (lower == 0 || upper < lower) {
        __builtin_trap();
    }
    return (size_t)(upper - lower);
}
