#include "avp/runtime/memory/runtime_bootstrap_memory.h"
#include "avp/runtime/ipc/runtime_domain.h"
#include "avp/runtime/task/runtime_task.h"

#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include <sys/wait.h>
#include <unistd.h>


typedef struct {
    RuntimeMemorySpan usable_regions[3];
    uintptr_t mappings[3];
    RuntimeMemorySpan published[16];
    uint8_t published_types[16];
    uint8_t published_permissions[16];
    size_t published_count;
    size_t platform_calls;
} MemoryFixture;

static RuntimeBootstrapMemoryBitmap forbidden_page_ids;
static RuntimeBootstrapMemoryBitmap allocated_page_ids;
static RuntimeBootstrapMemoryBitmapRecord bitmap_records[2];
static RuntimeBootstrapMemoryMapEntry memory_map[2];
static uintptr_t reserved_ranges[2];
static MemoryFixture fixture;
static RuntimeBootstrapMemoryDescriptor descriptor;
static RuntimeBootstrapMemoryProfile profile;
static RuntimeBootstrapMemoryState state;
static RuntimeTask boot_task;
static RuntimeTask other_task;
static const RuntimeTask *current_task;
static uint8_t current_domain;

uint8_t runtime_current_domain(void) {
    return current_domain;
}

RuntimeTask *runtime_task_lookup_ordinal(uint8_t ordinal) {
    assert(ordinal == 0);
    return &boot_task;
}

const RuntimeTask *runtime_task_current(void) {
    return current_task;
}

static void clear_bytes(void *storage, size_t size) {
    uint8_t *bytes = storage;
    for (size_t index = 0; index < size; ++index) {
        bytes[index] = 0;
    }
}

static bool region_is_usable(void *context, RuntimeMemorySpan region) {
    MemoryFixture *memory = context;
    for (size_t index = 0; index < 3; ++index) {
        if (memory->usable_regions[index].lower_bound == region.lower_bound &&
            memory->usable_regions[index].upper_bound == region.upper_bound) {
            return true;
        }
    }
    return false;
}

static uintptr_t map_region(
    void *context,
    RuntimeBootstrapMemoryRegionKind kind,
    RuntimeMemorySpan region
) {
    MemoryFixture *memory = context;
    assert(kind <= RUNTIME_BOOTSTRAP_MEMORY_AUXILIARY_STORAGE);
    assert(region_is_usable(context, region));
    memory->mappings[kind] = region.lower_bound + UINT64_C(0x100000000);
    return memory->mappings[kind];
}

static uintptr_t initialize_platform_layout(
    void *context,
    const RuntimeBootstrapMemoryDescriptor *boot_descriptor
) {
    MemoryFixture *memory = context;
    assert(boot_descriptor == &descriptor);
    ++memory->platform_calls;
    return UINT64_C(0x70080000);
}

static void publish_available_region(
    void *context,
    RuntimeMemorySpan region,
    uint8_t memory_type,
    uint8_t permissions
) {
    MemoryFixture *memory = context;
    assert(memory->published_count < 16);
    const size_t index = memory->published_count++;
    memory->published[index] = region;
    memory->published_types[index] = memory_type;
    memory->published_permissions[index] = permissions;
}

