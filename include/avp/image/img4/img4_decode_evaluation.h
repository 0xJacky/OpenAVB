#pragma once

#include "avp/image/img4/img4_trust_evaluation.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


typedef struct {
    const DERItem *item;
    uint64_t guard;
} Img4StackGuardPrologueState;

#define IMG4_DECODE_EVALUATION_PUBLIC __attribute__((used, retain))

IMG4_DECODE_EVALUATION_PUBLIC bool
img4_decode_stack_guard_compare_saved_x8(uint64_t saved_guard);
IMG4_DECODE_EVALUATION_PUBLIC uint64_t
img4_decode_stack_guard_load_to_x8(void);
IMG4_DECODE_EVALUATION_PUBLIC Img4StackGuardPrologueState
img4_decode_stack_guard_preserve_x0_load_to_x9(const DERItem *item);
IMG4_DECODE_EVALUATION_PUBLIC bool
img4_decode_stack_guard_compare_saved_x8_clone(uint64_t saved_guard);
IMG4_DECODE_EVALUATION_PUBLIC void *
img4_decode_property_outlined_bind_output_and_save_stack_guard(void *output);
IMG4_DECODE_EVALUATION_PUBLIC uint64_t
img4_decode_stack_guard_load_to_x8_clone(void);
IMG4_DECODE_EVALUATION_PUBLIC void
img4_decode_property_outlined_zero_decode_scratch(void *scratch);
IMG4_DECODE_EVALUATION_PUBLIC DERReturn
img4_decode_property_outlined_decode_private_tag(
    const DERItem *property,
    uint32_t fourcc,
    Img4DecodedProperty *decoded
);
IMG4_DECODE_EVALUATION_PUBLIC const DERItem *
img4_decode_property_outlined_prepare_value_parse(
    const Img4DecodedProperty *decoded);

IMG4_DECODE_EVALUATION_PUBLIC DERReturn Img4DecodeCopyPayloadHashWithCallback(
    const img4_decode_context_t *image,
    void *digest,
    DERSize digest_length,
    const Img4DecodeImplementation *implementation
);
IMG4_DECODE_EVALUATION_PUBLIC DERReturn Img4DecodeCopyManifestHashWithCallback(
    const img4_decode_context_t *image,
    void *digest,
    DERSize digest_length,
    const Img4DecodeImplementation *implementation
);
IMG4_DECODE_EVALUATION_PUBLIC int Img4DecodeComputeDigest(
    const void *data,
    DERSize data_length,
    void *digest,
    DERSize digest_length,
    const Img4DecodeImplementation *implementation
);

IMG4_DECODE_EVALUATION_PUBLIC DERReturn
Img4DecodeEvaluateCertificateProperties(img4_decode_context_t *image);
IMG4_DECODE_EVALUATION_PUBLIC DERReturn
Img4DecodeEvaluateCertificatePropertiesInternal(
    img4_decode_context_t *image,
    DERItem *manifest_extension
);
IMG4_DECODE_EVALUATION_PUBLIC DERReturn
Img4DecodeEvaluateDictionaryProperties(
    const DERItem *dictionary,
    unsigned dictionary_kind,
    Img4EvaluatePropertyCallback callback,
    void *context
);
