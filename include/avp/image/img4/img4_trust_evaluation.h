#pragma once

#include "avp/image/img4/img4_context_compat.h"
#include "avp/image/img4/img4_trust_compat.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


typedef struct {
    const uint8_t *data;
    uint32_t length;
    DERTag tag;
} Img4PropertyValue;

typedef DERReturn (*Img4EvaluatePropertyCallback)(
    DERTag property_tag,
    const Img4PropertyValue *value,
    bool object_property,
    void *context
);

typedef const uint8_t *(*Img4GetTrustedManifestHashCallback)(
    const img4_decode_context_t *image,
    void *context
);

typedef DERReturn (*Img4FindPropertyCallback)(
    const DERItem *dictionary,
    DERTag expected_tag,
    DERTag value_tag,
    Img4DecodedProperty *property,
    void *context
);

typedef struct Img4PropertyCallbacks {
    Img4EvaluatePropertyCallback evaluate_property;
    Img4GetTrustedManifestHashCallback get_trusted_manifest_hash;
    Img4FindPropertyCallback find_property;
} Img4PropertyCallbacks;

typedef int (*Img4VerifyChainCallback)(
    const uint8_t *chain,
    unsigned int chain_length,
    const uint8_t **leaf_public_key,
    DERSize *leaf_public_key_length,
    const uint8_t **extension,
    DERSize *extension_length,
    const Img4DecodeImplementation *implementation,
    void *context
);

typedef int (*Img4EvaluateCertificatePropertiesCallback)(
    img4_decode_context_t *image,
    void *context
);

typedef int (*Img4CrackChainCallback)(
    const DERItem *chain,
    DERItem *certificates,
    unsigned int certificate_count
);

#define IMG4_TRUST_EVALUATION_PUBLIC __attribute__((used, retain))

IMG4_TRUST_EVALUATION_PUBLIC DERReturn
Img4DecodePerformTrustEvaluationWithCallbacks(
    unsigned int object_type,
    img4_decode_context_t *image,
    const Img4PropertyCallbacks *property_callbacks,
    const Img4DecodeImplementation *implementation,
    void *callback_context
);

IMG4_TRUST_EVALUATION_PUBLIC DERReturn
_Img4DecodePerformTrustEvaluationWithCallbacksInternal(
    unsigned int object_type,
    img4_decode_context_t *image,
    const Img4PropertyCallbacks *property_callbacks,
    const Img4DecodeImplementation *implementation,
    bool manifest_only,
    void *callback_context
);

IMG4_TRUST_EVALUATION_PUBLIC int verify_chain_img4_v2_with_crack_callback(
    const uint8_t *chain,
    unsigned int chain_length,
    const uint8_t **leaf_public_key,
    DERSize *leaf_public_key_length,
    const uint8_t **extension,
    DERSize *extension_length,
    const Img4DecodeImplementation *implementation,
    Img4CrackChainCallback crack_callback
);

IMG4_TRUST_EVALUATION_PUBLIC int verify_chain_img4_v2(
    const uint8_t *chain,
    unsigned int chain_length,
    const uint8_t **leaf_public_key,
    DERSize *leaf_public_key_length,
    const uint8_t **extension,
    DERSize *extension_length,
    const Img4DecodeImplementation *implementation
);

IMG4_TRUST_EVALUATION_PUBLIC int crack_chain_rsa4k_sha384(
    const DERItem *chain,
    DERItem *certificates,
    unsigned int certificate_count
);

IMG4_TRUST_EVALUATION_PUBLIC const DERItem *img4_rsa4k_sha384_trust_anchor(void);
