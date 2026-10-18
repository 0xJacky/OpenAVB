#include "avp/platform/platform_boot_policy_image_metadata.h"

#include "avp/crypto/support/corecrypto_mbedtls.h"
#include "avp/platform/platform_boot_policy_owner.h"


enum {
    BOOT_IMAGE_DESCRIPTOR_EXPECTED_DIGEST_BYPASS = 1u << 3,
};

static bool metadata_equal(
    const uint8_t *left,
    const uint8_t *right,
    size_t size
) {
    uint8_t difference = 0;
    for (size_t index = 0; index < size; ++index) {
        difference |= left[index] ^ right[index];
    }
    return difference == 0;
}

int platform_boot_policy_publish_image_evaluation(
    const AVPBootImageView *image,
    const AVPBootImagePolicy *policy
) {
    if (image == 0 || policy == 0 ||
        (policy->expected_manifest_digest == 0 &&
         policy->expected_manifest_digest_size != 0) ||
        (policy->expected_manifest_digest != 0 &&
         policy->expected_manifest_digest_size !=
             PLATFORM_BOOT_POLICY_ENTROPY_SEED_SIZE) ||
        (image->encoded_manifest_bytes == 0 &&
         image->encoded_manifest_size != 0)) {
        return -1;
    }

    uint8_t digest[PLATFORM_BOOT_POLICY_ENTROPY_SEED_SIZE];
    bool digest_present = image->encoded_manifest_bytes != 0;
    if (digest_present &&
        ccdigest(&avp_ccsha384_mbedtls_di, image->encoded_manifest_size,
                 image->encoded_manifest_bytes, digest) != 0) {
        return -1;
    }
    if (!digest_present && policy->expected_manifest_digest != 0) {
        return -1;
    }

    const bool digest_matches_expected =
        digest_present && policy->expected_manifest_digest != 0 &&
        metadata_equal(
            digest,
            policy->expected_manifest_digest,
            sizeof(digest)
        );
    const bool flag25 = !image->payload_manm &&
        (((image->flags & BOOT_IMAGE_DESCRIPTOR_EXPECTED_DIGEST_BYPASS) != 0) ||
         digest_matches_expected);

    /* The owner writes these four values consecutively only after
     * the image evaluator has accepted and materialized the payload. */
    platform_boot_policy_flag24_set(image->payload_dpro);
    platform_boot_policy_set_entropy_context_word(image->image_type);
    if (digest_present) {
        platform_boot_policy_set_entropy_seed_span(
            digest,
            digest,
            digest + sizeof(digest),
            &platform_boot_policy_byte_type
        );
    } else {
        platform_boot_policy_set_entropy_seed_span(0, 0, 0, 0);
    }
    platform_boot_policy_flag25_set(flag25);
    return 0;
}
