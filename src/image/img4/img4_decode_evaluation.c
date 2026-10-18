#include "avp/image/img4/img4_decode_evaluation.h"

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


enum {
    ASN1_BOOLEAN = 1,
    ASN1_INTEGER = 2,
    ASN1_OCTET_STRING = 4,
    ASN1_IA5_STRING = 22,
    IMG4_MANIFEST_EXTENSION_MAGIC = IMG4_FOURCC('m', 'a', 'n', 'x'),
    IMG4_OBJECT_PROPERTIES_MAGIC = IMG4_FOURCC('O', 'B', 'J', 'P'),
};

static bool bytes_equal(
    const uint8_t *left,
    const uint8_t *right,
    size_t length
) {
    for (size_t index = 0; index < length; ++index) {
        if (left[index] != right[index]) {
            return false;
        }
    }
    return true;
}

static DERReturn compare_certificate_dictionary(
    const DERItem *requirements,
    const DERItem *actual
) {
    DERSequence sequence;
    DERReturn result = DERDecodeSeqContentInit(requirements, &sequence);
    if (result != DR_Success) {
        return result;
    }
    for (;;) {
        DERDecodedInfo encoded_requirement;
        result = DERDecodeSeqNext(&sequence, &encoded_requirement);
        if (result == DR_EndOfSequence) {
            return DR_Success;
        }
        if (result != DR_Success) {
            return (DERReturn)-1;
        }
        Img4DecodedProperty requirement;
        result = DERImg4DecodeProperty(
            &encoded_requirement.content,
            encoded_requirement.tag,
            &requirement
        );
        if (result != DR_Success) {
            return result;
        }
        DERItem actual_encoded;
        const DERReturn find_result = DERImg4DecodeContentFindItemWithTag(
            actual,
            encoded_requirement.tag,
            &actual_encoded
        );
        if (requirement.value.tag == UINT64_C(0xa000000000000000)) {
            continue;
        }
        if (requirement.value.tag == UINT64_C(0xa000000000000001)) {
            if (find_result != DR_EndOfSequence) {
                return DR_UnexpectedTag;
            }
            continue;
        }
        if (requirement.value.tag != ASN1_BOOLEAN &&
            requirement.value.tag != ASN1_INTEGER &&
            requirement.value.tag != ASN1_OCTET_STRING) {
            return DR_UnexpectedTag;
        }
        if (find_result != DR_Success) {
            return find_result;
        }
        Img4DecodedProperty actual_property;
        result = DERImg4DecodeProperty(
            &actual_encoded,
            encoded_requirement.tag,
            &actual_property
        );
        if (result != DR_Success) {
            return result;
        }
        if (actual_property.value.tag != requirement.value.tag ||
            actual_property.value.item.length != requirement.value.item.length ||
            !bytes_equal(actual_property.value.item.data,
                         requirement.value.item.data,
                         requirement.value.item.length)) {
            return (DERReturn)-1;
        }
    }
}

DERReturn IMG4_DECODE_EVALUATION_PUBLIC
Img4DecodeEvaluateCertificatePropertiesInternal(
    img4_decode_context_t *image,
    DERItem *manifest_extension
) {
    if (image == NULL) {
        return DR_ParamErr;
    }
    const DERItem *certificate_properties =
        (const DERItem *)&image->manifest.reserved[0];
    if (certificate_properties->length == 0) {
        return DR_DecodeError;
    }
    DERTag outer_tag;
    DERSequence outer;
    DERReturn result = DERDecodeSeqInit(
        certificate_properties,
        &outer_tag,
        &outer
    );
    if (result != DR_Success) {
        return result;
    }
    if (outer_tag != ASN1_CONSTR_SET) {
        return DR_UnexpectedTag;
    }

    const DERTag manifest_tag =
        Img4DecodePrivateTag(IMG4_MANIFEST_PROPERTIES_MAGIC);
    const DERTag object_tag =
        Img4DecodePrivateTag(IMG4_OBJECT_PROPERTIES_MAGIC);
    const DERTag extension_tag =
        Img4DecodePrivateTag(IMG4_MANIFEST_EXTENSION_MAGIC);
    for (;;) {
        DERDecodedInfo encoded_dictionary;
        result = DERDecodeSeqNext(&outer, &encoded_dictionary);
        if (result == DR_EndOfSequence) {
            return DR_Success;
        }
        if (result != DR_Success) {
            return (DERReturn)-1;
        }
        if (encoded_dictionary.tag == extension_tag) {
            if (manifest_extension != NULL) {
                *manifest_extension = encoded_dictionary.content;
            }
            continue;
        }
        const DERItem *actual;
        if (encoded_dictionary.tag == manifest_tag) {
            actual = (const DERItem *)&image->reserved_28[0x10];
        } else if (encoded_dictionary.tag == object_tag) {
            actual = (const DERItem *)&image->reserved_28[0x20];
            if (actual->data == NULL && actual->length == 0) {
                continue;
            }
        } else {
            return DR_UnexpectedTag;
        }
        Img4DecodedProperty dictionary;
        result = DERImg4DecodeProperty(
            &encoded_dictionary.content,
            encoded_dictionary.tag,
            &dictionary
        );
        if (result != DR_Success) {
            return result;
        }
        if (dictionary.value.tag != ASN1_CONSTR_SET) {
            return DR_UnexpectedTag;
        }
        result = compare_certificate_dictionary(
            &dictionary.value.item,
            actual
        );
        if (result != DR_Success) {
            return result;
        }
    }
}

DERReturn IMG4_DECODE_EVALUATION_PUBLIC
Img4DecodeEvaluateCertificateProperties(img4_decode_context_t *image) {
    return Img4DecodeEvaluateCertificatePropertiesInternal(image, NULL);
}

DERReturn IMG4_DECODE_EVALUATION_PUBLIC
Img4DecodeEvaluateDictionaryProperties(
    const DERItem *dictionary,
    unsigned dictionary_kind,
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
        DERDecodedInfo encoded_property;
        result = DERDecodeSeqNext(&sequence, &encoded_property);
        if (result == DR_EndOfSequence) {
            return DR_Success;
        }
        if (result != DR_Success || (encoded_property.tag >> 61) != 7u) {
            return result == DR_Success ? DR_UnexpectedTag : result;
        }
        Img4DecodedProperty property;
        result = DERImg4DecodeProperty(
            &encoded_property.content,
            encoded_property.tag,
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
        result = callback(
            encoded_property.tag,
            &value,
            dictionary_kind != 0,
            context
        );
        if (result != DR_Success) {
            return result;
        }
    }
}