static void initialize_fixture(void) {
    clear_bytes(&forbidden_page_ids, sizeof(forbidden_page_ids));
    clear_bytes(&allocated_page_ids, sizeof(allocated_page_ids));
    clear_bytes(bitmap_records, sizeof(bitmap_records));
    clear_bytes(memory_map, sizeof(memory_map));
    clear_bytes(&fixture, sizeof(fixture));
    clear_bytes(&descriptor, sizeof(descriptor));
    clear_bytes(&profile, sizeof(profile));
    clear_bytes(&state, sizeof(state));

    fixture.usable_regions[0] = (RuntimeMemorySpan){0x100000, 0x104000};
    fixture.usable_regions[1] = (RuntimeMemorySpan){0x200000, 0x218000};
    fixture.usable_regions[2] = (RuntimeMemorySpan){0x300000, 0x308000};
    bitmap_records[0].page_id = RUNTIME_BOOTSTRAP_MEMORY_PAGE_ID_SENTINEL;
    bitmap_records[1].page_id = 42;
    memory_map[0] = (RuntimeBootstrapMemoryMapEntry){
        .lower_page = 0x10,
        .upper_page = 0x20,
        .memory_type = 3,
        .permissions = 5,
        .flags = 1u << 1,
    };
    memory_map[1] = (RuntimeBootstrapMemoryMapEntry){
        .lower_page = 0x30,
        .upper_page = 0x40,
        .flags = 1u << 2,
    };
    descriptor = (RuntimeBootstrapMemoryDescriptor){
        .memory_map = memory_map,
        .memory_map_count = 2,
        .replica_count = 2,
        .replica_stride = 0x100000,
        .primary_storage = {0x100000, 0x4000},
        .page_table_storage = {0x200000, 0x18000},
        .bitmap_records = {
            (uintptr_t)bitmap_records,
            sizeof(bitmap_records),
        },
        .auxiliary_storage = {0x300000, 0x8000},
    };
    reserved_ranges[0] = 0x50002;
    reserved_ranges[1] = 0x70001;
    profile = (RuntimeBootstrapMemoryProfile){
        .descriptor_enabled = true,
        .image_region = {0x100000, 0x140000},
        .runtime_static_region = {0x70058000, 0x70064000},
        .reserved_ranges = reserved_ranges,
        .reserved_range_count = 2,
        .forbidden_page_ids = &forbidden_page_ids,
        .allocated_page_ids = &allocated_page_ids,
        .operations = {
            .region_is_usable = region_is_usable,
            .map_region = map_region,
            .initialize_platform_layout = initialize_platform_layout,
            .publish_available_region = publish_available_region,
            .context = &fixture,
        },
    };
}

static void expect_fatal(void (*operation)(void)) {
    const pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        operation();
        _Exit(0);
    }
    int status = 0;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status));
}

static void test_bitmap(void) {
    RuntimeBootstrapMemoryBitmap bitmap = {0};
    assert(!runtime_memory_bitmap_test(&bitmap, 0));
    assert(!runtime_memory_bitmap_test(&bitmap, 637));
    runtime_memory_bitmap_set(&bitmap, 0);
    runtime_memory_bitmap_set(&bitmap, 637);
    assert(runtime_memory_bitmap_test(&bitmap, 0));
    assert(runtime_memory_bitmap_test(&bitmap, 637));
}

static void test_subtract_reserved_ranges(void) {
    const RuntimeMemorySpan input = {0x10000, 0x30000};
    const uintptr_t reserved[] = {0x18002, 0x28001};
    RuntimeMemorySpan output[3];
    const size_t count = runtime_memory_subtract_reserved_ranges(
        &input,
        1,
        reserved,
        2,
        output,
        3
    );
    assert(count == 3);
    assert(output[0].lower_bound == 0x10000);
    assert(output[0].upper_bound == 0x18000);
    assert(output[1].lower_bound == 0x20000);
    assert(output[1].upper_bound == 0x28000);
    assert(output[2].lower_bound == 0x2c000);
    assert(output[2].upper_bound == 0x30000);
}

static void test_descriptor_initialize(void) {
    initialize_fixture();
    runtime_bootstrap_memory_initialize(
        &descriptor,
        sizeof(descriptor),
        &profile,
        &state
    );
    assert(state.initialized);
    assert(state.page_table_page_count == 6);
    assert(state.primary_storage_mapping == UINT64_C(0x100100000));
    assert(state.page_table_storage_mapping == UINT64_C(0x100200000));
    assert(state.auxiliary_storage_mapping == UINT64_C(0x100300000));
    assert(state.platform_high_watermark == UINT64_C(0x70080000));
    assert(state.image_region.lower_bound == 0x100000);
    assert(state.runtime_static_region.upper_bound == 0x70064000);
    assert(runtime_memory_bitmap_test(&state.published_page_ids, 42));
    assert(!runtime_memory_bitmap_test(
        &state.published_page_ids,
        RUNTIME_BOOTSTRAP_MEMORY_PAGE_ID_SENTINEL
    ));
    assert(fixture.platform_calls == 1);
    assert(fixture.published_count == 4);
    assert(fixture.published[0].lower_bound == 0x40000);
    assert(fixture.published[0].upper_bound == 0x50000);
    assert(fixture.published[1].lower_bound == 0x58000);
    assert(fixture.published[1].upper_bound == 0x70000);
    assert(fixture.published[2].lower_bound == 0x74000);
    assert(fixture.published[2].upper_bound == 0x80000);
    assert(fixture.published[3].lower_bound == 0x140000);
    assert(fixture.published[3].upper_bound == 0x180000);
    for (size_t index = 0; index < fixture.published_count; ++index) {
        assert(fixture.published_types[index] == 3);
        assert(fixture.published_permissions[index] == 5);
    }
}

