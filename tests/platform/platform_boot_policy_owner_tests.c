#include "avp/platform/platform_boot_policy_owner.h"
#include "avp/platform/platform_boot_policy_image_metadata.h"
#include "avp/platform/platform_identity_status_nonce_owner.h"
#include "avp/platform/platform_entropy_transport_owner.h"

#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

static PlatformBootPolicyRecord record;
static uint32_t entropy_context;
static uint16_t effective_production_mode;
static uint32_t production_mode_config;
static size_t clear_count;
static uintptr_t clear_address;
static size_t clear_size;
static size_t watchdog_count;
static uint32_t hardware_words[PLATFORM_IDENTITY_HARDWARE_WORD_COUNT];
static bool hardware_words_present;
static bool status_flag3;
static uint8_t primary_request_data[PLATFORM_ENTROPY_REQUEST_DATA_SIZE];
static bool primary_request_enabled;
static uint8_t primary_handoff_data[PLATFORM_ENTROPY_HANDOFF_DATA_SIZE];
static size_t primary_handoff_count;
static jmp_buf panic_environment;
static bool panic_expected;

_Noreturn void runtime_firebloom_panic(void) {
  assert(panic_expected);
  longjmp(panic_environment, 1);
}

uint16_t platform_read_effective_production_mode(void) {
  return effective_production_mode;
}

uint32_t j_platform_read_production_mode_config_0x08(void) {
  return production_mode_config;
}

int platform_identity_set_boot_hardware_words(
    const uint32_t *current,
    const uint32_t *lower_bound,
    const uint32_t *upper_bound,
    const RuntimeFirebloomType *type_context
) {
  if (current == 0) {
    hardware_words_present = false;
    memset(hardware_words, 0, sizeof(hardware_words));
    return 0;
  }
  assert(current == lower_bound);
  assert(upper_bound - current == PLATFORM_IDENTITY_HARDWARE_WORD_COUNT);
  assert(type_context == &platform_identity_u32_type);
  memcpy(hardware_words, current, sizeof(hardware_words));
  hardware_words_present = true;
  return 0;
}

void platform_status_flag3_update(bool enabled) { status_flag3 = enabled; }

const RuntimeFirebloomType platform_identity_u32_type = {
    .identity = 0x00144f10,
    .element_size = sizeof(uint32_t),
    .bytewise_copyable = true,
};

const PlatformEntropyRequestRecord platform_entropy_primary_request = {
    .identifier_low = UINT64_C(0x2b49a37503201907),
    .identifier_high = UINT64_C(0x87f84d2f812d97a7),
    .data_size = PLATFORM_ENTROPY_REQUEST_DATA_SIZE,
    .kind = 2,
};

int platform_entropy_write_request(
    const PlatformEntropyRequestRecord *request,
    const void *input,
    size_t input_size
) {
  assert(request == &platform_entropy_primary_request);
  assert(input_size == sizeof(primary_request_data));
  memcpy(primary_request_data, input, input_size);
  return 0;
}

int platform_entropy_enable_request(
    const PlatformEntropyRequestRecord *request
) {
  assert(request == &platform_entropy_primary_request);
  primary_request_enabled = true;
  return 0;
}

void platform_entropy_materialize_primary_handoff(
    const void *input,
    size_t input_size
) {
  assert(input_size == sizeof(primary_handoff_data));
  memcpy(primary_handoff_data, input, input_size);
  ++primary_handoff_count;
}

static void clear_memory(void *context, uintptr_t address, size_t size) {
  assert(context == &record);
  ++clear_count;
  clear_address = address;
  clear_size = size;
}

static void service_watchdog(void *context) {
  assert(context == &record);
  ++watchdog_count;
}

