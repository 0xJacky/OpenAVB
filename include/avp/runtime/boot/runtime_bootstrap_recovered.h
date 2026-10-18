#pragma once

#include "avp/runtime/prng/runtime_prng_recovered.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


enum {
    RUNTIME_BOOTSTRAP_SECRET_SIZE = 16,
    RUNTIME_BOOTSTRAP_TRACE_PRNG_BEGIN = 0x3c486570,
    RUNTIME_BOOTSTRAP_TRACE_PRNG_END = 0x3e486570,
};

typedef struct {
    uint8_t secret[RUNTIME_BOOTSTRAP_SECRET_SIZE];
    uint64_t guard;
} RuntimeBootstrapSecretFrame;

typedef void (*RuntimeBootstrapSecretPublish)(
    uint32_t secret_class,
    uint64_t first,
    uint64_t second,
    void *context
);
typedef void (*RuntimeBootstrapStageHook)(void *context);
typedef void (*RuntimeBootstrapTraceRegister)(
    uint32_t event,
    uint32_t flags,
    void *context
);

typedef struct {
    RuntimeBootstrapSecretPublish publish_secret;
    RuntimeBootstrapStageHook initialize_prng;
    RuntimeBootstrapStageHook initialize_events;
    RuntimeBootstrapStageHook initialize_memory;
    RuntimeBootstrapTraceRegister register_trace;
    void *context;
} RuntimeBootstrapRecoveredHooks;

#define RUNTIME_BOOTSTRAP_RECOVERED_PUBLIC __attribute__((used, retain))

void runtime_bootstrap_recovered_bind_hooks(
    const RuntimeBootstrapRecoveredHooks *hooks
);
RUNTIME_BOOTSTRAP_RECOVERED_PUBLIC bool
runtime_bootstrap_generate_and_publish_secret(uint32_t secret_class);
RUNTIME_BOOTSTRAP_RECOVERED_PUBLIC RuntimePRNGRecoveredSpan
runtime_bootstrap_secret_stack_span(RuntimeBootstrapSecretFrame *frame);
RUNTIME_BOOTSTRAP_RECOVERED_PUBLIC void
runtime_bootstrap_secret_stack_initialize(RuntimeBootstrapSecretFrame *frame);
RUNTIME_BOOTSTRAP_RECOVERED_PUBLIC bool
runtime_bootstrap_secret_stack_guard_compare(
    const RuntimeBootstrapSecretFrame *frame
);
RUNTIME_BOOTSTRAP_RECOVERED_PUBLIC bool
runtime_bootstrap_initialize_core_services(void);
RUNTIME_BOOTSTRAP_RECOVERED_PUBLIC void
runtime_bootstrap_initialize_prng_trace_and_events(void);
RUNTIME_BOOTSTRAP_RECOVERED_PUBLIC RuntimePRNGRecoveredSpan
runtime_bootstrap_outlined_prepare_stack_secret_span(
    RuntimeBootstrapSecretFrame *frame
);
RUNTIME_BOOTSTRAP_RECOVERED_PUBLIC size_t
runtime_bootstrap_outlined_compute_stack_secret_length_x1(
    const uint8_t *lower,
    const uint8_t *upper
);

