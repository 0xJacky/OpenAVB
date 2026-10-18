#include "avp/runtime/boot/runtime_boot_handoff_owner.h"

#include <assert.h>
#include <setjmp.h>
#include <stdio.h>

static unsigned sync_count;
static unsigned disable_count;
static unsigned architecture_count;
static unsigned entry_count;
static uint32_t observed_type;
static jmp_buf failure_jump;

static void sync_code(const void *address, size_t length, void *context) {
    assert(address == context);
    assert(length == 16);
    ++sync_count;
}

static void disable_services(uint32_t image_type, void *context) {
    (void)context;
    observed_type = image_type;
    ++disable_count;
}

static void commit_architecture(void *context) {
    (void)context;
    ++architecture_count;
}

static void failure(uint32_t source_line, void *context) {
    (void)context;
    assert(source_line != 0);
    longjmp(failure_jump, 1);
}

static void payload_entry(
    uintptr_t image_base,
    size_t image_size,
    uint32_t image_type,
    uintptr_t context
) {
    assert(image_base == 0x70000000u);
    assert(image_size == 0x4000u);
    assert(image_type == 2u);
    assert(context == 0x1234u);
    ++entry_count;
}

int main(void) {
    uint8_t trampoline[16];
    uint8_t trampoline_source[4] = {1, 2, 3, 4};
    uint8_t scrub_a[7] = {1, 2, 3, 4, 5, 6, 7};
    uint8_t scrub_b[3] = {8, 9, 10};
    RuntimeBootHandoffRange ranges[] = {
        {.begin = scrub_a, .end = scrub_a + sizeof(scrub_a)},
        {.begin = scrub_b, .end = scrub_b + sizeof(scrub_b)},
    };
    RuntimeBootHandoffPlan plan = {
        .entry = payload_entry,
        .image_base = 0x70000000u,
        .image_size = 0x4000u,
        .image_type = 2u,
        .context = 0x1234u,
        .scrub_ranges = ranges,
        .scrub_range_count = 2,
    };
    RuntimeBootHandoffTransport transport = {
        .sync_code = sync_code,
        .disable_services = disable_services,
        .commit_architecture = commit_architecture,
        .failure = failure,
        .context = trampoline,
    };

    runtime_boot_handoff_bind_transport(transport);
    runtime_boot_handoff_reset();
    assert(runtime_boot_source_token() == UINT64_C(0x08dc9d2b26e32122));
    assert(runtime_boot_handoff_install_scrub_trampoline(
        trampoline,
        sizeof(trampoline),
        trampoline_source,
        sizeof(trampoline_source)
    ));
    assert(sync_count == 1);
    for (size_t index = 0; index < sizeof(trampoline); ++index) {
        assert(trampoline[index] == (index < 4 ? index + 1 : 0));
    }

    assert(boot_finalize_services_and_enter_payload(&plan));
    assert(runtime_boot_handoff_is_committed());
    assert(disable_count == 1);
    assert(architecture_count == 1);
    assert(entry_count == 1);
    assert(observed_type == 2u);
    for (size_t index = 0; index < sizeof(scrub_a); ++index) {
        assert(scrub_a[index] == 0);
    }
    for (size_t index = 0; index < sizeof(scrub_b); ++index) {
        assert(scrub_b[index] == 0);
    }

    if (setjmp(failure_jump) == 0) {
        runtime_boot_handoff_commit_and_enter(&plan);
        assert(false);
    }
    if (setjmp(failure_jump) == 0) {
        runtime_boot_handoff_zero_range(scrub_a + 1, scrub_a);
        assert(false);
    }

    puts("runtime boot handoff owner tests passed");
    return 0;
}

