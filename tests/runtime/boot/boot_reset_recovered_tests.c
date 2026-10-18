#include "avp/runtime/boot/boot_reset_recovered.h"

#include <assert.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/wait.h>
#include <unistd.h>


static uint32_t psci_version;
static uint32_t invoked_functions[4];
static unsigned invoke_count;

_Noreturn void pvpanic_trigger(void) {
    __builtin_trap();
}

static int64_t psci_invoke(
    uint32_t function_id,
    uint64_t argument0,
    uint64_t argument1,
    uint64_t argument2,
    void *context
) {
    (void)context;
    assert(argument0 == 0);
    assert(argument1 == 0);
    assert(argument2 == 0);
    invoked_functions[invoke_count++] = function_id;
    if (function_id == PSCI_VERSION_FUNCTION_ID) {
        return psci_version;
    }
    return -1;
}

static void expect_trap(void (*operation)(void)) {
    const pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        operation();
        _exit(0);
    }
    int status = 0;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status));
    assert(WTERMSIG(status) == SIGILL || WTERMSIG(status) == SIGTRAP);
}

static void unsupported_version(void) {
    psci_version = 0x00020000;
    platform_psci_require_supported_version();
}

static void reset_returns_to_fatal(void) {
    psci_version = PSCI_VERSION_1_1;
    invoke_count = 0;
    platform_psci_system_reset();
}

static void test_versions(void) {
    assert(platform_psci_version_is_supported(PSCI_VERSION_0_2));
    assert(platform_psci_version_is_supported(PSCI_VERSION_1_0));
    assert(platform_psci_version_is_supported(PSCI_VERSION_1_1));
    assert(!platform_psci_version_is_supported(0));
    assert(!platform_psci_version_is_supported(0x00020000));
    const uint32_t supported[] = {
        PSCI_VERSION_0_2,
        PSCI_VERSION_1_0,
        PSCI_VERSION_1_1,
    };
    for (size_t index = 0; index < sizeof(supported) / sizeof(supported[0]);
         ++index) {
        psci_version = supported[index];
        invoke_count = 0;
        platform_psci_require_supported_version();
        assert(invoke_count == 1);
        assert(invoked_functions[0] == PSCI_VERSION_FUNCTION_ID);
    }
    expect_trap(unsupported_version);
}

static void test_reset_transcript(void) {
    expect_trap(reset_returns_to_fatal);
    avp_reset_early_platform_hook_noop(0x100000, 0x180000);
}

int main(void) {
    platform_psci_bind_invoke(psci_invoke, 0);
    test_versions();
    test_reset_transcript();
    return 0;
}
