#include "avp/runtime/ipc/runtime_provider.h"

#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include <sys/wait.h>
#include <unistd.h>


typedef struct {
    uint64_t marker;
} TestImplementation;

static AVPRuntimeProvider death_provider_a;
static AVPRuntimeProvider death_provider_b;
static TestImplementation death_implementation;

static void expect_trap(void (*operation)(void)) {
    pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        operation();
        _exit(0);
    }
    int status = 0;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status));
}

static void register_provider(
    AVPRuntimeProvider *provider,
    TestImplementation *implementation,
    uint64_t owner,
    const char *name
) {
    runtime_provider_register(
        provider,
        owner,
        name,
        "firmware",
        "IMG4",
        implementation,
        (uintptr_t)implementation,
        (uintptr_t)implementation + sizeof(*implementation),
        UINT64_C(0xabc000) + owner
    );
}

static void duplicate_registration(void) {
    runtime_provider_registry_initialize();
    register_provider(
        &death_provider_a,
        &death_implementation,
        1,
        "virt_firmware"
    );
    register_provider(
        &death_provider_b,
        &death_implementation,
        2,
        "virt_firmware"
    );
}

static void unregister_retained(void) {
    runtime_provider_registry_initialize();
    register_provider(
        &death_provider_a,
        &death_implementation,
        1,
        "virt_firmware"
    );
    assert(runtime_provider_publish(&death_implementation) == 0);
    (void)runtime_provider_lookup("virt_firmware");
    runtime_provider_unregister(&death_provider_a);
}

static void stale_handle_resolve(void) {
    runtime_provider_registry_initialize();
    register_provider(
        &death_provider_a,
        &death_implementation,
        1,
        "virt_firmware"
    );
    assert(runtime_provider_publish(&death_implementation) == 0);
    AVPRuntimeProviderHandle handle =
        runtime_provider_lookup("virt_firmware");
    runtime_provider_release(handle);
    runtime_provider_unpublish(&death_provider_a);
    runtime_provider_unregister(&death_provider_a);
    register_provider(
        &death_provider_a,
        &death_implementation,
        1,
        "virt_firmware"
    );
    assert(runtime_provider_publish(&death_implementation) == 0);
    (void)runtime_provider_resolve(handle);
}

int main(void) {
    AVPRuntimeProvider firmware = {0};
    AVPRuntimeProvider storage = {0};
    TestImplementation firmware_implementation = {
        .marker = UINT64_C(0xf17ef17e),
    };
    TestImplementation storage_implementation = {
        .marker = UINT64_C(0x570a6e),
    };

    runtime_provider_registry_initialize();
    register_provider(
        &firmware,
        &firmware_implementation,
        1,
        "virt_firmware"
    );
    register_provider(
        &storage,
        &storage_implementation,
        3,
        "storage"
    );
    assert(runtime_provider_registered_count() == 2);
    assert(runtime_provider_published_count() == 0);

    assert(runtime_provider_publish(&firmware_implementation) == 0);
    assert(runtime_provider_publish(&storage_implementation) == 0);
    assert(runtime_provider_publish(&firmware_implementation) == 0);
    TestImplementation missing_implementation = {0};
    assert(runtime_provider_publish(&missing_implementation) == -1);
    assert(runtime_provider_published_count() == 2);

    AVPRuntimeProviderHandle handle =
        runtime_provider_lookup("virt_firmware");
    assert(handle.provider == &firmware);
    assert(runtime_provider_resolve(handle) == &firmware_implementation);
    assert(firmware.owner_domain == 1);
    assert(firmware.metadata_a[0] == 'f');
    assert(firmware.metadata_b[0] == 'I');
    assert(firmware.references == 1);
    assert(runtime_provider_retain_implementation(handle) == 0);
    assert(firmware.references == 2);

    runtime_provider_unpublish(&firmware);
    assert(firmware.state == AVP_RUNTIME_PROVIDER_STATE_RETIRING);
    AVPRuntimeProviderHandle hidden =
        runtime_provider_lookup("virt_firmware");
    assert(hidden.provider == 0);
    assert(runtime_provider_resolve(handle) == &firmware_implementation);
    runtime_provider_release(handle);
    assert(firmware.state == AVP_RUNTIME_PROVIDER_STATE_RETIRING);
    assert(firmware.references == 1);
    runtime_provider_release(handle);
    assert(firmware.state == AVP_RUNTIME_PROVIDER_STATE_REGISTERED);
    assert(firmware.references == 0);
    runtime_provider_unregister(&firmware);
    assert(firmware.state == AVP_RUNTIME_PROVIDER_STATE_REGISTERED);
    assert(runtime_provider_registered_count() == 1);

    handle = runtime_provider_lookup("storage");
    assert(handle.provider == &storage);
    runtime_provider_release(handle);
    runtime_provider_unpublish(&storage);
    runtime_provider_unregister(&storage);
    assert(runtime_provider_registered_count() == 0);

    expect_trap(duplicate_registration);
    expect_trap(unregister_retained);
    expect_trap(stale_handle_resolve);
    return 0;
}
