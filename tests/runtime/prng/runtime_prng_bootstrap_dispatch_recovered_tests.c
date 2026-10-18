#include "avp/runtime/boot/boot_policy_dispatch_recovered.h"
#include "avp/runtime/boot/runtime_bootstrap_recovered.h"
#include "avp/runtime/prng/runtime_prng_recovered.h"

#include "mbedtls/md.h"

#include <assert.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>


typedef struct {
    uint32_t published_class;
    uint64_t published[2];
    uint32_t stage_order[8];
    size_t stage_count;
    uint32_t trace[4];
    size_t trace_count;
} Transcript;

static RuntimePRNG mock_prng;
static uint64_t mock_guard = UINT64_C(0x123456789abcdef0);
static uint8_t generated_seed;
static uint32_t firmware_main_calls;
static uint32_t generate_calls;
static uint32_t reseed_calls;
static uint32_t configured_entropy_length = 32;
static uint32_t configured_reseed_interval = 77;
static _Alignas(uint32_t) uint8_t allocation[128];
static size_t allocation_size;
static uint32_t allocation_calls;
static uint32_t release_calls;

typedef struct {
    const mbedtls_md_info_t *implementation;
    uint32_t output_size;
    uint32_t block_size;
} DigestInfoPrefix;

const mbedtls_md_info_t *mbedtls_md_info_from_type(mbedtls_md_type_t type) {
    assert(type == MBEDTLS_MD_SHA384);
    return (const mbedtls_md_info_t *)(uintptr_t)0x1000;
}

PlatformEntropyStatus platform_entropy_request(
    PlatformEntropy *entropy,
    uint8_t *output,
    size_t size
) {
    assert(entropy != 0);
    for (size_t index = 0; index < size; ++index) {
        output[index] = (uint8_t)(0xa0u + index);
    }
    return PLATFORM_ENTROPY_STATUS_OK;
}

RuntimePRNGStatus runtime_prng_initialize(
    RuntimePRNG *prng,
    PlatformEntropy *entropy,
    const uint8_t *personalization,
    size_t personalization_size
) {
    assert(prng == &mock_prng);
    assert(entropy != 0);
    assert(personalization != 0);
    assert(personalization_size == 4);
    prng->entropy = entropy;
    prng->initialized = true;
    return RUNTIME_PRNG_STATUS_OK;
}

RuntimePRNGStatus runtime_prng_generate_with_additional(
    RuntimePRNG *prng,
    uint8_t *output,
    size_t size,
    const uint8_t *additional,
    size_t additional_size
) {
    assert(prng == &mock_prng);
    assert(additional == 0 || additional_size == 3);
    ++generate_calls;
    for (size_t index = 0; index < size; ++index) {
        output[index] = generated_seed++;
    }
    return RUNTIME_PRNG_STATUS_OK;
}

RuntimePRNGStatus runtime_prng_reseed(
    RuntimePRNG *prng,
    const uint8_t *additional,
    size_t additional_size
) {
    assert(prng == &mock_prng);
    assert(prng->entropy != 0);
    assert(additional == 0 || additional_size == 3);
    ++reseed_calls;
    return RUNTIME_PRNG_STATUS_OK;
}

void runtime_prng_deinitialize(RuntimePRNG *prng) {
    prng->initialized = false;
}

void mbedtls_hmac_drbg_set_entropy_len(
    mbedtls_hmac_drbg_context *context,
    size_t length
) {
    (void)context;
    assert(length == 32);
}

void mbedtls_hmac_drbg_set_reseed_interval(
    mbedtls_hmac_drbg_context *context,
    int interval
) {
    (void)context;
    assert(interval == 77);
}

int mbedtls_hmac_drbg_update(
    mbedtls_hmac_drbg_context *context,
    const unsigned char *additional,
    size_t additional_length
) {
    (void)context;
    assert(additional != 0);
    assert(additional_length == 64);
    return 0;
}

RuntimePRNGStatus runtime_prng_generate(
    RuntimePRNG *prng,
    uint8_t *output,
    size_t size
) {
    return runtime_prng_generate_with_additional(prng, output, size, 0, 0);
}

RuntimePRNG *runtime_prng_global(void) {
    return &mock_prng;
}

void runtime_prng_platform_initialize(void) {
    mock_prng.initialized = true;
}

uint64_t runtime_stack_guard(void) {
    return mock_guard;
}

void mbedtls_platform_zeroize(void *buffer, size_t size) {
    memset(buffer, 0, size);
}

void *runtime_malloc(size_t size) {
    assert(size <= sizeof(allocation));
    assert(allocation_calls == release_calls);
    memset(allocation, 0x5a, sizeof(allocation));
    allocation_size = size;
    ++allocation_calls;
    return allocation;
}

void runtime_allocator_release_address(void *pointer) {
    assert(pointer == allocation);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(allocation[index] == 0);
    }
    ++release_calls;
}