static void reset_fixture(void) {
  memset(&record, 0, sizeof(record));
  entropy_context = 0;
  effective_production_mode = 1;
  production_mode_config = 1;
  clear_count = 0;
  clear_address = 0;
  clear_size = 0;
  watchdog_count = 0;
  memset(hardware_words, 0, sizeof(hardware_words));
  hardware_words_present = false;
  status_flag3 = false;
  memset(primary_request_data, 0, sizeof(primary_request_data));
  primary_request_enabled = false;
  memset(primary_handoff_data, 0, sizeof(primary_handoff_data));
  primary_handoff_count = 0;
  panic_expected = false;
  platform_boot_policy_bind_transport((PlatformBootPolicyTransport){
      .record = &record,
      .entropy_context = &entropy_context,
      .clear_memory = clear_memory,
      .service_watchdog = service_watchdog,
      .context = &record,
  });
}

static void test_platform_constants_and_hooks(void) {
  reset_fixture();
  platform_watchdog_service_noop();
  assert(!platform_boot_source_attempt_completion_progress_is_enabled());
  assert(!j_platform_boot_source_attempt_completion_progress_is_enabled());
  assert(platform_boot_normal_image_type_illb() ==
         PLATFORM_BOOT_POLICY_IMAGE_TYPE_ILLB);
  assert(platform_boot_policy_flag27_default_is_set());
  assert(!platform_memory_policy_range_is_exempt(0, UINT64_MAX));
}

static void test_policy_reset_profiles_and_clear_transport(void) {
  reset_fixture();
  effective_production_mode = 1;
  production_mode_config = 1;
  record.auxiliary_state = 0xff;
  assert(platform_boot_policy_reset(false) == 0);
  assert(record.flags ==
         (UINT64_C(0x222c0000) | PLATFORM_BOOT_POLICY_FLAG4 |
          PLATFORM_BOOT_POLICY_FLAG5 | PLATFORM_BOOT_POLICY_FLAG20 |
          PLATFORM_BOOT_POLICY_FLAG27));
  assert(record.auxiliary_state == 0);
  assert(record.memory_lower_bound == PLATFORM_BOOT_POLICY_BOOT_MEMORY_BASE);
  assert(record.memory_upper_bound ==
         PLATFORM_BOOT_POLICY_BOOT_MEMORY_BASE +
             PLATFORM_BOOT_POLICY_BOOT_MEMORY_SIZE);
  assert(clear_count == 0);
  assert(watchdog_count == 0);

  effective_production_mode = 0x101;
  production_mode_config = 0x101;
  assert(platform_boot_policy_reset(true) == 0);
  assert(record.flags == (UINT64_C(0x222c0000) | PLATFORM_BOOT_POLICY_FLAG28 |
                          PLATFORM_BOOT_POLICY_FLAG27));
  assert(clear_count == 1);
  assert(clear_address == PLATFORM_BOOT_POLICY_BOOT_MEMORY_BASE);
  assert(clear_size == PLATFORM_BOOT_POLICY_BOOT_MEMORY_SIZE);
  assert(watchdog_count == 1);
}

static void test_bounds_tracking_and_policy(void) {
  reset_fixture();
  platform_boot_policy_memory_bounds_update(0, 0, 0, 0, 0, true);
  assert(record.memory_lower_bound == UINT64_MAX);
  assert(record.memory_upper_bound == 0);

  platform_boot_policy_memory_bounds_update(
      (const void *)(uintptr_t)0x2000,
      0,
      0,
      0,
      0x100,
      false
  );
  platform_boot_policy_memory_bounds_update(
      (const void *)(uintptr_t)0x1800,
      0,
      0,
      0,
      0x80,
      false
  );
  platform_boot_policy_memory_bounds_update(
      (const void *)(uintptr_t)0x2080,
      0,
      0,
      0,
      0x200,
      false
  );
  assert(record.memory_lower_bound == 0x1800);
  assert(record.memory_upper_bound == 0x2280);
  assert(platform_boot_policy_memory_span_allowed(0x1800, 0, 0, 0, 0x480));
  assert(j_platform_boot_policy_memory_span_allowed(0x2000, 0, 0, 0, 0x280));
  assert(!platform_boot_policy_memory_span_allowed(0x17ff, 0, 0, 0, 1));
  assert(!platform_boot_policy_memory_span_allowed(0x2200, 0, 0, 0, 0x81));
  assert(!platform_boot_policy_memory_span_allowed(UINT64_MAX - 3, 0, 0, 0, 8));

  const uint64_t prior_lower = record.memory_lower_bound;
  const uint64_t prior_upper = record.memory_upper_bound;
  record.flags |= PLATFORM_BOOT_POLICY_FLAG16;
  platform_boot_policy_memory_bounds_update(
      (const void *)(uintptr_t)0x10,
      0,
      0,
      0,
      1,
      false
  );
  assert(record.memory_lower_bound == prior_lower);
  assert(record.memory_upper_bound == prior_upper);
  assert(
      platform_boot_policy_memory_span_allowed(UINT64_MAX, 0, 0, 0, SIZE_MAX));
}

