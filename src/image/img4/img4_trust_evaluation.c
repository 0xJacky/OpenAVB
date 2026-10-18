/*
 * The orchestration mirrors libimg4's callback ABI while DER traversal stays
 * in the shared Apple libDER compatibility layer.
 */
#include "avp/image/img4/img4_trust_evaluation.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>


enum {
    ASN1_BOOLEAN = 1,
    ASN1_INTEGER = 2,
    ASN1_OCTET_STRING = 4,
    ASN1_IA5_STRING = 22,
    IMG4_MAX_DIGEST_SIZE = 48,
};

static const Img4DigestDescriptor *digest_descriptor(
    const Img4DecodeImplementation *implementation) {
    if (implementation == NULL || implementation->digest_configuration == NULL ||
        implementation->digest_configuration->descriptor == NULL) {
        return NULL;
    }
    return *implementation->digest_configuration->descriptor;
}

static Img4VerifyChainCallback verify_chain_callback(
    const Img4DecodeImplementation *implementation) {
    return (Img4VerifyChainCallback)implementation->reserved_08;
}

static Img4EvaluateCertificatePropertiesCallback certificate_callback(
    const Img4DecodeImplementation *implementation) {
    return (Img4EvaluateCertificatePropertiesCallback)
        implementation->reserved_18;
}

static bool digests_equal(
    const uint8_t *left,
    const uint8_t *right,
    size_t length
) {
    uint8_t difference = 0;
    for (size_t index = 0; index < length; ++index) {
        difference |= left[index] ^ right[index];
    }
    return difference == 0;
}

static DERReturn evaluate_dictionary(
    const DERItem *dictionary,
    bool object_property,
    Img4EvaluatePropertyCallback callback,
    void *context
) {
    if (dictionary == NULL || callback == NULL) {
        return DR_ParamErr;
    }
    DERSequence sequence;
    DERReturn result = DERDecodeSeqContentInit(dictionary, &sequence);
    if (result != DR_Success) {
        return result;
    }
    for (;;) {
        DERDecodedInfo decoded;
        result = DERDecodeSeqNext(&sequence, &decoded);
        if (result == DR_EndOfSequence) {
            return DR_Success;
        }
        if (result != DR_Success || (decoded.tag >> 61) != 7u) {
            return result == DR_Success ? DR_UnexpectedTag : result;
        }
        Img4DecodedProperty property;
        result = DERImg4DecodeProperty(
            &decoded.content,
            decoded.tag,
            &property
        );
        if (result != DR_Success) {
            return result;
        }
        if (property.value.tag != ASN1_BOOLEAN &&
            property.value.tag != ASN1_INTEGER &&
            property.value.tag != ASN1_OCTET_STRING &&
            property.value.tag != ASN1_IA5_STRING) {
            return DR_UnexpectedTag;
        }
        if (property.value.item.length > UINT32_MAX) {
            return DR_BufOverflow;
        }
        const Img4PropertyValue value = {
            property.value.item.data,
            (uint32_t)property.value.item.length,
            property.value.tag,
        };
        result = callback(decoded.tag, &value, object_property, context);
        if (result != DR_Success) {
            return result;
        }
    }
}

static DERReturn select_object_dictionary(
    unsigned int object_type,
    img4_decode_context_t *image,
    const Img4PropertyCallbacks *callbacks,
    const DERItem *dictionary,
    void *context
) {
    Img4DecodedProperty property;
    const DERTag property_tag = Img4DecodePrivateTag(object_type);
    DERReturn result;
    if (callbacks->find_property != NULL) {
        result = callbacks->find_property(
            dictionary,
            property_tag,
            ASN1_CONSTR_SET,
            &property,
            context
        );
    } else {
        result = DERImg4DecodeFindProperty(
            dictionary,
            property_tag,
            ASN1_CONSTR_SET,
            &property
        );
    }
    if (result == DR_Success) {
        DERItem *object_properties = (DERItem *)&image->reserved_28[0x20];
        *object_properties = property.value.item;
    }
    return result;
}

static DERReturn compute_digest(
    const DERItem *item,
    uint8_t *output,
    size_t output_size,
    const Img4DecodeImplementation *implementation
) {
    if (item->data == NULL || item->length == 0) {
        return DR_ParamErr;
    }
    return implementation->compute_digest(
        item->data,
        item->length,
        output,
        output_size,
        implementation
    );
}

DERReturn IMG4_TRUST_EVALUATION_PUBLIC
Img4DecodePerformTrustEvaluationWithCallbacks(
    unsigned int object_type,
    img4_decode_context_t *image,
    const Img4PropertyCallbacks *property_callbacks,
    const Img4DecodeImplementation *implementation,
    void *callback_context
) {
    return _Img4DecodePerformTrustEvaluationWithCallbacksInternal(
        object_type,
        image,
        property_callbacks,
        implementation,
        false,
        callback_context
    );
}