void firmware_main(void) {
    ++firmware_main_calls;
}

static uint32_t read_configuration(uint32_t key, void *context) {
    assert(context == &mock_prng);
    return key == RUNTIME_PRNG_CONFIGURATION_ENTROPY_LENGTH
        ? configured_entropy_length
        : configured_reseed_interval;
}

static void expect_invalid_byte_tuple_trap(void) {
    const pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        uint8_t byte = 0;
        RuntimePRNGRecoveredSpan span = {
            .lower = &byte,
            .cursor = &byte,
            .upper = &byte + 1,
            .type = 0,
        };
        (void)runtime_prng_validate_byte_tuple(span);
        _exit(0);
    }
    int status = 0;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status));
    assert(WTERMSIG(status) == SIGILL || WTERMSIG(status) == SIGTRAP);
}

static void capture_secret(
    uint32_t secret_class,
    uint64_t first,
    uint64_t second,
    void *context
) {
    Transcript *transcript = context;
    transcript->published_class = secret_class;
    transcript->published[0] = first;
    transcript->published[1] = second;
}

static void record_stage(void *context) {
    Transcript *transcript = context;
    transcript->stage_order[transcript->stage_count] =
        (uint32_t)(transcript->stage_count + 1);
    ++transcript->stage_count;
}

static void capture_trace(uint32_t event, uint32_t flags, void *context) {
    Transcript *transcript = context;
    assert(flags == 0);
    transcript->trace[transcript->trace_count++] = event;
}