static void test_entropy_identity_publication(void) {
  reset_fixture();
  assert(platform_boot_policy_reset(false) == 0);
  platform_boot_policy_apply_entropy_identity_state();
  assert(!hardware_words_present);
  assert(status_flag3);

  for (size_t index = 0; index < sizeof(record.entropy_seed); ++index) {
    record.entropy_seed[index] = (uint8_t)(0x30 + index);
  }
  record.flags |= PLATFORM_BOOT_POLICY_FLAG26;
  record.flags &= ~PLATFORM_BOOT_POLICY_FLAG25;
  entropy_context = UINT32_C(0x696c6c62);
  platform_boot_policy_apply_entropy_identity_state();
  assert(hardware_words_present);
  assert(memcmp(hardware_words, record.entropy_seed,
                sizeof(record.entropy_seed)) == 0);
  assert(!status_flag3);

  assert(platform_boot_policy_publish_entropy_request() == 0);
  assert(primary_request_enabled);
  assert(primary_handoff_count == 1);
  assert(memcmp(primary_handoff_data, record.entropy_seed,
                sizeof(primary_handoff_data)) == 0);
  assert(memcmp(primary_request_data, record.entropy_seed,
                sizeof(record.entropy_seed)) == 0);
  assert(primary_request_data[48] == 0x62);
  assert(primary_request_data[49] == 0x6c);
  assert(primary_request_data[50] == 0x6c);
  assert(primary_request_data[51] == 0x69);
  for (size_t index = sizeof(record.entropy_seed) + sizeof(entropy_context);
       index < sizeof(primary_request_data); ++index) {
    assert(primary_request_data[index] == 0);
  }
}

static void test_bounds_overflow_is_observable(void) {
  reset_fixture();
  panic_expected = true;
  if (setjmp(panic_environment) == 0) {
    platform_boot_policy_memory_bounds_update(
        (const void *)(uintptr_t)(UINTPTR_MAX - 3),
        0,
        0,
        0,
        8,
        false
    );
    assert(false);
  }
  panic_expected = false;
}

