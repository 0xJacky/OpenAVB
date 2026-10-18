#include "avp/crypto/support/corecrypto_mbedtls.h"
#include "avp/image/img4/img4_decode_evaluation.h"

#include <stddef.h>
#include <stdint.h>


enum { IMG4_MAX_DIGEST_SIZE = 48 };

static const Img4DigestDescriptor *digest_descriptor(
    const Img4DecodeImplementation *implementation) {
    if (implementation == NULL || implementation->digest_configuration == NULL ||
        implementation->digest_configuration->descriptor == NULL) {
        return NULL;
    }
    return *implementation->digest_configuration->descriptor;
}

static void copy_bytes(void *destination, const void *source, size_t length) {
    uint8_t *output = destination;
    const uint8_t *input = source;
    for (size_t index = 0; index < length; ++index) {
        output[index] = input[index];
    }
}

static DERReturn copy_hash(
    const DERItem *encoded,
    const uint8_t *cached,
    uint8_t cache_valid,
    void *digest,
    DERSize digest_length,
    const Img4DecodeImplementation *implementation
) {
    const Img4DigestDescriptor *descriptor = digest_descriptor(implementation);
    if (encoded == NULL || digest == NULL || implementation == NULL ||
        implementation->compute_digest == NULL || descriptor == NULL) {
        return DR_ParamErr;
    }
    if (encoded->data == NULL || encoded->length == 0) {
        return DR_EndOfSequence;
    }
    if (digest_length > IMG4_MAX_DIGEST_SIZE ||
        descriptor->output_size > digest_length ||
        descriptor->output_size > IMG4_MAX_DIGEST_SIZE) {
        return DR_BufOverflow;
    }
    if (cache_valid == 1) {
        copy_bytes(digest, cached, descriptor->output_size);
        return DR_Success;
    }
    uint8_t temporary[IMG4_MAX_DIGEST_SIZE] = {0};
    const int result = implementation->compute_digest(
        encoded->data,
        encoded->length,
        temporary,
        digest_length,
        implementation
    );
    if (result != DR_Success) {
        return (DERReturn)result;
    }
    copy_bytes(digest, temporary, descriptor->output_size);
    return DR_Success;
}

DERReturn IMG4_DECODE_EVALUATION_PUBLIC Img4DecodeCopyPayloadHashWithCallback(
    const img4_decode_context_t *image,
    void *digest,
    DERSize digest_length,
    const Img4DecodeImplementation *implementation
) {
    if (image == NULL) {
        return DR_ParamErr;
    }
    if (image->payload.payload.data == NULL ||
        image->payload.payload.length == 0) {
        return DR_EndOfSequence;
    }
    return copy_hash(
        &image->encoded_payload,
        image->payload_hash,
        image->state[0],
        digest,
        digest_length,
        implementation
    );
}

DERReturn IMG4_DECODE_EVALUATION_PUBLIC Img4DecodeCopyManifestHashWithCallback(
    const img4_decode_context_t *image,
    void *digest,
    DERSize digest_length,
    const Img4DecodeImplementation *implementation
) {
    if (image == NULL) {
        return DR_ParamErr;
    }
    return copy_hash(
        &image->encoded_manifest,
        &image->manifest.reserved[0x10],
        image->state[1],
        digest,
        digest_length,
        implementation
    );
}

int IMG4_DECODE_EVALUATION_PUBLIC Img4DecodeComputeDigest(
    const void *data,
    DERSize data_length,
    void *digest,
    DERSize digest_length,
    const Img4DecodeImplementation *implementation
) {
    const Img4DigestDescriptor *descriptor = digest_descriptor(implementation);
    if (data == NULL || data_length == 0 || digest == NULL ||
        descriptor == NULL || descriptor->output_size != digest_length ||
        digest_length != AVP_CCSHA384_OUTPUT_SIZE) {
        return -1;
    }
    return ccdigest(&avp_ccsha384_mbedtls_di, data_length, data, digest) == 0
               ? DR_Success
               : -1;
}