int main(void) {
    _Alignas(uint32_t) uint8_t bytes[96] = {0};
    PlatformEntropy entropy = {0};
    RuntimePRNGRecoveredSpan workspace = {
        .lower = bytes,
        .cursor = bytes,
        .upper = bytes + sizeof(bytes),
        .type = (uintptr_t)runtime_prng_u32_type(),
    };
    runtime_prng_recovered_bind_configuration_reader(
        read_configuration,
        &mock_prng
    );
    const RuntimePRNGRecoveredConfiguration *configuration =
        runtime_prng_prepare_hmac_drbg_configuration();
    assert(configuration->entropy_length == 32);
    assert(configuration->reseed_interval == 77);
    assert(runtime_prng_read_configuration_u32(0x0b00) == 32);
    const RuntimePRNGRecoveredSpan digest =
        runtime_prng_sha384_digest_info_span();
    assert((size_t)(digest.upper - digest.lower) == 0x50);
    assert(digest.type != (uintptr_t)mbedtls_md_info_from_type(
        MBEDTLS_MD_SHA384
    ));
    const RuntimeFirebloomType *digest_type =
        (const RuntimeFirebloomType *)digest.type;
    assert(digest_type->identity == UINT64_C(0x001451f0));
    assert(digest_type->element_size == 0x50);
    const DigestInfoPrefix *digest_prefix =
        (const DigestInfoPrefix *)(const void *)digest.lower;
    assert(digest_prefix->implementation ==
        mbedtls_md_info_from_type(MBEDTLS_MD_SHA384));
    assert(digest_prefix->output_size == 48);
    assert(digest_prefix->block_size == 128);
    assert(runtime_prng_source_token() == UINT64_C(0x083eeee7ebe28907));

    runtime_prng_fill_entropy_words(&entropy, workspace, 64);
    assert(bytes[0] == 0xa0 && bytes[63] == 0xdf);
    static const uint8_t personalization[4] = {'t', 'e', 's', 't'};
    assert(runtime_prng_initialize_with_entropy_workspace(
        &mock_prng,
        &entropy,
        workspace,
        personalization,
        sizeof(personalization)
    ) == RUNTIME_PRNG_STATUS_OK);
    mock_prng.initialized = false;
    assert(runtime_prng_initialize_allocating(
        &mock_prng,
        &entropy,
        personalization,
        sizeof(personalization)
    ) == RUNTIME_PRNG_STATUS_OK);
    assert(allocation_calls == 1);
    assert(release_calls == 1);
    assert(allocation_size == 64);
    assert(generate_calls == 2);
    memset(bytes, 0, sizeof(bytes));
    workspace.type = (uintptr_t)runtime_prng_byte_type();
    generated_seed = 0;
    generate_calls = 0;
    assert(runtime_prng_generate_with_entropy_workspace(
        &mock_prng,
        &entropy,
        workspace,
        16,
        (const uint8_t *)"add",
        3
    ) == RUNTIME_PRNG_STATUS_OK);
    assert(bytes[0] == 0 && bytes[15] == 15);
    assert(generate_calls == 1);
    assert(runtime_prng_generate_with_entropy_workspace(
        &mock_prng,
        &entropy,
        workspace,
        16,
        0,
        0
    ) == RUNTIME_PRNG_STATUS_OK);
    assert(bytes[0] == 16 && bytes[15] == 31);
    assert(generate_calls == 1);
    RuntimePRNGRecoveredSpan canonical_byte =
        runtime_prng_validate_byte_tuple(workspace);
    assert(canonical_byte.type == (uintptr_t)runtime_prng_byte_type());
    workspace.type = (uintptr_t)runtime_prng_u32_type();
    RuntimePRNGRecoveredSpan canonical_u32 =
        runtime_prng_validate_u32_tuple(workspace);
    assert(canonical_u32.type == (uintptr_t)runtime_prng_u32_type());
    expect_invalid_byte_tuple_trap();

    configured_reseed_interval = 1;
    runtime_prng_recovered_bind_configuration_reader(
        read_configuration,
        &mock_prng
    );
    workspace.type = (uintptr_t)runtime_prng_byte_type();
    generate_calls = 0;
    reseed_calls = 0;
    assert(runtime_prng_generate_with_entropy_workspace(
        &mock_prng, &entropy, workspace, 48, 0, 0
    ) == RUNTIME_PRNG_STATUS_OK);
    assert(runtime_prng_generate_with_entropy_workspace(
        &mock_prng, &entropy, workspace, 1, 0, 0
    ) == RUNTIME_PRNG_STATUS_OK);
    assert(generate_calls == 2);
    assert(reseed_calls == 1);
    assert(mock_prng.entropy == &entropy);
    RuntimePRNGRecoveredTuple tuple = {1, 2, 3, 4};
    RuntimePRNGRecoveredTuple restored =
        runtime_prng_restore_saved_tuple_x22_x19(tuple);
    assert(memcmp(&restored, &tuple, sizeof(tuple)) == 0);
    RuntimePRNGRecoveredArguments arguments = {1, 2, 3, 4, 5, 6};
    RuntimePRNGRecoveredArguments cleared =
        runtime_prng_clear_additional_input_arguments(arguments);
    assert(cleared.x0 == 1 && cleared.x1 == 2);
    assert(cleared.x2 == 3 && cleared.x3 == 4);
    assert(cleared.x4 == 0 && cleared.x5 == 0);

    Transcript transcript = {0};
    const RuntimeBootstrapRecoveredHooks bootstrap_hooks = {
        .publish_secret = capture_secret,
        .initialize_prng = record_stage,
        .initialize_events = record_stage,
        .initialize_memory = record_stage,
        .register_trace = capture_trace,
        .context = &transcript,
    };
    runtime_bootstrap_recovered_bind_hooks(&bootstrap_hooks);
    runtime_bootstrap_initialize_prng_trace_and_events();
    assert(transcript.stage_count == 2);
    assert(transcript.trace_count == 2);
    assert(transcript.trace[0] == RUNTIME_BOOTSTRAP_TRACE_PRNG_BEGIN);
    assert(transcript.trace[1] == RUNTIME_BOOTSTRAP_TRACE_PRNG_END);

    RuntimeBootstrapSecretFrame frame;
    runtime_bootstrap_secret_stack_initialize(&frame);
    RuntimePRNGRecoveredSpan secret_span =
        runtime_bootstrap_outlined_prepare_stack_secret_span(&frame);
    assert(secret_span.lower == frame.secret);
    assert(runtime_bootstrap_outlined_compute_stack_secret_length_x1(
        secret_span.lower,
        secret_span.upper
    ) == RUNTIME_BOOTSTRAP_SECRET_SIZE);
    assert(runtime_bootstrap_secret_stack_guard_compare(&frame));
    ++mock_guard;
    assert(!runtime_bootstrap_secret_stack_guard_compare(&frame));
    --mock_guard;
    generated_seed = 0x10;
    assert(runtime_bootstrap_generate_and_publish_secret(8));
    assert(transcript.published_class == 8);
    assert(transcript.published[0] == UINT64_C(0x1716151413121110));
    assert(transcript.published[1] == UINT64_C(0x1f1e1d1c1b1a1918));
    mock_prng.initialized = true;
    assert(runtime_bootstrap_initialize_core_services());
    assert(transcript.stage_count == 5);
    assert(transcript.trace_count == 4);

    boot_dispatch_recovered_bind_hooks(0);
    boot_dispatch_main();
    assert(firmware_main_calls == 1);
    transcript.stage_count = 0;
    const BootDispatchRecoveredHooks dispatch_hooks = {
        .initialize_queues = record_stage,
        .preflight = record_stage,
        .initialize_core_services = record_stage,
        .initialize_firmware_services = record_stage,
        .dispatch_sources = record_stage,
        .finalize_handoff = record_stage,
        .context = &transcript,
    };
    boot_dispatch_recovered_bind_hooks(&dispatch_hooks);
    boot_dispatch_task_entry();
    assert(transcript.stage_count == 6);
    return 0;
}