static void test_flag_profile_and_accessors(void) {
  reset_fixture();
  record.flags = PLATFORM_BOOT_POLICY_FLAG4 | PLATFORM_BOOT_POLICY_FLAG18 |
                 PLATFORM_BOOT_POLICY_FLAG19 | PLATFORM_BOOT_POLICY_FLAG22 |
                 PLATFORM_BOOT_POLICY_FLAG24 | PLATFORM_BOOT_POLICY_FLAG29 |
                 PLATFORM_BOOT_POLICY_FLAG40;
  assert(platform_boot_policy_flags_accept_and_promote_request(
      PLATFORM_BOOT_POLICY_FLAG4 | PLATFORM_BOOT_POLICY_FLAG24));
  assert((record.flags & PLATFORM_BOOT_POLICY_FLAG4) != 0);
  assert((record.flags & PLATFORM_BOOT_POLICY_FLAG5) != 0);
  assert((record.flags & PLATFORM_BOOT_POLICY_FLAG24) != 0);
  assert((record.flags &
          (PLATFORM_BOOT_POLICY_FLAG18 | PLATFORM_BOOT_POLICY_FLAG19 |
           PLATFORM_BOOT_POLICY_FLAG29)) == 0);
  assert(platform_boot_policy_flag22_is_set());
  assert(!platform_boot_policy_flag40_is_clear());

  record.flags = PLATFORM_BOOT_POLICY_FLAG4 | PLATFORM_BOOT_POLICY_FLAG18;
  assert(!platform_boot_policy_flags_accept_and_promote_request(
      PLATFORM_BOOT_POLICY_FLAG4 | PLATFORM_BOOT_POLICY_FLAG18));
  assert((record.flags & PLATFORM_BOOT_POLICY_FLAG18) == 0);

  record.flags = PLATFORM_BOOT_POLICY_FLAG18 | PLATFORM_BOOT_POLICY_FLAG19 |
                 PLATFORM_BOOT_POLICY_FLAG29;
  platform_boot_policy_select_flag5_profile();
  assert(record.flags == PLATFORM_BOOT_POLICY_FLAG5);

  platform_boot_policy_flag24_set(true);
  platform_boot_policy_flag25_set(true);
  platform_boot_policy_flag27_set();
  platform_boot_policy_flag30_set();
  platform_boot_policy_flag42_set();
  assert((record.flags &
          (PLATFORM_BOOT_POLICY_FLAG24 | PLATFORM_BOOT_POLICY_FLAG25 |
           PLATFORM_BOOT_POLICY_FLAG27 | PLATFORM_BOOT_POLICY_FLAG30 |
           PLATFORM_BOOT_POLICY_FLAG42)) ==
         (PLATFORM_BOOT_POLICY_FLAG24 | PLATFORM_BOOT_POLICY_FLAG25 |
          PLATFORM_BOOT_POLICY_FLAG27 | PLATFORM_BOOT_POLICY_FLAG30 |
          PLATFORM_BOOT_POLICY_FLAG42));
  platform_boot_policy_flag24_set(false);
  platform_boot_policy_flag25_set(false);
  assert((record.flags &
          (PLATFORM_BOOT_POLICY_FLAG24 | PLATFORM_BOOT_POLICY_FLAG25)) == 0);
  assert(platform_boot_policy_flag40_is_clear());
}

static void test_production_mode_comparison(void) {
  reset_fixture();
  production_mode_config = 1;
  assert(platform_boot_policy_compare_production_mode_byte(0) == 1);
  assert(platform_boot_policy_compare_production_mode_byte(1) == 1);
  production_mode_config = 0x101;
  assert(platform_boot_policy_compare_production_mode_byte(1) == 1);
  assert(platform_boot_policy_compare_production_mode_byte(0) == 0x101);
  assert(platform_boot_policy_compare_production_mode_byte(2) == 0x101);
}

static void test_entropy_seed_and_context(void) {
  reset_fixture();
  uint8_t seed[PLATFORM_BOOT_POLICY_ENTROPY_SEED_SIZE];
  for (size_t index = 0; index < sizeof(seed); ++index) {
    seed[index] = (uint8_t)(index * 7u + 3u);
  }
  platform_boot_policy_set_entropy_seed_span(
      seed,
      seed,
      seed + sizeof(seed),
      &platform_boot_policy_byte_type
  );
  assert((record.flags & PLATFORM_BOOT_POLICY_FLAG26) != 0);
  assert(memcmp((const void *)record.entropy_seed, seed, sizeof(seed)) == 0);

  platform_boot_policy_set_entropy_context_word(0xa1b2c3d4u);
  assert(entropy_context == 0xa1b2c3d4u);

  platform_boot_policy_set_entropy_seed_span(0, 0, 0, 0);
  assert((record.flags & PLATFORM_BOOT_POLICY_FLAG26) == 0);
  assert(memcmp((const void *)record.entropy_seed, seed, sizeof(seed)) == 0);
}

