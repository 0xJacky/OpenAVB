#include "avp/crypto/support/corecrypto_runtime.h"

#include "mbedtls/platform_util.h"

static AVPCCRuntimePlatform runtime_platform;
static bool runtime_platform_bound;

static _Noreturn void cc_runtime_contract_fail(void) {
    __builtin_trap();
    __builtin_unreachable();
}

void cc_runtime_bind_platform(const AVPCCRuntimePlatform *platform) {
    if (platform == 0 || platform->allocate == 0 || platform->release == 0 ||
        platform->abort == 0) {
        cc_runtime_contract_fail();
    }
    runtime_platform = *platform;
    runtime_platform_bound = true;
}

void cc_clear(size_t length, void *destination) {
    if (length != 0 && destination == 0) {
        cc_runtime_contract_fail();
    }
    mbedtls_platform_zeroize(destination, length);
}

void *cc_malloc_clear(size_t byte_count) {
    if (!runtime_platform_bound) {
        cc_runtime_contract_fail();
    }
    void *allocation = runtime_platform.allocate(
        runtime_platform.context,
        byte_count
    );
    if (allocation != 0) {
        cc_clear(byte_count, allocation);
    }
    return allocation;
}

uint64_t *cc_ws_alloc(AVPCCWorkspace *workspace, size_t count) {
    if (workspace == 0 || workspace->storage == 0 ||
        workspace->offset > workspace->capacity ||
        count > workspace->capacity - workspace->offset) {
        cc_try_abort("cc_ws_alloc");
    }
    uint64_t *allocation = workspace->storage + workspace->offset;
    workspace->offset += count;
    return allocation;
}

void cc_ws_free(AVPCCWorkspace *workspace) {
    if (workspace == 0 || workspace->offset > workspace->capacity ||
        workspace->capacity > SIZE_MAX / sizeof(*workspace->storage) ||
        !runtime_platform_bound) {
        cc_try_abort("cc_ws_free");
    }
    cc_clear(workspace->capacity * sizeof(*workspace->storage), workspace->storage);
    runtime_platform.release(runtime_platform.context, workspace->owner);
    *workspace = (AVPCCWorkspace){0};
}

_Noreturn void cc_abort(const char *message) {
    if (!runtime_platform_bound) {
        cc_runtime_contract_fail();
    }
    runtime_platform.abort(runtime_platform.context, message);
    cc_runtime_contract_fail();
}

_Noreturn void cc_try_abort(const char *message) {
    cc_abort(message);
}

void cc_try_abort_if(bool condition, const char *message) {
    if (condition) {
        cc_try_abort_if_cold(condition, message);
    }
}

_Noreturn void cc_try_abort_if_cold(bool condition, const char *message) {
    if (condition) {
        cc_abort(message);
    }
    cc_runtime_contract_fail();
}

void cc_fault_canary_set(
    uint8_t *output,
    const uint8_t *canary,
    size_t length,
    const uint8_t *first,
    const uint8_t *second
) {
    if (output == 0 || canary == 0 ||
        (length != 0 && (first == 0 || second == 0))) {
        cc_runtime_contract_fail();
    }
    for (size_t index = 0; index < 16; ++index) {
        output[index] = canary[index];
    }
    for (size_t index = 0; index < length; ++index) {
        output[index & 15] ^= first[index] ^ second[index];
    }
}
