#include "avp/crypto/support/corecrypto_mbedtls.h"

#include "mbedtls/memory_buffer_alloc.h"

#include <assert.h>
#include <string.h>


static uint8_t hex_digit(char character) {
    if (character >= '0' && character <= '9') {
        return (uint8_t)(character - '0');
    }
    return (uint8_t)(character - 'a' + 10);
}

static size_t decode_hex(const char *hex, uint8_t *output, size_t capacity) {
    const size_t length = strlen(hex);
    assert((length & 1) == 0 && length / 2 <= capacity);
    for (size_t index = 0; index < length / 2; ++index) {
        output[index] = (uint8_t)((hex_digit(hex[index * 2]) << 4) |
            hex_digit(hex[index * 2 + 1]));
    }
    return length / 2;
}

static void assert_hex_equal(
    const uint8_t *actual,
    size_t size,
    const char *expected_hex
) {
    uint8_t expected[AVP_CCSHA384_OUTPUT_SIZE];
    assert(decode_hex(expected_hex, expected, sizeof(expected)) == size);
    assert(memcmp(actual, expected, size) == 0);
}

static void test_comparison(void) {
    const uint8_t equal_left[] = { 0x00, 0x80, 0xff, 0x42 };
    const uint8_t equal_right[] = { 0x00, 0x80, 0xff, 0x42 };
    const uint8_t different[] = { 0x00, 0x80, 0x7f, 0x42 };

    assert(cc_cmp_safe_internal(sizeof(equal_left), equal_left, equal_right) == 0);
    assert(cc_cmp_safe(sizeof(equal_left), equal_left, different) == 1);
    assert(cc_cmp_safe(0, equal_left, equal_right) == 1);
}

static void test_digest(void) {
    static const char expected[] =
        "cb00753f45a35e8bb5a03d699ac65007272c32ab0eded163"
        "1a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7";
    uint8_t digest[AVP_CCSHA384_OUTPUT_SIZE];

    assert(ccdigest(
        &avp_ccsha384_mbedtls_di,
        3,
        "abc",
        digest
    ) == 0);
    assert_hex_equal(digest, sizeof(digest), expected);

    AVPCCDigestContext context;
    assert(ccdigest_init_internal(&avp_ccsha384_mbedtls_di, &context) == 0);
    assert(ccdigest_update_internal(
        &avp_ccsha384_mbedtls_di,
        &context,
        1,
        "a"
    ) == 0);
    assert(ccdigest_update_internal(
        &avp_ccsha384_mbedtls_di,
        &context,
        2,
        "bc"
    ) == 0);
    assert(ccdigest_finish_mbedtls(
        &avp_ccsha384_mbedtls_di,
        &context,
        digest
    ) == 0);
    assert_hex_equal(digest, sizeof(digest), expected);
    ccdigest_free_mbedtls(&context);
}

static void test_hmac(void) {
    static const char expected[] =
        "afd03944d84895626b0825f4ab46907f15f9dadbe4101ec6"
        "82aa034c7cebc59cfaea9ea9076ede7f4af152e8b2fa9cb6";
    uint8_t key[20];
    uint8_t mac[AVP_CCSHA384_OUTPUT_SIZE];
    memset(key, 0x0b, sizeof(key));

    assert(cchmac_internal(
        &avp_ccsha384_mbedtls_di,
        sizeof(key),
        key,
        8,
        "Hi There",
        mac
    ) == 0);
    assert_hex_equal(mac, sizeof(mac), expected);

    AVPCCHmacContext context;
    assert(cchmac_init_internal(
        &avp_ccsha384_mbedtls_di,
        &context,
        sizeof(key),
        key
    ) == 0);
    assert(cchmac_update_internal(
        &avp_ccsha384_mbedtls_di,
        &context,
        3,
        "Hi "
    ) == 0);
    assert(cchmac_update_internal(
        &avp_ccsha384_mbedtls_di,
        &context,
        5,
        "There"
    ) == 0);
    assert(cchmac_final_internal(
        &avp_ccsha384_mbedtls_di,
        &context,
        mac
    ) == 0);
    assert_hex_equal(mac, sizeof(mac), expected);
    cchmac_free_mbedtls(&context);
}

static void test_hmac_long_key(void) {
    static const char expected[] =
        "4ece084485813e9088d2c63a041bc5b44f9ef1012a2b588f"
        "3cd11f05033ac4c60c2ef6ab4030fe8296248df163f44952";
    static const char message[] =
        "Test Using Larger Than Block-Size Key - Hash Key First";
    uint8_t key[131];
    uint8_t mac[AVP_CCSHA384_OUTPUT_SIZE];
    memset(key, 0xaa, sizeof(key));

    assert(cchmac_internal(
        &avp_ccsha384_mbedtls_di,
        sizeof(key),
        key,
        sizeof(message) - 1,
        message,
        mac
    ) == 0);
    assert_hex_equal(mac, sizeof(mac), expected);
}

int main(void) {
    static uint8_t heap[8192];
    mbedtls_memory_buffer_alloc_init(heap, sizeof(heap));
    test_comparison();
    test_digest();
    test_hmac();
    test_hmac_long_key();
    return 0;
}