static void test_image_evaluation_metadata_publication(void) {
  static const uint8_t encoded_manifest[] = {
      0x30, 0x06, 0x16, 0x04, 'I', 'M', '4', 'M',
  };
  static const uint8_t manifest_digest[PLATFORM_BOOT_POLICY_ENTROPY_SEED_SIZE] = {
      0xf4, 0xa1, 0x47, 0x8a, 0xe2, 0x7c, 0x48, 0x32,
      0xfa, 0xa5, 0xd7, 0x8d, 0x17, 0x41, 0x25, 0xae,
      0xc5, 0xec, 0xc2, 0xda, 0x59, 0xcd, 0x58, 0x12,
      0xaa, 0xca, 0xe7, 0xa8, 0x21, 0xc1, 0x50, 0x36,
      0xb5, 0x8c, 0x29, 0x6a, 0x30, 0x55, 0x4d, 0x36,
      0x85, 0x7f, 0x75, 0x86, 0x02, 0x73, 0xac, 0x40,
  };

  reset_fixture();
  const AVPBootImageView image = {
      .image_type = AVP_BOOT_IMAGE_FOURCC('i', 'l', 'l', 'b'),
      .encoded_manifest_bytes = encoded_manifest,
      .encoded_manifest_size = sizeof(encoded_manifest),
      .payload_dpro = true,
  };
  const AVPBootImagePolicy policy = {
      .expected_manifest_digest = manifest_digest,
      .expected_manifest_digest_size = sizeof(manifest_digest),
  };
  assert(platform_boot_policy_publish_image_evaluation(&image, &policy) == 0);
  assert((record.flags & PLATFORM_BOOT_POLICY_FLAG24) != 0);
  assert((record.flags & PLATFORM_BOOT_POLICY_FLAG25) != 0);
  assert((record.flags & PLATFORM_BOOT_POLICY_FLAG26) != 0);
  assert(entropy_context == AVP_BOOT_IMAGE_FOURCC('i', 'l', 'l', 'b'));
  assert(memcmp((const void *)record.entropy_seed, manifest_digest,
                sizeof(manifest_digest)) == 0);

  AVPBootImageView manm = image;
  manm.payload_dpro = false;
  manm.payload_manm = true;
  assert(platform_boot_policy_publish_image_evaluation(&manm, &policy) == 0);
  assert((record.flags & PLATFORM_BOOT_POLICY_FLAG24) == 0);
  assert((record.flags & PLATFORM_BOOT_POLICY_FLAG25) == 0);

  AVPBootImageView bypass = image;
  bypass.flags = 1u << 3;
  const AVPBootImagePolicy no_expected = {0};
  assert(platform_boot_policy_publish_image_evaluation(
             &bypass, &no_expected) == 0);
  assert((record.flags & PLATFORM_BOOT_POLICY_FLAG25) != 0);

  const AVPBootImageView manifestless = {
      .image_type = AVP_BOOT_IMAGE_FOURCC('i', 'b', 's', 's'),
  };
  assert(platform_boot_policy_publish_image_evaluation(
             &manifestless, &no_expected) == 0);
  assert((record.flags & PLATFORM_BOOT_POLICY_FLAG26) == 0);
  assert((record.flags & PLATFORM_BOOT_POLICY_FLAG25) == 0);
}

int main(void) {
  test_platform_constants_and_hooks();
  test_policy_reset_profiles_and_clear_transport();
  test_bounds_tracking_and_policy();
  test_entropy_identity_publication();
  test_bounds_overflow_is_observable();
  test_flag_profile_and_accessors();
  test_production_mode_comparison();
  test_entropy_seed_and_context();
  test_image_evaluation_metadata_publication();
  platform_boot_policy_reset_transport();
  puts("platform boot policy owner tests passed");
  return 0;
}
