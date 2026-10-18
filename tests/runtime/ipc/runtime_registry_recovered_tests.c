#include "avp/runtime/ipc/runtime_registry_recovered.h"

#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include <sys/wait.h>
#include <unistd.h>


static void expect_fatal(void (*operation)(void)) {
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

static void unsupported_tag(void) {
    runtime_registry_copy_typed_value_unsupported_tag_fatal();
}

int main(void) {
    uint64_t index_roots[3] = {0};
    uint32_t type_tags[3] = {0};
    runtime_registry_recovered_bind_tables(
        index_roots,
        sizeof(index_roots),
        type_tags,
        sizeof(type_tags)
    );
    RuntimeFirebloomSpan roots = runtime_registry_index_root_table_span();
    RuntimeFirebloomSpan tags = runtime_registry_type_tag_table_span();
    assert(roots.current == index_roots && roots.upper_bound == index_roots + 3);
    assert(tags.current == type_tags && tags.upper_bound == type_tags + 3);

    assert(runtime_registry_source_token() == UINT64_C(0x0fa351f01526260a));
    RuntimeRegistryUpperBoundResult upper =
        runtime_registry_outlined_compute_table_upper_bound_checked(0x1000, 3);
    assert(upper.upper_bound == 0x1000 + 3 * 0x38);
    assert(!upper.multiplication_overflow && upper.addition_succeeded);
    upper = runtime_registry_outlined_compute_table_upper_bound_checked(
        UINTPTR_MAX - 8,
        1
    );
    assert(!upper.addition_succeeded);
    upper = runtime_registry_outlined_compute_table_upper_bound_checked(
        0,
        SIZE_MAX
    );
    assert(upper.multiplication_overflow);

    AVPRuntimeRegistryRecord records[2] = {0};
    RuntimeRegistryCursorAdvanceResult advanced =
        runtime_registry_outlined_advance_record_cursor_checked(records);
    assert(advanced.addition_succeeded && advanced.cursor == records + 1);
    assert(!runtime_registry_outlined_compare_cursor_to_table_end(
        records,
        records + 1
    ));
    assert(runtime_registry_outlined_compare_cursor_to_table_end(
        records + 1,
        records + 1
    ));

    expect_fatal(unsupported_tag);
    return 0;
}
