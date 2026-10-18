#include "avp/image/der/apple_libder.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    DERItem integer;
    DERItem optional_boolean;
    DERItem oid_encoding;
} TestSequence;

static DERItem item(const uint8_t *bytes, size_t count) {
    return (DERItem){(DERByte *)bytes, count};
}

static void test_decode_complete_and_partial_items(void) {
    uint8_t complete_bytes[] = {0x04, 0x03, 0xaa, 0xbb, 0xcc};
    DERItem complete = item(complete_bytes, sizeof(complete_bytes));
    DERDecodedInfo decoded = {0};
    assert(DERDecodeItem(&complete, &decoded) == DR_Success);
    assert(decoded.tag == 0x04);
    assert(decoded.content.length == 3);
    assert(decoded.content.data[2] == 0xcc);

    uint8_t partial_bytes[] = {0x04, 0x82, 0x01, 0x00, 0xaa, 0xbb};
    DERItem partial = item(partial_bytes, sizeof(partial_bytes));
    DERSize encoded_length = 0;
    assert(
        DERDecodeItemPartialBufferGetLength(
            &partial,
            &decoded,
            &encoded_length
        ) == DR_Success
    );
    assert(encoded_length == 256);
    assert(decoded.content.length == 2);
    assert(DERDecodeItem(&partial, &decoded) == DR_DecodeError);

    uint8_t noncanonical_length[] = {0x04, 0x81, 0x7f};
    DERItem noncanonical = item(
        noncanonical_length,
        sizeof(noncanonical_length)
    );
    assert(DERDecodeItem(&noncanonical, &decoded) == DR_DecodeError);
}

static void test_scalar_parsers(void) {
    uint8_t bit_bytes[] = {3, 0xa8};
    DERItem bits = item(bit_bytes, sizeof(bit_bytes));
    DERItem payload = {0};
    DERByte unused = 0;
    assert(DERParseBitString(&bits, &payload, &unused) == DR_Success);
    assert(unused == 3 && payload.length == 1 && payload.data[0] == 0xa8);
    bit_bytes[1] = 0xa9;
    assert(DERParseBitString(&bits, &payload, &unused) == DR_DecodeError);

    uint8_t true_byte[] = {0xff};
    DERItem boolean = item(true_byte, sizeof(true_byte));
    bool value = false;
    assert(DERParseBoolean(&boolean, &value) == DR_Success && value);
    DERItem empty = item(true_byte, 0);
    assert(
        DERParseBooleanWithDefault(&empty, true, &value) == DR_Success && value
    );

    uint8_t integer_bytes[] = {0x00, 0x80, 0x00, 0x00, 0x00};
    DERItem integer = item(integer_bytes, sizeof(integer_bytes));
    uint32_t integer32 = 0;
    uint64_t integer64 = 0;
    assert(DERParseInteger(&integer, &integer32) == DR_Success);
    assert(integer32 == UINT32_C(0x80000000));
    assert(DERParseInteger64(&integer, &integer64) == DR_Success);
    assert(integer64 == UINT64_C(0x80000000));

    uint8_t redundant_zero_bytes[] = {0x00, 0x7f};
    DERItem redundant_zero = item(
        redundant_zero_bytes,
        sizeof(redundant_zero_bytes)
    );
    assert(DERParseInteger64(&redundant_zero, &integer64) == DR_DecodeError);

    uint8_t overflow_bytes[] = {0x01, 0, 0, 0, 0};
    DERItem overflow = item(overflow_bytes, sizeof(overflow_bytes));
    assert(DERParseInteger(&overflow, &integer32) == DR_BufOverflow);
}

static void test_sequence_iteration_and_templates(void) {
    uint8_t sequence_bytes[] = {
        0x30, 0x0a,
        0x02, 0x01, 0x2a,
        0x01, 0x01, 0xff,
        0x06, 0x02, 0x2a, 0x03,
    };
    DERItem encoded = item(sequence_bytes, sizeof(sequence_bytes));
    DERTag tag = 0;
    DERSequence sequence = {0};
    assert(DERDecodeSeqInit(&encoded, &tag, &sequence) == DR_Success);
    assert(tag == ASN1_CONSTR_SEQUENCE);

    DERDecodedInfo decoded = {0};
    assert(DERDecodeSeqNext(&sequence, &decoded) == DR_Success);
    assert(decoded.tag == 0x02 && decoded.content.data[0] == 0x2a);
    assert(DERDecodeSeqNext(&sequence, &decoded) == DR_Success);
    assert(decoded.tag == 0x01 && decoded.content.data[0] == 0xff);
    assert(DERDecodeSeqNext(&sequence, &decoded) == DR_Success);
    assert(decoded.tag == 0x06 && decoded.content.length == 2);
    assert(DERDecodeSeqNext(&sequence, &decoded) == DR_EndOfSequence);

    const DERItemSpec specs[] = {
        {offsetof(TestSequence, integer), 0x02, DER_DEC_NO_OPTS},
        {offsetof(TestSequence, optional_boolean), 0x01, DER_DEC_OPTIONAL},
        {offsetof(TestSequence, oid_encoding), 0x06, DER_DEC_SAVE_DER},
    };
    TestSequence output;
    assert(
        DERParseSequence(
            &encoded,
            sizeof(specs) / sizeof(specs[0]),
            specs,
            &output,
            sizeof(output)
        ) == DR_Success
    );
    assert(output.integer.length == 1 && output.integer.data[0] == 0x2a);
    assert(
        output.optional_boolean.length == 1 &&
        output.optional_boolean.data[0] == 0xff
    );
    assert(output.oid_encoding.length == 4);
    assert(output.oid_encoding.data[0] == 0x06);
}

static void test_sequence_contracts_and_oid_comparison(void) {
    uint8_t incomplete_bytes[] = {0x30, 0x03, 0x02, 0x01, 0x01};
    DERItem encoded = item(incomplete_bytes, sizeof(incomplete_bytes));
    const DERItemSpec specs[] = {
        {offsetof(TestSequence, integer), 0x02, DER_DEC_NO_OPTS},
        {offsetof(TestSequence, optional_boolean), 0x01, DER_DEC_NO_OPTS},
    };
    TestSequence output;
    assert(
        DERParseSequence(
            &encoded,
            sizeof(specs) / sizeof(specs[0]),
            specs,
            &output,
            sizeof(output)
        ) == DR_IncompleteSeq
    );

    uint8_t oid_a_bytes[] = {0x2a, 0x86, 0x48};
    uint8_t oid_b_bytes[] = {0x2a, 0x86, 0x48};
    uint8_t oid_c_bytes[] = {0x2a, 0x86, 0x49};
    DERItem oid_a = item(oid_a_bytes, sizeof(oid_a_bytes));
    DERItem oid_b = item(oid_b_bytes, sizeof(oid_b_bytes));
    DERItem oid_c = item(oid_c_bytes, sizeof(oid_c_bytes));
    assert(DEROidCompare(&oid_a, &oid_b));
    assert(!DEROidCompare(&oid_a, &oid_c));
    assert(!DEROidCompare(NULL, &oid_a));
}

int main(void) {
    test_decode_complete_and_partial_items();
    test_scalar_parsers();
    test_sequence_iteration_and_templates();
    test_sequence_contracts_and_oid_comparison();
    puts("apple_libder_tests: ok");
}
