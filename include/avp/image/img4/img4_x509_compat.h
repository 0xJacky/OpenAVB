#pragma once

#include "avp/image/der/apple_libder.h"

#include <stdint.h>

typedef struct {
    DERItem tbs_certificate;
    DERItem signature_algorithm;
    DERItem signature_value;
} Img4CertificateFields;

typedef struct {
    DERItem version;
    DERItem serial_number;
    DERItem signature;
    DERItem issuer;
    DERItem validity;
    DERItem subject;
    DERItem subject_public_key_info;
    DERItem issuer_unique_id;
    DERItem subject_unique_id;
    DERItem extensions;
} Img4TBSCertificateFields;

#define IMG4_X509_PUBLIC __attribute__((used, retain))

IMG4_X509_PUBLIC int parse_chain(
    const DERItem *certificates,
    unsigned int certificate_count,
    Img4CertificateFields *certificate_fields,
    Img4TBSCertificateFields *tbs_fields,
    DERItem *subject_public_keys,
    DERItem *extensions
);

IMG4_X509_PUBLIC int _crack_chain_with_anchor(
    const DERItem *encoded_chain,
    DERItem *certificate_chain,
    unsigned int certificate_count
);

IMG4_X509_PUBLIC int parse_extensions(
    const Img4TBSCertificateFields *certificate,
    DERItem *extension_value
);

IMG4_X509_PUBLIC DERReturn parse_algorithm_identifier(
    const DERItem *content,
    DERItem algorithm[2]
);
