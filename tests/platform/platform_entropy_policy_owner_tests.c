#include "avp/platform/platform_entropy_policy_owner.h"

#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>


uintptr_t __stack_chk_guard = UINT64_C(0xa171c32d5e7094b6);

static PlatformEntropy entropy;
static PlatformEntropyPolicyState policy;
static uint32_t source_word;
static jmp_buf panic_environment;
static bool panic_expected;

_Noreturn void runtime_firebloom_panic(void) {
    assert(panic_expected);
    longjmp(panic_environment, 1);
}

static uint32_t read_word(void *context) {
    assert(context == &source_word);
    return ++source_word;
}

static void reset_fixture(void) {
    memset(&entropy, 0, sizeof(entropy));
    memset(&policy, 0, sizeof(policy));
    source_word = 0;
    panic_expected = false;
    assert(platform_entropy_initialize(
        &entropy, read_word, &source_word) == PLATFORM_ENTROPY_STATUS_OK);
    platform_entropy_policy_bind((PlatformEntropyPolicyBinding){
        .entropy = &entropy,
        .policy = &policy,
    });
}

static void test_policy_derivation(void) {
    reset_fixture();
    bool primary = true;
    bool secondary = true;
    platform_entropy_derive_boot_policy_outputs(&primary, &secondary);
    assert(!primary && secondary);

    policy.production_mode = 0x100;
    platform_entropy_derive_boot_policy_outputs(&primary, &secondary);
    assert(primary && secondary);
    policy.boot_policy_flags = PLATFORM_ENTROPY_POLICY_FLAG24;
    platform_entropy_derive_boot_policy_outputs(&primary, &secondary);
    assert(!primary && secondary);
    policy.initialized = true;
    platform_entropy_derive_boot_policy_outputs(&primary, &secondary);
    assert(primary && secondary);
    policy.boot_policy_flags |= PLATFORM_ENTROPY_POLICY_FLAG31;
    platform_entropy_derive_boot_policy_outputs(&primary, &secondary);
    assert(!primary && !secondary);
}

static void test_seed_enable_and_thunks(void) {
    reset_fixture();
    for (size_t index = 0; index < sizeof(policy.seed); ++index) {
        policy.seed[index] = (uint8_t)(0x40 + index);
    }
    policy.seed_present = true;
    policy.seed_context = UINT32_C(0x13572468);
    assert(platform_entropy_seed_and_enable_primary_request() ==
        PLATFORM_ENTROPY_STATUS_OK);
    assert(entropy.state == PLATFORM_ENTROPY_STATE_ENABLED);
    assert(entropy.generation == 1);
    assert(policy.seed_context == UINT32_C(0x13572468));
    assert(memcmp(entropy.seed, policy.seed, sizeof(policy.seed)) == 0);

    reset_fixture();
    assert(platform_entropy_policy_initialize() ==
        PLATFORM_ENTROPY_STATUS_OK);
    assert(policy.initialized);
    assert(entropy.state == PLATFORM_ENTROPY_STATE_ENABLED);
    uint8_t empty[PLATFORM_ENTROPY_SEED_SIZE] = {0};
    assert(memcmp(entropy.seed, empty, sizeof(empty)) == 0);

    reset_fixture();
    uint8_t seed[PLATFORM_ENTROPY_SEED_SIZE];
    memset(seed, 0xa5, sizeof(seed));
    assert(platform_entropy_seed_primary_data_thunk(
        seed, sizeof(seed), 7) == PLATFORM_ENTROPY_STATUS_OK);
    assert(policy.seed_context == 7);
    assert(platform_entropy_enable_primary_request_thunk() ==
        PLATFORM_ENTROPY_STATUS_OK);
    assert(entropy.state == PLATFORM_ENTROPY_STATE_ENABLED);
    assert(platform_entropy_seed_primary_data_thunk(
        seed, sizeof(seed) - 1, 8) ==
        PLATFORM_ENTROPY_STATUS_INVALID_ARGUMENT);
}

static void test_semantic_outliners(void) {
    reset_fixture();
    uint64_t words[2] = {1, 2};
    PlatformEntropyQwordTuple tuple = {
        .current = (uintptr_t)words,
        .lower_bound = (uintptr_t)words,
        .upper_bound = (uintptr_t)(words + 2),
        .type = &platform_entropy_policy_qword_type,
    };
    assert(platform_entropy_outlined_validate_saved_qword_tuple_x19_x22(
        tuple).current == (uintptr_t)words);
    assert(platform_entropy_outlined_validate_saved_qword_tuple_x19_x23(
        tuple).upper_bound == (uintptr_t)(words + 2));

    uint8_t seed64[PLATFORM_ENTROPY_REQUEST_DATA_SIZE] = {0};
    RuntimeFirebloomSpan span =
        platform_entropy_outlined_prepare_stack_seed_span(seed64);
    assert(span.current == seed64);
    assert((uintptr_t)span.upper_bound - (uintptr_t)span.current == 0x40);
    uint8_t seed48[PLATFORM_ENTROPY_SEED_SIZE] = {0};
    span = platform_entropy_outlined_prepare_stack_seed_span_sp8_x20(seed48);
    assert((uintptr_t)span.upper_bound - (uintptr_t)span.current == 0x30);
    span = platform_entropy_return_empty_seed_span();
    assert(span.current == 0 && span.type == 0);

    assert(platform_entropy_outlined_load_request_status_register_to_w8() ==
        PLATFORM_ENTROPY_REQUEST_STATUS_ADDRESS);
    assert(platform_entropy_outlined_load_request_type_to_x3() ==
        &platform_entropy_primary_request);
    PlatformEntropyRequestRecord request =
        platform_entropy_outlined_copy_primary_request_identifier_to_stack();
    assert(platform_entropy_request_records_equal(
        &request, &platform_entropy_primary_request));
    PlatformEntropyWriteFallbackState fallback =
        platform_entropy_outlined_clear_write_fallback_state();
    assert(fallback.byte_offset == 0 && fallback.word_index == 0);
    assert(fallback.register_base == 0x00600090);
    PlatformEntropySavedRequest saved =
        platform_entropy_outlined_save_request_tuple_and_stack_guard(
            &request,
            __stack_chk_guard
        );
    assert(saved.stack_guard == __stack_chk_guard);
    assert(platform_entropy_request_records_equal(
        &saved.request, &platform_entropy_primary_request));
    platform_entropy_outlined_control_flag_frame_prologue();
    platform_entropy_outlined_control_flag_frame_epilogue();
    assert(platform_entropy_stack_guard_load_x8() == __stack_chk_guard);
    assert(platform_entropy_stack_guard_compare_x8(__stack_chk_guard));
    assert(!platform_entropy_stack_guard_compare_x8(__stack_chk_guard ^ 1));
    assert(platform_outlined_load_byte_type_to_x3() ==
        &platform_entropy_policy_byte_type);
}

static void test_fatal_paths(void) {
    reset_fixture();
    panic_expected = true;
    policy.initialized = true;
    if (setjmp(panic_environment) == 0) {
        (void)platform_entropy_policy_initialize();
        assert(false);
    }
    PlatformEntropyQwordTuple tuple = {0};
    if (setjmp(panic_environment) == 0) {
        (void)platform_entropy_outlined_validate_saved_qword_tuple_x19_x22(
            tuple);
        assert(false);
    }
    panic_expected = false;
}

int main(void) {
    test_policy_derivation();
    test_seed_enable_and_thunks();
    test_semantic_outliners();
    test_fatal_paths();
    platform_entropy_policy_reset();
    puts("platform entropy policy owner tests passed");
}