DERReturn IMG4_TRUST_EVALUATION_PUBLIC
_Img4DecodePerformTrustEvaluationWithCallbacksInternal(
    unsigned int object_type,
    img4_decode_context_t *image,
    const Img4PropertyCallbacks *property_callbacks,
    const Img4DecodeImplementation *implementation,
    bool manifest_only,
    void *callback_context
) {
    const Img4DigestDescriptor *descriptor = digest_descriptor(implementation);
    if (image == NULL || property_callbacks == NULL ||
        property_callbacks->evaluate_property == NULL ||
        implementation == NULL || implementation->compute_digest == NULL ||
        verify_chain_callback(implementation) == NULL ||
        implementation->verify_signature == NULL ||
        certificate_callback(implementation) == NULL || descriptor == NULL) {
        return DR_ParamErr;
    }
    const size_t digest_size = descriptor->output_size;
    if (digest_size == 0 || digest_size > IMG4_MAX_DIGEST_SIZE) {
        return DR_BufOverflow;
    }

    DERReturn result = compute_digest(
        &image->encoded_manifest,
        &image->manifest.reserved[0x10],
        digest_size,
        implementation
    );
    if (result != DR_Success) {
        return result;
    }
    image->state[1] = 1;

    bool cryptographic_evaluation_required = true;
    if (property_callbacks->get_trusted_manifest_hash != NULL) {
        const uint8_t *trusted_hash =
            property_callbacks->get_trusted_manifest_hash(
                image,
                callback_context
            );
        if (trusted_hash != NULL &&
            digests_equal(trusted_hash, &image->manifest.reserved[0x10],
                          digest_size)) {
            cryptographic_evaluation_required = false;
        }
    }

    if (cryptographic_evaluation_required) {
        const uint8_t *leaf_public_key = NULL;
        DERSize leaf_public_key_length = 0;
        DERItem *manifest_extension = (DERItem *)&image->manifest.reserved[0];
        if (image->manifest.certificates.length > UINT32_MAX) {
            return DR_BufOverflow;
        }
        result = verify_chain_callback(implementation)(
            image->manifest.certificates.data,
            (unsigned int)image->manifest.certificates.length,
            &leaf_public_key,
            &leaf_public_key_length,
            (const uint8_t **)&manifest_extension->data,
            &manifest_extension->length,
            implementation,
            callback_context
        );
        if (result != DR_Success) {
            return result;
        }
        result = compute_digest(
            &image->manifest.properties,
            &image->manifest.reserved[0x40],
            digest_size,
            implementation
        );
        if (result != DR_Success) {
            return result;
        }
        if (leaf_public_key_length > UINT32_MAX ||
            image->manifest.signature.length > UINT32_MAX) {
            return DR_BufOverflow;
        }
        result = implementation->verify_signature(
            leaf_public_key,
            (unsigned int)leaf_public_key_length,
            image->manifest.signature.data,
            (unsigned int)image->manifest.signature.length,
            &image->manifest.reserved[0x40],
            (unsigned int)digest_size,
            implementation,
            callback_context
        );
        if (result != DR_Success) {
            return result;
        }
    }

    DERItem object_dictionary;
    DERItem manifest_properties;
    result = DERImg4DecodeParseManifestProperties(
        image,
        &object_dictionary,
        &manifest_properties
    );
    if (result != DR_Success) {
        return result;
    }
    if (!manifest_only) {
        result = select_object_dictionary(
            object_type,
            image,
            property_callbacks,
            &object_dictionary,
            callback_context
        );
        if (result != DR_Success) {
            return result;
        }
    }

    if (cryptographic_evaluation_required) {
        result = certificate_callback(implementation)(
            image,
            callback_context
        );
        if (result != DR_Success) {
            return result;
        }
    }

    if (image->payload.payload.data != NULL &&
        image->payload.payload.length != 0) {
        result = compute_digest(
            &image->encoded_payload,
            image->payload_hash,
            digest_size,
            implementation
        );
        if (result != DR_Success) {
            return result;
        }
        image->state[0] = 1;
    }

    result = evaluate_dictionary(
        &manifest_properties,
        false,
        property_callbacks->evaluate_property,
        callback_context
    );
    if (result != DR_Success || manifest_only) {
        return result;
    }
    const DERItem *object_properties =
        (const DERItem *)&image->reserved_28[0x20];
    return evaluate_dictionary(
        object_properties,
        true,
        property_callbacks->evaluate_property,
        callback_context
    );
}