static void test_matching_profile(void) {
    runtime_bootstrap_memory_initialize_matching();
    const RuntimeBootstrapMemoryState *matching =
        runtime_bootstrap_memory_state();
    assert(matching->initialized);
    assert(matching->runtime_static_region.lower_bound == 0x70058000);
    assert(matching->runtime_static_region.upper_bound == 0x70064000);
    assert(matching->page_table_page_count == 0);
}

static void test_boot_owner(void) {
    current_domain = AVP_RUNTIME_DOMAIN_BOOT;
    current_task = &boot_task;
    assert(runtime_bootstrap_memory_is_boot_task());
    assert(runtime_bootstrap_memory_is_boot_owner());

    current_task = &other_task;
    assert(!runtime_bootstrap_memory_is_boot_task());
    assert(!runtime_bootstrap_memory_is_boot_owner());

    current_task = &boot_task;
    current_domain = AVP_RUNTIME_DOMAIN_SIMPLE_VIRTIO;
    assert(runtime_bootstrap_memory_is_boot_task());
    assert(!runtime_bootstrap_memory_is_boot_owner());
}

static void fatal_descriptor_size(void) {
    initialize_fixture();
    runtime_bootstrap_memory_initialize(
        &descriptor,
        sizeof(descriptor) - 1,
        &profile,
        &state
    );
}

static void fatal_page_table_size(void) {
    initialize_fixture();
    descriptor.page_table_storage.size = 0x80000;
    runtime_bootstrap_memory_initialize(
        &descriptor,
        sizeof(descriptor),
        &profile,
        &state
    );
}

static void fatal_reserved_pair(void) {
    initialize_fixture();
    descriptor.reserved_pair.base = 1;
    runtime_bootstrap_memory_initialize(
        &descriptor,
        sizeof(descriptor),
        &profile,
        &state
    );
}

static void fatal_forbidden_page_id(void) {
    initialize_fixture();
    runtime_memory_bitmap_set(&forbidden_page_ids, 42);
    runtime_bootstrap_memory_initialize(
        &descriptor,
        sizeof(descriptor),
        &profile,
        &state
    );
}

static void fatal_unpaired_region(void) {
    initialize_fixture();
    descriptor.primary_storage.size = 0;
    runtime_bootstrap_memory_initialize(
        &descriptor,
        sizeof(descriptor),
        &profile,
        &state
    );
}

static void fatal_output_capacity(void) {
    const RuntimeMemorySpan input = {0x10000, 0x30000};
    RuntimeMemorySpan output[1];
    const uintptr_t reserved[] = {0x18002, 0x28001};
    (void)runtime_memory_subtract_reserved_ranges(
        &input,
        1,
        reserved,
        2,
        output,
        1
    );
}

int main(void) {
    test_bitmap();
    test_subtract_reserved_ranges();
    test_descriptor_initialize();
    test_matching_profile();
    test_boot_owner();
    expect_fatal(fatal_descriptor_size);
    expect_fatal(fatal_page_table_size);
    expect_fatal(fatal_reserved_pair);
    expect_fatal(fatal_forbidden_page_id);
    expect_fatal(fatal_unpaired_region);
    expect_fatal(fatal_output_capacity);
}
