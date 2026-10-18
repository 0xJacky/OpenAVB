#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CORECRYPTO_RUNTIME_PUBLIC __attribute__((used, retain))

typedef struct {
    uint64_t *storage;
    size_t capacity;
    size_t offset;
    void *owner;
} AVPCCWorkspace;

typedef struct {
    void *context;
    void *(*allocate)(void *context, size_t size);
    void (*release)(void *context, void *allocation);
    void (*abort)(void *context, const char *message);
} AVPCCRuntimePlatform;

void cc_runtime_bind_platform(const AVPCCRuntimePlatform *platform);
CORECRYPTO_RUNTIME_PUBLIC void *cc_malloc_clear(size_t byte_count);
CORECRYPTO_RUNTIME_PUBLIC uint64_t *cc_ws_alloc(
    AVPCCWorkspace *workspace,
    size_t count
);
CORECRYPTO_RUNTIME_PUBLIC void cc_ws_free(AVPCCWorkspace *workspace);
CORECRYPTO_RUNTIME_PUBLIC _Noreturn void cc_try_abort(const char *message);
CORECRYPTO_RUNTIME_PUBLIC void cc_try_abort_if(
    bool condition,
    const char *message
);
CORECRYPTO_RUNTIME_PUBLIC _Noreturn void cc_try_abort_if_cold(
    bool condition,
    const char *message
);
CORECRYPTO_RUNTIME_PUBLIC void cc_clear(size_t length, void *destination);
CORECRYPTO_RUNTIME_PUBLIC void cc_fault_canary_set(
    uint8_t *output,
    const uint8_t *canary,
    size_t length,
    const uint8_t *first,
    const uint8_t *second
);
CORECRYPTO_RUNTIME_PUBLIC _Noreturn void cc_abort(const char *message);
