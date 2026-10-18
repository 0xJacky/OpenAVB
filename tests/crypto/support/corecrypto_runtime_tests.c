#include "avp/crypto/support/corecrypto_runtime.h"

#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

static uint8_t heap[128];
static size_t allocation_size;
static void *released;
static const char *abort_message;
static jmp_buf abort_environment;

static void *allocate_hook(void *context, size_t size) {
    assert(context == heap);
    allocation_size = size;
    memset(heap, 0xa5, sizeof(heap));
    return size <= sizeof(heap) ? heap : 0;
}

static void release_hook(void *context, void *allocation) {
    assert(context == heap);
    released = allocation;
}

static _Noreturn void abort_hook(void *context, const char *message) {
    assert(context == heap);
    abort_message = message;
    longjmp(abort_environment, 1);
}

int main(void) {
    const AVPCCRuntimePlatform platform = {
        .context = heap,
        .allocate = allocate_hook,
        .release = release_hook,
        .abort = abort_hook,
    };
    cc_runtime_bind_platform(&platform);

    uint8_t *allocation = cc_malloc_clear(32);
    assert(allocation == heap && allocation_size == 32);
    for (size_t index = 0; index < 32; ++index) {
        assert(allocation[index] == 0);
    }

    uint64_t limbs[8];
    memset(limbs, 0xa5, sizeof(limbs));
    AVPCCWorkspace workspace = {
        .storage = limbs,
        .capacity = 8,
        .owner = heap,
    };
    assert(cc_ws_alloc(&workspace, 3) == limbs);
    assert(cc_ws_alloc(&workspace, 5) == limbs + 3);
    assert(workspace.offset == 8);
    cc_ws_free(&workspace);
    assert(released == heap && workspace.storage == 0);
    for (size_t index = 0; index < 8; ++index) {
        assert(limbs[index] == 0);
    }

    uint8_t canary[16];
    uint8_t output[16];
    uint8_t first[20];
    uint8_t second[20];
    memset(canary, 0x5a, sizeof(canary));
    memset(first, 0x11, sizeof(first));
    memset(second, 0x22, sizeof(second));
    cc_fault_canary_set(output, canary, sizeof(first), first, second);
    for (size_t index = 0; index < 4; ++index) {
        assert(output[index] == (uint8_t)(0x5a ^ 0x33 ^ 0x33));
    }
    for (size_t index = 4; index < 16; ++index) {
        assert(output[index] == (uint8_t)(0x5a ^ 0x33));
    }

    cc_try_abort_if(false, "unused");
    if (setjmp(abort_environment) == 0) {
        cc_try_abort_if(true, "fatal");
        assert(false);
    }
    assert(strcmp(abort_message, "fatal") == 0);
    puts("corecrypto_runtime_tests: ok");
}
